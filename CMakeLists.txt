cmake_minimum_required (VERSION 3.0)
project(libsml-prometheus-exporter)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")
find_package(UUID REQUIRED)

include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

ExternalProject_Add(project_libsml
  GIT_REPOSITORY    https://github.com/volkszaehler/libsml.git
  GIT_TAG           d435fb3f172f1c78053d4de08c3d94256e747696
  GIT_SHALLOW       YES
  BUILD_COMMAND     make -C sml
  CONFIGURE_COMMAND ""
  BUILD_IN_SOURCE   YES
  INSTALL_COMMAND   ""
)
ExternalProject_Get_Property(project_libsml SOURCE_DIR)
SET(sml_lib_dir "${SOURCE_DIR}/sml/lib")
SET(sml_inc_dir "${SOURCE_DIR}/sml/include")

ADD_LIBRARY(sml STATIC IMPORTED)
SET_PROPERTY(TARGET sml PROPERTY IMPORTED_LOCATION "${sml_lib_dir}/libsml.a")

ExternalProject_Add(project_prometheus-cpp
  GIT_REPOSITORY    https://github.com/jupp0r/prometheus-cpp
  GIT_TAG           a0d7c52ac4b48f4f2bb32f5c6f452205594afe1a
  GIT_SHALLOW       YES
  INSTALL_COMMAND   make DESTDIR=<INSTALL_DIR> install
)
ExternalProject_Get_Property(project_prometheus-cpp INSTALL_DIR)

include_directories(${sml_inc_dir} ${UUID_INCLUDE_DIRS})

add_executable(libsml-prometheus-exporter libsml-prometheus-exporter.cxx)

SET_TARGET_PROPERTIES(libsml-prometheus-exporter
                      PROPERTIES
                      CXX_STANDARD 14)
TARGET_LINK_LIBRARIES(libsml-prometheus-exporter sml ${UUID_LIBRARIES})

add_dependencies(libsml-prometheus-exporter project_libsml project_prometheus-cpp)
