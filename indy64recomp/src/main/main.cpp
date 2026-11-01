// Babylon Infernalis - host program for the recompiled Indiana Jones and the Infernal Machine (N64).
// SDL window and audio setup follow the N64: Recompiled project template (Zelda64Recomp, via Harvest Moon 64:
// Recompiled, GPL-3.0); RecompFrontend provides menus/input/RT64, N64ModernRuntime libultra. Launcher: launcher.cpp.
#include <cstdio>
#include <cassert>
#include <vector>
#include <array>
#include <filesystem>
#include <stdexcept>
#include <cinttypes>
#include <chrono>
#include <string>
#include <thread>

#include "nfd.h"

#include "ultramodern/ultra64.h"
#include "ultramodern/ultramodern.hpp"
#include "ultramodern/config.hpp"
#define SDL_MAIN_HANDLED
#ifdef _WIN32
#include "SDL.h"
#else
#include "SDL2/SDL.h"
#include "SDL2/SDL_syswm.h"
#undef None
#undef Status
#undef LockMask
#undef ControlMask
#undef Success
#undef Always
#endif

#include "recompui/recompui.h"
#include "recompui/program_config.h"
#include "recompui/renderer.h"
#include "recompui/config.h"
#include "util/file.h"
#include "recompinput/input_events.h"
#include "recompinput/recompinput.h"
#include "recompinput/profiles.h"
#include "indy64_config.h"
#include "indy64_game.h"
#include "librecomp/game.hpp"
#include "librecomp/mods.hpp"
#include "librecomp/helpers.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <timeapi.h>
#include "SDL_syswm.h"
#endif

const std::string version_string = "0.1.0";

template<typename... Ts>
void exit_error(const char* str, Ts ...args) {
    ((void)fprintf(stderr, str, args), ...);
    assert(false);
    ultramodern::error_handling::quick_exit(__FILE__, __LINE__, __FUNCTION__);
}

ultramodern::gfx_callbacks_t::gfx_data_t create_gfx() {
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC) > 0) {
        exit_error("Failed to initialize SDL2: %s\n", SDL_GetError());
    }

    fprintf(stdout, "SDL Video Driver: %s\n", SDL_GetCurrentVideoDriver());
    return {};
}

ultramodern::input::connected_device_info_t get_connected_device_info(int controller_num) {
    if (recompinput::players::is_single_player_mode() || recompinput::players::get_player_is_assigned(controller_num)) {
        return ultramodern::input::connected_device_info_t{
            .connected_device = ultramodern::input::Device::Controller,
            .connected_pak = ultramodern::input::Pak::RumblePak,
        };
    }

    return ultramodern::input::connected_device_info_t{
        .connected_device = ultramodern::input::Device::None,
        .connected_pak = ultramodern::input::Pak::None,
    };
}

SDL_Window* window;

ultramodern::renderer::WindowHandle create_window(ultramodern::gfx_callbacks_t::gfx_data_t) {
    uint32_t flags = SDL_WINDOW_RESIZABLE;

#if defined(__APPLE__)
    flags |= SDL_WINDOW_METAL;
#elif defined(RT64_SDL_WINDOW_VULKAN)
    flags |= SDL_WINDOW_VULKAN;
#endif

    // Test aid: INDY64_BACKGROUND opens the window outside the visible desktop and without taking the focus.
    int window_x = SDL_WINDOWPOS_CENTERED;
    int window_y = SDL_WINDOWPOS_CENTERED;
    if (getenv("INDY64_BACKGROUND") != nullptr) {
        SDL_SetHint("SDL_WINDOW_NO_ACTIVATION_WHEN_SHOWN", "1");
        window_x = -4000;
        window_y = -4000;
    }

    window = SDL_CreateWindow(" ", window_x, window_y, 1600, 900, flags);
    if (window == nullptr) {
        exit_error("Failed to create window: %s\n", SDL_GetError());
    }

    SDL_SetWindowTitle(window, indy64::program_name.c_str());

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);

#if defined(_WIN32)
    indy64::set_report_window(wmInfo.info.win.window);
    return ultramodern::renderer::WindowHandle{ wmInfo.info.win.window, GetCurrentThreadId() };
#elif defined(__linux__) || defined(__ANDROID__)
    return ultramodern::renderer::WindowHandle{ window };
#elif defined(__APPLE__)
    SDL_MetalView view = SDL_Metal_CreateView(window);
    return ultramodern::renderer::WindowHandle{ wmInfo.info.cocoa.window, SDL_Metal_GetLayer(view) };
#else
    static_assert(false && "Unimplemented");
#endif
}

void update_gfx(void*) {
    recompinput::handle_events();
}

// Audio output, as in the N64: Recompiled project template.

static SDL_AudioCVT audio_convert;
static SDL_AudioDeviceID audio_device = 0;

static uint32_t sample_rate = 48000;
static uint32_t output_sample_rate = 48000;
constexpr uint32_t input_channels = 2;
static uint32_t output_channels = 2;

// A frame is one sample per channel.
constexpr uint32_t duplicated_input_frames = 4;
static uint32_t discarded_output_frames;

constexpr uint32_t bytes_per_frame = input_channels * sizeof(float);

void queue_samples(int16_t* audio_data, size_t sample_count) {
    static std::vector<float> swap_buffer;
    static std::array<float, duplicated_input_frames * input_channels> duplicated_sample_buffer;

    size_t resampled_sample_count = sample_count + duplicated_input_frames * input_channels;
    size_t max_sample_count = std::max(resampled_sample_count, resampled_sample_count * audio_convert.len_mult);
    if (max_sample_count > swap_buffer.size()) {
        swap_buffer.resize(max_sample_count);
    }

    for (size_t i = 0; i < duplicated_input_frames * input_channels; i++) {
        swap_buffer[i] = duplicated_sample_buffer[i];
    }

    // Bring-up aid: INDY64_AUDIO_RAW=<file> records the game's output (16-bit stereo, left first) before the volume
    // and resampling; the rate goes to stderr as "[audio] rate".
    static FILE* raw_file = getenv("INDY64_AUDIO_RAW") ? fopen(getenv("INDY64_AUDIO_RAW"), "wb") : nullptr;
    if (raw_file != nullptr) {
        // Once a second: frames delivered by the game, its rate, and how much SDL still has queued.
        static auto second_start = std::chrono::steady_clock::now();
        static uint64_t second_frames = 0;
        static uint32_t second_buffers = 0;
        second_frames += sample_count / input_channels;
        second_buffers++;
        const auto now = std::chrono::steady_clock::now();
        if (now - second_start >= std::chrono::seconds(1)) {
            const uint64_t queued_ms = uint64_t(SDL_GetQueuedAudioSize(audio_device)) / (output_channels * sizeof(float)) * 1000 / output_sample_rate;
            fprintf(stderr, "[audio] %llu frames in %u buffers, rate %u, queued %llu ms\n", (unsigned long long)second_frames,
                second_buffers, sample_rate, (unsigned long long)queued_ms);
            second_start = now;
            second_frames = 0;
            second_buffers = 0;
        }
        for (size_t i = 0; i < sample_count; i += input_channels) {
            const int16_t frame[2] = { audio_data[i + 1], audio_data[i + 0] };
            fwrite(frame, sizeof(frame), 1, raw_file);
        }
        fflush(raw_file);
    }

    // Convert to float and swap the channels back (the samples are word-swapped like all of rdram).
    float cur_main_volume = static_cast<float>(recompui::config::sound::get_main_volume()) / 100.0f;
    for (size_t i = 0; i < sample_count; i += input_channels) {
        swap_buffer[i + 0 + duplicated_input_frames * input_channels] = audio_data[i + 1] * (0.5f / 32768.0f) * cur_main_volume;
        swap_buffer[i + 1 + duplicated_input_frames * input_channels] = audio_data[i + 0] * (0.5f / 32768.0f) * cur_main_volume;
    }

    assert(sample_count > duplicated_input_frames * input_channels);

    for (size_t i = 0; i < duplicated_input_frames * input_channels; i++) {
        duplicated_sample_buffer[i] = swap_buffer[i + sample_count];
    }

    audio_convert.buf = reinterpret_cast<Uint8*>(swap_buffer.data());
    audio_convert.len = (sample_count + duplicated_input_frames * input_channels) * sizeof(swap_buffer[0]);

    if (SDL_ConvertAudio(&audio_convert) < 0) {
        printf("Error using SDL audio converter: %s\n", SDL_GetError());
        throw std::runtime_error("Error using SDL audio converter");
    }

    // The queue holds converted audio (output rate and channels); measuring it with the game's rate overstated it by
    // output_sample_rate / sample_rate and made the skipping below drop every other frame at 22050 Hz.
    // Bring-up aid (INDY64_STALL_TRACE): a loud buffer equal to the one 8 buffers earlier is the MusyX ring replaying
    // without new content; record which game threads run at that moment.
    if (getenv("INDY64_STALL_TRACE") != nullptr) {
        static std::array<uint64_t, 8> hashes{};
        static size_t ring_pos = 0;
        uint64_t hash = 1469598103934665603ULL;
        int peak = 0;
        for (size_t i = 0; i < sample_count; i++) {
            hash = (hash ^ uint16_t(audio_data[i])) * 1099511628211ULL;
            peak = std::max(peak, std::abs(int(audio_data[i])));
        }
        if ((peak >= 64) && (hashes[ring_pos] == hash)) {
            indy64::trace_threads_now("audio buffer repeats the one 8 before");
        }
        hashes[ring_pos] = hash;
        ring_pos = (ring_pos + 1) % hashes.size();
    }

    // Watcher for the session log: the queue running dry once audio has been flowing for a while is a dropout.
    static uint32_t buffers_queued = 0;
    if ((buffers_queued++ > 600) && (SDL_GetQueuedAudioSize(audio_device) == 0)) {
        indy64::note_audio_underrun();
    }

    uint64_t cur_queued_microseconds =uint64_t(SDL_GetQueuedAudioSize(audio_device)) / (output_channels * sizeof(float)) * 1000000 / output_sample_rate;
    uint32_t num_bytes_to_queue = audio_convert.len_cvt - output_channels * discarded_output_frames * sizeof(swap_buffer[0]);
    float* samples_to_queue = swap_buffer.data() + output_channels * discarded_output_frames / 2;

    // Skip samples when too much audio is queued already, so latency does not build up.
    uint32_t skip_factor = cur_queued_microseconds / 100000;
    if (skip_factor != 0) {
        uint32_t skip_ratio = 1 << skip_factor;
        num_bytes_to_queue /= skip_ratio;
        for (size_t i = 0; i < num_bytes_to_queue / (output_channels * sizeof(swap_buffer[0])); i++) {
            samples_to_queue[2 * i + 0] = samples_to_queue[2 * skip_ratio * i + 0];
            samples_to_queue[2 * i + 1] = samples_to_queue[2 * skip_ratio * i + 1];
        }
    }

    SDL_QueueAudio(audio_device, samples_to_queue, num_bytes_to_queue);
}

size_t get_frames_remaining() {
    constexpr float buffer_offset_frames = 1.0f;
    uint64_t buffered_byte_count = SDL_GetQueuedAudioSize(audio_device);
    buffered_byte_count = buffered_byte_count * 2 * sample_rate / output_sample_rate / output_channels;

    uint32_t frames_per_vi = (sample_rate / 60);
    if (buffered_byte_count > (buffer_offset_frames * bytes_per_frame * frames_per_vi)) {
        buffered_byte_count -= (buffer_offset_frames * bytes_per_frame * frames_per_vi);
    }
    else {
        buffered_byte_count = 0;
    }

    return static_cast<uint32_t>(buffered_byte_count / bytes_per_frame);
}

void update_audio_converter() {
    if (SDL_BuildAudioCVT(&audio_convert, AUDIO_F32, input_channels, sample_rate, AUDIO_F32, output_channels, output_sample_rate) < 0) {
        printf("Error creating SDL audio converter: %s\n", SDL_GetError());
        throw std::runtime_error("Error creating SDL audio converter");
    }

    discarded_output_frames = duplicated_input_frames * output_sample_rate / sample_rate;
}

void set_frequency(uint32_t freq) {
    fprintf(stderr, "[audio] rate %u\n", freq);
    sample_rate = freq;
    update_audio_converter();
}

bool reset_audio(uint32_t output_freq) {
    SDL_AudioSpec spec_desired{
        .freq = (int)output_freq,
        .format = AUDIO_F32,
        .channels = (Uint8)output_channels,
        .silence = 0,
        .samples = 0x100,
        .padding = 0,
        .size = 0,
        .callback = nullptr,
        .userdata = nullptr
    };

    audio_device = SDL_OpenAudioDevice(nullptr, false, &spec_desired, nullptr, 0);
    if (audio_device == 0) {
        std::string audio_error = std::string("No audio device could be found. Please make sure an audio device is available.\nError opening audio device: ") + std::string(SDL_GetError());
        recompui::message_box(audio_error.c_str());
        return false;
    }

    SDL_PauseAudioDevice(audio_device, 0);
    output_sample_rate = output_freq;
    update_audio_converter();
    return true;
}

// Factor 5 audio microcode, recompiled with RSPRecomp (recomp/rsp/f5_audio.rsp.toml). Graphics tasks go to RT64.
extern RspUcodeFunc f5_audio_ucode;

// INDY64_AUDIO_HLE=1: run the audio tasks through mupen64plus-rsp-hle's MusyX v2 code (src/audio_hle) instead.
extern "C" void indy64_musyx_hle(unsigned char* rdram, unsigned char* dmem);

static RspExitReason f5_audio_hle(uint8_t* rdram, uint32_t ucode_addr) {
    (void)ucode_addr;
    indy64_musyx_hle(rdram, dmem);
    return RspExitReason::Broke;
}

RspUcodeFunc* get_rsp_microcode(const OSTask* task) {
    switch (task->t.type) {
    case M_AUDTASK: {
        indy64::note_audio_task();
        // The MusyX task refills data_size buffers of the game's ring (see ultramodern's AI thread).
        ultramodern::notify_audio_refill(task->t.data_size);
        // Bring-up aid: audio tasks and buffers requested (data_size) per second, next to queue_samples' counts.
        static auto second_start = std::chrono::steady_clock::now();
        static uint32_t tasks = 0, requested = 0;
        tasks++;
        requested += task->t.data_size;
        if (getenv("INDY64_AUDIO_RAW") && (std::chrono::steady_clock::now() - second_start >= std::chrono::seconds(1))) {
            fprintf(stderr, "[audio] %u tasks, %u buffers requested\n", tasks, requested);
            second_start = std::chrono::steady_clock::now();
            tasks = requested = 0;
        }
        static uint32_t logged = 0;
        if (logged++ < 6) {
            fprintf(stderr, "[audio] task ucode %08X data %08X/%X dl %08X/%X out %08X flags %X\n",
                (uint32_t)task->t.ucode, (uint32_t)task->t.ucode_data, task->t.ucode_data_size, (uint32_t)task->t.data_ptr,
                task->t.data_size, (uint32_t)task->t.output_buff, task->t.flags);
        }
        static const bool use_hle = getenv("INDY64_AUDIO_HLE") != nullptr;
        return use_hle ? f5_audio_hle : f5_audio_ucode;
    }

    default:
        fprintf(stderr, "Unknown task: %" PRIu32 "\n", task->t.type);
        return nullptr;
    }
}

extern "C" void recomp_entrypoint(uint8_t* rdram, recomp_context* ctx);
gpr get_entrypoint_address();

std::vector<recomp::GameEntry> supported_games = {
    {
        // XXH3-64 of the big-endian (z64) USA ROM, SHA-1 93300D63412D6C7432109B5AD2E4A8B9348E9538.
        .rom_hash = 0xAA0401123B477996ULL,
        .internal_name = "Indiana Jones",
        .display_name = "Indiana Jones and the Infernal Machine",
        .game_id = u8"indiana_jones_infernal_machine",
        .mod_game_id = "indy64",
        // The game probes the EEPROM; the exact size is not verified yet.
        .save_type = recomp::SaveType::AllowAll,
        .is_enabled = true,
        .decompression_routine = nullptr,
        .has_compressed_code = false,
        .entrypoint_address = get_entrypoint_address(),
        .entrypoint = recomp_entrypoint,
        .on_init_callback = indy64::game_on_init,
    },
};

namespace indy64 {
    // Thread ids and priorities from idle_thread/boot_main (see NOTES.md).
    std::string get_game_thread_name(const OSThread* t) {
        std::string name = "[Game] ";
        if (t->id == 0 && t->priority == 0) {
            name += "IDLE";
        }
        else if (t->priority == 9) {
            name += "MAIN";
        }
        else if (t->priority == 127) {
            name += "VI_INIT";
        }
        else {
            name += std::to_string(t->id) + "/" + std::to_string(t->priority);
        }

        return name;
    }
}

void enable_texture_pack(recomp::mods::ModContext& context, const recomp::mods::ModHandle& mod) {
    recompui::renderer::enable_texture_pack(context, mod);
}

void disable_texture_pack(recomp::mods::ModContext&, const recomp::mods::ModHandle& mod) {
    recompui::renderer::disable_texture_pack(mod);
}

void reorder_texture_pack(recomp::mods::ModContext&) {
    recompui::renderer::trigger_texture_pack_update();
}

void on_launcher_init(recompui::LauncherMenu* menu) {
    auto game_options_menu = menu->init_game_options_menu(
        supported_games[0].game_id,
        supported_games[0].mod_game_id,
        supported_games[0].display_name,
        supported_games[0].thumbnail_bytes,
        recompui::GameOptionsMenuLayout::Center
    );
    // The default options without "Mods" (see init_config).
    game_options_menu->add_start_game_or_load_rom_option();
    game_options_menu->add_setup_controls_option();
    game_options_menu->add_settings_option();
    game_options_menu->add_exit_option();
    indy64::setup_launcher(menu);

    // Test aid: INDY64_AUTOSTART starts the game without clicking "Start Game" (no focus or mouse needed).
    if (getenv("INDY64_AUTOSTART") != nullptr) {
        std::thread([]() {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            recomp::start_game(supported_games[0].game_id, {});
            recompui::hide_all_contexts();
        }).detach();
    }
}

// Hook at the end of the audio frame (FUN_40033e28, see the recomp toml). The frame queues ROM loads for the dialogue
// streams (FUN_40035c70 -> DMA list at DAT_4005ba8c) but only hands the list to the loader thread (FUN_40031548) when a
// buffer was refilled. The next frame drops the list and still reports the loads as done, so the "mort" table and the
// dialogue chunks stayed zero. On the N64 every frame refills a buffer; here an AI event can come without one. If the
// list was not issued, issue it now, as FUN_40031548 would.
extern "C" void FUN_40005274(uint8_t* rdram, recomp_context* ctx);

extern "C" void indy64_audio_dma_fix(uint8_t* rdram, recomp_context* ctx) {
    const uint32_t count = MEM_HU(0, S32(0x4005BA66));
    if ((count == 0) || (MEM_BU(0, S32(0x40065B2C)) != 0)) {
        return;
    }
    static uint32_t logged = 0;
    if (logged++ < 5) {
        indy64::session_log("Audio: %u Ladeauftraege ohne Puffer-Auffuellung nachgereicht", count);
    }
    const recomp_context saved = *ctx;
    MEM_B(0, S32(0x40065B2C)) = 1;
    ctx->r4 = S32(MEM_W(0, S32(0x4005BA8C)));
    ctx->r5 = count;
    ctx->r6 = S32(0x40065B10);
    FUN_40005274(rdram, ctx);
    *ctx = saved;
}

// Diagnostics hook in the speech loader thread (FUN_410098a8, see the recomp toml): request, loaded file, MusyX handle.
extern "C" void indy64_voice_log(uint8_t* rdram, recomp_context* ctx, int stage) {
    static const char* const stages[] = { "request", "file", "handle" };
    const uint32_t value = (stage == 0) ? uint32_t(ctx->r16) : uint32_t(ctx->r2);
    indy64::session_log("Sprache: %s %s%u / %08X%s", stages[stage], (stage == 0) ? "s_" : "", (stage == 0) ? value : uint32_t(ctx->r16), value,
        (stage == 0) ? (" Lautstaerke " + std::to_string(uint32_t(ctx->r19) & 0xFF)).c_str() : "");

    // INDY64_VOICE_DUMP: RAM dumps 0.25 s and 2 s after the first speech was started (compare with the emulator).
    static bool dumped = false;
    if ((stage == 2) && !dumped && getenv("INDY64_VOICE_DUMP")) {
        dumped = true;
        std::thread([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            indy64::dump_ram("ram_voice_a.bin");
            std::this_thread::sleep_for(std::chrono::milliseconds(1750));
            indy64::dump_ram("ram_voice_b.bin");
        }).detach();
    }
}

// Test aid: INDY64_AUTOINPUT="<s>:<button>[/<ms>],..." presses N64 buttons inside the program, timed from the first
// input poll, so tests need no window focus. Buttons: A B Z START L R CU CD CL CR DU DD DL DR, stick UP DOWN LEFT RIGHT,
// REPORT saves a bug report like F9, QUIT ends the program like the Quit button of the close prompt.
static bool scripted_get_input(int controller_num, uint16_t* buttons, float* x, float* y) {
    const bool result = recompinput::profiles::get_n64_input(controller_num, buttons, x, y);
    {
        // Bring-up aid: report the real input once per second while anything is non-zero.
        static auto last_report = std::chrono::steady_clock::now();
        const auto now_tp = std::chrono::steady_clock::now();
        if ((controller_num == 0) && ((*buttons != 0) || (*x != 0.0f) || (*y != 0.0f)) && (now_tp - last_report > std::chrono::seconds(1))) {
            last_report = now_tp;
            fprintf(stderr, "[input] buttons %04X stick %.3f %.3f\n", *buttons, *x, *y);
        }
    }
    static const char* script = getenv("INDY64_AUTOINPUT");
    if ((script == nullptr) || (controller_num != 0)) {
        return result;
    }

    struct Press { double start; double end; std::string button; };
    static std::vector<Press> presses = []() {
        std::vector<Press> list;
        std::string text = script;
        size_t pos = 0;
        while (pos < text.size()) {
            size_t comma = text.find(',', pos);
            std::string item = text.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
            pos = (comma == std::string::npos) ? text.size() : comma + 1;
            const size_t colon = item.find(':');
            if (colon == std::string::npos) {
                continue;
            }
            const double start = atof(item.substr(0, colon).c_str());
            std::string button = item.substr(colon + 1);
            double hold_ms = 150.0;
            const size_t slash = button.find('/');
            if (slash != std::string::npos) {
                hold_ms = atof(button.substr(slash + 1).c_str());
                button = button.substr(0, slash);
            }
            list.push_back({ start, start + hold_ms / 1000.0, button });
        }
        return list;
    }();
    static const auto first_poll = std::chrono::steady_clock::now();
    const double now = std::chrono::duration<double>(std::chrono::steady_clock::now() - first_poll).count();

    static const std::pair<const char*, uint16_t> button_bits[] = {
        { "A", 0x8000 }, { "B", 0x4000 }, { "Z", 0x2000 }, { "START", 0x1000 }, { "DU", 0x0800 }, { "DD", 0x0400 },
        { "DL", 0x0200 }, { "DR", 0x0100 }, { "L", 0x0020 }, { "R", 0x0010 }, { "CU", 0x0008 }, { "CD", 0x0004 },
        { "CL", 0x0002 }, { "CR", 0x0001 },
    };
    static size_t reported = 0;
    for (size_t i = 0; i < presses.size(); i++) {
        const Press& press = presses[i];
        if ((now < press.start) || (now >= press.end)) {
            continue;
        }
        if (i >= reported) {
            reported = i + 1;
            fprintf(stderr, "[autoinput] %.2f s: %s\n", now, press.button.c_str());
            if (press.button == "REPORT") {
                indy64::request_bug_report();
            }
            else if (press.button == "QUIT") {
                ultramodern::quit();
            }
        }
        for (const auto& [name, bit] : button_bits) {
            if (press.button == name) {
                *buttons |= bit;
            }
        }
        if (press.button == "UP") *y = 1.0f;
        if (press.button == "DOWN") *y = -1.0f;
        if (press.button == "LEFT") *x = -1.0f;
        if (press.button == "RIGHT") *x = 1.0f;
    }
    return true;
}

int main(int argc, char** argv) {
#ifdef _WIN32
    // The program reads assets\ and recompcontrollerdb.txt from the working directory. Started by double-click from
    // build\ that is the wrong folder (the window closed at once), so look next to the exe and one level up.
    {
        std::error_code ec;
        if (!std::filesystem::exists("assets", ec)) {
            wchar_t exe_path[MAX_PATH];
            GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
            const std::filesystem::path exe_dir = std::filesystem::path(exe_path).parent_path();
            for (const auto& dir : { exe_dir, exe_dir.parent_path() }) {
                if (std::filesystem::exists(dir / "assets", ec)) {
                    std::filesystem::current_path(dir, ec);
                    break;
                }
            }
        }
    }
#endif

    // Unbuffered, so redirected logs survive a crash or a forced exit.
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    indy64::start_session_log();
    indy64::install_crash_log();

    recomp::Version project_version{};
    if (!recomp::Version::from_string(version_string, project_version)) {
        ultramodern::error_handling::message_box(("Invalid version string: " + version_string).c_str());
        return EXIT_FAILURE;
    }

#ifdef _WIN32
    timeBeginPeriod(1);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--show-console") == 0) {
            if (GetConsoleWindow() == nullptr) {
                AllocConsole();
                freopen("CONIN$", "r", stdin);
                freopen("CONOUT$", "w", stderr);
                freopen("CONOUT$", "w", stdout);
            }
            break;
        }
    }

    SetConsoleOutputCP(CP_UTF8);

    // WASAPI, as directsound has issues with sample queueing.
    SDL_setenv("SDL_AUDIODRIVER", "wasapi", true);
#else
    (void)argc;
    (void)argv;
#endif

    NFD_Init();

    recompui::programconfig::set_program_name(indy64::program_name);
    recompui::programconfig::set_program_id(indy64::program_id);

    SDL_InitSubSystem(SDL_INIT_AUDIO);
    if (!reset_audio(48000)) {
        return EXIT_FAILURE;
    }

    std::u8string controller_db_path = (recompui::file::get_program_path() / "recompcontrollerdb.txt").u8string();
    if (SDL_GameControllerAddMappingsFromFile(reinterpret_cast<const char*>(controller_db_path.c_str())) < 0) {
        fprintf(stderr, "Failed to load controller mappings: %s\n", SDL_GetError());
    }

    // Nintendo Switch Online N64 controller (Bluetooth, SDL's HIDAPI driver). SDL reports its buttons in Switch terms:
    // A b0, B b1, C-Left b2 (x), C-Down a5 (righttrigger), C-Up b3 (y), C-Right b4 (back), Home b5, Start b6,
    // ZR b7 (leftstick), L b9, R b10, Z a4 (lefttrigger), D-pad b11-b14, Capture b15 (found by pressing them in order,
    // see the session log). Remapped so the frontend's default N64 bindings fit: B on the west button, R on the right
    // trigger, the C buttons on the right stick (like the other N64 controllers in recompcontrollerdb.txt), Home opens
    // the menu (back), Capture stays misc1 (bug report).
    SDL_GameControllerAddMapping(
        "0300b7e67e050000192000000000680c,Nintendo N64 Controller,"
        "a:b0,x:b1,back:b5,start:b6,leftshoulder:b9,righttrigger:b10,lefttrigger:a4,"
        "-righty:b3,+righty:+a5,-rightx:b2,+rightx:b4,"
        "dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,leftx:a0,lefty:a1,misc1:b15,crc:e6b7,platform:Windows,");

    // Font from the template's assets folder; the family name must match the one inside the file.
    recompui::register_primary_font("InterVariable.ttf", "Inter Variable");

    recomp::register_config_path(recompui::file::get_app_folder_path());

    for (const auto& game : supported_games) {
        recomp::register_game(game);
    }

    recompui::register_ui_exports();

    indy64::register_overlays();

    recompinput::players::set_single_player_mode(true);

    // Keyboard layout for Indy: WASD to move, Space = A (jump), E/F = B (use, fire), left Shift = Z (target lock,
    // crouch), arrow keys = C buttons (quick slots, C-Up look around), R = R (switch target), Enter = Start.
    // Must happen before init_config, which reads the defaults.
    recompinput::set_default_mapping_for_keyboard(recompinput::GameInput::B,
        { recompinput::InputField::keyboard(SDL_SCANCODE_E), recompinput::InputField::keyboard(SDL_SCANCODE_F) });
    recompinput::set_default_mapping_for_keyboard(recompinput::GameInput::Z, { recompinput::InputField::keyboard(SDL_SCANCODE_LSHIFT) });
    recompinput::set_default_mapping_for_keyboard(recompinput::GameInput::L, { recompinput::InputField::keyboard(SDL_SCANCODE_Q) });

    indy64::init_config();

    recompui::register_launcher_init_callback(on_launcher_init);

    recomp::rsp::callbacks_t rsp_callbacks{
        .get_rsp_microcode = get_rsp_microcode,
    };

    ultramodern::renderer::callbacks_t renderer_callbacks{
        .create_render_context = [](uint8_t* rdram, ultramodern::renderer::WindowHandle window_handle, bool developer_mode) {
            auto presentation_mode = ultramodern::renderer::PresentationMode::PresentEarly;
            return recompui::renderer::create_render_context(rdram, window_handle, presentation_mode, developer_mode);
        },
    };

    ultramodern::gfx_callbacks_t gfx_callbacks{
        .create_gfx = create_gfx,
        .create_window = create_window,
        .update_gfx = update_gfx,
    };

    ultramodern::audio_callbacks_t audio_callbacks{
        .queue_samples = queue_samples,
        .get_frames_remaining = get_frames_remaining,
        .set_frequency = set_frequency,
    };

    ultramodern::input::callbacks_t input_callbacks{
        .poll_input = recompinput::poll_inputs,
        .get_input = scripted_get_input,
        .set_rumble = recompinput::set_rumble,
        .get_connected_device_info = get_connected_device_info,
    };

    ultramodern::events::callbacks_t thread_callbacks{
        .vi_callback = recompinput::update_rumble,
        .gfx_init_callback = nullptr,
    };

    ultramodern::error_handling::callbacks_t error_handling_callbacks{
        .message_box = recompui::message_box,
    };

    ultramodern::threads::callbacks_t threads_callbacks{
        .get_game_thread_name = indy64::get_game_thread_name,
    };

    recomp::mods::ModContentType texture_pack_content_type{
        .content_filename = "rt64.json",
        .allow_runtime_toggle = true,
        .on_enabled = enable_texture_pack,
        .on_disabled = disable_texture_pack,
        .on_reordered = reorder_texture_pack,
    };
    auto texture_pack_content_type_id = recomp::mods::register_mod_content_type(texture_pack_content_type);
    recomp::mods::register_mod_container_type("rtz", std::vector{ texture_pack_content_type_id }, false);

    recomp::start(
        project_version,
        {},
        rsp_callbacks,
        renderer_callbacks,
        audio_callbacks,
        input_callbacks,
        gfx_callbacks,
        thread_callbacks,
        error_handling_callbacks,
        threads_callbacks
    );

    NFD_Quit();

#ifdef _WIN32
    timeEndPeriod(1);

    // Game threads that are never joined (blocked on message queues) and the watcher threads still run; tearing down
    // the C++ runtime under them crashed on exit. Saving is done at this point, so end the process right away.
    indy64::session_log("Programm beendet");
    fflush(nullptr);
    TerminateProcess(GetCurrentProcess(), EXIT_SUCCESS);
#endif

    return EXIT_SUCCESS;
}
