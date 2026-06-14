add_subdirectory(imgui)
add_subdirectory(imgui-ws)
add_subdirectory(incppect)

add_library(libgui_core INTERFACE)
target_link_libraries(libgui_core INTERFACE imgui-ws)
target_include_directories(libgui_core INTERFACE
    ${LIBGUI_SOURCE_DIR}/app/src
)

add_library(libgui::core ALIAS libgui_core)
add_library(libgui::imgui ALIAS imgui)
add_library(libgui::imgui-ws ALIAS imgui-ws)
add_library(libgui::incppect ALIAS incppect)
