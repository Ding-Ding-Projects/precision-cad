if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.29)
  project(PrecisionSketch LANGUAGES CXX)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  find_package(Qt6 6.8 REQUIRED COMPONENTS Core)
endif()

set(PRECISION_SOLVESPACE_SOURCE "$ENV{LOCALAPPDATA}/material-virtualbox-toolchain/solvespace-v3.2" CACHE PATH "Canonical SolveSpace 3.2 source checkout")
set(PRECISION_SOLVESPACE_COMMIT "27b6a080c8b669421bd4d444650c3b8eddec5687")
foreach(required include/slvs.h src/slvs/lib.cpp src/constrainteq.cpp src/entity.cpp src/expr.cpp src/system.cpp src/util.cpp src/platform/platformbase.cpp extlib/eigen/Eigen/SparseCore extlib/mimalloc/CMakeLists.txt)
  if(NOT EXISTS "${PRECISION_SOLVESPACE_SOURCE}/${required}")
    message(FATAL_ERROR "SolveSpace 3.2 source is unavailable at ${PRECISION_SOLVESPACE_SOURCE}. Run scripts/bootstrap-sketch-solver.ps1.")
  endif()
endforeach()
include("${CMAKE_CURRENT_LIST_DIR}/verify_solver_source.cmake")
add_custom_target(precision_verify_sketch_solver
  COMMAND "${CMAKE_COMMAND}" "-DPRECISION_SOLVESPACE_SOURCE=${PRECISION_SOLVESPACE_SOURCE}" "-DPRECISION_SOLVESPACE_COMMIT=${PRECISION_SOLVESPACE_COMMIT}"
    -P "${CMAKE_CURRENT_LIST_DIR}/verify_solver_source.cmake"
  COMMENT "Verifying immutable SolveSpace source and solver submodules"
  VERBATIM)

set(MI_OVERRIDE OFF CACHE BOOL "" FORCE)
set(MI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(MI_BUILD_OBJECT OFF CACHE BOOL "" FORCE)
set(MI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
if(NOT TARGET mimalloc-static)
  add_subdirectory("${PRECISION_SOLVESPACE_SOURCE}/extlib/mimalloc" "${CMAKE_CURRENT_BINARY_DIR}/solvespace-mimalloc" EXCLUDE_FROM_ALL)
endif()

add_library(precision_sketch STATIC
  sketch_model.cpp
  "${PRECISION_SOLVESPACE_SOURCE}/src/slvs/lib.cpp"
  "${PRECISION_SOLVESPACE_SOURCE}/src/constrainteq.cpp"
  "${PRECISION_SOLVESPACE_SOURCE}/src/entity.cpp"
  "${PRECISION_SOLVESPACE_SOURCE}/src/expr.cpp"
  "${PRECISION_SOLVESPACE_SOURCE}/src/system.cpp"
  "${PRECISION_SOLVESPACE_SOURCE}/src/util.cpp"
  "${PRECISION_SOLVESPACE_SOURCE}/src/platform/platformbase.cpp"
)
target_compile_features(precision_sketch PUBLIC cxx_std_20)
target_compile_definitions(precision_sketch PRIVATE STATIC_LIB LIBRARY _USE_MATH_DEFINES)
target_include_directories(precision_sketch PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" PRIVATE "${PRECISION_SOLVESPACE_SOURCE}/include" "${PRECISION_SOLVESPACE_SOURCE}/src" "${PRECISION_SOLVESPACE_SOURCE}/extlib/eigen" "${PRECISION_SOLVESPACE_SOURCE}/extlib/mimalloc/include")
target_link_libraries(precision_sketch PUBLIC Qt6::Core PRIVATE mimalloc-static)

add_dependencies(precision_sketch precision_verify_sketch_solver)
add_dependencies(mimalloc-static precision_verify_sketch_solver)
