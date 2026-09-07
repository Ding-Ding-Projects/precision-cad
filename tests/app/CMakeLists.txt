find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
qt_add_executable(precision_fake_geometry_worker fake_geometry_worker.cpp)
target_link_libraries(precision_fake_geometry_worker PRIVATE Qt6::Core)
qt_add_executable(tst_workspace_controller
  tst_workspace_controller.cpp
  ${CMAKE_SOURCE_DIR}/src/app/workspace_controller.cpp
  ${CMAKE_SOURCE_DIR}/src/app/workspace_controller.h)
set_target_properties(tst_workspace_controller PROPERTIES AUTOMOC ON)
target_link_libraries(tst_workspace_controller PRIVATE precision_core Qt6::Core Qt6::Test)
target_include_directories(tst_workspace_controller PRIVATE ${CMAKE_SOURCE_DIR}/src/core ${CMAKE_SOURCE_DIR}/src/app)
target_compile_definitions(tst_workspace_controller PRIVATE FAKE_WORKER_PATH="$<TARGET_FILE:precision_fake_geometry_worker>")
add_dependencies(tst_workspace_controller precision_fake_geometry_worker)
add_test(NAME workspace-controller COMMAND tst_workspace_controller)
set_tests_properties(workspace-controller PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${QT6_INSTALL_PREFIX}/bin")
