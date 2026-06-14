#include "font_atlas.h"

#include "imgui-ws/imgui-ws.h"

#include <cstdio>
#include <vector>

void FontAtlas::load(const std::string & fontsDir) {
    ImGuiIO & io = ImGui::GetIO();
    io.Fonts->Clear();
    list.clear();
    selected = 0;

    const ImWchar * ranges = io.Fonts->GetGlyphRangesCyrillic();

    auto tryAdd = [&](const char * filename, const char * label) {
        const std::string path = fontsDir + "/" + filename;
        ImFont * font = io.Fonts->AddFontFromFileTTF(path.c_str(), 16.0f, nullptr, ranges);
        if (font) {
            list.push_back({ label, font });
        } else {
            fprintf(stderr, "Failed to load font '%s'\n", path.c_str());
        }
    };

    tryAdd("JetBrainsMono-Medium.ttf", "JetBrainsMono-Medium.ttf");
    //tryAdd("CascadiaCode.ttf", "CascadiaCode.ttf");
    //tryAdd("CascadiaMono.ttf", "CascadiaMono.ttf");
    //tryAdd("Ubuntu-R.ttf", "Ubuntu-R.ttf");
    //tryAdd("UbuntuMono-R.ttf", "UbuntuMono-R.ttf");
    //tryAdd("DroidSans.ttf", "Droid Sans");
    //tryAdd("Roboto-Medium.ttf", "Roboto Medium");

    ImFontConfig defaultCfg;
    defaultCfg.GlyphRanges = ranges;
    if (ImFont * font = io.Fonts->AddFontDefault(&defaultCfg)) {
        list.push_back({ "Default", font });
    }

    if (!list.empty()) {
        selected = 0;
        io.FontDefault = list[0].font;
    }
}

void FontAtlas::setSelected(int idx) {
    if (idx < 0 || idx >= (int) list.size()) {
        return;
    }
    selected = idx;
    ImGui::GetIO().FontDefault = list[idx].font;
}

void FontAtlas::uploadTexture(ImGuiWS & imguiWS) {
    unsigned char * pixels = nullptr;
    int width = 0;
    int height = 0;
    ImGui::GetIO().Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);
    imguiWS.setTexture(0, ImGuiWS::Texture::Type::Alpha8, width, height, (const char *) pixels);
}

bool FontAtlas::comboGetter(void * data, int idx, const char ** out_text) {
    auto & entries = *static_cast<std::vector<Entry> *>(data);
    if (idx < 0 || idx >= (int) entries.size()) {
        return false;
    }
    *out_text = entries[idx].label.c_str();
    return true;
}