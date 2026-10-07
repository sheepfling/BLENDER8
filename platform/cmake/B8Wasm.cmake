# Entered only with the real Emscripten toolchain. Core targets are defined by the parent.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
option(B8_WASM_TEST_FIXTURES "Build a deliberately nonreturning Wasm test image (never publish it)" OFF)
set(B8_WEB_DIR "${CMAKE_CURRENT_BINARY_DIR}/web")
add_library(b8_pixel_probe OBJECT examples/pixel_probe/firmware.cpp)
target_link_libraries(b8_pixel_probe PRIVATE blender8_sdk)
if(B8_NUMERIC_PROFILE STREQUAL "B16")
  target_compile_definitions(b8_pixel_probe PRIVATE B8_TARGET_B16=1 B8_NUMERIC_PROFILE=16)
endif()
add_library(b8_known_starter OBJECT firmware/firmware.cpp)
target_link_libraries(b8_known_starter PRIVATE blender8_sdk)
target_include_directories(b8_known_starter PRIVATE firmware)

function(b8_web_module target image label)
  add_executable(${target} wasm/bridge.cpp $<TARGET_OBJECTS:${image}>)
  target_include_directories(${target} PRIVATE wasm)
  target_link_libraries(${target} PRIVATE b8_view)
  target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_compile_definitions(${target} PRIVATE B8_FIRMWARE_NAME="${label}")
  set_target_properties(${target} PROPERTIES SUFFIX ".mjs" RUNTIME_OUTPUT_DIRECTORY "${B8_WEB_DIR}")
  target_link_options(${target} PRIVATE
    --no-entry -fexceptions
    -sMODULARIZE=1 -sEXPORT_ES6=1 "-sENVIRONMENT=web,worker,node"
    "-sINCOMING_MODULE_JS_API=['locateFile','wasmBinary','print','printErr']"
    -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=33554432 -sMAXIMUM_MEMORY=268435456
    -sSTACK_SIZE=1048576 -sFILESYSTEM=0 -sASSERTIONS=1
    "-sEXPORTED_FUNCTIONS=['_b8_wasm_abi','_b8_wasm_init','_b8_wasm_hello','_b8_wasm_command','_b8_wasm_dispose','_b8_view_abi','_b8_view_init','_b8_view_resize','_b8_view_frame','_b8_view_event','_b8_view_pixels','_b8_view_width','_b8_view_height','_b8_view_status','_b8_view_journal']"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','HEAPU8']")
endfunction()
b8_web_module(b8_student student_firmware "student")
add_dependencies(b8_student firmware_memory)
b8_web_module(b8_probe b8_pixel_probe "pixel_probe_not_product")
b8_web_module(b8_starter b8_known_starter "safe_starter_fixture")
target_compile_definitions(b8_starter PRIVATE B8_FIXED_PRODUCTION_FUSE=1)
if(B8_NUMERIC_PROFILE STREQUAL "B16")
  foreach(target b8_student b8_probe)
    target_compile_definitions(${target} PRIVATE B8_TARGET_B16=1 B8_NUMERIC_PROFILE=16)
  endforeach()
endif()
if(B8_WASM_TEST_FIXTURES)
  add_library(b8_hang_fixture OBJECT tests/hang_firmware.cpp)
  target_link_libraries(b8_hang_fixture PRIVATE blender8_sdk)
  b8_web_module(b8_hang b8_hang_fixture "TEST_ONLY_HANG")
endif()
# Atomic, allowlisted static release: sources/build caches never become web assets.
# The diagnostic hang module is intentionally excluded by the packaging script.
add_custom_target(b8_web ALL
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/wasm_site.py"
          --source "${CMAKE_CURRENT_SOURCE_DIR}" --build "${CMAKE_CURRENT_BINARY_DIR}"
          --compiler "${CMAKE_CXX_COMPILER}" --device "${B8_NUMERIC_PROFILE}"
  DEPENDS b8_student b8_probe b8_starter firmware_memory firmware_memory
  VERBATIM)

# The bridge lifecycle test uses the SAME ABI implementation, executed by Node in this build.
if(BUILD_TESTING)
  add_executable(b8_wasm_abi_tests tests/wasm_abi_tests.cpp wasm/bridge.cpp $<TARGET_OBJECTS:b8_known_starter>)
  target_include_directories(b8_wasm_abi_tests PRIVATE wasm)
  target_link_libraries(b8_wasm_abi_tests PRIVATE b8_view)
  target_compile_definitions(b8_wasm_abi_tests PRIVATE B8_FIXED_PRODUCTION_FUSE=1)
  target_link_options(b8_wasm_abi_tests PRIVATE -sASSERTIONS=1)
  foreach(case lifecycle commands bench validation exceptions scene)
    add_test(NAME wasm_abi.${case} COMMAND b8_wasm_abi_tests ${case})
    set_tests_properties(wasm_abi.${case} PROPERTIES TIMEOUT 30)
  endforeach()
endif()
