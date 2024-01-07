cmake_minimum_required(VERSION 3.20)
project(Knowboy)

# Detect platform
if(NOT DEFINED PLATFORM)
    if(APPLE)
        set(PLATFORM "osx")
    elseif(UNIX)
        set(PLATFORM "unix")
    elseif(WIN32)
        set(PLATFORM "win")
    endif()
endif()

# Set platform specific variables
if(${PLATFORM} STREQUAL "unix")
    set(FPIC "-fPIC")
    set(SHARED "-shared")
elseif(${PLATFORM} STREQUAL "osx")
    set(FPIC "-fPIC")
    set(SHARED "-dynamiclib")
elseif(${PLATFORM} STREQUAL "win")
    set(CMAKE_SYSTEM_NAME Windows)
    set(CMAKE_SYSTEM_VERSION 1)
    # set(SHARED "-shared")
else()
    message(FATAL_ERROR "Platform ${PLATFORM} is not supported.")
endif()

# Set compilation flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")

if(DEFINED SANITIZER)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=${SANITIZER}")
endif()

if(DEFINED DEBUG AND DEBUG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -DDEBUG")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
endif()

# Source files
FILE(GLOB LIB_SOURCES
    src/*
    ../../knowboy/src/*
    )

# Define the output target
set(TARGET_NAME "knowboy_libretro")

include_directories(
    ../../knowboy/include
    ./include
)

add_library(${TARGET_NAME} SHARED ${LIB_SOURCES})
target_compile_options(${TARGET_NAME} PRIVATE ${FPIC})
if(NOT ${PLATFORM} STREQUAL "win")
    set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS "${SHARED} -Wl,--version-script=${CMAKE_SOURCE_DIR}/link.T")
endif()

set_target_properties(${TARGET_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    PREFIX ""
)
