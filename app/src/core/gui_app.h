#pragma once

#include "app_options.h"
#include "vsync.h"

#include "gui/gui_context.h"
#include "input/input.h"
#include "session/imgui_session.h"

#include "imgui-ws/imgui-ws.h"

class GuiApp {
    public:
        GuiApp();
        int run(int argc, char ** argv);

    private:
        AppOptions options_;
        ImGuiWS imguiWS_;
        ImGuiSession session_;
        GuiContext guiContext_;
        VSync vsync_;

        bool init();
        void tick();
        void shutdown();
};
