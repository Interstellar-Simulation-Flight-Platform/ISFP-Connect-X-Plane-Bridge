#include "isfp_plugin.h"
#include <fstream>
#include <iomanip>
#include <sstream>

#include <XPLMUtilities.h>
#include "logger.h"

namespace ISFP {

ConfigManager* g_config = nullptr;

ConfigManager::ConfigManager() = default;
ConfigManager::~ConfigManager() { Save(); }

// ==================== Schema Registration ====================
// Central place defining every setting. Adding a new setting is one line.
// This drives both the JSON structure and the default fallback on load.

void ConfigManager::Register(
    const char* key,
    json default_value,
    const char* description,
    const char* section)
{
    schema_.push_back({ key, std::move(default_value), description, section });
}

void ConfigManager::RegisterDefaults() {
    // ── meta ──────────────────────────────────────────────
    Register("meta.version",                 1,           "Config schema version for migration",               "meta");

    // ── window geometry ───────────────────────────────────
    Register("window.position.x",            50,          "Window left edge (pixels from screen left)",         "window");
    Register("window.position.y",            50,          "Window top edge (pixels from screen top)",           "window");
    Register("window.size.width",            820,         "Window content width (pixels)",                      "window");
    Register("window.size.height",           560,         "Window content height (pixels)",                     "window");

    // ── UI appearance ─────────────────────────────────────
    Register("ui.language",                  "EN",        "Interface language: EN or CN",                      "ui");
    Register("ui.scale",                     1.0f,        "UI scaling factor (0.75 / 1.0 / 1.25 / 1.5)",       "ui");
    Register("ui.font",                      "Default",   "Active font name (or Default for system font)",      "ui");
    Register("ui.hide_on_startup",           false,       "Hide the UI panel when the plugin starts",           "ui");

    // ── plugin features ───────────────────────────────────
    Register("plugin.fsd.enabled",           true,        "FSD server active on startup",                      "plugin");
    Register("plugin.csl.enabled",           true,        "CSL model rendering active on startup",             "plugin");
    Register("plugin.csl.log_enabled",       false,       "Enable CSL debug logging to X-Plane log.txt",        "plugin");
    Register("plugin.mouseyoke.hidden",      false,       "Hide the on-screen yoke / control-surface indicator", "plugin");

    // ── hotkey ────────────────────────────────────────────
    Register("hotkey.toggle_ui.vkey",        73,          "Virtual-key code of the UI toggle hotkey",           "hotkey");
    Register("hotkey.toggle_ui.ctrl",        false,       "Ctrl modifier for the hotkey",                      "hotkey");
    Register("hotkey.toggle_ui.shift",       true,        "Shift modifier for the hotkey",                     "hotkey");
    Register("hotkey.toggle_ui.alt",         false,       "Alt modifier for the hotkey",                       "hotkey");
    Register("hotkey.mouseyoke.vkey",        0,           "Virtual-key code of the mouse-roller toggle hotkey","hotkey");
    Register("hotkey.mouseyoke.ctrl",        false,       "Ctrl modifier for the mouse-roller hotkey",         "hotkey");
    Register("hotkey.mouseyoke.shift",       false,       "Shift modifier for the mouse-roller hotkey",        "hotkey");
    Register("hotkey.mouseyoke.alt",         false,       "Alt modifier for the mouse-roller hotkey",          "hotkey");
}

// ==================== File I/O ====================

bool ConfigManager::Load() {
    file_path_ = GetProfilesPath() + "\\Settings.json";

    // Register schema on first call
    if (schema_.empty()) {
        RegisterDefaults();
    }

    std::ifstream file(file_path_);
    if (!file.is_open()) {
        // No file yet — start from schema defaults
        data_ = json::object();
        ApplyDefaults();
        Logger::Main("ISFP-xLink:Config:No settings file, using schema defaults\n");
        return true;
    }

    try {
        data_ = json::parse(file, nullptr, false);
        file.close();

        if (data_.is_discarded() || !data_.is_object()) {
            data_ = json::object();
            ApplyDefaults();
            Logger::Main("ISFP-xLink:Config:Settings corrupt, reset to defaults\n");
            return false;
        }

        // Fill any schema keys missing from the on-disk file (e.g. after an update)
        ApplyDefaults();

        Logger::Main("ISFP-xLink:Config:Settings loaded\n");
        return true;
    }
    catch (...) {
        data_ = json::object();
        ApplyDefaults();
        Logger::Main("ISFP-xLink:Config:Settings parse failed, reset to defaults\n");
        return false;
    }
}

bool ConfigManager::Save() {
    if (file_path_.empty()) return false;

    // Ensure the directory exists
    std::string dir = file_path_.substr(0, file_path_.find_last_of("\\/"));
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    std::ofstream file(file_path_);
    if (!file.is_open()) {
        Logger::Main("ISFP-xLink:Config:Failed to open settings file for writing\n");
        return false;
    }

    file << std::setw(4) << data_ << std::endl;
    Logger::Main("ISFP-xLink:Config:Settings saved\n");
    return true;
}

// ==================== Defaults Filler ====================

void ConfigManager::ApplyDefaults() {
    if (!data_.is_object()) data_ = json::object();

    for (const auto& entry : schema_) {
        std::string leaf;
        json* parent = ResolvePtr(leaf, entry.key, true);
        if (parent && !parent->contains(leaf)) {
            (*parent)[leaf] = entry.default_value;
        }
    }

    // Ensure meta.version always exists
    if (!data_.contains("meta") || !data_["meta"].is_object()) {
        data_["meta"] = json::object();
    }
    if (!data_["meta"].contains("version")) {
        data_["meta"]["version"] = 1;
    }
}

// ==================== Dot-Notation Resolution ====================

json* ConfigManager::ResolvePtr(std::string& out_leaf, const std::string& key, bool create) {
    if (key.empty()) return nullptr;

    size_t dot = key.find('.');
    if (dot == std::string::npos) {
        out_leaf = key;
        return &data_;
    }

    std::string segment = key.substr(0, dot);
    std::string rest = key.substr(dot + 1);

    json* parent = &data_;
    while (true) {
        if (create && !parent->contains(segment)) {
            (*parent)[segment] = json::object();
        }
        if (!parent->is_object() || !parent->contains(segment)) {
            return nullptr;
        }
        json* child = &(*parent)[segment];

        dot = rest.find('.');
        if (dot == std::string::npos) {
            out_leaf = rest;
            return child;
        }
        segment = rest.substr(0, dot);
        rest = rest.substr(dot + 1);
        parent = child;
    }
}

const json* ConfigManager::ResolvePtr(std::string& out_leaf, const std::string& key) const {
    return const_cast<ConfigManager*>(this)->ResolvePtr(out_leaf, key, false);
}

// ==================== Getters ====================

int ConfigManager::GetInt(const std::string& key, int default_val) const {
    std::string leaf;
    const json* parent = ResolvePtr(leaf, key);
    if (!parent || !parent->contains(leaf) || !(*parent)[leaf].is_number_integer())
        return default_val;
    return (*parent)[leaf].get<int>();
}

float ConfigManager::GetFloat(const std::string& key, float default_val) const {
    std::string leaf;
    const json* parent = ResolvePtr(leaf, key);
    if (!parent || !parent->contains(leaf) || !(*parent)[leaf].is_number_float())
        return default_val;
    return (*parent)[leaf].get<float>();
}

bool ConfigManager::GetBool(const std::string& key, bool default_val) const {
    std::string leaf;
    const json* parent = ResolvePtr(leaf, key);
    if (!parent || !parent->contains(leaf) || !(*parent)[leaf].is_boolean())
        return default_val;
    return (*parent)[leaf].get<bool>();
}

std::string ConfigManager::GetString(const std::string& key, const std::string& default_val) const {
    std::string leaf;
    const json* parent = ResolvePtr(leaf, key);
    if (!parent || !parent->contains(leaf) || !(*parent)[leaf].is_string())
        return default_val;
    return (*parent)[leaf].get<std::string>();
}

// ==================== Setters ====================

void ConfigManager::SetInt(const std::string& key, int val) {
    std::string leaf;
    json* parent = ResolvePtr(leaf, key, true);
    if (parent) (*parent)[leaf] = val;
}

void ConfigManager::SetFloat(const std::string& key, float val) {
    std::string leaf;
    json* parent = ResolvePtr(leaf, key, true);
    if (parent) (*parent)[leaf] = val;
}

void ConfigManager::SetBool(const std::string& key, bool val) {
    std::string leaf;
    json* parent = ResolvePtr(leaf, key, true);
    if (parent) (*parent)[leaf] = val;
}

void ConfigManager::SetString(const std::string& key, const std::string& val) {
    std::string leaf;
    json* parent = ResolvePtr(leaf, key, true);
    if (parent) (*parent)[leaf] = val;
}

bool ConfigManager::HasKey(const std::string& key) const {
    std::string leaf;
    const json* parent = ResolvePtr(leaf, key);
    return parent && parent->contains(leaf);
}

void ConfigManager::RemoveKey(const std::string& key) {
    std::string leaf;
    json* parent = ResolvePtr(leaf, key, false);
    if (parent && parent->contains(leaf)) {
        parent->erase(leaf);
    }
}

} // namespace ISFP
