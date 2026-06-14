#include "input.h"

#include "imgui-ws/imgui-ws.h"

#include <cstdio>

namespace {

// =============================================================================
// Helpers: логирование
// =============================================================================

std::string quotedPreview(const std::string & text) {
    const std::string preview = text.size() > 200 ? text.substr(0, 200) + "..." : text;
    std::string out = "\"";
    for (const char c : preview) {
        switch (c) {
            case '"':
            case '\\':
                out += '\\';
                out += c;
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out += c;
                break;
        }
    }
    out += "\"";
    return out;
}

void logCopyAttempt(const std::string & text) {
    fprintf(stderr, "попытка скопировать текст %s\n", quotedPreview(text).c_str());
}

void logPasteAttempt(const std::string & text) {
    fprintf(stderr, "попытка вставить текст %s\n", quotedPreview(text).c_str());
}

} // namespace

// =============================================================================
// Singleton
// =============================================================================

GuiInput & GuiInput::instance() {
    static GuiInput input;
    return input;
}

// =============================================================================
// Буфер обмена
// =============================================================================

const char * GuiInput::imguiClipboardText() const {
    return clipboard.fromBrowser.c_str();
}

void GuiInput::setToBrowser(const char * text) {
    clipboard.toBrowser = text ? text : "";
    clipboard.fromBrowser = clipboard.toBrowser;
    ++clipboard.toBrowserRev;
    logCopyAttempt(clipboard.toBrowser);
}

void GuiInput::setFromBrowser(std::string text) {
    clipboard.fromBrowser = std::move(text);
    logPasteAttempt(clipboard.fromBrowser);
}

int32_t GuiInput::toBrowserRev() const {
    return clipboard.toBrowserRev;
}

std::string_view GuiInput::toBrowserRevVar() const {
    return std::string_view(
        reinterpret_cast<const char*>(&clipboard.toBrowserRev),
        sizeof(clipboard.toBrowserRev));
}

std::string_view GuiInput::toBrowserVar() const {
    if (clipboard.toBrowser.empty()) {
        static char empty = '\0';
        return std::string_view(&empty, 1);
    }
    return std::string_view(clipboard.toBrowser.c_str(), clipboard.toBrowser.size() + 1);
}

void GuiInput::bindImGuiClipboard() {
    ImGuiIO & io = ImGui::GetIO();
    io.GetClipboardTextFn = imguiGetClipboardFn;
    io.SetClipboardTextFn = imguiSetClipboardFn;
    io.ClipboardUserData = &instance();
}

void GuiInput::registerIncppectVars(ImGuiWS & imguiWS) {
    imguiWS.addVar("imgui.clipboard_rev", [](const auto &) {
        return GuiInput::instance().toBrowserRevVar();
    });

    imguiWS.addVar("imgui.clipboard_text", [](const auto &) {
        return GuiInput::instance().toBrowserVar();
    });
}

const char * GuiInput::imguiGetClipboardFn(void * user_data) {
    return static_cast<GuiInput *>(user_data)->imguiClipboardText();
}

void GuiInput::imguiSetClipboardFn(void * user_data, const char * text) {
    static_cast<GuiInput *>(user_data)->setToBrowser(text);
}

// =============================================================================
// Keymap: JS keyCode → ImGuiKey
// =============================================================================

ImGuiKey GuiInput::mapKey(int32_t keyCode) {
    switch (keyCode) {
        case 8: return ImGuiKey_Backspace;
        case 9: return ImGuiKey_Tab;
        case 13: return ImGuiKey_Enter;
        case 16: return ImGuiKey_ModShift;
        case 17: return ImGuiKey_ModCtrl;
        case 18: return ImGuiKey_ModAlt;
        case 19: return ImGuiKey_Pause;
        case 20: return ImGuiKey_CapsLock;
        case 27: return ImGuiKey_Escape;
        case 32: return ImGuiKey_Space;
        case 33: return ImGuiKey_PageUp;
        case 34: return ImGuiKey_PageDown;
        case 35: return ImGuiKey_End;
        case 36: return ImGuiKey_Home;
        case 37: return ImGuiKey_LeftArrow;
        case 38: return ImGuiKey_UpArrow;
        case 39: return ImGuiKey_RightArrow;
        case 40: return ImGuiKey_DownArrow;
        case 45: return ImGuiKey_Insert;
        case 46: return ImGuiKey_Delete;
        case 48: return ImGuiKey_0;
        case 49: return ImGuiKey_1;
        case 50: return ImGuiKey_2;
        case 51: return ImGuiKey_3;
        case 52: return ImGuiKey_4;
        case 53: return ImGuiKey_5;
        case 54: return ImGuiKey_6;
        case 55: return ImGuiKey_7;
        case 56: return ImGuiKey_8;
        case 57: return ImGuiKey_9;
        case 65: return ImGuiKey_A;
        case 66: return ImGuiKey_B;
        case 67: return ImGuiKey_C;
        case 68: return ImGuiKey_D;
        case 69: return ImGuiKey_E;
        case 70: return ImGuiKey_F;
        case 71: return ImGuiKey_G;
        case 72: return ImGuiKey_H;
        case 73: return ImGuiKey_I;
        case 74: return ImGuiKey_J;
        case 75: return ImGuiKey_K;
        case 76: return ImGuiKey_L;
        case 77: return ImGuiKey_M;
        case 78: return ImGuiKey_N;
        case 79: return ImGuiKey_O;
        case 80: return ImGuiKey_P;
        case 81: return ImGuiKey_Q;
        case 82: return ImGuiKey_R;
        case 83: return ImGuiKey_S;
        case 84: return ImGuiKey_T;
        case 85: return ImGuiKey_U;
        case 86: return ImGuiKey_V;
        case 87: return ImGuiKey_W;
        case 88: return ImGuiKey_X;
        case 89: return ImGuiKey_Y;
        case 90: return ImGuiKey_Z;
        case 91: return ImGuiKey_ModSuper;
        case 92: return ImGuiKey_ModSuper;
        case 93: return ImGuiKey_ModSuper;
        case 96: return ImGuiKey_Keypad0;
        case 97: return ImGuiKey_Keypad1;
        case 98: return ImGuiKey_Keypad2;
        case 99: return ImGuiKey_Keypad3;
        case 100: return ImGuiKey_Keypad4;
        case 101: return ImGuiKey_Keypad5;
        case 102: return ImGuiKey_Keypad6;
        case 103: return ImGuiKey_Keypad7;
        case 104: return ImGuiKey_Keypad8;
        case 105: return ImGuiKey_Keypad9;
        case 106: return ImGuiKey_KeypadMultiply;
        case 107: return ImGuiKey_KeypadAdd;
        case 108: return ImGuiKey_KeypadEnter;
        case 109: return ImGuiKey_KeypadSubtract;
        case 110: return ImGuiKey_KeypadDecimal;
        case 111: return ImGuiKey_KeypadDivide;
        case 112: return ImGuiKey_F1;
        case 113: return ImGuiKey_F2;
        case 114: return ImGuiKey_F3;
        case 115: return ImGuiKey_F4;
        case 116: return ImGuiKey_F5;
        case 117: return ImGuiKey_F6;
        case 118: return ImGuiKey_F7;
        case 119: return ImGuiKey_F8;
        case 120: return ImGuiKey_F9;
        case 121: return ImGuiKey_F10;
        case 122: return ImGuiKey_F11;
        case 123: return ImGuiKey_F12;
        case 144: return ImGuiKey_NumLock;
        case 145: return ImGuiKey_ScrollLock;
        case 186: return ImGuiKey_Semicolon;
        case 187: return ImGuiKey_Equal;
        case 188: return ImGuiKey_Comma;
        case 189: return ImGuiKey_Minus;
        case 190: return ImGuiKey_Period;
        case 191: return ImGuiKey_Slash;
        case 219: return ImGuiKey_LeftBracket;
        case 220: return ImGuiKey_Backslash;
        case 221: return ImGuiKey_RightBracket;
        case 222: return ImGuiKey_Apostrophe;
        default: return ImGuiKey_COUNT;
    }
}
