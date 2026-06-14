#pragma once

#include "imgui/imgui.h"

#include <functional>
#include <string>
#include <vector>

class ViewManager {
    public:
        using ViewFn = std::function<void()>;

        struct Entry {
            std::string category;
            std::string title;
            std::string titleFn;
            bool open = true;
            double cpuMsLast = 0.0;
            ViewFn draw;
            ImGuiWindowFlags windowFlags = 0;
            bool managesOwnWindow = false;
        };

        Entry & addView(const char * category, const char * title, ViewFn draw, bool open = true);
        void setLauncherCpuMs(double ms);

        void drawLauncher(const ViewFn & drawContent);
        void drawLauncherPanel();
        void drawViews();

        bool & currentOpen();

        std::vector<Entry> & entries();
        const std::vector<Entry> & entries() const;

    private:
        std::vector<Entry> entries_;
        Entry * currentEntry_ = nullptr;
        double launcherCpuMs_ = 0.0;

        static double measureMs(const ViewFn & draw);
};
