find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network)

add_library(precision_update STATIC
  update_service.cpp
  update_service.h
  private_staging.cpp
  private_staging.h
)
target_compile_features(precision_update PUBLIC cxx_std_20)
target_link_libraries(precision_update PUBLIC Qt6::Core Qt6::Network)
if(WIN32)
  target_link_libraries(precision_update PRIVATE Advapi32)
endif()
target_include_directories(precision_update PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
