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

        void tri1(State *state, DisplayList **dl) {
            // Get raw indices BEFORE division
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
                static int err_count = 0;
                if (err_count < 5) {
                    fprintf(stderr, "[F3DWAVE] tri1: Invalid raw indices 0x%02X,0x%02X,0x%02X (w1=0x%08X) - terminating DL\n",
                            raw_v0, raw_v1, raw_v2, (*dl)->w1);
                    err_count++;
                }
                *dl = nullptr;
                return;
            }

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