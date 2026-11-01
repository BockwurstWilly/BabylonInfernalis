// Babylon Infernalis launcher: the RecompFrontend launcher menu with our own background (assets/Babylon.svg, made by
// tools/make_branding.py). The pixel title is part of the picture, so the default title label is removed.
#include "recompui/recompui.h"
#include "base/ui_launcher.h"

#include "indy64_game.h"

namespace indy64 {
    void setup_launcher(recompui::LauncherMenu* menu) {
        menu->remove_default_title();

        // Top sky colour, visible only while the picture loads.
        menu->get_background_container()->set_background_color({ 0x14, 0x0C, 0x26, 0xFF });

        // Fill the whole window; the SVG keeps its proportions and crops the edges (preserveAspectRatio slice).
        recompui::Svg* background = menu->set_launcher_background_svg("Babylon.svg");
        background->set_left(0.0f);
        background->set_top(0.0f);
        background->set_width(100.0f, recompui::Unit::Percent);
        background->set_height(100.0f, recompui::Unit::Percent);
        background->set_translate_2D(0.0f, 0.0f);
    }
}
