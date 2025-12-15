
cmake_minimum_required(VERSION 3.28)

project(mvll)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 23)
set(SYCL_FLAGS -fsycl -fsycl-targets=nvptx64-nvidia-cuda)

find_package(PkgConfig REQUIRED)
find_package(Catch2 3.0 REQUIRED)

#############################################################################
pkg_check_modules(WaylandProtocol REQUIRED wayland-protocols>=1.38)
pkg_get_variable(WaylandProtocolPath wayland-protocols pkgdatadir)

set(XDG_SHELL_PROTOCOL              ${WaylandProtocolPath}/stable/xdg-shell/xdg-shell.xml)
set(ZWP_TABLET_V2_PROTOCOL          ${WaylandProtocolPath}/unstable/tablet/tablet-unstable-v2.xml)
set(ZWP_LINUX_DMABUF_V1_PROTOCOL    ${WaylandProtocolPath}/stable/linux-dmabuf/linux-dmabuf-v1.xml)
message(XDG_SHELL_PROTOCOL="${XDG_SHELL_PROTOCOL}")
message(ZWP_TABLET_V2_PROTOCOL="${ZWP_TABLET_V2_PROTOCOL}")
message(ZWP_LINUX_DMA_BUF_V1="${ZWP_LINUX_DMABUF_V1_PROTOCOL}")
add_custom_command(
  OUTPUT xdg-shell-private.c
  COMMAND wayland-scanner client-header ${XDG_SHELL_PROTOCOL} xdg-shell-client.h
  COMMAND wayland-scanner private-code  ${XDG_SHELL_PROTOCOL} xdg-shell-private.c)
add_custom_command(
  OUTPUT zwp-tablet-v2-private.c
  COMMAND wayland-scanner client-header ${ZWP_TABLET_V2_PROTOCOL} zwp-tablet-v2-client.h
  COMMAND wayland-scanner private-code  ${ZWP_TABLET_V2_PROTOCOL} zwp-tablet-v2-private.c)
add_custom_command(
  OUTPUT zwp-linux-dmabuf-v1-private.c
  COMMAND wayland-scanner client-header ${ZWP_LINUX_DMABUF_V1_PROTOCOL} zwp-linux-dmabuf-v1-client.h
  COMMAND wayland-scanner private-code  ${ZWP_LINUX_DMABUF_V1_PROTOCOL} zwp-linux-dmabuf-v1-private.c)
add_library(mvll-host-code
  mvll-host-code.cc
  xdg-shell-private.c
  zwp-tablet-v2-private.c
  zwp-linux-dmabuf-v1-private.c
)
target_compile_options(mvll-host-code PRIVATE
  -Wall
  -Wextra
)
target_include_directories(mvll-host-code PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)

#############################################################################
add_library(mvll-device-code
  mvll-host-code.cc
)
target_compile_options(mvll-device-code PRIVATE
  -Wall
  -Wextra
  ${SYCL_FLAGS}
)
target_include_directories(mvll-device-code PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(mvll-device-code PRIVATE
  sycl
)

#############################################################################
add_executable(mvll
  main.cc
)
target_compile_options(mvll PRIVATE
  -Wall
  -Wextra
  ${SYCL_FLAGS}
)
target_include_directories(mvll PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(mvll PRIVATE
  sycl
  mvll-host-code
  mvll-device-code
)

#############################################################################
enable_testing()
add_executable(mvll-test
  mvll-test.cc
)
target_compile_options(mvll-test PRIVATE
  -Wall
  -Wextra
)
target_include_directories(mvll-test PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(mvll-test PRIVATE
  sycl
  Catch2::Catch2WithMain
)
add_test(NAME run_mvll_test COMMAND mvll-test)

#############################################################################
add_custom_target(run
  DEPENDS mvll
  DEPENDS mvll-test
  COMMAND ctest --verbose && ./mvll
)
