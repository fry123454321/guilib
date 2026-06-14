#pragma once

class ViewManager;

bool & viewOpen();
void registerViews(ViewManager & manager);
void renderViews(ViewManager & manager);
