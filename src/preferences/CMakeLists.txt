cmake_minimum_required(VERSION 3.21)
project(precision_cad_preferences LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core)
qt_standard_project_setup()

qt_add_library(precision_cad_preferences STATIC
    preferences_store.cpp
    preferences_store.h
    personal_vocabulary_store.cpp
    personal_vocabulary_store.h
)
target_compile_features(precision_cad_preferences PUBLIC cxx_std_20)
target_link_libraries(precision_cad_preferences PUBLIC Qt6::Core)
target_include_directories(precision_cad_preferences PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
