add_executable(libgui_app
    src/main.cpp
    src/core/gui_app.cpp
    src/core/app_options.cpp
    src/core/env.cpp
    src/session/imgui_session.cpp
    src/input/input.cpp
    src/gui/gui_context.cpp
    src/gui/font_atlas.cpp
    src/gui/view_manager.cpp
    src/views/view_registry.cpp
    src/views/view_launcher.cpp
    src/views/view_incppect.cpp
    src/views/view_clipboard.cpp
    src/views/view_gfx.cpp
    src/views/view_texture_sync.cpp
)

set_target_properties(libgui_app PROPERTIES OUTPUT_NAME app)

libgui_apply_target_defaults(libgui_app)

target_include_directories(libgui_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(libgui_app PRIVATE
    libgui::core
)

add_executable(libgui::app ALIAS libgui_app)
