#pragma once

#include "imgui-ws/imgui-ws.h"

#include <string>

struct AppOptions {
    int port = 5000;
    double fps = 120.0;
    ImGuiWS::InitOptions wsInit;
    bool showUsage = false;
    bool usageIsError = false;

    static AppOptions parse(int argc, char ** argv);
    void printUsage(const char * prog) const;
};
