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
            uint8_t v0 = (*dl)->p1(16, 8) / 5;
            uint8_t v1 = (*dl)->p1(8, 8) / 5;
            uint8_t v2 = (*dl)->p1(0, 8) / 5;
            // Safety bounds check - DL might walk into garbage data
            // Wave Race typically loads 32 vertices at a time, max valid index is ~31
            // But some DLs load more, so use 48 as threshold
            if (v0 >= 48 || v1 >= 48 || v2 >= 48) {
                // Invalid vertex indices - likely parsing garbage data
                // Skip this tri and terminate the DL by setting dl to nullptr
                fprintf(stderr, "[F3DWAVE] tri1: Invalid indices %d,%d,%d (w1=0x%08X) - terminating DL\n",
                        v0, v1, v2, (*dl)->w1);
                fflush(stderr);
                *dl = nullptr;
                return;
            }
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