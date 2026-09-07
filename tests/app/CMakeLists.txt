find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
qt_add_executable(tst_workspace_controller
  tst_workspace_controller.cpp
  ${CMAKE_SOURCE_DIR}/src/app/workspace_controller.cpp
  ${CMAKE_SOURCE_DIR}/src/app/workspace_controller.h)
set_target_properties(tst_workspace_controller PROPERTIES AUTOMOC ON)
target_link_libraries(tst_workspace_controller PRIVATE precision_core Qt6::Core Qt6::Test)
target_include_directories(tst_workspace_controller PRIVATE ${CMAKE_SOURCE_DIR}/src/core ${CMAKE_SOURCE_DIR}/src/app)
add_test(NAME workspace-controller COMMAND tst_workspace_controller)
