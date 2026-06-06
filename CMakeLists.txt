cmake_minimum_required(VERSION 3.24)
project(libgui LANGUAGES CXX)

#------------------------------------------------------------------------
# Cmake settings
#------------------------------------------------------------------------
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib_static)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # compile_commands.json
set(CMAKE_CXX_EXTENSIONS OFF)           # Без gnu++17-расширений
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # Без этого на Linux могут быть проблемы при линковке shared lib.

set(CMAKE_BUILD_RPATH ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")

# LTO оптимизации
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

#------------------------------------------------------------------------
# Comliper settings
#------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wno-reorder
    )

    add_compile_options($<$<CONFIG:Debug>:-g3>)
    add_compile_options($<$<CONFIG:Debug>:-O0>)
    add_compile_options($<$<CONFIG:RelWithDebInfo>:-O2>)
    add_compile_options($<$<CONFIG:RelWithDebInfo>:-g>)

    # Sanitize (Debug only)
    add_compile_options($<$<CONFIG:Debug>:-fsanitize=address>)
    add_compile_options($<$<CONFIG:Debug>:-fno-omit-frame-pointer>)
    add_link_options($<$<CONFIG:Debug>:-fsanitize=address>)

endif()

#------------------------------------------------------------------------
# Globals
#------------------------------------------------------------------------
option(INCPPECT_NO_SSL "Disable SSL in incppect" ON)
option(IMGUI_WS_BUILD_EXAMPLES_NULL "Build headless imgui-ws examples" ON)

#------------------------------------------------------------------------
# Conan (генерируется через Makefile)
#------------------------------------------------------------------------
option(CONAN_FORCE_AUTO_UPDATE OFF)
if(CONAN_FORCE_AUTO_UPDATE)

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
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

find_package(uwebsockets CONFIG REQUIRED)

#------------------------------------------------------------------------
# Projects
#------------------------------------------------------------------------

add_subdirectory(third-party)
