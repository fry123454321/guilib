#include "views.h"

#include "imgui/imgui.h"

#include <cstring>
#include <vector>

namespace {

constexpr std::size_t kTextCapacity = 64 * 1024;

std::vector<char> & editorBuffer() {
    static std::vector<char> buf(kTextCapacity, '\0');
    return buf;
}

void appendClipboardText(char * dst, std::size_t capacity) {
    const char * clip = ImGui::GetClipboardText();
    if (clip == nullptr || clip[0] == '\0') {
        return;
    }

    std::size_t used = std::strlen(dst);
    if (used > 0 && used < capacity - 1 && dst[used - 1] != '\n') {
        dst[used++] = '\n';
        dst[used] = '\0';
    }

    const std::size_t space = capacity - 1 - used;
    if (space == 0) {
        return;
    }

    std::strncat(dst, clip, space);
}

} // namespace

void viewClipboard() {
    auto & text = editorBuffer();

    if (ImGui::Button("Скопировать в буфер обмена")) {
        ImGui::SetClipboardText(text.data());
    }
    ImGui::SameLine();
    if (ImGui::Button("Вставить из буфера обмена")) {
        appendClipboardText(text.data(), text.size());
    }

    ImGui::Spacing();

    const float footer = ImGui::GetFrameHeightWithSpacing();
    const ImVec2 editorSize(-1.0f, ImGui::GetContentRegionAvail().y - footer);
    ImGui::InputTextMultiline(
        "##clipboard_editor",
        text.data(),
        text.size(),
        editorSize,
        ImGuiInputTextFlags_AllowTabInput);

    ImGui::Separator();
    ImGui::TextDisabled("Символов: %zu", std::strlen(text.data()));
}
