cmake_minimum_required (VERSION 3.0)
project(libsml-prometheus-exporter)
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

find_package(UUID REQUIRED)

ExternalProject_Add(project_libsml
  GIT_REPOSITORY    https://github.com/volkszaehler/libsml.git
  GIT_TAG           1.0.0
  GIT_SHALLOW       YES
  BUILD_COMMAND     make -C sml
  CONFIGURE_COMMAND ""
  BUILD_IN_SOURCE   YES
  INSTALL_COMMAND   ""
)
ExternalProject_Get_Property(project_libsml SOURCE_DIR)
set(sml_lib_dir "${SOURCE_DIR}/sml/lib")
set(sml_inc_dir "${SOURCE_DIR}/sml/include")
add_library(sml STATIC IMPORTED)
set_property(TARGET sml PROPERTY IMPORTED_LOCATION "${sml_lib_dir}/libsml.a")

ExternalProject_Add(project_prometheus-cpp
  GIT_REPOSITORY    https://github.com/jupp0r/prometheus-cpp
  GIT_TAG           v0.8.0
  GIT_SHALLOW       YES
  CMAKE_ARGS        -DBUILD_SHARED_LIBS=ON
  INSTALL_COMMAND   make DESTDIR=<INSTALL_DIR> install
)
ExternalProject_Get_Property(project_prometheus-cpp INSTALL_DIR)
set(prometheus_cpp_lib_dir "${INSTALL_DIR}/usr/local/lib")
set(prometheus_cpp_inc_dir "${INSTALL_DIR}/usr/local/include")
add_library(prometheus-cpp-core SHARED IMPORTED)
set_property(TARGET prometheus-cpp-core PROPERTY IMPORTED_LOCATION "${prometheus_cpp_lib_dir}/libprometheus-cpp-core.so")
add_library(prometheus-cpp-pull SHARED IMPORTED)
set_property(TARGET prometheus-cpp-pull PROPERTY IMPORTED_LOCATION "${prometheus_cpp_lib_dir}/libprometheus-cpp-pull.so")


include_directories("3rdparty/tclap/include" ${sml_inc_dir} ${prometheus_cpp_inc_dir} ${UUID_INCLUDE_DIRS})

add_executable(libsml-prometheus-exporter libsml-prometheus-exporter.cxx)

set_target_properties(libsml-prometheus-exporter PROPERTIES CXX_STANDARD 14)
target_link_libraries(libsml-prometheus-exporter sml prometheus-cpp-pull prometheus-cpp-core ${UUID_LIBRARIES})

add_dependencies(libsml-prometheus-exporter project_libsml project_prometheus-cpp)
