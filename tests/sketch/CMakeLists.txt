if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.29)
  project(PrecisionSketchTests LANGUAGES CXX)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  find_package(Qt6 6.8 REQUIRED COMPONENTS Core Test)
endif()
include(CTest)
enable_testing()
if(NOT TARGET precision_sketch)
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../src/sketch" sketch-build)
endif()
add_executable(precision_sketch_tests sketch_model_tests.cpp)
target_link_libraries(precision_sketch_tests PRIVATE precision_sketch Qt6::Core)
add_test(NAME precision_sketch_tests COMMAND "${CMAKE_COMMAND}" -E env "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$ENV{PATH}" $<TARGET_FILE:precision_sketch_tests>)

find_program(PRECISION_PWSH pwsh REQUIRED)
add_test(NAME precision_sketch_solver_provenance COMMAND "${PRECISION_PWSH}" -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_LIST_DIR}/solver_provenance_tests.ps1" -Verifier "${CMAKE_CURRENT_LIST_DIR}/../../src/sketch/verify_solver_source.cmake" -FixtureParent "${CMAKE_CURRENT_BINARY_DIR}/provenance-fixtures")
add_test(NAME precision_sketch_cmake_options COMMAND "${CMAKE_COMMAND}"
  "-DFIXTURE_SOURCE=${CMAKE_CURRENT_LIST_DIR}/cmake-options"
  "-DFIXTURE_BUILD=${CMAKE_CURRENT_BINARY_DIR}/parent-options-fixture"
  "-DSKETCH_SOURCE=${CMAKE_CURRENT_LIST_DIR}/../../src/sketch"
  "-DGENERATOR=${CMAKE_GENERATOR}" "-DQt6_DIR=${Qt6_DIR}"
  "-DC_COMPILER=${CMAKE_C_COMPILER}" "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
  -P "${CMAKE_CURRENT_LIST_DIR}/cmake_options_test.cmake")
