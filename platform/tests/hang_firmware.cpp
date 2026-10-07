#include "blender8/firmware.hpp"
namespace firmware {
const b8::VectorTable vectors{};
void reset(){}
void step(){volatile bool keep_running=true;while(keep_running){} }
}
