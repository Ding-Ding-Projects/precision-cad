cmake_minimum_required(VERSION 3.21)
get_filename_component(APP_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  project(precision_workspace_tests LANGUAGES CXX)
  enable_testing()
  find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Quick3D OpenGL Test)
  qt_standard_project_setup()
  add_subdirectory("${APP_SOURCE_ROOT}/src/core" core)
  add_subdirectory("${APP_SOURCE_ROOT}/src/preferences" preferences)
  add_subdirectory("${APP_SOURCE_ROOT}/src/app" app)
endif()
find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
qt_add_executable(tst_native_diagnostics
  tst_native_diagnostics.cpp
  "${APP_SOURCE_ROOT}/src/app/native_diagnostics.cpp"
  "${APP_SOURCE_ROOT}/src/app/native_diagnostics.h")
set_target_properties(tst_native_diagnostics PROPERTIES AUTOMOC ON WIN32_EXECUTABLE OFF)
target_compile_definitions(tst_native_diagnostics PRIVATE PRECISION_DIAGNOSTICS_TESTING)
target_link_libraries(tst_native_diagnostics PRIVATE Qt6::Core Qt6::Qml Qt6::Test)
if(WIN32)
  target_link_libraries(tst_native_diagnostics PRIVATE advapi32)
endif()
target_include_directories(tst_native_diagnostics PRIVATE "${APP_SOURCE_ROOT}/src/app")
add_test(NAME native-diagnostics COMMAND tst_native_diagnostics -o "${CMAKE_CURRENT_BINARY_DIR}/native-diagnostics.txt,txt")
set_tests_properties(native-diagnostics PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
qt_add_executable(tst_viewport_math
  tst_viewport_math.cpp
  "${APP_SOURCE_ROOT}/src/app/viewport/viewport_camera.cpp"
  "${APP_SOURCE_ROOT}/src/app/viewport/viewport_picker.cpp"
  "${APP_SOURCE_ROOT}/src/app/viewport/mesh_geometry.cpp")
target_link_libraries(tst_viewport_math PRIVATE Qt6::Core Qt6::Gui Qt6::Quick3D Qt6::Qml Qt6::Test)
target_include_directories(tst_viewport_math PRIVATE "${APP_SOURCE_ROOT}/src/app")
add_test(NAME viewport-math COMMAND tst_viewport_math -o "${CMAKE_CURRENT_BINARY_DIR}/viewport-math.txt,txt")
set_target_properties(tst_viewport_math PROPERTIES AUTOMOC ON WIN32_EXECUTABLE OFF)
set_tests_properties(viewport-math PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
qt_add_executable(precision_fake_geometry_worker fake_geometry_worker.cpp)
target_link_libraries(precision_fake_geometry_worker PRIVATE Qt6::Core)
qt_add_executable(tst_workspace_controller
  tst_workspace_controller.cpp
  ${APP_SOURCE_ROOT}/src/app/workspace_controller.cpp
  ${APP_SOURCE_ROOT}/src/app/workspace_controller.h)
set_target_properties(tst_workspace_controller PROPERTIES AUTOMOC ON)
target_link_libraries(tst_workspace_controller PRIVATE precision_core Qt6::Core Qt6::Test)
target_include_directories(tst_workspace_controller PRIVATE ${APP_SOURCE_ROOT}/src/core ${APP_SOURCE_ROOT}/src/app)
target_compile_definitions(tst_workspace_controller PRIVATE FAKE_WORKER_PATH="$<TARGET_FILE:precision_fake_geometry_worker>")
add_dependencies(tst_workspace_controller precision_fake_geometry_worker)
add_test(NAME workspace-controller COMMAND tst_workspace_controller -o "${CMAKE_CURRENT_BINARY_DIR}/workspace-controller.txt,txt")
set_tests_properties(workspace-controller PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")

qt_add_executable(tst_workspace_qml tst_workspace_qml.cpp
 "${APP_SOURCE_ROOT}/src/app/workspace_controller.cpp" "${APP_SOURCE_ROOT}/src/app/workspace_controller.h"
 "${APP_SOURCE_ROOT}/src/app/mesh_canvas.cpp" "${APP_SOURCE_ROOT}/src/app/mesh_canvas.h" "${APP_SOURCE_ROOT}/src/app/ui_text.h"
 "${APP_SOURCE_ROOT}/src/app/viewport/mesh_geometry.cpp" "${APP_SOURCE_ROOT}/src/app/viewport/mesh_geometry.h"
 "${APP_SOURCE_ROOT}/src/app/viewport/viewport_camera.cpp" "${APP_SOURCE_ROOT}/src/app/viewport/viewport_camera.h" "${APP_SOURCE_ROOT}/src/app/viewport/viewport_picker.cpp")
set_target_properties(tst_workspace_qml PROPERTIES AUTOMOC ON WIN32_EXECUTABLE OFF)
set_target_properties(tst_workspace_controller precision_fake_geometry_worker PROPERTIES WIN32_EXECUTABLE OFF)
target_link_libraries(tst_workspace_qml PRIVATE precision_core precision_cad_preferences Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml Qt6::QuickControls2 Qt6::Quick3D Qt6::Test)
target_include_directories(tst_workspace_qml PRIVATE "${APP_SOURCE_ROOT}/src/app")
target_compile_definitions(tst_workspace_qml PRIVATE MAIN_QML_PATH="${APP_SOURCE_ROOT}/src/app/Main.qml")
add_test(NAME workspace-qml COMMAND tst_workspace_qml -o "${CMAKE_CURRENT_BINARY_DIR}/workspace-qml.txt,txt")
set_tests_properties(workspace-qml PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
if(TARGET precision_geometry AND TARGET precision_sketch)
  qt_add_executable(precision_sketch_worker_adapter sketch_worker_adapter.cpp)
  target_link_libraries(precision_sketch_worker_adapter PRIVATE precision_geometry)
  qt_add_executable(precision_forged_sketch_worker sketch_worker_adapter.cpp)
  target_link_libraries(precision_forged_sketch_worker PRIVATE precision_geometry)
  target_compile_definitions(precision_forged_sketch_worker PRIVATE PRECISION_FORGE_SKETCH_REPLY)
  set_target_properties(precision_forged_sketch_worker PROPERTIES WIN32_EXECUTABLE OFF)
  qt_add_executable(tst_associative_workspace tst_associative_workspace.cpp
    ${APP_SOURCE_ROOT}/src/app/workspace_controller.cpp ${APP_SOURCE_ROOT}/src/app/workspace_controller.h)
  set_target_properties(tst_associative_workspace precision_sketch_worker_adapter PROPERTIES AUTOMOC ON WIN32_EXECUTABLE OFF)
  target_link_libraries(tst_associative_workspace PRIVATE precision_core Qt6::Core Qt6::Test)
  target_include_directories(tst_associative_workspace PRIVATE ${APP_SOURCE_ROOT}/src/app)
  target_compile_definitions(tst_associative_workspace PRIVATE REAL_SKETCH_WORKER_PATH="$<TARGET_FILE:precision_sketch_worker_adapter>")
  target_compile_definitions(tst_associative_workspace PRIVATE FORGED_SKETCH_WORKER_PATH="$<TARGET_FILE:precision_forged_sketch_worker>")
  add_dependencies(tst_associative_workspace precision_sketch_worker_adapter precision_forged_sketch_worker)
  target_compile_definitions(tst_workspace_qml PRIVATE REAL_SKETCH_WORKER_PATH="$<TARGET_FILE:precision_sketch_worker_adapter>")
  add_dependencies(tst_workspace_qml precision_sketch_worker_adapter)
  add_test(NAME associative-workspace COMMAND tst_associative_workspace -o "${CMAKE_CURRENT_BINARY_DIR}/associative-workspace.txt,txt")
endif()
