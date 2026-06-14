#include "views.h"

#include "gui/font_atlas.h"
#include "gui/gui_context.h"
#include "session/imgui_session.h"

#include "imgui/imgui.h"
#include "imgui-ws/imgui-ws.h"

#include <algorithm>

void viewLauncher() {
    auto & ctx = GuiContext::instance();
    if (ImGui::Combo("Font", &ctx.fonts.selected, FontAtlas::comboGetter, &ctx.fonts.list, (int) ctx.fonts.list.size())) {
        ctx.fonts.setSelected(ctx.fonts.selected);
    }
    if (ImGui::ColorEdit4("Clear color", &ctx.canvasClearColor.x)) {
        ++ctx.canvasClearColorRev;
    }
    if (ImGui::SliderFloat("Масштаб UI", &ctx.uiScale, 0.25f, 4.0f, "%.2fx")) {
        ctx.uiScale = std::max(0.25f, ctx.uiScale);
    }
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::Separator();

    ImGui::TextUnformatted("ImGuiIO");
    ImGui::Indent();
    {
        const ImGuiIO & io = ImGui::GetIO();
        ImGui::Text("io.DisplaySize (логич.): %.0f x %.0f", io.DisplaySize.x, io.DisplaySize.y);
        ImGui::Text("io.DisplayFramebufferScale: %.2f x %.2f", io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        ImGui::Text("io.DeltaTime: %.3f ms", 1000.0f * io.DeltaTime);
    }
    ImGui::Unindent();
    ImGui::TextUnformatted("Клиент");
    ImGui::Indent();
    {
        const ImVec2 canvas = ctx.session.physicalCanvasSize();
        static ImGuiSession::ConnectedClient defaultClient{};
        const ImGuiSession::ConnectedClient * client = &defaultClient;
        const auto clientIt = ctx.session.clients.find(ctx.session.activeClientId);
        if (clientIt != ctx.session.clients.end() && clientIt->second.canvasWidth > 0) {
            client = &clientIt->second;
        }

        ImGui::Text("Масштаб UI: %.2fx (input scale: %.2fx)", ctx.uiScale, ctx.session.inputCoordinateScale());
        ImGui::Text("devicePixelRatio: %.2f", client->devicePixelRatio);
        ImGui::Text("Размер Canvas (пиксели): %.0f x %.0f", canvas.x, canvas.y);
        ImGui::Text("Canvas (клиент): %d x %d", client->canvasWidth, client->canvasHeight);
        ImGui::Text("CSS (оценка): %.0f x %.0f",
                client->canvasWidth / client->devicePixelRatio,
                client->canvasHeight / client->devicePixelRatio);
    }
    ImGui::Unindent();
    ImGui::TextUnformatted("Рендеринг");
    ImGui::Indent();
    {
        const auto encode = ctx.imguiWS.lastDrawDataEncodeInfo();
        const auto fontTex = ctx.imguiWS.textureInfo(0);

        ImGui::Text("Draw Lists: %d", ctx.renderStats.drawLists);
        ImGui::Text("Draw Commands: %d", ctx.renderStats.drawCmds);
        ImGui::Text("Вершины / индексы: %d / %d (%d треугольников)", ctx.renderStats.vtx, ctx.renderStats.idx, (ctx.renderStats.idx / 3));
        ImGui::Text("Время кодирования кадра: %.3f ms, %zu B", encode.encodeMs, encode.payloadBytes);
        ImGui::Text("Атлас шрифта: %d x %d, %zu B", fontTex.width, fontTex.height, fontTex.byteSize);
    }
    ImGui::Unindent();

    ImGui::Separator();
    ImGui::Text("WebSocket клиентов: %d", ctx.imguiWS.nConnected());
    for (auto & [cid, client] : ctx.session.clients) {
        ImGui::Text("%3d : %s", cid, client.ip.c_str());
        if (client.isControlling) {
            ImGui::SameLine();
            ImGui::TextDisabled(" [has control for %4.2f seconds]", ctx.session.controlSwitchTime - ImGui::GetTime());
        }
    }
}
