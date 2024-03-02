cmake_minimum_required(VERSION 3.20)
project(Knowboy)

set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/app)
set(EXTRA_LIBS "")

if(WIN32)
    set(CMAKE_C_FLAGS_RELEASE "/O2")
    set(CMAKE_C_FLAGS_DEBUG "/Od /Zi")
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    list(APPEND EXTRA_LIBS ${GTK3_LIBRARIES})
    set(CMAKE_C_FLAGS_RELEASE "-O2")
    set(CMAKE_C_FLAGS_DEBUG "-O0 -g")
endif()

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${CMAKE_SOURCE_DIR}/cmake")

include(build_dependencies)
link_directories(${SDL2_LIBRARY_DIR} ${SDL2_TTF_LIBRARY_DIR} ${NFD_LIBRARY_DIR})

file(GLOB LIB_SOURCES
    ./src/main.c
    ./src/logging.c
    ../../knowboy/src/*
)

set(TARGET_NAME "Knowboy")

include_directories(
    ../../knowboy/include
    ./include
    ./SDL/include
)

add_executable(${TARGET_NAME} ${LIB_SOURCES})
add_dependencies(${TARGET_NAME} SDL2 SDL2_ttf NFD)
target_link_libraries(${TARGET_NAME} "${SDL2_LIBRARIES};${SDL2_TTF_LIBRARY};${NFD_LIBRARY};${EXTRA_LIBS}")
target_include_directories(${TARGET_NAME} PUBLIC ${SDL2_INCLUDE_DIR} ${SDL2_TTF_INCLUDE_DIR} ${NFD_INCLUDE_DIR})

IF(WIN32)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_INSTALL_PREFIX}/${CMAKE_BUILD_TYPE}/bin
                $<TARGET_FILE_DIR:${TARGET_NAME}>)
ENDIF()

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:${TARGET_NAME}>/resources)