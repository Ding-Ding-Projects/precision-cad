add_library(precision_core STATIC
  precision_document.cpp
  model_evaluator.cpp
)
target_compile_features(precision_core PUBLIC cxx_std_20)
target_link_libraries(precision_core PUBLIC Qt6::Core)
target_include_directories(precision_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
