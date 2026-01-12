
cmake_minimum_required(VERSION 3.28)

project(mvll)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 17)
set(SYCL_FLAGS -fsycl -fsycl-targets=nvptx64-nvidia-cuda)

# for clangd
include_directories(${SYCL_INCLUDE_PATH})

find_package(PkgConfig REQUIRED)
find_package(Catch2 3.0 REQUIRED)

#############################################################################
find_path(HERSHEY_INCLUDE_DIR hersheyfont.h)
find_library(HERSHEY_LIBRARY hersheyfont)
set(HERSHEY_DATA_DIR "/usr/share/harshey-fonts") #!!!

#############################################################################
pkg_check_modules(WaylandProtocol REQUIRED wayland-protocols>=1.38)
pkg_get_variable(WaylandProtocolPath wayland-protocols pkgdatadir)

set(XDG_SHELL_PROTOCOL                  ${WaylandProtocolPath}/stable/xdg-shell/xdg-shell.xml)
set(ZWP_LINUX_DMABUF_V1_PROTOCOL        ${WaylandProtocolPath}/stable/linux-dmabuf/linux-dmabuf-v1.xml)
set(WP_FRACTIONAL_SCALE_V1_PROTOCOL     ${WaylandProtocolPath}/staging/fractional-scale/fractional-scale-v1.xml)
set(ZWP_POINTER_CONSTRAINTS_V1_PROTOCOL ${WaylandProtocolPath}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml)
set(ZWP_TABLET_V2_PROTOCOL              ${WaylandProtocolPath}/unstable/tablet/tablet-unstable-v2.xml)
message(XDG_SHELL_PROTOCOL="${XDG_SHELL_PROTOCOL}")
message(ZWP_TABLET_V2_PROTOCOL="${ZWP_TABLET_V2_PROTOCOL}")
message(ZWP_LINUX_DMA_BUF_V1="${ZWP_LINUX_DMABUF_V1_PROTOCOL}")
message(WP_FRACTIONAL_SCALE_V1_PROTOCOL="$WP_FRACTIONAL_SCALE_V1_PROTOCOL}")
message(ZWP_POINTER_CONSTRAINTS_V1_PROTOCOL="$ZWP_POINTER_CONSTRAINTS_V1_PROTOCOL")
add_custom_command(
  OUTPUT xdg-shell-private.c
  COMMAND wayland-scanner client-header ${XDG_SHELL_PROTOCOL} xdg-shell-client.h
  COMMAND wayland-scanner private-code  ${XDG_SHELL_PROTOCOL} xdg-shell-private.c)
add_custom_command(
  OUTPUT zwp-linux-dmabuf-v1-private.c
  COMMAND wayland-scanner client-header ${ZWP_LINUX_DMABUF_V1_PROTOCOL} zwp-linux-dmabuf-v1-client.h
  COMMAND wayland-scanner private-code  ${ZWP_LINUX_DMABUF_V1_PROTOCOL} zwp-linux-dmabuf-v1-private.c)
add_custom_command(
  OUTPUT wp-fractional-scale-v1-private.c
  COMMAND wayland-scanner client-header ${WP_FRACTIONAL_SCALE_V1_PROTOCOL} wp-fractional-scale-v1-client.h
  COMMAND wayland-scanner private-code  ${WP_FRACTIONAL_SCALE_V1_PROTOCOL} wp-fractional-scale-v1-private.c)
add_custom_command(
  OUTPUT zwp-pointer-constraints-v1-private.c
  COMMAND wayland-scanner client-header ${ZWP_POINTER_CONSTRAINTS_V1_PROTOCOL} zwp-pointer-constraints-v1-client.h
  COMMAND wayland-scanner private-code  ${ZWP_POINTER_CONSTRAINTS_V1_PROTOCOL} zwp-pointer-constraints-v1-private.c)
add_custom_command(
  OUTPUT zwp-tablet-v2-private.c
  COMMAND wayland-scanner client-header ${ZWP_TABLET_V2_PROTOCOL} zwp-tablet-v2-client.h
  COMMAND wayland-scanner private-code  ${ZWP_TABLET_V2_PROTOCOL} zwp-tablet-v2-private.c)

add_library(wayland-protocols-mvll
  xdg-shell-private.c
  zwp-linux-dmabuf-v1-private.c
  wp-fractional-scale-v1-private.c
  zwp-pointer-constraints-v1-private.c
  zwp-tablet-v2-private.c
)
target_compile_options(wayland-protocols-mvll PRIVATE
  -Wall
  -Wextra)

#############################################################################
add_executable(mvll
  main.cc
  mvll-device-code.cc
  mvll-host-code.cc
)
target_compile_definitions(mvll PRIVATE
  HERSHEY_FONTS_DIR="${HERSHEY_DATA_DIR}"
)
target_compile_options(mvll PRIVATE
  -Wall
  -Wextra
#  -save-temps
  ${SYCL_FLAGS}
)
target_include_directories(mvll PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${HERSHEY_INCLUDE_DIR}
)
target_link_libraries(mvll PRIVATE
  sycl
  wayland-client
  wayland-protocols-mvll
  ${HERSHEY_LIBRARY}
)

#############################################################################
enable_testing()
add_executable(mvll-test
  mvll-test.cc
  test/unique-test.cc
  test/generator-test.cc
  test/channel-test.cc
  test/reflector-test.cc
  test/proxy-meta-test.cc
)
target_compile_definitions(mvll PRIVATE
  HERSHEY_FONTS_DIR="${HERSHEY_DATA_DIR}"
)
target_compile_options(mvll-test PRIVATE
  -Wall
  -Wextra
)
target_include_directories(mvll-test PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${HERSHEY_INCLUDE_DIR}
)
target_link_libraries(mvll-test PRIVATE
  sycl
  wayland-client
  wayland-protocols-mvll
  Catch2::Catch2WithMain
  ${HERSHEY_LIBRARY}
)
add_test(NAME run_mvll_test COMMAND mvll-test)

#############################################################################
add_custom_target(run
  DEPENDS mvll
  DEPENDS mvll-test
  COMMAND ctest --verbose && ./mvll
)

add_custom_target(run-test
  DEPENDS mvll-test
  COMMAND ctest --verbose
)
