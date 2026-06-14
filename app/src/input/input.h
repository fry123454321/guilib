#pragma once

#include "imgui/imgui.h"

#include <cstdint>
#include <string>
#include <string_view>

class ImGuiWS;

// Мост ввода между браузером и ImGui: буфер обмена и keymap JS → ImGuiKey.
struct GuiInput {
    GuiInput() = default;

    // -------------------------------------------------------------------------
    // Буфер обмена (синхронизация с браузером через incppect)
    // -------------------------------------------------------------------------

    struct Clipboard {
        std::string fromBrowser;
        std::string toBrowser;
        int32_t toBrowserRev = 0;
    };

    Clipboard clipboard;

    void bindImGuiClipboard();
    void registerIncppectVars(ImGuiWS & imguiWS);

    const char * imguiClipboardText() const;
    void setFromBrowser(std::string text);
    void setToBrowser(const char * text);

    int32_t toBrowserRev() const;
    std::string_view toBrowserRevVar() const;
    std::string_view toBrowserVar() const;

    // -------------------------------------------------------------------------
    // Keymap: JS keyCode → ImGuiKey
    // -------------------------------------------------------------------------

    static ImGuiKey mapKey(int32_t keyCode);

    // -------------------------------------------------------------------------
    // Singleton (один ImGui-контекст на процесс)
    // -------------------------------------------------------------------------

    static GuiInput & instance();

private:
    static const char * imguiGetClipboardFn(void * user_data);
    static void imguiSetClipboardFn(void * user_data, const char * text);
};
