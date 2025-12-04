#include "engine/engine.h"
#include "engine/window.h"

int main()
{
    auto windowConfig = common::GetWindowConfig();
    windowConfig.renderer = common::WindowConfig::RendererType::OPENGLES;
    SetWindowConfig(windowConfig);


    common::RunEngine();
}
