#include "gui_context.h"

#include "font_atlas.h"

#include "imgui-ws/imgui-ws.h"

GuiContext * GuiContext::instance_ = nullptr;

GuiContext::GuiContext(ImGuiWS & imguiWS, ImGuiSession & session)
    : imguiWS(imguiWS), session(session) {}

GuiContext & GuiContext::instance() {
    return *instance_;
}

void GuiContext::init(const std::string & fontsDir) {
    fonts.load(fontsDir);
    fonts.uploadTexture(imguiWS);
    registerGuiContextVars();
}

void GuiContext::bind(GuiContext & ctx) {
    instance_ = &ctx;
}

ViewManager & GuiContext::viewManager() {
    return viewManager_;
}

void GuiContext::registerGuiContextVars() {
    imguiWS.addVar("imgui.clear_color_rev", [this](const auto &) {
        return std::string_view(
            reinterpret_cast<const char*>(&canvasClearColorRev),
            sizeof(canvasClearColorRev));
    });

    imguiWS.addVar("imgui.clear_color[%d]", [this](const auto & idxs) {
        if (idxs.empty() || idxs[0] < 0 || idxs[0] > 3) {
            return std::string_view{};
        }
        const float * components = &canvasClearColor.x;
        return std::string_view(
            reinterpret_cast<const char*>(&components[idxs[0]]),
            sizeof(float));
    });

    imguiWS.addVar("imgui.ui_scale", [this](const auto &) {
        return std::string_view(reinterpret_cast<const char*>(&uiScale), sizeof(uiScale));
    });
}

 

void GuiContext::updateRenderStats(const ImDrawData * drawData) {
    if (drawData == nullptr) {
        return;
    }

    renderStats = {};
    renderStats.drawLists = drawData->CmdListsCount;
    renderStats.vtx = drawData->TotalVtxCount;
    renderStats.idx = drawData->TotalIdxCount;
    for (int i = 0; i < drawData->CmdListsCount; ++i) {
        renderStats.drawCmds += drawData->CmdLists[i]->CmdBuffer.Size;
    }
}
