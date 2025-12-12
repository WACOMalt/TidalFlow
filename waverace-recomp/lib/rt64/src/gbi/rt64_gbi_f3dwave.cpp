//
// RT64
//

#include "rt64_gbi_f3dwave.h"

#include "hle/rt64_interpreter.h"

#include "rt64_gbi_f3d.h"

#include <cstdio>

namespace RT64 {
    namespace GBI_F3DWAVE {
        // F3D-style vertex command (opcode 0x04)
        void vertex(State *state, DisplayList **dl) {
            uint32_t addr = (*dl)->w1;
            uint8_t vtxCount = (*dl)->p0(9, 7);
            uint32_t dstIndex = (*dl)->p0(16, 8) / 5;
            state->rsp->setVertex(addr, vtxCount, dstIndex);
        }

        // F3DEX2-style vertex command (opcode 0x01)
        // Wave Race uses this format in some display lists
        void vertexF3DEX2(State *state, DisplayList **dl) {
            uint32_t addr = (*dl)->w1;
            uint8_t vtxCount = (*dl)->p0(12, 8);
            uint32_t dstIndex = (*dl)->p0(1, 7) - vtxCount;
            state->rsp->setVertex(addr, vtxCount, dstIndex);
        }

        // Track successful tri1 calls for debugging
        static int g_tri1_success_count = 0;
        static int g_tri1_error_count = 0;

        void tri1(State *state, DisplayList **dl) {
            // Wave Race uses TWO different G_TRI1 formats!
            // 1. F3D-style: indices in w1, raw values /5 for vertex index
            // 2. F3DEX2-style: indices in w0, 7-bit values at positions 17/9/1
            //
            // Detect which format by checking if w1 looks like command data (high byte is opcode)
            uint8_t w1_high = ((*dl)->w1 >> 24) & 0xFF;

            // If w1's high byte is a GBI opcode (0xB0-0xFF range), use F3DEX2 format
            if (w1_high >= 0xB0) {
                // F3DEX2-style: indices encoded in w0
                uint8_t v0 = (*dl)->p0(17, 7);
                uint8_t v1 = (*dl)->p0(9, 7);
                uint8_t v2 = (*dl)->p0(1, 7);

                // Basic sanity check
                if (v0 >= 32 || v1 >= 32 || v2 >= 32) {
                    g_tri1_error_count++;
                    if (g_tri1_error_count <= 5) {
                        fprintf(stderr, "[F3DWAVE] tri1 F3DEX2-style: Invalid indices %d,%d,%d (w0=0x%08X w1=0x%08X) - skipping\n",
                                v0, v1, v2, (*dl)->w0, (*dl)->w1);
                    }
                    return;
                }

                g_tri1_success_count++;
                state->rsp->drawIndexedTri(v0, v1, v2);
                return;
            }

            // F3D-style: indices in w1, divided by 5
            uint8_t raw_v0 = (*dl)->p1(16, 8);
            uint8_t raw_v1 = (*dl)->p1(8, 8);
            uint8_t raw_v2 = (*dl)->p1(0, 8);

            // Safety bounds check on RAW indices
            // Valid vertex indices should be multiples of 5 (0, 5, 10, 15, ...)
            // Maximum vertex buffer is 32, so max raw index = 31*5 = 155 (0x9B)
            // Raw indices >= 0xA0 (160) are likely garbage data
            if (raw_v0 >= 0xA0 || raw_v1 >= 0xA0 || raw_v2 >= 0xA0) {
                // Invalid vertex indices - likely parsing garbage data
                // Terminate the DL silently (don't spam logs)
                g_tri1_error_count++;
                if (g_tri1_error_count <= 5) {
                    fprintf(stderr, "[F3DWAVE] tri1 F3D-style: Invalid raw indices 0x%02X,0x%02X,0x%02X (w0=0x%08X w1=0x%08X) after %d good tris - terminating DL\n",
                            raw_v0, raw_v1, raw_v2, (*dl)->w0, (*dl)->w1, g_tri1_success_count);
                }
                *dl = nullptr;
                return;
            }

            g_tri1_success_count++;
            uint8_t v0 = raw_v0 / 5;
            uint8_t v1 = raw_v1 / 5;
            uint8_t v2 = raw_v2 / 5;
            state->rsp->drawIndexedTri(v0, v1, v2);
        }

        void tri2(State *state, DisplayList **dl) {
            state->rsp->drawIndexedTri((*dl)->p0(16, 8) / 5, (*dl)->p0(8, 8) / 5, (*dl)->p0(0, 8) / 5);
            state->rsp->drawIndexedTri((*dl)->p1(16, 8) / 5, (*dl)->p1(8, 8) / 5, (*dl)->p1(0, 8) / 5);
        }

        void quad(State *state, DisplayList **dl) {
            const uint8_t v0 = (*dl)->p1(24, 8) / 5;
            const uint8_t v1 = (*dl)->p1(16, 8) / 5;
            const uint8_t v2 = (*dl)->p1(8, 8) / 5;
            const uint8_t v3 = (*dl)->p1(0, 8) / 5;
            state->rsp->drawIndexedTri(v0, v1, v2);
            state->rsp->drawIndexedTri(v0, v2, v3);
        }

        void setup(GBI *gbi) {
            GBI_F3D::setup(gbi);

            gbi->map[F3DWAVE_G_UNKNOWN] = nullptr; // FIXME: Replaces a function set by base F3D with nothing until it's figured out.
            gbi->map[F3DWAVE_G_RDPHALF_1] = &GBI_F3D::rdpHalf1;
            gbi->map[F3DWAVE_G_RDPHALF_2] = &GBI_F3D::rdpHalf2;
            gbi->map[F3D_G_VTX] = vertex;         // F3D VTX opcode (0x04)
            gbi->map[0x01] = vertexF3DEX2;        // F3DEX2-style VTX opcode - Wave Race uses this!
            gbi->map[F3D_G_TRI1] = tri1;
            gbi->map[F3DWAVE_G_TRI2] = &tri2;
            gbi->map[F3D_G_QUAD] = quad;
        }
    }
};