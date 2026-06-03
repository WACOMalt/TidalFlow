#pragma once

#include "plume_render_interface.h"

using namespace plume;

// Re-export plume types into RT64 namespace so that RT64::RenderFormat etc. work
namespace RT64 {
    using namespace plume;
}
