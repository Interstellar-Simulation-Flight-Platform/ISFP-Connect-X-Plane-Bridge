#pragma once
#include <string>
#include <mutex>
#include <fstream>

// File-based logger, four categories, four files.
// Thread-safe (mutex-guarded writes).
class Logger {
public:
    static void Init(const std::string& logs_dir, bool enabled);
    static void SetEnabled(bool enabled);
    static void Shutdown();

    static void Network(const std::string& msg);
    static void ImGui(const std::string& msg);
    static void CSL(const std::string& msg);
    static void Main(const std::string& msg);

    static void Network(const char* msg) { Network(std::string(msg)); }
    static void ImGui(const char* msg)   { ImGui(std::string(msg)); }
    static void CSL(const char* msg)     { CSL(std::string(msg)); }
    static void Main(const char* msg)    { Main(std::string(msg)); }

    static void FlushAll();

private:
    static void EnsureOpen();
    static void Write(std::ofstream& file, const std::string& msg);
    static std::ofstream s_network;
    static std::ofstream s_imgui;
    static std::ofstream s_csl;
    static std::ofstream s_main;
    static std::mutex    s_mutex;
    static bool          s_initialized;
    static bool          s_enabled;
    static std::string   s_logs_dir;
};
