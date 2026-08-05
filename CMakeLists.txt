# SPDX-License-Identifier: GPL-2.0-or-later
cmake_minimum_required(VERSION 3.16)
project(azmap-gtk VERSION 0.2.0 LANGUAGES C)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local" CACHE PATH "Install prefix" FORCE)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED gtk4)
pkg_check_modules(EPOXY REQUIRED epoxy)
pkg_check_modules(CURL REQUIRED libcurl)

# shapelib
pkg_check_modules(SHAPELIB shapelib)
if(NOT SHAPELIB_FOUND)
    find_library(SHAPELIB_LIBRARIES NAMES shp shapelib)
    find_path(SHAPELIB_INCLUDE_DIRS shapefil.h)
    if(NOT SHAPELIB_LIBRARIES)
        message(FATAL_ERROR "shapelib not found. Install with: sudo pacman -S shapelib")
    endif()
endif()

add_executable(azmap-gtk
    src/main.c
    src/config.c
    src/projection.c
    src/map_data.c
    src/renderer.c
    src/camera.c
    src/input.c
    src/text.c
    src/grid.c
    src/solar.c
    src/nightmesh.c
    src/qrz.c
    src/cJSON.c
    src/overlay.c
    src/fetch.c
    src/beacon.c
)

target_include_directories(azmap-gtk PRIVATE
    ${GTK4_INCLUDE_DIRS}
    ${EPOXY_INCLUDE_DIRS}
    ${SHAPELIB_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
)

target_link_libraries(azmap-gtk PRIVATE
    ${GTK4_LIBRARIES}
    ${EPOXY_LIBRARIES}
    ${SHAPELIB_LIBRARIES}
    ${CURL_LIBRARIES}
    OpenGL::GL
    m
    pthread
)

target_compile_definitions(azmap-gtk PRIVATE
    AZMAP_VERSION="${PROJECT_VERSION}"
    $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>
)

target_compile_options(azmap-gtk PRIVATE
    ${GTK4_CFLAGS_OTHER}
    ${EPOXY_CFLAGS_OTHER}
    -Wall -Wextra
    -fstack-protector-strong
    -fstack-clash-protection
    -Wformat -Wformat-security -Werror=format-security
)

target_link_options(azmap-gtk PRIVATE
    -Wl,-z,relro,-z,now
)

# Copy shaders to build directory
file(GLOB SHADER_FILES ${CMAKE_SOURCE_DIR}/shaders/*.vert ${CMAKE_SOURCE_DIR}/shaders/*.frag)
add_custom_target(copy_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders ${CMAKE_BINARY_DIR}/shaders
    DEPENDS ${SHADER_FILES}
    COMMENT "Copying shaders to build directory"
)
add_dependencies(azmap-gtk copy_shaders)

# Install
include(GNUInstallDirs)
install(TARGETS azmap-gtk RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY shaders/ DESTINATION ${CMAKE_INSTALL_DATADIR}/azmap-gtk/shaders)
install(FILES packaging/azmap-gtk.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(DIRECTORY data/ DESTINATION ${CMAKE_INSTALL_DATADIR}/azmap-gtk/data
    OPTIONAL
    FILES_MATCHING
    PATTERN "*.shp" PATTERN "*.shx" PATTERN "*.dbf" PATTERN "*.prj"
)
