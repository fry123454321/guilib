#pragma once

#include "imgui/imgui.h"
#include "imgui-ws/imgui-ws.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct GuiInput;

// Сессия удалённого ImGui: несколько браузеров, один серверный контекст.
struct ImGuiSession {
    explicit ImGuiSession(GuiInput & input);

    // -------------------------------------------------------------------------
    // Подключённые клиенты
    // -------------------------------------------------------------------------

    struct ConnectedClient {
        bool isControlling = false;

        std::string ip = "---";
        int32_t canvasWidth = 0;
        int32_t canvasHeight = 0;
        float devicePixelRatio = 1.0f;
    };

    std::map<int, ConnectedClient> clients;

    // -------------------------------------------------------------------------
    // Round-robin: кто управляет UI
    // -------------------------------------------------------------------------

    float controlLeaseSeconds = 10.0f;
    float controlSwitchTime = 0.0f;
    int controlRoundRobinIndex = 0;
    int activeClientId = -1;

    // -------------------------------------------------------------------------
    // Canvas / масштаб
    // -------------------------------------------------------------------------

    static constexpr float kMinCanvasLogical = 256.0f;

    ImVec2 fallbackCanvasSize{1920.0f, 1080.0f};
    float fallbackDevicePixelRatio = 1.0f;

    void setUiScaleSource(const float * source);

    static bool isCanvasSizeUsable(int32_t width, int32_t height, float pixelRatio);
    void storeValidCanvasSize(int32_t width, int32_t height, float pixelRatio);

    ImVec2 physicalCanvasSize() const;
    ImVec2 imguiDisplaySize() const;
    float devicePixelRatio() const;
    float inputCoordinateScale() const;

    // -------------------------------------------------------------------------
    // Очередь ввода
    // -------------------------------------------------------------------------

    struct PendingInputEvent {
        enum Type {
            EKey,
            EMousePos,
            EMouseButton,
            EMouseWheel,
        };

        Type type = EMousePos;
        bool isDown = false;

        ImGuiKey key = ImGuiKey_COUNT;
        ImGuiMouseButton mouseButton = -1;
        ImVec2 mousePos;
        float mouseWheelX = 0.0f;
        float mouseWheelY = 0.0f;
    };

    std::vector<PendingInputEvent> pendingInput;
    std::string pendingTextInput;

    // -------------------------------------------------------------------------
    // Вставка из браузера
    // -------------------------------------------------------------------------

    bool simulateCtrlVDown = false;
    bool simulateCtrlVUp = false;

    bool canReceiveInputFrom(int clientId) const;

    // -------------------------------------------------------------------------
    // Кадр
    // -------------------------------------------------------------------------

    void onClientEvent(ImGuiWS::Event && event);
    void applyInputToImGui();

private:
    GuiInput * input_ = nullptr;
    const float * uiScaleSource_ = nullptr;

    float effectiveUiScale() const;
};
