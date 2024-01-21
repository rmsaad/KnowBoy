cmake_minimum_required(VERSION 3.20)
project(Knowboy)
if(MSVC)
    set(CMAKE_C_FLAGS_RELEASE "/O2")
    set(CMAKE_C_FLAGS_DEBUG "/Od")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
endif()

# Add the SDL subdirectory
# This should build the SDL library as part of your project
add_subdirectory(SDL)
add_subdirectory(SDL_ttf)
add_subdirectory(NFD)

# Source files
file(GLOB LIB_SOURCES
    ./src/main.c
    ./src/logging.c
    ../../knowboy/src/*
)

# Define the output target
set(TARGET_NAME "Knowboy")

include_directories(
    ../../knowboy/include
    ./include
    ./SDL/include
)

# Link SDL2 library
# Ensure that the SDL2 target is built before your application
add_executable(${TARGET_NAME} ${LIB_SOURCES})
add_dependencies(${TARGET_NAME} SDL2-static SDL2_ttf nfd)

# No need to manually copy the library file
# Link the library directly from the build directory
target_link_libraries(${TARGET_NAME} SDL2-static SDL2_ttf nfd)