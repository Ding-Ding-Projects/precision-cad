if(NOT DEFINED PRECISION_CAD_VERSION)
  set(PRECISION_CAD_VERSION "unavailable")
endif()
if(NOT DEFINED PRECISION_CAD_BUILD_TIME)
  set(PRECISION_CAD_BUILD_TIME "unavailable")
endif()
if(NOT DEFINED PRECISION_CAD_RESOURCE_VERSION)
  set(PRECISION_CAD_RESOURCE_VERSION "0.0.0.0")
endif()
if(NOT DEFINED PRECISION_CAD_FILE_VERSION_COMMA)
  set(PRECISION_CAD_FILE_VERSION_COMMA "0,0,0,0")
endif()

qt_add_executable(precision_cad
  "${CMAKE_CURRENT_BINARY_DIR}/precision_cad_version.rc"
  main.cpp
  ui_text.h
  workspace_controller.cpp
  workspace_controller.h
  mesh_canvas.cpp
  mesh_canvas.h
)
set(PRECISION_CAD_FILE_DESCRIPTION "Precision CAD")
set(PRECISION_CAD_INTERNAL_NAME "precision_cad")
set(PRECISION_CAD_ORIGINAL_FILENAME "precision_cad.exe")
configure_file("${CMAKE_CURRENT_LIST_DIR}/../../resources/native-version.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/precision_cad_version.rc" @ONLY)
set_target_properties(precision_cad PROPERTIES AUTOMOC ON AUTORCC ON AUTOUIC ON)

qt_add_qml_module(precision_cad
  URI PrecisionCad
  VERSION 1.0
  QML_FILES Main.qml
)

target_compile_features(precision_cad PRIVATE cxx_std_20)
target_link_libraries(precision_cad PRIVATE precision_core precision_cad_preferences Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::OpenGL)
target_include_directories(precision_cad PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../core ${CMAKE_CURRENT_SOURCE_DIR}/../preferences)

target_compile_definitions(precision_cad PRIVATE
  PRECISION_CAD_VERSION="${PRECISION_CAD_VERSION}"
  PRECISION_CAD_BUILD_TIME="${PRECISION_CAD_BUILD_TIME}")
