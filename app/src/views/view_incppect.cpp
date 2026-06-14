#include "views.h"

#include "gui/gui_context.h"

#include "imgui/imgui.h"
#include "imgui-ws/imgui-ws.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

int countPathIndices(const std::string & path) {
    int count = 0;
    std::string::size_type pos = 0;
    while ((pos = path.find("[%d]", pos)) != std::string::npos) {
        ++count;
        pos += 4;
    }
    return count;
}

bool isDrawListPath(const std::string & path) {
    return path.find("draw_list[%d]") != std::string::npos;
}

bool isTextureDataPath(const std::string & path) {
    return path == "imgui.texture_data[%d]";
}

std::vector<int> makePreviewIdxs(const std::string & path, int previewIndex) {
    const int n = countPathIndices(path);
    return std::vector<int>(n, previewIndex);
}

bool isMostlyPrintable(const std::vector<char> & data) {
    if (data.empty()) {
        return true;
    }

    int printable = 0;
    for (const char c : data) {
        const auto uc = static_cast<unsigned char>(c);
        if (uc == 0) {
            break;
        }
        if (std::isprint(uc)) {
            ++printable;
        }
    }

    return printable * 2 >= (int) data.size();
}

std::string truncateText(const std::vector<char> & data, std::size_t maxLen = 48) {
    std::string out;
    out.reserve(data.size());
    for (const char c : data) {
        if (c == 0) {
            break;
        }
        if ((std::size_t) out.size() >= maxLen) {
            out += "...";
            break;
        }
        out.push_back(c);
    }
    return out;
}

std::string formatHexPreview(const std::vector<char> & data, std::size_t maxBytes = 32) {
    std::string out;
    const std::size_t n = data.size() < maxBytes ? data.size() : maxBytes;
    char buf[4] = {};

    for (std::size_t i = 0; i < n; ++i) {
        std::snprintf(buf, sizeof(buf), "%02X ", (unsigned char) data[i]);
        out += buf;
    }

    if (data.size() > n) {
        out += "...";
    }
    return out;
}

std::string formatVarPreview(const std::vector<char> & data) {
    if (data.empty()) {
        return "(empty)";
    }

    if (isMostlyPrintable(data) && data.size() <= 128) {
        return "\"" + truncateText(data) + "\"";
    }

    if (data.size() == 4) {
        int32_t i32 = 0;
        float f32 = 0.0f;
        std::memcpy(&i32, data.data(), sizeof(i32));
        std::memcpy(&f32, data.data(), sizeof(f32));
        char buf[64];
        std::snprintf(buf, sizeof(buf), "i32=%d  f32=%.4g", (int) i32, (double) f32);
        return buf;
    }

    if (data.size() == 8) {
        double f64 = 0.0;
        int64_t i64 = 0;
        std::memcpy(&f64, data.data(), sizeof(f64));
        std::memcpy(&i64, data.data(), sizeof(i64));
        char buf[80];
        std::snprintf(buf, sizeof(buf), "i64=%lld  f64=%.6g", (long long) i64, f64);
        return buf;
    }

    if (data.size() > 256) {
        return std::to_string(data.size()) + " B (binary)";
    }

    return std::to_string(data.size()) + " B  " + formatHexPreview(data, 12);
}

const char * textureTypeName(ImGuiWS::Texture::Type type) {
    switch (type) {
        case ImGuiWS::Texture::Type::Alpha8: return "Alpha8";
        case ImGuiWS::Texture::Type::Gray8: return "Gray8";
        case ImGuiWS::Texture::Type::RGB24: return "RGB24";
        case ImGuiWS::Texture::Type::RGBA32: return "RGBA32";
    }
    return "?";
}

void showTexturePreview(ImGuiWS & imguiWS, ImGuiWS::TextureId texId) {
    const auto info = imguiWS.textureInfo(texId);
    if (!info.valid) {
        ImGui::TextDisabled("(текстура не найдена)");
        return;
    }

    ImGui::Text(
        "id=%u  type=%s  %dx%d  rev=%d  (%zu B)",
        info.id,
        textureTypeName(info.type),
        info.width,
        info.height,
        info.revision,
        info.byteSize);

    constexpr float maxDisplay = 200.0f;
    const float scale = std::min({
        maxDisplay / std::max(info.width, 1),
        maxDisplay / std::max(info.height, 1),
        1.0f,
    });

    ImGui::Image(
        (ImTextureID) (intptr_t) texId,
        ImVec2(info.width * scale, info.height * scale));
}

} // namespace

void viewIncppect() {
    auto & ctx = GuiContext::instance();
    auto & imguiWS = ctx.imguiWS;

    static int selectedVar = -1;
    static int previewIndex = 0;

    if (ImGui::CollapsingHeader("Как работает передача", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped(
            "HTTP: браузер загружает index.html, incppect.js, imgui-ws.js из /static/* "
            "(LIBGUI_ROOT_DIR/static, по умолчанию ./static от корня приложения).\n"
            "WebSocket /incppect: клиент сначала отправляет карту var path -> id, затем каждые ~16 ms "
            "список id нужных переменных. Сервер вызывает getter для каждой var и отвечает бинарным пакетом "
            "(полные данные или XOR-diff). Клиент читает vars через get_*() и рисует ImGui сцену в WebGL."
        );
        ImGui::Spacing();
        ImGui::Text("staticRoot: %s", imguiWS.staticRoot().c_str());
        ImGui::Text("httpRoot: %s", imguiWS.httpRoot().c_str());
        ImGui::Text("Клиентов: %d", imguiWS.nConnected());
    }

    if (ImGui::CollapsingHeader("Vars", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto vars = imguiWS.varPaths();
        ImGui::Text("Зарегистрировано: %zu", vars.size());
        ImGui::Separator();

        if (ImGui::BeginChild("vars_list", ImVec2(0, 160), true)) {
            for (int i = 0; i < (int) vars.size(); ++i) {
                const bool selected = selectedVar == i;
                if (ImGui::Selectable(vars[i].c_str(), selected)) {
                    selectedVar = i;
                }
            }
        }
        ImGui::EndChild();

        if (selectedVar >= 0 && selectedVar < (int) vars.size()) {
            const auto & path = vars[selectedVar];

            ImGui::Separator();
            ImGui::Text("Выбрано: %s", path.c_str());

            if (isDrawListPath(path)) {
                ImGui::TextDisabled("Предпросмотр недоступен для draw_list");
            } else if (isTextureDataPath(path)) {
                ImGui::SliderInt("Texture ID", &previewIndex, 0, 32);
                showTexturePreview(imguiWS, (ImGuiWS::TextureId) previewIndex);
            } else {
                if (countPathIndices(path) > 0) {
                    ImGui::SliderInt("Индекс [%d]", &previewIndex, 0, 32);
                }

                const auto data = imguiWS.sampleVar(path, makePreviewIdxs(path, previewIndex));
                ImGui::Text("Размер: %zu B", data.size());
                ImGui::TextWrapped("Preview: %s", formatVarPreview(data).c_str());

                if (!data.empty() && data.size() <= 256) {
                    ImGui::TextWrapped("hex: %s", formatHexPreview(data, data.size()).c_str());
                } else if (data.size() > 256) {
                    ImGui::TextWrapped("hex: %s", formatHexPreview(data, 32).c_str());
                }
            }
        } else {
            ImGui::Separator();
            ImGui::TextDisabled("Выберите var для предпросмотра");
        }
    }

    if (ImGui::CollapsingHeader("HTTP routes")) {
        ImGui::TextWrapped("Маршруты из init(resources). Пустая строка = корень не регистрируется.");
        ImGui::Separator();
        for (const auto & route : imguiWS.httpResourceRoutes()) {
            ImGui::BulletText("/%s", route.c_str());
        }
        ImGui::Separator();
        ImGui::TextDisabled("Встроенные маршруты:");
        ImGui::BulletText("/incppect (WebSocket)");
        ImGui::BulletText("/incppect.js");
        ImGui::BulletText("/static/*");
    }

    if (ImGui::CollapsingHeader("Embedded resources")) {
        ImGui::TextWrapped("Контент в памяти (setResource), отдаётся без чтения с диска.");
        ImGui::Separator();
        const auto resources = imguiWS.embeddedResources();
        if (resources.empty()) {
            ImGui::TextDisabled("(нет)");
        } else {
            for (const auto & url : resources) {
                ImGui::BulletText("%s", url.c_str());
            }
        }
    }
}
