# Shared CMake defaults for libgui (standalone + add_subdirectory embed mode).

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(LIBGUI_STANDALONE ON)
else()
    set(LIBGUI_STANDALONE OFF)
endif()

set(LIBGUI_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(LIBGUI_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

option(LIBGUI_BUILD_APP "Build libgui app executable (libgui::app)" ON)
option(LIBGUI_FIND_DEPS "Run find_package for libgui dependencies" ${LIBGUI_STANDALONE})

if(LIBGUI_STANDALONE)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

    if(NOT CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 17)
        set(CMAKE_CXX_STANDARD_REQUIRED ON)
    endif()

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib_static)

    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)

    set(CMAKE_BUILD_RPATH ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")

    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)

    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
            "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
    endif()
endif()

function(libgui_apply_target_defaults target)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
        INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
        INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON
    )

    if(LIBGUI_STANDALONE)
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${LIBGUI_BINARY_DIR}/bin
            LIBRARY_OUTPUT_DIRECTORY ${LIBGUI_BINARY_DIR}/lib
            ARCHIVE_OUTPUT_DIRECTORY ${LIBGUI_BINARY_DIR}/lib_static
        )
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wno-reorder
            $<$<CONFIG:Debug>:-g3>
            $<$<CONFIG:Debug>:-O0>
            $<$<CONFIG:Debug>:-fsanitize=address>
            $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
            $<$<CONFIG:RelWithDebInfo>:-O2>
            $<$<CONFIG:RelWithDebInfo>:-g>
        )
        target_link_options(${target} PRIVATE
            $<$<CONFIG:Debug>:-fsanitize=address>
        )
    endif()
endfunction()

function(libgui_configure_ssl)
    if(LIBGUI_BUILD_SSL)
        if(LIBGUI_STANDALONE)
            set(INCPPECT_NO_SSL OFF CACHE BOOL "Disable SSL in incppect" FORCE)
        else()
            set(INCPPECT_NO_SSL OFF CACHE BOOL "Disable SSL in incppect")
        endif()
    else()
        if(LIBGUI_STANDALONE)
            set(INCPPECT_NO_SSL ON CACHE BOOL "Disable SSL in incppect" FORCE)
        elseif(NOT DEFINED CACHE{INCPPECT_NO_SSL})
            set(INCPPECT_NO_SSL ON CACHE BOOL "Disable SSL in incppect")
        endif()
    endif()
endfunction()
