#include "gui_app.h"

#include "core/env.h"
#include "views/view_registry.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <string>
#include <thread>

GuiApp::GuiApp()
    : session_(GuiInput::instance())
    , guiContext_(imguiWS_, session_)
    , vsync_(120.0) {}

int GuiApp::run(int argc, char ** argv) {
    options_ = AppOptions::parse(argc, argv);
    if (options_.showUsage) {
        options_.printUsage(argv[0]);
        return options_.usageIsError ? 1 : 0;
    }

    vsync_ = VSync(options_.fps);

    if (!init()) {
        return 1;
    }

    while (true) {
        tick();
    }

    shutdown();
    return 0;
}

bool GuiApp::init() {
    const LibguiEnv & env = LibguiEnv::instance();
    options_.wsInit.staticRoot = env.staticDir();

    printf("Root: %s\n", env.rootDir().c_str());
    printf("Data: %s\n", env.dataDir().c_str());
    printf("Static: %s\n", env.staticDir().c_str());

    const char * scheme = options_.wsInit.useSsl ? "https" : "http";
    printf("%s://0.0.0.0:%d/static/index.html (all interfaces)\n", scheme, options_.port);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().MouseDrawCursor = false;

    static const std::string imguiIniPath = env.dataDir() + "/imgui.ini";
    ImGui::GetIO().IniFilename = imguiIniPath.c_str();
    ImGui::StyleColorsDark();

    GuiInput::instance().bindImGuiClipboard();

    if (!imguiWS_.init(options_.port, env.staticDir(), { "", "index.html" }, options_.wsInit)) {
        fprintf(stderr, "Failed to start imgui-ws server\n");
        return false;
    }

    GuiContext::bind(guiContext_);
    session_.setUiScaleSource(&guiContext_.uiScale);
    guiContext_.init(env.fontsDir());

    GuiInput::instance().registerIncppectVars(imguiWS_);

    registerViews(guiContext_.viewManager());

    return true;
}

void GuiApp::tick() {
    auto events = imguiWS_.takeEvents();
    for (auto & event : events) {
        session_.onClientEvent(std::move(event));
    }
    session_.applyInputToImGui();

    {
        auto & io = ImGui::GetIO();
        io.DisplaySize = session_.imguiDisplaySize();
        io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
        io.DeltaTime = vsync_.delta_s();
    }

    ImGui::NewFrame();

    renderViews(guiContext_.viewManager());

    ImGui::Render();

    if (const ImDrawData * drawData = ImGui::GetDrawData()) {
        guiContext_.updateRenderStats(drawData);
        if (imguiWS_.nConnected() > 0) {
            imguiWS_.setDrawData(drawData);
        }
    }

    if (imguiWS_.nConnected() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    vsync_.wait();
}

void GuiApp::shutdown() {
    ImGui::DestroyContext();
}
