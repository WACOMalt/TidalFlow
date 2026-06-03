#include <thread>
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <variant>
#include <unordered_map>
#include <utility>
#include <mutex>
#include <queue>
#include <cstring>

#include "blockingconcurrentqueue.h"

#include "ultramodern/ultra64.h"
#include "ultramodern/ultramodern.hpp"

#include "ultramodern/rsp.hpp"
#include "ultramodern/renderer_context.hpp"

// Forward declaration for display list fix function (defined in sp.cpp)
extern "C" void fix_display_list_for_rt64(uint8_t* rdram, uint32_t data_ptr);

static ultramodern::events::callbacks_t events_callbacks{};

void ultramodern::events::set_callbacks(const ultramodern::events::callbacks_t& callbacks) {
    events_callbacks = callbacks;
}

struct SpTaskAction {
    OSTask task;
};

struct ScreenUpdateAction {
    ultramodern::renderer::ViRegs regs;
};

struct UpdateConfigAction {
};

using Action = std::variant<SpTaskAction, ScreenUpdateAction, UpdateConfigAction>;

struct ViState {
    const OSViMode* mode;
    PTR(void) framebuffer;
    PTR(OSMesg) mq;
    OSMesg msg;
    uint32_t state;
    uint32_t control;
    int retrace_count = 1;
};

#define VI_STATE_BLACK 0x20
#define VI_STATE_REPEATLINE 0x40

static struct {
    struct {
        std::thread thread;
        int cur_state;
        int field;
        ViState states[2];
        ultramodern::renderer::ViRegs regs;
        ultramodern::renderer::ViRegs update_screen_regs;

        ViState* get_next_state() {
            return &states[cur_state ^ 1];
        }
        ViState* get_cur_state() {
            return &states[cur_state];
        }
        void update_vi() {
            static int update_vi_count = 0;
            // ALWAYS print for first 100 calls after game starts
            static bool game_started_seen = false;
            if (!game_started_seen && ultramodern::is_game_started()) {
                game_started_seen = true;
                update_vi_count = 0; // Reset counter when game starts
                fprintf(stderr, "[DEBUG-VI] update_vi(): GAME JUST STARTED, resetting counter\n");
            }
            if (update_vi_count < 20 || update_vi_count % 60 == 0) {
                fprintf(stderr, "[DEBUG-VI] update_vi() call #%d: cur_state=%d, cur_fb=0x%08X, next_fb=0x%08X, game_started=%d\n",
                    update_vi_count, cur_state,
                    (uint32_t)get_cur_state()->framebuffer,
                    (uint32_t)get_next_state()->framebuffer,
                    ultramodern::is_game_started() ? 1 : 0);
            }
            update_vi_count++;
            ViState* next_state = get_next_state();
            const OSViMode* next_mode = next_state->mode;

            // WAVE RACE FIX: If mode is NULL (game just started but osViSetMode not called yet),
            // skip this update to avoid crash. The game will set the mode soon.
            if (next_mode == nullptr) {
                static bool mode_null_warned = false;
                if (!mode_null_warned) {
                    fprintf(stderr, "[DEBUG-VI] update_vi: mode is NULL, skipping this update\n");
                    mode_null_warned = true;
                }
                return;
            }

            const OSViCommonRegs* common_regs = &next_mode->comRegs;
            const OSViFieldRegs* field_regs = &next_mode->fldRegs[field];
            PTR(void) framebuffer = osVirtualToPhysical(next_state->framebuffer);
            PTR(void) origin = framebuffer + field_regs->origin;

            // Process the VI state flags.
            uint32_t hStart = common_regs->hStart;
            if (next_state->state & VI_STATE_BLACK) {
                hStart = 0;
            }

            uint32_t yScale = field_regs->yScale;
            if (next_state->state & VI_STATE_REPEATLINE) {
                yScale = 0;
                origin = framebuffer;
            }

            // TODO implement osViFade

            // Update VI registers.
            regs.VI_ORIGIN_REG = origin;
            regs.VI_WIDTH_REG = common_regs->width;
            regs.VI_TIMING_REG = common_regs->burst;
            regs.VI_V_SYNC_REG = common_regs->vSync;
            regs.VI_H_SYNC_REG = common_regs->hSync;
            regs.VI_LEAP_REG = common_regs->leap;
            regs.VI_H_START_REG = hStart;
            regs.VI_V_START_REG = field_regs->vStart; // TODO implement osViExtendVStart
            regs.VI_V_BURST_REG = field_regs->vBurst;
            regs.VI_INTR_REG = field_regs->vIntr;
            regs.VI_X_SCALE_REG = common_regs->xScale; // TODO implement osViSetXScale
            regs.VI_Y_SCALE_REG = yScale; // TODO implement osViSetYScale
            regs.VI_STATUS_REG = next_state->control;
            
            // Swap VI states.
            cur_state ^= 1;
            // Copy mode/event settings but NOT framebuffer - preserve double-buffering
            ViState* new_next = get_next_state();
            ViState* new_cur = get_cur_state();
            new_next->mode = new_cur->mode;
            new_next->mq = new_cur->mq;
            new_next->msg = new_cur->msg;
            new_next->state = new_cur->state;
            new_next->control = new_cur->control;
            new_next->retrace_count = new_cur->retrace_count;
            // framebuffer is NOT copied - stays from previous osViSwapBuffer call
        }
    } vi;
    struct {
        std::thread gfx_thread;
        std::thread task_thread;
        PTR(OSMesgQueue) mq = NULLPTR;
        OSMesg msg = (OSMesg)0;
    } sp;
    struct {
        PTR(OSMesgQueue) mq = NULLPTR;
        OSMesg msg = (OSMesg)0;
    } dp;
    struct {
        PTR(OSMesgQueue) mq = NULLPTR;
        OSMesg msg = (OSMesg)0;
    } ai;
    struct {
        PTR(OSMesgQueue) mq = NULLPTR;
        OSMesg msg = (OSMesg)0;
    } si;
    // The same message queue may be used for multiple events, so share a mutex for all of them
    std::mutex message_mutex;
    uint8_t* rdram;
    moodycamel::BlockingConcurrentQueue<Action> action_queue{};
    moodycamel::BlockingConcurrentQueue<OSTask*> sp_task_queue{};
    moodycamel::ConcurrentQueue<OSThread*> deleted_threads{};
} events_context{};

ultramodern::renderer::ViRegs* ultramodern::renderer::get_vi_regs() {
    return &events_context.vi.update_screen_regs;
}

extern "C" void osSetEventMesg(RDRAM_ARG OSEvent event_id, PTR(OSMesgQueue) mq_, OSMesg msg) {
    std::lock_guard lock{ events_context.message_mutex };

    switch (event_id) {
        case OS_EVENT_SP:
            {
                // WAVE RACE FIX: Send an initial "fake" SP completion message to the queue
                // This allows the first frame to proceed without waiting for a previous RSP task.
                // The game's render loop waits for SP completion before starting each frame,
                // but on the first frame, there's no previous task to complete.
                static bool sp_first_time = true;
                if (sp_first_time && mq_ != NULLPTR) {
                    sp_first_time = false;
                    fprintf(stderr, "[DEBUG-SP] osSetEventMesg(OS_EVENT_SP): Pre-seeding SP queue 0x%08X with initial message 0x%08X\n",
                            (uint32_t)mq_, (uint32_t)(intptr_t)msg);
                    // Send initial completion message to allow first frame to proceed
                    osSendMesg(PASS_RDRAM mq_, msg, OS_MESG_NOBLOCK);
                }
                events_context.sp.msg = msg;
                events_context.sp.mq = mq_;
            }
            break;
        case OS_EVENT_DP:
            {
                // WAVE RACE FIX: Pre-seed DP queue similar to SP queue
                static bool dp_first_time = true;
                if (dp_first_time && mq_ != NULLPTR) {
                    dp_first_time = false;
                    fprintf(stderr, "[DEBUG-DP] osSetEventMesg(OS_EVENT_DP): Pre-seeding DP queue 0x%08X with initial message 0x%08X\n",
                            (uint32_t)mq_, (uint32_t)(intptr_t)msg);
                    osSendMesg(PASS_RDRAM mq_, msg, OS_MESG_NOBLOCK);
                }
                events_context.dp.msg = msg;
                events_context.dp.mq = mq_;
            }
            break;
        case OS_EVENT_AI:
            events_context.ai.msg = msg;
            events_context.ai.mq = mq_;
            break;
        case OS_EVENT_SI:
            {
                // WAVE RACE FIX: Pre-seed SI queue for controller init
                static bool si_first_time = true;
                if (si_first_time && mq_ != NULLPTR) {
                    si_first_time = false;
                    fprintf(stderr, "[DEBUG-SI] osSetEventMesg(OS_EVENT_SI): Pre-seeding SI queue 0x%08X\n", (uint32_t)mq_);
                    osSendMesg(PASS_RDRAM mq_, msg, OS_MESG_NOBLOCK);
                }
                events_context.si.msg = msg;
                events_context.si.mq = mq_;
            }
            break;
    }
}

extern "C" void osViSetEvent(RDRAM_ARG PTR(OSMesgQueue) mq_, OSMesg msg, u32 retrace_count) {
    std::lock_guard lock{ events_context.message_mutex };
    // Set BOTH cur and next state so VI messages start immediately
    // Without this, cur_state->mq stays NULLPTR until the first state swap
    ViState* cur_state = events_context.vi.get_cur_state();
    ViState* next_state = events_context.vi.get_next_state();
    cur_state->mq = mq_;
    cur_state->msg = msg;
    cur_state->retrace_count = retrace_count;
    next_state->mq = mq_;
    next_state->msg = msg;
    next_state->retrace_count = retrace_count;
}

uint64_t total_vis = 0;


extern std::atomic_bool exited;
extern moodycamel::LightweightSemaphore graphics_shutdown_ready;

void set_dummy_vi(bool odd);

void vi_thread_func() {
    ultramodern::set_native_thread_name("VI Thread");
    // This thread should be prioritized over every other thread in the application, as it's what allows
    // the game to generate new audio and gfx lists.
    ultramodern::set_native_thread_priority(ultramodern::ThreadPriority::Critical);
    using namespace std::chrono_literals;

    int remaining_retraces = 1;

    while (!exited) {
        static int vi_loop_count = 0;
        static bool game_was_started = false;
        if (!game_was_started && ultramodern::is_game_started()) {
            game_was_started = true;
            vi_loop_count = 0;
        }
        if (game_was_started && vi_loop_count < 30) {
            fprintf(stderr, "[DEBUG-VI-LOOP] Loop iteration #%d starting, total_vis=%llu\n", vi_loop_count, (unsigned long long)total_vis);
        }

        // Determine the next VI time (more accurate than adding 16ms each VI interrupt)
        auto next = ultramodern::get_start() + (total_vis * 1000000us) / (60 * ultramodern::get_speed_multiplier());
        //if (next > std::chrono::high_resolution_clock::now()) {
        //    printf("Sleeping for %" PRIu64 " us to get from %" PRIu64 " us to %" PRIu64 " us \n",
        //        (next - std::chrono::high_resolution_clock::now()) / 1us,
        //        (std::chrono::high_resolution_clock::now() - events_context.start) / 1us,
        //        (next - events_context.start) / 1us);
        //} else {
        //    printf("No need to sleep\n");
        //}
        // Detect if there's more than a second to wait and wait a fixed amount instead for the next VI if so, as that usually means the system clock went back in time.
        if (std::chrono::floor<std::chrono::seconds>(next - std::chrono::high_resolution_clock::now()) > 1s) {
            // printf("Skipping the next VI wait\n");
            next = std::chrono::high_resolution_clock::now();
        }

        if (game_was_started && vi_loop_count < 30) {
            fprintf(stderr, "[DEBUG-VI-LOOP] About to sleep_until...\n");
        }
        ultramodern::sleep_until(next);
        if (game_was_started && vi_loop_count < 30) {
            fprintf(stderr, "[DEBUG-VI-LOOP] Woke up from sleep_until\n");
        }
        vi_loop_count++;
        auto time_now = ultramodern::time_since_start();
        // Calculate how many VIs have passed
        uint64_t new_total_vis = (time_now * (60 * ultramodern::get_speed_multiplier()) / 1000ms) + 1;
        if (new_total_vis > total_vis + 1) {
            //printf("Skipped % " PRId64 " frames in VI interupt thread!\n", new_total_vis - total_vis - 1);
        }
        total_vis = new_total_vis;

        // If the game hasn't started yet, set a dummy VI mode and origin.
        if (!ultramodern::is_game_started()) {
            static bool odd = false;
            static int dummy_vi_count = 0;
            if (dummy_vi_count < 10 || dummy_vi_count % 60 == 0) {
                fprintf(stderr, "[DEBUG-VI-THREAD] Game not started yet (call #%d), setting dummy VI\n", dummy_vi_count);
            }
            dummy_vi_count++;
            set_dummy_vi(odd);
            odd = !odd;
        } else {
            static bool first_time = true;
            if (first_time) {
                fprintf(stderr, "[DEBUG-VI-THREAD] Game started! Using real framebuffers now\n");
                first_time = false;
            }
        }

        // Queue a screen update for the graphics thread with the current VI register state.
        // Doing this before the VI update is equivalent to updating the screen after the previous frame's scanout finished.
        events_context.action_queue.enqueue(ScreenUpdateAction{ events_context.vi.regs });

        // Update VI registers and swap VI modes.
        events_context.vi.update_vi();

        // If the game has started, handle sending VI and AI events.
        if (ultramodern::is_game_started()) {
            remaining_retraces--;
            
            uint8_t* rdram = events_context.rdram;
            std::lock_guard lock{ events_context.message_mutex };
            ViState* cur_state = events_context.vi.get_cur_state();
            if (remaining_retraces == 0) {
                if (cur_state->mq != NULLPTR) {
                    static int vi_msg_count = 0;
                    if (vi_msg_count < 10 || vi_msg_count % 60 == 0) {
                        fprintf(stderr, "[DEBUG-VI-MSG] Sending VI message to mq=0x%08X, msg=0x%08X (#%d)\n",
                            (uint32_t)cur_state->mq, (uint32_t)cur_state->msg, vi_msg_count);
                    }
                    vi_msg_count++;
                    if (osSendMesg(PASS_RDRAM cur_state->mq, cur_state->msg, OS_MESG_NOBLOCK) == -1) {
                        fprintf(stderr, "[DEBUG-VI-MSG] WARNING: VI message FAILED (queue full)\n");
                    }
                } else {
                    static bool warned = false;
                    if (!warned) {
                        fprintf(stderr, "[DEBUG-VI-MSG] WARNING: cur_state->mq is NULLPTR, not sending VI message\n");
                        warned = true;
                    }
                }
                remaining_retraces = cur_state->retrace_count;
            }
            if (events_context.ai.mq != NULLPTR) {
                if (osSendMesg(PASS_RDRAM events_context.ai.mq, events_context.ai.msg, OS_MESG_NOBLOCK) == -1) {
                    //printf("Game skipped a AI frame!\n");
                }
            }
        }

        if (events_callbacks.vi_callback != nullptr) {
            events_callbacks.vi_callback();
        }
    }
}

void sp_complete() {
    static int sp_count = 0;
    if (sp_count < 20 || sp_count % 60 == 0) {
        fprintf(stderr, "[DEBUG-SP] sp_complete() #%d\n", sp_count);
    }
    sp_count++;
    uint8_t* rdram = events_context.rdram;
    std::lock_guard lock{ events_context.message_mutex };
    osSendMesg(PASS_RDRAM events_context.sp.mq, events_context.sp.msg, OS_MESG_NOBLOCK);
}

void dp_complete() {
    static int dp_count = 0;
    if (dp_count < 20 || dp_count % 60 == 0) {
        fprintf(stderr, "[DEBUG-DP] dp_complete() #%d\n", dp_count);
    }
    dp_count++;
    uint8_t* rdram = events_context.rdram;
    std::lock_guard lock{ events_context.message_mutex };
    osSendMesg(PASS_RDRAM events_context.dp.mq, events_context.dp.msg, OS_MESG_NOBLOCK);
}

void task_thread_func(uint8_t* rdram, moodycamel::LightweightSemaphore* thread_ready) {
    ultramodern::set_native_thread_name("SP Task Thread");
    ultramodern::set_native_thread_priority(ultramodern::ThreadPriority::Normal);

    // Notify the caller thread that this thread is ready.
    thread_ready->signal();

    while (true) {
        // Wait until an RSP task has been sent
        OSTask* task;
        events_context.sp_task_queue.wait_dequeue(task);

        if (task == nullptr) {
            return;
        }

        if (!ultramodern::rsp::run_task(PASS_RDRAM task)) {
            fprintf(stderr, "Failed to execute task type: %" PRIu32 "\n", task->t.type);
            ULTRAMODERN_QUICK_EXIT();
        }

        // Tell the game that the RSP has completed
        sp_complete();
    }
}

std::atomic_uint32_t display_refresh_rate = 60;
std::atomic<float> resolution_scale = 1.0f;

uint32_t ultramodern::get_target_framerate(uint32_t original) {
    auto& config = ultramodern::renderer::get_graphics_config();

    switch (config.rr_option) {
        case ultramodern::renderer::RefreshRate::Original:
        default:
            return original;
        case ultramodern::renderer::RefreshRate::Manual:
            return config.rr_manual_value;
        case ultramodern::renderer::RefreshRate::Display:
            return display_refresh_rate.load();
    }
}

uint32_t ultramodern::get_display_refresh_rate() {
    return display_refresh_rate.load();
}

float ultramodern::get_resolution_scale() {
    return resolution_scale.load();
}

void ultramodern::trigger_config_action() {
    events_context.action_queue.enqueue(UpdateConfigAction{});
}

std::atomic<ultramodern::renderer::SetupResult> renderer_setup_result = ultramodern::renderer::SetupResult::Success;
std::atomic<ultramodern::renderer::GraphicsApi> renderer_chosen_api = ultramodern::renderer::GraphicsApi::Auto;

void gfx_thread_func(uint8_t* rdram, moodycamel::LightweightSemaphore* thread_ready, ultramodern::renderer::WindowHandle window_handle) {
    bool enabled_instant_present = false;
    using namespace std::chrono_literals;

    ultramodern::set_native_thread_name("Gfx Thread");
    ultramodern::set_native_thread_priority(ultramodern::ThreadPriority::Normal);

    auto old_config = ultramodern::renderer::get_graphics_config();

    auto renderer_context = ultramodern::renderer::create_render_context(rdram, window_handle, ultramodern::renderer::get_graphics_config().developer_mode);

    renderer_chosen_api.store(renderer_context->get_chosen_api());
    if (!renderer_context->valid()) {
        renderer_setup_result.store(renderer_context->get_setup_result());
        // Notify the caller thread that this thread is ready.
        thread_ready->signal();
        return;
    }

    if (events_callbacks.gfx_init_callback != nullptr) {
        events_callbacks.gfx_init_callback();
    }

    ultramodern::rsp::init();

    // Notify the caller thread that this thread is ready.
    thread_ready->signal();

    while (!exited) {
        // Try to pull an action from the queue
        Action action;
        if (events_context.action_queue.wait_dequeue_timed(action, 1ms)) {
            // Determine the action type and act on it
            if (const auto* task_action = std::get_if<SpTaskAction>(&action)) {
                // Turn on instant present if the game has been started and it hasn't been turned on yet.
                if (ultramodern::is_game_started() && !enabled_instant_present) {
                    renderer_context->enable_instant_present();
                    enabled_instant_present = true;
                }
                // Tell the game that the RSP completed instantly. This will allow it to queue other task types, but it won't
                // start another graphics task until the RDP is also complete. Games usually preserve the RSP inputs until the RDP
                // is finished as well, so sending this early shouldn't be an issue in most cases.
                // If this causes issues then the logic can be replaced with responding to yield requests.
                static int dl_count = 0;
                if (dl_count < 20 || dl_count % 60 == 0) {
                    fprintf(stderr, "[DEBUG-GFX-THREAD] Processing DL #%d, data_ptr=0x%08X\n",
                            dl_count, task_action->task.t.data_ptr);
                }
                sp_complete();
                ultramodern::measure_input_latency();

                // FIX: Apply display list fix right before send_dl, NOT in osSpTaskStartGo
                // This prevents the game from overwriting our modifications between submit and processing
                fix_display_list_for_rt64(rdram, task_action->task.t.data_ptr);

                [[maybe_unused]] auto renderer_start = std::chrono::high_resolution_clock::now();
                if (dl_count < 20 || dl_count % 60 == 0) {
                    fprintf(stderr, "[DEBUG-GFX-THREAD] Calling send_dl #%d...\n", dl_count);
                }
                renderer_context->send_dl(&task_action->task);
                if (dl_count < 20 || dl_count % 60 == 0) {
                    fprintf(stderr, "[DEBUG-GFX-THREAD] send_dl #%d returned\n", dl_count);
                }
                [[maybe_unused]] auto renderer_end = std::chrono::high_resolution_clock::now();
                dp_complete();
                dl_count++;
                // printf("Renderer ProcessDList time: %d us\n", static_cast<u32>(std::chrono::duration_cast<std::chrono::microseconds>(renderer_end - renderer_start).count()));
            }
            else if (const auto* screen_update_action = std::get_if<ScreenUpdateAction>(&action)) {
                events_context.vi.update_screen_regs = screen_update_action->regs;
                renderer_context->update_screen();
                display_refresh_rate = renderer_context->get_display_framerate();
                resolution_scale = renderer_context->get_resolution_scale();
            }
            else if (const auto* config_action = std::get_if<UpdateConfigAction>(&action)) {
                (void)config_action;
                auto new_config = ultramodern::renderer::get_graphics_config();
                if (renderer_context->update_config(old_config, new_config)) {
                    old_config = new_config;
                }
            }
        }
    }

    graphics_shutdown_ready.wait();
    renderer_context->shutdown();
}

#define VI_CTRL_TYPE_16             0x00002
#define VI_CTRL_TYPE_32             0x00003
#define VI_CTRL_GAMMA_DITHER_ON     0x00004
#define VI_CTRL_GAMMA_ON            0x00008
#define VI_CTRL_DIVOT_ON            0x00010
#define VI_CTRL_SERRATE_ON          0x00040
#define VI_CTRL_ANTIALIAS_MASK      0x00300
#define VI_CTRL_ANTIALIAS_MODE_1    0x00100
#define VI_CTRL_ANTIALIAS_MODE_2    0x00200
#define VI_CTRL_ANTIALIAS_MODE_3    0x00300
#define VI_CTRL_PIXEL_ADV_MASK      0x01000
#define VI_CTRL_PIXEL_ADV_1         0x01000
#define VI_CTRL_PIXEL_ADV_2         0x02000
#define VI_CTRL_PIXEL_ADV_3         0x03000
#define VI_CTRL_DITHER_FILTER_ON    0x10000

static const OSViMode dummy_mode = []() {
    OSViMode ret{};

    ret.type = 2;
    ret.comRegs.ctrl = VI_CTRL_TYPE_16 | VI_CTRL_GAMMA_DITHER_ON | VI_CTRL_GAMMA_ON | VI_CTRL_DIVOT_ON | VI_CTRL_ANTIALIAS_MODE_1 | VI_CTRL_PIXEL_ADV_3;
    ret.comRegs.width = 0x140;
    ret.comRegs.burst = 0x03E52239;
    ret.comRegs.vSync = 0x20D;
    ret.comRegs.hSync = 0xC15;
    ret.comRegs.leap = 0x0C150C15;
    ret.comRegs.hStart = 0x006C02EC;
    ret.comRegs.xScale = 0x200;
    ret.comRegs.vCurrent = 0x0;

    for (int field = 0; field < 2; field++) {
        ret.fldRegs[field].origin = 0x280;
        ret.fldRegs[field].yScale = 0x400;
        ret.fldRegs[field].vStart = 0x2501FF;
        ret.fldRegs[field].vBurst = 0xE0204;
        ret.fldRegs[field].vIntr = 0x2;
    }

    return ret;
}();

// Track if osViSwapBuffer was called by the game (prevents set_dummy_vi from overwriting)
static std::atomic_bool game_vi_swap_called = false;

void set_dummy_vi(bool odd) {
    // Don't overwrite framebuffer if the game has already set it via osViSwapBuffer
    if (game_vi_swap_called.load()) {
        return;
    }
    ViState* next_state = events_context.vi.get_next_state();
    next_state->mode = &dummy_mode;
    // Set up a dummy framebuffer.
    next_state->framebuffer = 0x80700000;
    if (odd) {
        next_state->framebuffer += 0x25800;
    }
}

extern "C" void osViSwapBuffer(RDRAM_ARG PTR(void) frameBufPtr) {
    std::lock_guard lock{ events_context.message_mutex };
    // Mark that the game has started setting framebuffers - prevents set_dummy_vi overwrite
    if (!game_vi_swap_called.load()) {
        fprintf(stderr, "[DEBUG-VI] osViSwapBuffer: FIRST CALL, setting game_vi_swap_called=true\n");
    }
    game_vi_swap_called.store(true);
    fprintf(stderr, "[DEBUG-VI] osViSwapBuffer(fb=0x%08X), cur_state=%d, next_state fb was 0x%08X\n",
        (uint32_t)frameBufPtr, events_context.vi.cur_state, (uint32_t)events_context.vi.get_next_state()->framebuffer);
    events_context.vi.get_next_state()->framebuffer = frameBufPtr;

    // WAVE RACE FIX: Some games (like Wave Race) spin-loop waiting for osViGetCurrentFramebuffer
    // to return the new buffer. They expect the swap to happen immediately, not waiting for
    // the next VI interrupt. Force an immediate state swap so spin-loops work correctly.
    // This is less accurate but necessary for games that don't use osViSetEvent before polling.
    events_context.vi.cur_state ^= 1;
    fprintf(stderr, "[DEBUG-VI] osViSwapBuffer: FORCED immediate state swap, cur_state now=%d\n", events_context.vi.cur_state);
}

extern "C" void osViSetMode(RDRAM_ARG PTR(OSViMode) mode_) {
    std::lock_guard lock{ events_context.message_mutex };
    OSViMode* mode = TO_PTR(OSViMode, mode_);
    ViState* next_state = events_context.vi.get_next_state();
    next_state->mode = mode;
    next_state->control = next_state->mode->comRegs.ctrl;
}

#define OS_VI_GAMMA_ON          0x0001
#define OS_VI_GAMMA_OFF         0x0002
#define OS_VI_GAMMA_DITHER_ON   0x0004
#define OS_VI_GAMMA_DITHER_OFF  0x0008
#define OS_VI_DIVOT_ON          0x0010
#define OS_VI_DIVOT_OFF         0x0020
#define OS_VI_DITHER_FILTER_ON  0x0040
#define OS_VI_DITHER_FILTER_OFF 0x0080

extern "C" void osViSetSpecialFeatures(uint32_t func) {
    std::lock_guard lock{ events_context.message_mutex };
    ViState* next_state = events_context.vi.get_next_state();
    uint32_t* control_out = &next_state->control;
    if ((func & OS_VI_GAMMA_ON) != 0) {
        *control_out |= VI_CTRL_GAMMA_ON;
    }

    if ((func & OS_VI_GAMMA_OFF) != 0) {
        *control_out &= ~VI_CTRL_GAMMA_ON;
    }

    if ((func & OS_VI_GAMMA_DITHER_ON) != 0) {
        *control_out |= VI_CTRL_GAMMA_DITHER_ON;
    }

    if ((func & OS_VI_GAMMA_DITHER_OFF) != 0) {
        *control_out &= ~VI_CTRL_GAMMA_DITHER_ON;
    }

    if ((func & OS_VI_DIVOT_ON) != 0) {
        *control_out |= VI_CTRL_DIVOT_ON;
    }

    if ((func & OS_VI_DIVOT_OFF) != 0) {
        *control_out &= ~VI_CTRL_DIVOT_ON;
    }

    if ((func & OS_VI_DITHER_FILTER_ON) != 0) {
        *control_out |= VI_CTRL_DITHER_FILTER_ON;
        *control_out &= ~VI_CTRL_ANTIALIAS_MASK;
    }

    if ((func & OS_VI_DITHER_FILTER_OFF) != 0) {
        *control_out &= ~VI_CTRL_DITHER_FILTER_ON;
        *control_out |= next_state->mode->comRegs.ctrl & VI_CTRL_ANTIALIAS_MASK;
    }
}

extern "C" void osViBlack(uint8_t active) {
    std::lock_guard lock{ events_context.message_mutex };
    ViState* next_state = events_context.vi.get_next_state();
    uint32_t* state_out = &next_state->state;
    if (active) {
        *state_out |= VI_STATE_BLACK;
    } else {
        *state_out &= ~VI_STATE_BLACK;
    }
}

extern "C" void osViRepeatLine(uint8_t active) {
    std::lock_guard lock{ events_context.message_mutex };
    ViState* next_state = events_context.vi.get_next_state();
    uint32_t* state_out = &next_state->state;
    if (active) {
        *state_out |= VI_STATE_REPEATLINE;
    } else {
        *state_out &= ~VI_STATE_REPEATLINE;
    }
}

extern "C" void osViSetXScale(float scale) {
    if (scale != 1.0f) {
        assert(false);
    }
}

extern "C" void osViSetYScale(float scale) {
    if (scale != 1.0f) {
        assert(false);
    }
}

extern "C" PTR(void) osViGetNextFramebuffer() {
    return events_context.vi.get_next_state()->framebuffer;
}

extern "C" PTR(void) osViGetCurrentFramebuffer() {
    return events_context.vi.get_cur_state()->framebuffer;
}

// External sequence counter from waverace_stubs.cpp
extern "C" uint64_t get_next_sequence();

void ultramodern::submit_rsp_task(RDRAM_ARG PTR(OSTask) task_) {
    OSTask* task = TO_PTR(OSTask, task_);
    uint64_t seq = get_next_sequence();

    // Send gfx tasks to the graphics action queue
    if (task->t.type == M_GFXTASK) {
        static int gfx_num = 0;
        // Print more frames to see timing
        if (gfx_num < 20 || gfx_num % 100 == 0) {
            fprintf(stderr, "[SEQ-%llu] submit_rsp_task GFX #%d: data_ptr=0x%08X data_size=0x%X\n",
                    (unsigned long long)seq, gfx_num, task->t.data_ptr, task->t.data_size);
            // Dump first 96 bytes of display list (12 commands)
            uint32_t dl_addr = task->t.data_ptr & 0x7FFFFFF;
            if (dl_addr < 0x800000) {
                uint32_t* dl = (uint32_t*)(events_context.rdram + dl_addr);
                fprintf(stderr, "[SEQ-%llu] DL dump (first 12 cmds at 0x%08X):\n", (unsigned long long)seq, task->t.data_ptr);
                for (int i = 0; i < 12; i++) {
                    // Swap for big-endian display
                    uint32_t w0 = __builtin_bswap32(dl[i*2]);
                    uint32_t w1 = __builtin_bswap32(dl[i*2+1]);
                    fprintf(stderr, "  [+0x%02X] %08X %08X\n", i*8, w0, w1);
                }
            }
        }
        gfx_num++;
        events_context.action_queue.enqueue(SpTaskAction{ *task });
    }
    // Set all other tasks as the RSP task
    else {
        fprintf(stderr, "[SEQ-%llu] submit_rsp_task: non-GFX task type=%d\n",
                (unsigned long long)seq, task->t.type);
        events_context.sp_task_queue.enqueue(task);
    }
}

void ultramodern::send_si_message() {
    uint8_t* rdram = events_context.rdram;
    osSendMesg(PASS_RDRAM events_context.si.mq, events_context.si.msg, OS_MESG_NOBLOCK);
}

void ultramodern::init_events(RDRAM_ARG ultramodern::renderer::WindowHandle window_handle) {
    moodycamel::LightweightSemaphore gfx_thread_ready;
    moodycamel::LightweightSemaphore task_thread_ready;
    events_context.rdram = rdram;
    events_context.sp.gfx_thread = std::thread{ gfx_thread_func, rdram, &gfx_thread_ready, window_handle };
    events_context.sp.task_thread = std::thread{ task_thread_func, rdram, &task_thread_ready };

    // Wait for the two sp threads to be ready before continuing to prevent the game from
    // running before we're able to handle RSP tasks.
    gfx_thread_ready.wait();
    task_thread_ready.wait();

    ultramodern::renderer::SetupResult setup_result = renderer_setup_result.load();
    if (setup_result != ultramodern::renderer::SetupResult::Success) {
        auto show_renderer_error = [](const std::string& msg) {
            std::string error_msg = "An error has been encountered on startup: " + msg;

            ultramodern::error_handling::message_box(error_msg.c_str());
        };

        const std::string driver_os_suffix = "\nPlease make sure your GPU drivers and your OS are up to date.";
        switch (setup_result) {
            case ultramodern::renderer::SetupResult::Success:
                break;
            case ultramodern::renderer::SetupResult::DynamicLibrariesNotFound:
                show_renderer_error("Failed to load dynamic libraries. Make sure the DLLs are next to the recomp executable.");
                break;
            case ultramodern::renderer::SetupResult::InvalidGraphicsAPI:
                show_renderer_error(ultramodern::renderer::get_graphics_api_name(renderer_chosen_api.load()) + " is not supported on this platform. Please select a different graphics API.");
                break;
            case ultramodern::renderer::SetupResult::GraphicsAPINotFound:
                show_renderer_error("Unable to initialize " + ultramodern::renderer::get_graphics_api_name(renderer_chosen_api.load()) + "." + driver_os_suffix);
                break;
            case ultramodern::renderer::SetupResult::GraphicsDeviceNotFound:
                show_renderer_error("Unable to find compatible graphics device." + driver_os_suffix);
                break;
        }
        throw std::runtime_error("Failed to initialize the renderer");
    }

    events_context.vi.thread = std::thread{ vi_thread_func };
}

void ultramodern::join_event_threads() {
    events_context.sp.gfx_thread.join();
    events_context.vi.thread.join();

    // Send a null RSP task to indicate that the RSP task thread should exit.
    events_context.sp_task_queue.enqueue(nullptr);
    events_context.sp.task_thread.join();
}
