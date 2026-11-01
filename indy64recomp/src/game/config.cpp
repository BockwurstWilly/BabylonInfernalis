#include "indy64_config.h"
#include "recompui/recompui.h"
#include "recompui/config.h"
#include "recompinput/recompinput.h"
#include "ultramodern/config.hpp"
#include "librecomp/files.hpp"
#include "librecomp/config.hpp"
#include "util/file.h"
#include <filesystem>

static void set_control_descriptions() {
    // Generic descriptions; the game's own control layout is not documented here yet.
    recompinput::set_game_input_description(recompinput::GameInput::Y_AXIS_POS, "Move Down / Menu Down");
    recompinput::set_game_input_description(recompinput::GameInput::Y_AXIS_NEG, "Move Up / Menu Up");
    recompinput::set_game_input_description(recompinput::GameInput::X_AXIS_NEG, "Move Left / Menu Left");
    recompinput::set_game_input_description(recompinput::GameInput::X_AXIS_POS, "Move Right / Menu Right");
    recompinput::set_game_input_description(recompinput::GameInput::START, "Pause Menu");
}

void indy64::init_config() {
    std::filesystem::path recomp_dir = recompui::file::get_app_folder_path();

    if (!recomp_dir.empty()) {
        std::filesystem::create_directories(recomp_dir);
    }

    recompui::config::GeneralTabOptions general_options{};
    general_options.has_rumble_strength = true;
    general_options.has_gyro_sensitivity = false;
    general_options.has_mouse_sensitivity = false;
    recompui::config::create_general_tab(general_options);

    recomp::config::Config& graphics = recompui::config::create_graphics_tab();
    // The renderer always uses 4:3 and the original frame rate (rt64_render_context.cpp): widescreen shows holes in the
    // game's portal culling, and RT64's frame interpolation pairs matrices by address, which this game reallocates
    // every frame. Hide the options that would have no effect.
    graphics.update_option_hidden(recompui::config::graphics::options::ar_option, true);
    graphics.update_option_hidden(recompui::config::graphics::options::hr_option, true);
    graphics.update_option_hidden(recompui::config::graphics::options::rr_option, true);
    graphics.update_option_hidden(recompui::config::graphics::options::rr_manual_value, true);

    set_control_descriptions();
    recompui::config::create_controls_tab();

    recompui::config::create_sound_tab();

    // No mods tab: there are no mods for this game and the mod APIs of the project template are not built in.

    recompui::config::finalize();
}
