#pragma once

#include "imgui/imgui.h"
#include "font_atlas.h"
#include "view_manager.h"

#include <cstdint>
#include <string>

class ImGuiWS;
struct ImGuiSession;

struct RenderStats {
    int vtx = 0;
    int idx = 0;
    int drawLists = 0;
    int drawCmds = 0;
};

class GuiContext {
    public:
        GuiContext(ImGuiWS & imguiWS, ImGuiSession & session);
        ImGuiWS & imguiWS;
        ImGuiSession & session;
        
        void init(const std::string & fontsDir);

        // -------------------------------------------------------------------------
        // Синглтон. проверить необходимость
        // -------------------------------------------------------------------------
        ViewManager & viewManager();
        static GuiContext & instance();
        static void bind(GuiContext & ctx);

        // -------------------------------------------------------------------------
        // Синхронизация с браузером (incppect vars)
        // -------------------------------------------------------------------------
        float uiScale = 1.0f;
        ImVec4 canvasClearColor{0.45f, 0.33f, 0.60f, 1.00f};
        int32_t canvasClearColorRev = 1;
        FontAtlas fonts;
        void registerGuiContextVars();
        
        // -------------------------------------------------------------------------
        // Статистика
        // -------------------------------------------------------------------------
        RenderStats renderStats;
        void updateRenderStats(const ImDrawData * drawData);

    private:
        ViewManager viewManager_;
        static GuiContext * instance_;
};
