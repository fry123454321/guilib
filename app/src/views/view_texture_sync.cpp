#include "views.h"

#include "imgui/imgui.h"

#include <cstdint>

void viewTextureSync() {
    ImGui::Image((ImTextureID) (intptr_t) 5, ImVec2(512, 512));
}
