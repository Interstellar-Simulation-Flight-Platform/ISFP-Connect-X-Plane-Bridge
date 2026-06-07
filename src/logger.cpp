#include "logger.h"
#include <ctime>
#include <sstream>
#include <iomanip>
#include <sys/stat.h>
#include <direct.h>

std::ofstream Logger::s_network;
std::ofstream Logger::s_imgui;
std::ofstream Logger::s_csl;
std::ofstream Logger::s_main;
std::mutex    Logger::s_mutex;
bool          Logger::s_initialized = false;
bool          Logger::s_enabled = false;
std::string   Logger::s_logs_dir;

static std::string Timestamp() {
    time_t now = time(nullptr);
    struct tm t;
    localtime_s(&t, &now);
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    return buf;
}

void Logger::Init(const std::string& logs_dir, bool enabled) {
    if (s_initialized) return;
    s_logs_dir = logs_dir;
    s_enabled = enabled;

    if (!enabled) {
        // Delete all log files then remove directory
        remove((s_logs_dir + "/Network.log").c_str());
        remove((s_logs_dir + "/ImGui.log").c_str());
        remove((s_logs_dir + "/CSL.log").c_str());
        remove((s_logs_dir + "/Main.log").c_str());
        _rmdir(s_logs_dir.c_str());
        s_initialized = false;
        return;
    }

    // Enabled: create directory and open files (truncate old content)
    _mkdir(s_logs_dir.c_str());
    s_network.open(s_logs_dir + "/Network.log", std::ios::trunc);
    s_imgui.open(s_logs_dir + "/ImGui.log", std::ios::trunc);
    s_csl.open(s_logs_dir + "/CSL.log", std::ios::trunc);
    s_main.open(s_logs_dir + "/Main.log", std::ios::trunc);
    s_initialized = true;
}

void Logger::Shutdown() {
    FlushAll();
    s_network.close();
    s_imgui.close();
    s_csl.close();
    s_main.close();
    s_initialized = false;
    s_enabled = false;
}

void Logger::SetEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (enabled == s_enabled) return;
    s_enabled = enabled;

    if (!enabled) {
        // Close files, delete them, remove directory
        s_network.close();
        s_imgui.close();
        s_csl.close();
        s_main.close();
        remove((s_logs_dir + "/Network.log").c_str());
        remove((s_logs_dir + "/ImGui.log").c_str());
        remove((s_logs_dir + "/CSL.log").c_str());
        remove((s_logs_dir + "/Main.log").c_str());
        _rmdir(s_logs_dir.c_str());
        s_initialized = false;
    }
    // When re-enabling, files will be created lazily on next Write()
}

void Logger::Write(std::ofstream& file, const std::string& msg) {
    if (!file.is_open()) return;
    std::string ts = Timestamp();
    std::string clean = msg;
    while (!clean.empty() && (clean.back() == '\n' || clean.back() == '\r'))
        clean.pop_back();
    file << "[" << ts << "] " << clean << std::endl;
}

void Logger::Network(const std::string& msg) {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_initialized && s_enabled) {
        // Lazy init: create directory and open files on first write
        _mkdir(s_logs_dir.c_str());
        s_network.open(s_logs_dir + "/Network.log", std::ios::trunc);
        s_imgui.open(s_logs_dir + "/ImGui.log", std::ios::trunc);
        s_csl.open(s_logs_dir + "/CSL.log", std::ios::trunc);
        s_main.open(s_logs_dir + "/Main.log", std::ios::trunc);
        s_initialized = true;
    }
    Write(s_network, msg);
}

void Logger::ImGui(const std::string& msg) {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_initialized && s_enabled) {
        _mkdir(s_logs_dir.c_str());
        s_network.open(s_logs_dir + "/Network.log", std::ios::trunc);
        s_imgui.open(s_logs_dir + "/ImGui.log", std::ios::trunc);
        s_csl.open(s_logs_dir + "/CSL.log", std::ios::trunc);
        s_main.open(s_logs_dir + "/Main.log", std::ios::trunc);
        s_initialized = true;
    }
    Write(s_imgui, msg);
}

void Logger::CSL(const std::string& msg) {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_initialized && s_enabled) {
        _mkdir(s_logs_dir.c_str());
        s_network.open(s_logs_dir + "/Network.log", std::ios::trunc);
        s_imgui.open(s_logs_dir + "/ImGui.log", std::ios::trunc);
        s_csl.open(s_logs_dir + "/CSL.log", std::ios::trunc);
        s_main.open(s_logs_dir + "/Main.log", std::ios::trunc);
        s_initialized = true;
    }
    Write(s_csl, msg);
}

void Logger::Main(const std::string& msg) {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_initialized && s_enabled) {
        _mkdir(s_logs_dir.c_str());
        s_network.open(s_logs_dir + "/Network.log", std::ios::trunc);
        s_imgui.open(s_logs_dir + "/ImGui.log", std::ios::trunc);
        s_csl.open(s_logs_dir + "/CSL.log", std::ios::trunc);
        s_main.open(s_logs_dir + "/Main.log", std::ios::trunc);
        s_initialized = true;
    }
    Write(s_main, msg);
}

void Logger::FlushAll() {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_network.is_open()) s_network.flush();
    if (s_imgui.is_open())   s_imgui.flush();
    if (s_csl.is_open())     s_csl.flush();
    if (s_main.is_open())    s_main.flush();
}
