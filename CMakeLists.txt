# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ESPZPressureSensor)

# Set the target
set(IDF_TARGET "esp32c6")
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")

# SPIFFS flash
# create SPIFF partition
spiffs_create_partition_image(storage spiffs FLASH_IN_PROJECT)

# Define the major version
set(MAJOR_VERSION "1.2.0")

# Find Python interpreter
find_package(Python3 REQUIRED)

# Add a custom target to run the make_version.py script
add_custom_target(
    make_version ALL
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/util/make_version.py ${CMAKE_SOURCE_DIR} ${MAJOR_VERSION}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running make_version.py script"
)

# Ensure that make_version runs before building the app
add_dependencies(app make_version)