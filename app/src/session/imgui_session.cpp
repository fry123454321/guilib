#include "imgui_session.h"

#include "input/input.h"

#include <cstdio>

namespace {

ImGuiMouseButton mapBrowserMouseButton(int browserButton) {
    switch (browserButton) {
        case 1: return ImGuiMouseButton_Middle;
        case 2: return ImGuiMouseButton_Right;
        default: return (ImGuiMouseButton) browserButton;
    }
}

ImVec2 scaledMousePos(const ImGuiSession & session, float x, float y) {
    const float inv = 1.0f / session.inputCoordinateScale();
    return { x * inv, y * inv };
}

void enqueueMousePos(ImGuiSession & session, float x, float y) {
    session.pendingInput.push_back(ImGuiSession::PendingInputEvent {
        ImGuiSession::PendingInputEvent::Type::EMousePos,
        false,
        ImGuiKey_COUNT,
        -1,
        scaledMousePos(session, x, y),
        0.0f,
        0.0f,
    });
}

void enqueueMouseButton(ImGuiSession & session, float x, float y, int browserButton, bool isDown) {
    session.pendingInput.push_back(ImGuiSession::PendingInputEvent {
        ImGuiSession::PendingInputEvent::Type::EMouseButton,
        isDown,
        ImGuiKey_COUNT,
        mapBrowserMouseButton(browserButton),
        scaledMousePos(session, x, y),
        0.0f,
        0.0f,
    });
}

void enqueueKey(ImGuiSession & session, ImGuiKey key, bool isDown) {
    session.pendingInput.push_back(ImGuiSession::PendingInputEvent {
        ImGuiSession::PendingInputEvent::Type::EKey,
        isDown,
        key,
        -1,
        { },
        0.0f,
        0.0f,
    });
}

void enqueueMouseWheel(ImGuiSession & session, float wheelX, float wheelY) {
    session.pendingInput.push_back(ImGuiSession::PendingInputEvent {
        ImGuiSession::PendingInputEvent::Type::EMouseWheel,
        false,
        ImGuiKey_COUNT,
        -1,
        { },
        wheelX,
        wheelY,
    });
}

void rotateActiveClient(ImGuiSession & session) {
    if (session.clients.find(session.activeClientId) != session.clients.end()) {
        session.clients[session.activeClientId].isControlling = false;
    }

    const int k = ++session.controlRoundRobinIndex % (int) session.clients.size();
    auto client = session.clients.begin();
    std::advance(client, k);

    client->second.isControlling = true;
    session.activeClientId = client->first;
    session.controlSwitchTime = ImGui::GetTime() + session.controlLeaseSeconds;

    ImGui::GetIO().ClearInputKeys();
    session.simulateCtrlVDown = false;
    session.simulateCtrlVUp = false;
}

void applySimulatedPaste(ImGuiIO & io, ImGuiSession & session) {
    if (session.simulateCtrlVDown) {
        io.AddKeyEvent(ImGuiKey_ModCtrl, true);
        io.AddKeyEvent(ImGuiKey_V, true);
        session.simulateCtrlVDown = false;
        session.simulateCtrlVUp = true;
        return;
    }

    if (session.simulateCtrlVUp) {
        io.AddKeyEvent(ImGuiKey_V, false);
        session.simulateCtrlVUp = false;
    }
}

void applyPendingInput(ImGuiIO & io, const ImGuiSession & session) {
    for (const auto & event : session.pendingInput) {
        switch (event.type) {
            case ImGuiSession::PendingInputEvent::Type::EKey:
                if (event.key >= ImGuiKey_NamedKey_BEGIN && event.key < ImGuiKey_COUNT) {
                    io.AddKeyEvent(event.key, event.isDown);
                }
                break;
            case ImGuiSession::PendingInputEvent::Type::EMousePos:
                io.AddMousePosEvent(event.mousePos.x, event.mousePos.y);
                break;
            case ImGuiSession::PendingInputEvent::Type::EMouseButton:
                io.AddMouseButtonEvent(event.mouseButton, event.isDown);
                io.AddMousePosEvent(event.mousePos.x, event.mousePos.y);
                break;
            case ImGuiSession::PendingInputEvent::Type::EMouseWheel:
                io.AddMouseWheelEvent(event.mouseWheelX, event.mouseWheelY);
                break;
        }
    }
}

} // namespace

// =============================================================================
// Конструктор / uiScale
// =============================================================================

ImGuiSession::ImGuiSession(GuiInput & input)
    : input_(&input) {}

void ImGuiSession::setUiScaleSource(const float * source) {
    uiScaleSource_ = source;
}

float ImGuiSession::effectiveUiScale() const {
    if (uiScaleSource_ != nullptr && *uiScaleSource_ > 0.01f) {
        return *uiScaleSource_;
    }
    return 1.0f;
}

// =============================================================================
// Canvas / display
// =============================================================================

bool ImGuiSession::isCanvasSizeUsable(int32_t width, int32_t height, float pixelRatio) {
    if (width <= 0 || height <= 0 || pixelRatio <= 0.01f) {
        return false;
    }
    return (width / pixelRatio) >= kMinCanvasLogical && (height / pixelRatio) >= kMinCanvasLogical;
}

void ImGuiSession::storeValidCanvasSize(int32_t width, int32_t height, float pixelRatio) {
    if (!isCanvasSizeUsable(width, height, pixelRatio)) {
        return;
    }
    fallbackCanvasSize = ImVec2((float) width, (float) height);
    fallbackDevicePixelRatio = pixelRatio;
}

ImVec2 ImGuiSession::physicalCanvasSize() const {
    if (activeClientId >= 0) {
        const auto it = clients.find(activeClientId);
        if (it != clients.end()
            && isCanvasSizeUsable(it->second.canvasWidth, it->second.canvasHeight, it->second.devicePixelRatio)) {
            return ImVec2((float) it->second.canvasWidth, (float) it->second.canvasHeight);
        }
    }

    for (const auto & [_, client] : clients) {
        if (isCanvasSizeUsable(client.canvasWidth, client.canvasHeight, client.devicePixelRatio)) {
            return ImVec2((float) client.canvasWidth, (float) client.canvasHeight);
        }
    }

    return fallbackCanvasSize;
}

ImVec2 ImGuiSession::imguiDisplaySize() const {
    const ImVec2 canvas = physicalCanvasSize();
    const float dpr = devicePixelRatio();
    const float scale = effectiveUiScale();
    return ImVec2(canvas.x / (dpr * scale), canvas.y / (dpr * scale));
}

float ImGuiSession::devicePixelRatio() const {
    if (activeClientId >= 0) {
        const auto it = clients.find(activeClientId);
        if (it != clients.end() && it->second.devicePixelRatio > 0.01f) {
            return it->second.devicePixelRatio;
        }
    }

    for (const auto & [_, client] : clients) {
        if (client.devicePixelRatio > 0.01f) {
            return client.devicePixelRatio;
        }
    }

    return fallbackDevicePixelRatio;
}

float ImGuiSession::inputCoordinateScale() const {
    return devicePixelRatio() * effectiveUiScale();
}

bool ImGuiSession::canReceiveInputFrom(int clientId) const {
    return activeClientId < 0 || clientId == activeClientId;
}

// =============================================================================
// WebSocket events → очередь ввода
// =============================================================================

void ImGuiSession::onClientEvent(ImGuiWS::Event && event) {
    switch (event.type) {
        case ImGuiWS::Event::Connected:
            clients[event.clientId].ip = event.ip;
            break;

        case ImGuiWS::Event::Resize:
            {
                auto & client = clients[event.clientId];
                client.canvasWidth = event.client_width;
                client.canvasHeight = event.client_height;
                client.devicePixelRatio = event.client_pixel_ratio;
                storeValidCanvasSize(event.client_width, event.client_height, event.client_pixel_ratio);
            }
            break;

        case ImGuiWS::Event::Disconnected:
            clients.erase(event.clientId);
            break;

        case ImGuiWS::Event::MouseMove:
            if (event.clientId == activeClientId) {
                enqueueMousePos(*this, event.mouse_x, event.mouse_y);
            }
            break;

        case ImGuiWS::Event::MouseDown:
            if (event.clientId == activeClientId) {
                enqueueMouseButton(*this, event.mouse_x, event.mouse_y, event.mouse_but, true);
            }
            break;

        case ImGuiWS::Event::MouseUp:
            if (event.clientId == activeClientId) {
                enqueueMouseButton(*this, event.mouse_x, event.mouse_y, event.mouse_but, false);
            }
            break;

        case ImGuiWS::Event::MouseWheel:
            if (event.clientId == activeClientId) {
                enqueueMouseWheel(*this, event.wheel_x, event.wheel_y);
            }
            break;

        case ImGuiWS::Event::KeyUp:
            if (event.clientId == activeClientId && event.key > 0) {
                const ImGuiKey keyImGui = GuiInput::mapKey(event.key);
                if (keyImGui != ImGuiKey_COUNT) {
                    enqueueKey(*this, keyImGui, false);
                }
            }
            break;

        case ImGuiWS::Event::KeyDown:
            if (event.clientId == activeClientId && event.key > 0) {
                const ImGuiKey keyImGui = GuiInput::mapKey(event.key);
                if (keyImGui != ImGuiKey_COUNT) {
                    enqueueKey(*this, keyImGui, true);
                }
            }
            break;

        case ImGuiWS::Event::KeyPress:
            if (event.clientId == activeClientId && !event.input_text.empty()) {
                pendingTextInput = event.input_text;
            }
            break;

        case ImGuiWS::Event::PasteFromBrowser:
            if (canReceiveInputFrom(event.clientId)) {
                input_->setFromBrowser(std::move(event.clipboard_text));
                simulateCtrlVDown = true;
            }
            break;

        default:
            printf("Unknown input event\n");
            break;
    }
}

// =============================================================================
// Очередь ввода → ImGuiIO
// =============================================================================

void ImGuiSession::applyInputToImGui() {
    if (clients.size() > 0
        && (clients.find(activeClientId) == clients.end() || ImGui::GetTime() > controlSwitchTime)) {
        rotateActiveClient(*this);
    }

    if (clients.empty()) {
        activeClientId = -1;
    }

    if (activeClientId == -1) {
        return;
    }

    ImGuiIO & io = ImGui::GetIO();

    applySimulatedPaste(io, *this);

    if (!pendingTextInput.empty()) {
        io.AddInputCharactersUTF8(pendingTextInput.c_str());
    }

    applyPendingInput(io, *this);

    pendingInput.clear();
    pendingTextInput.clear();
}
