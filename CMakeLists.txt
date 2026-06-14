cmake_minimum_required(VERSION 3.24)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/libgui_defaults.cmake)

if(LIBGUI_STANDALONE)
    project(libgui LANGUAGES CXX)
endif()

option(LIBGUI_BUILD_SSL "Enable HTTPS/WSS support (requires OpenSSL)" OFF)

libgui_configure_ssl()

#------------------------------------------------------------------------
# Conan (standalone: optional auto-install via Makefile / CONAN_FORCE_AUTO_UPDATE)
#------------------------------------------------------------------------
option(CONAN_FORCE_AUTO_UPDATE OFF)
if(LIBGUI_STANDALONE AND CONAN_FORCE_AUTO_UPDATE)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CONAN_BUILD_TYPE Debug)
    else()
        set(CONAN_BUILD_TYPE Release)
    endif()

    set(CONAN_OUTPUT_DIR ${CMAKE_BINARY_DIR}/.conan)
    execute_process(
        COMMAND conan install ${CMAKE_SOURCE_DIR}/conan -of ${CONAN_OUTPUT_DIR} -s build_type=${CMAKE_BUILD_TYPE}
        --build=missing
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMAND_ERROR_IS_FATAL ANY
        RESULT_VARIABLE conan_exit_code
    )

    if(NOT conan_exit_code EQUAL 0)
        message(FATAL_ERROR "Conan install failed!")
    endif()

    include(${CONAN_OUTPUT_DIR}/conan_toolchain.cmake)
endif()

if(LIBGUI_FIND_DEPS)
    find_package(uwebsockets CONFIG REQUIRED)
    find_package(Threads REQUIRED)
    find_package(ZLIB REQUIRED)
endif()

#------------------------------------------------------------------------
# Libraries + optional app
#------------------------------------------------------------------------
add_subdirectory(third-party)

if(LIBGUI_BUILD_APP)
    add_subdirectory(app)
endif()
