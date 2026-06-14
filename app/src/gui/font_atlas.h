#pragma once

#include "imgui/imgui.h"

#include <string>
#include <vector>

class ImGuiWS;

struct FontAtlas {
    struct Entry {
        std::string label;
        ImFont * font = nullptr;
    };

    std::vector<Entry> list;
    int selected = 0;

    void load(const std::string & fontsDir);
    void setSelected(int idx);
    void uploadTexture(ImGuiWS & imguiWS);

    static bool comboGetter(void * data, int idx, const char ** out_text);
};
