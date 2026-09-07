set(args -S "${FIXTURE_SOURCE}" -B "${FIXTURE_BUILD}" -G "${GENERATOR}"
  "-DSKETCH_SOURCE=${SKETCH_SOURCE}" "-DQt6_DIR=${Qt6_DIR}"
  "-DCMAKE_C_COMPILER=${C_COMPILER}" "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}")
execute_process(COMMAND "${CMAKE_COMMAND}" ${args} -DINJECT_CACHE_DRIFT=OFF
  RESULT_VARIABLE normal_result OUTPUT_VARIABLE normal_output ERROR_VARIABLE normal_error)
if(NOT normal_result EQUAL 0)
  message(FATAL_ERROR "Parent options regression: ${normal_output}\n${normal_error}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" ${args} -DINJECT_CACHE_DRIFT=ON
  RESULT_VARIABLE broken_result OUTPUT_VARIABLE broken_output ERROR_VARIABLE broken_error)
if(broken_result EQUAL 0 OR NOT broken_error MATCHES "Sketch changed parent option: MI_OVERRIDE")
  message(FATAL_ERROR "Parent options negative regression did not detect cache drift: ${broken_output}\n${broken_error}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" ${args} -DINJECT_CACHE_DRIFT=OFF
  RESULT_VARIABLE restored_result OUTPUT_VARIABLE restored_output ERROR_VARIABLE restored_error)
if(NOT restored_result EQUAL 0)
  message(FATAL_ERROR "Restored parent options did not pass: ${restored_output}\n${restored_error}")
endif()
message(STATUS "PASS: parent-option configuration, deliberate cache drift rejection, and restoration")
