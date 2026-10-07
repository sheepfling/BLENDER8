#pragma once
#include "blender8/device.hpp"
// User-owned definitions, consumed by the native runner or a target startup binding.
namespace firmware {
void reset();
void step();
extern const b8::VectorTable vectors;
}
