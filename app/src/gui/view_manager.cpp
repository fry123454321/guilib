#include "view_manager.h"

#include <chrono>

ViewManager::Entry & ViewManager::addView(const char * category, const char * title, ViewFn draw, bool open) {
    entries_.push_back({ category, title, {}, open, 0.0, std::move(draw), 0, false });
    return entries_.back();
}

void ViewManager::setLauncherCpuMs(double ms) {
    launcherCpuMs_ = ms;
}

bool & ViewManager::currentOpen() {
    return currentEntry_->open;
}

const std::vector<ViewManager::Entry> & ViewManager::entries() const {
    return entries_;
}

std::vector<ViewManager::Entry> & ViewManager::entries() {
    return entries_;
}

double ViewManager::measureMs(const ViewFn & draw) {
    const auto t0 = std::chrono::steady_clock::now();
    draw();
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

void ViewManager::drawLauncher(const ViewFn & drawContent) {
    const auto t0 = std::chrono::steady_clock::now();

    if (ImGui::Begin("Launcher")) {
        drawContent();
        drawLauncherPanel();
    }
    ImGui::End();

    const auto t1 = std::chrono::steady_clock::now();
    setLauncherCpuMs(std::chrono::duration<double, std::milli>(t1 - t0).count());
}

void ViewManager::drawLauncherPanel() {
    ImGui::Separator();
    ImGui::Text("Views");
    ImGui::Text("Launcher:  %.3f ms", launcherCpuMs_);

    if (ImGui::BeginTable("view_manager", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("on", ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableSetupColumn("group", ImGuiTableColumnFlags_WidthFixed, 72.0f);
        ImGui::TableSetupColumn("title", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("cpu", ImGuiTableColumnFlags_WidthFixed, 72.0f);
        ImGui::TableHeadersRow();

        for (auto & entry : entries_) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(entry.title.c_str());
            ImGui::Checkbox("##visible", &entry.open);
            ImGui::PopID();
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(entry.category.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(entry.title.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.3f ms", entry.cpuMsLast);
        }

        ImGui::EndTable();
    }
}

void ViewManager::drawViews() {
    for (auto & entry : entries_) {
        if (!entry.open || !entry.draw) {
            entry.cpuMsLast = 0.0;
            continue;
        }

        currentEntry_ = &entry;

        if (entry.managesOwnWindow) {
            entry.cpuMsLast = measureMs(entry.draw);
        } else {
            const char * windowTitle = entry.titleFn.empty() ? entry.title.c_str() : entry.titleFn.c_str();
            if (ImGui::Begin(windowTitle, &entry.open, entry.windowFlags)) {
                entry.cpuMsLast = measureMs(entry.draw);
            } else {
                entry.cpuMsLast = 0.0;
            }
            ImGui::End();
        }

        currentEntry_ = nullptr;
    }
}
