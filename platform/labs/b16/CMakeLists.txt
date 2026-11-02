cmake_minimum_required(VERSION 3.20)
project(astin_b16_subsystem_labs LANGUAGES CXX)

include(CTest)
add_library(astin_lab_firmware_helpers STATIC student.cpp adc_sample.cpp)
target_include_directories(astin_lab_firmware_helpers PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ../../sdk/include)
target_compile_features(astin_lab_firmware_helpers PRIVATE cxx_std_20)

add_library(astin_lab_support STATIC ../../host/src/backend.cpp fake_hardware.cpp)
target_include_directories(astin_lab_support PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ../../sdk/include
  ../../host/include)
target_compile_features(astin_lab_support PRIVATE cxx_std_20)

add_executable(b16_subsystem_labs tests.cpp)
target_include_directories(b16_subsystem_labs PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ../../sdk/include
  ../../host/include)
target_link_libraries(b16_subsystem_labs PRIVATE
  astin_lab_firmware_helpers
  astin_lab_support)
target_compile_features(b16_subsystem_labs PRIVATE cxx_std_20)
foreach(target astin_lab_firmware_helpers astin_lab_support b16_subsystem_labs)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endforeach()

if(BUILD_TESTING)
  foreach(lab temperature motor panel adc-ready adc-pending adc-status-flags fake-trace fake-overread)
    add_test(NAME b16.${lab} COMMAND b16_subsystem_labs ${lab})
  endforeach()
endif()
