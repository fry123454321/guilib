#include "view_registry.h"

#include "gui/gui_context.h"
#include "views.h"

#include "imgui/imgui.h"

bool & viewOpen() {
    return GuiContext::instance().viewManager().currentOpen();
}

void registerViews(ViewManager & manager) {
    static bool registered = false;
    if (registered) {
        return;
    }

    auto & demo = manager.addView("imgui", "Demo", []() { ImGui::ShowDemoWindow(&viewOpen()); });
    demo.managesOwnWindow = true;

    manager.addView("net", "Incppect", viewIncppect);
    manager.addView("app", "Clipboard", viewClipboard);
    manager.addView("graphics", "Lumen", viewGfx);
    manager.addView("graphics", "TextureSync", viewTextureSync);

    registered = true;
}

void renderViews(ViewManager & manager) {
    manager.drawLauncher(viewLauncher);
    manager.drawViews();
}
