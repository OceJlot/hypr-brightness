cmake_minimum_required(VERSION 3.10)
project(hypr-brightness C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-O3)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/main.c
    src/hyprland.c
    src/i2c.c
    src/cache.c
)

add_executable(hypr-brightness ${SOURCES})

install(TARGETS hypr-brightness DESTINATION bin)

add_custom_target(uninstall
    COMMAND xargs rm < install_manifest.txt
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)