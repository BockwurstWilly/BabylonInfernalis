// Play-test support: a session log per run (logs\), bug reports on F9 or the controller's capture button
// (bugreports\<time>\: screenshot, display list, RDRAM, info, logs) and watchers for frame rate, audio dropouts,
// level changes and controller input. A crash.log left by the previous run is moved into bugreports\ at startup.
#define NOMINMAX
#include <Windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "SDL.h"
#include "indy64_game.h"
#include "recompinput/input_events.h"
#include "ultramodern/ultramodern.hpp"

#pragma comment(lib, "gdiplus.lib")

namespace RT64 {
    extern std::atomic<uint32_t> indy64DisplayListCount;
    void requestDisplayListDump(const std::string &path);
}

namespace fs = std::filesystem;

namespace {
    // Level numbers as the game stores them at RDRAM 0x80C37 (GameShark "start on level" code).
    const char* const level_names[] = {
        "Canyonlands", "Babylon", "Tian Shan River", "Shambala Sanctuary", "Palawan Lagoon", "Palawan Volcano",
        "Palawan Temple", "Jeep Trek", "Teotihuacan", "Olmec Valley", "Pudovkin Ship", "Meroe Pyramids",
        "King Sol's Mines", "Nub's Tomb", "The Machine", "Aetherium", "Return to Peru",
    };
    constexpr uint32_t level_address = 0x80C37;

    std::mutex log_mutex;
    FILE* log_file = nullptr;
    fs::path log_path;
    fs::path stderr_path;
    std::deque<std::string> recent_lines;
    const auto session_start = std::chrono::steady_clock::now();

    std::atomic<uint8_t*> rdram_base{ nullptr };
    std::atomic<HWND> window_handle{ nullptr };
    std::atomic<uint32_t> audio_underruns{ 0 };
    std::atomic<bool> report_running{ false };
    std::atomic<int> last_fps{ 0 };

    fs::path program_dir() {
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        return fs::path(buf).parent_path();
    }

    std::string timestamp() {
        SYSTEMTIME t;
        GetLocalTime(&t);
        char buf[32];
        snprintf(buf, sizeof(buf), "%04u-%02u-%02u_%02u-%02u-%02u", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
        return buf;
    }

    std::string level_name(uint8_t level) {
        if (level < std::size(level_names)) {
            return level_names[level];
        }
        return "unbekannt (" + std::to_string(level) + ")";
    }

    uint8_t current_level() {
        uint8_t* rdram = rdram_base.load();
        return rdram ? rdram[level_address ^ 3] : 0xFF;
    }

    // Keeps the newest files matching a prefix, deletes the rest.
    void prune(const fs::path& dir, const std::string& prefix, size_t keep) {
        std::vector<fs::path> files;
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (entry.path().filename().string().rfind(prefix, 0) == 0) {
                files.push_back(entry.path());
            }
        }
        std::sort(files.begin(), files.end());
        for (size_t i = 0; i + keep < files.size(); i++) {
            fs::remove_all(files[i], ec);
        }
    }

    bool save_png(HBITMAP bitmap, const fs::path& path) {
        static ULONG_PTR token = 0;
        if (token == 0) {
            Gdiplus::GdiplusStartupInput input;
            Gdiplus::GdiplusStartup(&token, &input, nullptr);
        }

        UINT count = 0, size = 0;
        Gdiplus::GetImageEncodersSize(&count, &size);
        std::vector<uint8_t> buffer(size);
        auto* encoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
        Gdiplus::GetImageEncoders(count, size, encoders);
        for (UINT i = 0; i < count; i++) {
            if (wcscmp(encoders[i].MimeType, L"image/png") == 0) {
                Gdiplus::Bitmap image(bitmap, nullptr);
                return image.Save(path.wstring().c_str(), &encoders[i].Clsid, nullptr) == Gdiplus::Ok;
            }
        }
        return false;
    }

    bool save_screenshot(const fs::path& path) {
        HWND hwnd = window_handle.load();
        if (hwnd == nullptr) {
            return false;
        }
        RECT rect;
        GetClientRect(hwnd, &rect);
        const int width = rect.right - rect.left;
        const int height = rect.bottom - rect.top;
        HDC window_dc = GetDC(hwnd);
        HDC memory_dc = CreateCompatibleDC(window_dc);
        HBITMAP bitmap = CreateCompatibleBitmap(window_dc, width, height);
        HGDIOBJ old = SelectObject(memory_dc, bitmap);
        // PW_CLIENTONLY | PW_RENDERFULLCONTENT: also captures the D3D12 swap chain.
        PrintWindow(hwnd, memory_dc, 1 | 2);
        SelectObject(memory_dc, old);
        const bool ok = save_png(bitmap, path);
        DeleteObject(bitmap);
        DeleteDC(memory_dc);
        ReleaseDC(hwnd, window_dc);
        return ok;
    }

    void copy_tail(const fs::path& from, const fs::path& to, size_t max_lines) {
        std::ifstream in(from);
        if (!in) {
            return;
        }
        std::deque<std::string> lines;
        std::string line;
        while (std::getline(in, line)) {
            lines.push_back(line);
            if (lines.size() > max_lines) {
                lines.pop_front();
            }
        }
        std::ofstream out(to);
        for (const auto& l : lines) {
            out << l << "\n";
        }
    }

    void write_report() {
        const fs::path dir = program_dir() / "bugreports" / timestamp();
        std::error_code ec;
        fs::create_directories(dir, ec);
        indy64::session_log("FEHLERBERICHT gespeichert: %s", dir.string().c_str());

        RT64::requestDisplayListDump((dir / "display_list.txt").string());

        if (uint8_t* rdram = rdram_base.load()) {
            std::vector<uint8_t> bytes(0x800000);
            for (size_t i = 0; i < bytes.size(); i++) {
                bytes[i] = rdram[i ^ 3];
            }
            if (FILE* f = _wfopen((dir / "ram.bin").wstring().c_str(), L"wb")) {
                fwrite(bytes.data(), 1, bytes.size(), f);
                fclose(f);
            }
        }

        save_screenshot(dir / "screenshot.png");

        {
            std::ofstream info(dir / "info.txt");
            const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - session_start).count();
            info << "Zeit seit Programmstart: " << seconds / 60 << " min " << seconds % 60 << " s\n";
            info << "Level: " << level_name(current_level()) << "\n";
            info << "Bildrate zuletzt: " << last_fps.load() << " Bilder/s\n";
            info << "Ton-Aussetzer bisher: " << audio_underruns.load() << "\n";
            info << "Sitzungsprotokoll: " << log_path.filename().string() << "\n";
            for (int i = 0; i < SDL_NumJoysticks(); i++) {
                if (SDL_IsGameController(i)) {
                    info << "Controller " << i << ": " << (SDL_GameControllerNameForIndex(i) ? SDL_GameControllerNameForIndex(i) : "?") << "\n";
                }
            }
        }
        {
            std::ofstream note(dir / "notiz.txt");
            note << "Optional: hier kurz beschreiben, was falsch war.\n";
        }

        // Give RT64 a moment to write the next display list, then copy the logs.
        std::this_thread::sleep_for(std::chrono::seconds(1));
        {
            std::lock_guard lock(log_mutex);
            if (log_file) {
                fflush(log_file);
            }
        }
        fs::copy_file(log_path, dir / "session.log", fs::copy_options::overwrite_existing, ec);
        if (!stderr_path.empty()) {
            fflush(stderr);
            copy_tail(stderr_path, dir / "stderr_tail.log", 400);
        }

        // Feedback in the title bar, the game keeps running.
        if (HWND hwnd = window_handle.load()) {
            wchar_t title[256];
            GetWindowTextW(hwnd, title, 256);
            SetWindowTextW(hwnd, (std::wstring(title) + L"  —  Fehlerbericht gespeichert").c_str());
            std::this_thread::sleep_for(std::chrono::seconds(3));
            SetWindowTextW(hwnd, title);
        }
    }

    void trigger_report() {
        bool expected = false;
        if (!report_running.compare_exchange_strong(expected, true)) {
            return;
        }
        std::thread([]() {
            write_report();
            report_running = false;
        }).detach();
    }

    void on_sdl_event(const SDL_Event* event) {
        static std::set<std::pair<int, int>> seen_buttons;
        static std::set<std::pair<int, int>> seen_axes;
        switch (event->type) {
        case SDL_KEYDOWN:
            if ((event->key.keysym.scancode == SDL_SCANCODE_F9) && !event->key.repeat) {
                trigger_report();
            }
            break;
        case SDL_CONTROLLERDEVICEADDED: {
            const int index = event->cdevice.which;
            const char* name = SDL_GameControllerNameForIndex(index);
            char* mapping = SDL_GameControllerMappingForDeviceIndex(index);
            indy64::session_log("Controller verbunden: %s", name ? name : "?");
            indy64::session_log("  Zuordnung: %s", mapping ? mapping : "keine");
            if (mapping) {
                SDL_free(mapping);
            }
            break;
        }
        case SDL_CONTROLLERDEVICEREMOVED:
            indy64::session_log("Controller getrennt (Instanz %d)", event->cdevice.which);
            break;
        case SDL_CONTROLLERBUTTONDOWN: {
            // The capture/share button (MISC1, e.g. on the Switch Online N64 controller) also saves a bug report.
            if (event->cbutton.button == SDL_CONTROLLER_BUTTON_MISC1) {
                trigger_report();
            }
            if (seen_buttons.insert({ event->cbutton.which, event->cbutton.button }).second) {
                const char* name = SDL_GameControllerGetStringForButton(SDL_GameControllerButton(event->cbutton.button));
                indy64::session_log("Controller-Taste zum ersten Mal: %s", name ? name : "?");
            }
            break;
        }
        case SDL_CONTROLLERAXISMOTION:
            if ((event->caxis.value > 16000) || (event->caxis.value < -16000)) {
                const int sign = event->caxis.value > 0 ? 1 : -1;
                if (seen_axes.insert({ event->caxis.which, event->caxis.axis * sign }).second) {
                    const char* name = SDL_GameControllerGetStringForAxis(SDL_GameControllerAxis(event->caxis.axis));
                    indy64::session_log("Controller-Achse zum ersten Mal: %s %s", name ? name : "?", sign > 0 ? "+" : "-");
                }
            }
            break;
        }
    }

    void watcher_thread() {
        uint32_t last_lists = RT64::indy64DisplayListCount.load();
        uint32_t last_underruns = 0;
        uint32_t pending_underruns = 0;
        uint8_t last_level = 0xFF;
        std::deque<int> history;
        int seconds = 0;
        int last_fps_warning = -100;
        int last_underrun_report = -100;
        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            seconds++;
            if (!ultramodern::is_game_started()) {
                last_lists = RT64::indy64DisplayListCount.load();
                continue;
            }

            const uint32_t lists = RT64::indy64DisplayListCount.load();
            const int fps = int(lists - last_lists);
            last_lists = lists;
            last_fps = fps;

            history.push_back(fps);
            if (history.size() > 30) {
                history.pop_front();
            }
            std::vector<int> sorted(history.begin(), history.end());
            std::sort(sorted.begin(), sorted.end());
            const int median = sorted[sorted.size() / 2];
            if ((history.size() >= 10) && (median >= 10) && (fps * 10 < median * 6) && (seconds - last_fps_warning >= 10)) {
                indy64::session_log("Bildrate eingebrochen: %d Bilder/s (sonst %d)", fps, median);
                last_fps_warning = seconds;
            }
            if ((seconds % 60) == 0) {
                indy64::session_log("Bildrate: %d Bilder/s (Mittel der letzten 30 s: %d, min %d, max %d)", fps, median, sorted.front(), sorted.back());
            }

            const uint32_t underruns = audio_underruns.load();
            pending_underruns += underruns - last_underruns;
            last_underruns = underruns;
            if ((pending_underruns > 0) && (seconds - last_underrun_report >= 10)) {
                indy64::session_log("Ton-Aussetzer: %u in den letzten Sekunden", pending_underruns);
                pending_underruns = 0;
                last_underrun_report = seconds;
            }

            const uint8_t level = current_level();
            if (level != last_level) {
                indy64::session_log("Level: %s", level_name(level).c_str());
                last_level = level;
            }
        }
    }

    void move_previous_crash() {
        for (const fs::path& crash : { fs::current_path() / "crash.log", program_dir() / "crash.log" }) {
            std::error_code ec;
            if (!fs::exists(crash, ec)) {
                continue;
            }
            const fs::path dir = program_dir() / "bugreports" / (timestamp() + "_absturz");
            fs::create_directories(dir, ec);
            fs::rename(crash, dir / "crash.log", ec);
            if (ec) {
                fs::copy_file(crash, dir / "crash.log", fs::copy_options::overwrite_existing, ec);
                fs::remove(crash, ec);
            }

            // The newest earlier session log belongs to the crashed run.
            fs::path newest;
            for (const auto& entry : fs::directory_iterator(program_dir() / "logs", ec)) {
                const std::string name = entry.path().filename().string();
                if ((name.rfind("session_", 0) == 0) && (entry.path() != log_path) && (newest.empty() || entry.path() > newest)) {
                    newest = entry.path();
                }
            }
            if (!newest.empty()) {
                fs::copy_file(newest, dir / "session.log", fs::copy_options::overwrite_existing, ec);
            }
            indy64::session_log("Absturz der letzten Sitzung gefunden, gesichert in %s", dir.string().c_str());
            return;
        }
    }
}

void indy64::start_session_log() {
    const fs::path logs = program_dir() / "logs";
    std::error_code ec;
    fs::create_directories(logs, ec);
    prune(logs, "session_", 20);
    prune(logs, "stderr_", 20);

    const std::string ts = timestamp();
    log_path = logs / ("session_" + ts + ".log");
    log_file = _wfopen(log_path.wstring().c_str(), L"w");

    // Without a console or redirection (normal start from Explorer) the diagnostic output would be lost.
    const DWORD type = GetFileType(GetStdHandle(STD_ERROR_HANDLE));
    if ((type != FILE_TYPE_DISK) && (type != FILE_TYPE_PIPE) && (type != FILE_TYPE_CHAR)) {
        stderr_path = logs / ("stderr_" + ts + ".log");
        if (_wfreopen(stderr_path.wstring().c_str(), L"w", stderr) != nullptr) {
            setvbuf(stderr, nullptr, _IONBF, 0);
        }
        else {
            stderr_path.clear();
        }
    }

    session_log("Sitzung gestartet (%s), Programmordner %s", ts.c_str(), program_dir().string().c_str());
    session_log("Fehlerbericht: F9 oder Capture-Taste am Controller");
    move_previous_crash();
    recompinput::sdl_event_hook = on_sdl_event;
}

void indy64::session_log(const char* fmt, ...) {
    char message[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - session_start).count();
    char line[1100];
    snprintf(line, sizeof(line), "[%02lld:%02lld] %s", (long long)(seconds / 60), (long long)(seconds % 60), message);

    std::lock_guard lock(log_mutex);
    if (log_file) {
        fprintf(log_file, "%s\n", line);
        fflush(log_file);
    }
    recent_lines.push_back(line);
    if (recent_lines.size() > 200) {
        recent_lines.pop_front();
    }
}

void indy64::set_report_rdram(uint8_t* rdram) {
    rdram_base = rdram;
    std::thread(watcher_thread).detach();
}

void indy64::set_report_window(void* hwnd) {
    window_handle = static_cast<HWND>(hwnd);
}

void indy64::dump_ram(const char* path) {
    if (uint8_t* rdram = rdram_base.load()) {
        std::vector<uint8_t> bytes(0x800000);
        for (size_t i = 0; i < bytes.size(); i++) {
            bytes[i] = rdram[i ^ 3];
        }
        if (FILE* f = fopen(path, "wb")) {
            fwrite(bytes.data(), 1, bytes.size(), f);
            fclose(f);
        }
    }
}

void indy64::request_bug_report() {
    trigger_report();
}

void indy64::note_audio_underrun() {
    audio_underruns++;
}
