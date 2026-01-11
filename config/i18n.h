#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>

// Simple header-only i18n loader.
// - Loads all *.lang files found in a `locales/` subdirectory next to the executable (working dir).
// - Each `.lang` file: `key=value` lines. Lines starting with `#` or blank lines are ignored.
// - Consumers can call get(code, key) to obtain the localized string. If not found, falls back to EN.
// - Use placeholder `{SAVE_FILENAME}` in language files; caller may substitute runtime values.

class I18n {
public:
    using LocaleMap = std::unordered_map<std::string, std::string>;
    std::unordered_map<std::string, LocaleMap> locales; // code -> (id -> text)
    std::string fallback = "EN";

    // Name of the deliberate fallback language file. Create a file named
    // `locales/LangFallback_DO_NOT_EDIT.lang` and it will be loaded as code `LANGFALLBACK`.
    std::string fallbackFileCode = "LANGFALLBACK";

    // Diagnostics captured while loading locales for later querying (useful for --list-locales).
    std::vector<std::string> loadDiagnostics;

    // Essential keys that we require a locale to provide to be considered valid.
    // If a locale is missing any of these (or they are empty) we'll reject that locale
    // at load-time so a bad/misplaced file (e.g., from a random game) can't overwrite UI.
    const std::vector<std::string> requiredKeys = {"LANGUAGE_NAME","menu_title","choice","press_enter","available_languages"};

    // Validate a locale map has required keys and a reasonable number of entries.
    inline bool isLocaleValid(const LocaleMap &m, std::vector<std::string> &missing, size_t minKeys = 6) const {
        missing.clear();
        for (auto &k : requiredKeys) {
            auto it = m.find(k);
            if (it == m.end() || it->second.empty()) missing.push_back(k);
        }
        if (m.size() < minKeys) missing.push_back("TOO_FEW_KEYS");
        return missing.empty();
    }

    std::vector<std::string> getLoadDiagnostics() const { return loadDiagnostics; }

    I18n() {
        // Primary: try working directory / local run folder
        tryLoadLocalesFolder("locales");

        // Also try relative to header / project tree (helpful when working dir is different)
        try {
            std::filesystem::path headerDir = std::filesystem::path(__FILE__).parent_path();
            tryLoadLocalesFolder((headerDir / "locales").string());
            // also try parent of header (project root) in case locales are placed there
            tryLoadLocalesFolder((headerDir.parent_path() / "locales").string());
            // also try config/locales from working directory
            tryLoadLocalesFolder("config/locales");
            // also try absolute path to config/locales (robust against different working dirs)
            try {
                std::filesystem::path absConfigPath = std::filesystem::absolute(headerDir / "locales");
                tryLoadLocalesFolder(absConfigPath.string());
            } catch (...) { /* ignore */ }
        } catch (...) { /* ignore */ }

        if (locales.empty()) {
            // Helpful diagnostic so user knows why keys may show up as IDs
            std::cerr << "Warning: no locale files loaded (looked in working dir and project). UI keys will be shown instead of translations.\n";
        }
    }

    // Reload locales (useful if working directory changed after initialization)
    void reload() {
        locales.clear();
        loadDiagnostics.clear();
        // Re-run the same loading sequence as constructor
        tryLoadLocalesFolder("locales");
        try {
            std::filesystem::path headerDir = std::filesystem::path(__FILE__).parent_path();
            tryLoadLocalesFolder((headerDir / "locales").string());
            tryLoadLocalesFolder((headerDir.parent_path() / "locales").string());
            tryLoadLocalesFolder("config/locales");
            try {
                std::filesystem::path absConfigPath = std::filesystem::absolute(headerDir / "locales");
                tryLoadLocalesFolder(absConfigPath.string());
            } catch (...) { /* ignore */ }
        } catch (...) { /* ignore */ }
    }

    static inline std::string trim(const std::string &s) {
        size_t a = 0, b = s.size();
        while (a < b && isspace((unsigned char)s[a])) ++a;
        while (b > a && isspace((unsigned char)s[b-1])) --b;
        return s.substr(a, b - a);
    }

    static inline std::string unescape(const std::string &s) {
        std::string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                char n = s[i+1];
                if (n == 'n') { out.push_back('\n'); ++i; }
                else { out.push_back(n); ++i; }
            } else out.push_back(s[i]);
        }
        return out;
    }

    bool loadLocaleFile(const std::string &path) {
        std::filesystem::path p(path);
        if (!std::filesystem::exists(p)) return false;
        std::string code = p.stem().string();
        // If file is named like EN_extra.lang, treat it as EN (merge extras)
        auto underscorePos = code.find('_');
        if (underscorePos != std::string::npos) code = code.substr(0, underscorePos);
        // uppercase code for consistency
        for (auto &c : code) c = (char)toupper((unsigned char)c);
        std::ifstream ifs(path);
        if (!ifs.good()) return false;
        LocaleMap map;
        std::string line;
        while (std::getline(ifs, line)) {
            std::string tline = trim(line);
            if (tline.empty() || tline[0] == '#') continue;
            auto pos = tline.find('=');
            if (pos == std::string::npos) continue;
            std::string k = trim(tline.substr(0, pos));
            std::string v = trim(tline.substr(pos + 1));
            v = unescape(v);
            map[k] = v;
        }

        // Merge map with existing values in a temporary buffer so we can validate the
        // resulting locale without mutating the stored one if validation fails.
        LocaleMap merged;
        auto it = locales.find(code);
        if (it == locales.end()) merged = map;
        else {
            merged = it->second; // copy existing
            for (auto &kv : map) merged[kv.first] = kv.second;
        }

        // Validate merged locale using requiredKeys. If invalid, skip and warn.
        std::vector<std::string> missing;
        if (!isLocaleValid(merged, missing)) {
            std::cerr << "i18n: skipped '" << code << "' from " << path << " - missing keys:";
            for (size_t i = 0; i < missing.size(); ++i) std::cerr << (i ? ", " : " ") << missing[i];
            std::cerr << "\n";
            return false;
        }

        // OK - commit merged locale
        locales[code] = std::move(merged);
        std::cerr << "i18n: loaded '" << code << "' from " << path << "\n";
        return true;
    }

void tryLoadLocalesFolder(const std::string &folder) {
    std::filesystem::path p(folder);
    if (!std::filesystem::exists(p) || !std::filesystem::is_directory(p)) return;

    // Collect files by base code (strip suffix after first underscore), uppercase code
    std::unordered_map<std::string, std::vector<std::string>> filesByCode;
    for (auto &entry : std::filesystem::directory_iterator(p)) {
        try {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".lang") continue;
            std::string stem = entry.path().stem().string();
            auto underscorePos = stem.find('_');
            if (underscorePos != std::string::npos) stem = stem.substr(0, underscorePos);
            for (auto &c : stem) c = (char)toupper((unsigned char)c);
            filesByCode[stem].push_back(entry.path().string());
        } catch (...) { /* ignore */ }
    }

    // For each code, parse and merge all files first, then validate merged locale
    for (auto &kv : filesByCode) {
        const std::string code = kv.first;
        LocaleMap merged;
        // start from any existing locale (from previously scanned folders)
        auto itExist = locales.find(code);
        if (itExist != locales.end()) merged = itExist->second;

        for (auto &fp : kv.second) {
            try {
                std::ifstream ifs(fp);
                if (!ifs.good()) continue;
                std::string line;
                while (std::getline(ifs, line)) {
                    std::string tline = trim(line);
                    if (tline.empty() || tline[0] == '#') continue;
                    auto pos = tline.find('=');
                    if (pos == std::string::npos) continue;
                    std::string k = trim(tline.substr(0, pos));
                    std::string v = trim(tline.substr(pos + 1));
                    v = unescape(v);
                    merged[k] = v;
                }
            } catch (...) { /* ignore file read errors */ }
        }

        // Validate merged locale using required keys
        std::vector<std::string> missing;
        if (!isLocaleValid(merged, missing)) {
            std::ostringstream oss;
            oss << "i18n: skipped '" << code << "' from " << (kv.second.empty() ? std::string("<none>") : kv.second.front()) << " - missing keys:";
            for (size_t i = 0; i < missing.size(); ++i) oss << (i ? ", " : " ") << missing[i];
            std::string msg = oss.str();
            std::cerr << msg << "\n";
            loadDiagnostics.push_back(msg);
            continue; // skip this code entirely
        }

        // Commit merged locale
        locales[code] = std::move(merged);
        for (auto &fp : kv.second) {
            std::ostringstream oss;
            oss << "i18n: loaded '" << code << "' from " << fp;
            std::string msg = oss.str();
            std::cerr << msg << "\n";
            loadDiagnostics.push_back(msg);
        }
    }
}

    std::string get(const std::string &code, const std::string &id) const {
        std::string c = code;
        for (auto &ch : c) ch = (char)toupper((unsigned char)ch);
        // 1) Try requested locale (if present and non-empty)
        auto it = locales.find(c);
        if (it != locales.end()) {
            auto it2 = it->second.find(id);
            if (it2 != it->second.end() && !it2->second.empty()) return it2->second;
        }
        // 2) Try configured fallback (EN)
        auto fit = locales.find(fallback);
        if (fit != locales.end()) {
            auto it2 = fit->second.find(id);
            if (it2 != fit->second.end() && !it2->second.empty()) return it2->second;
        }
        // 3) Try the deliberate LangFallback file if present
        auto ff = locales.find(fallbackFileCode);
        if (ff != locales.end()) {
            auto it2 = ff->second.find(id);
            if (it2 != ff->second.end() && !it2->second.empty()) return it2->second;
        }
        // Nothing found
        return std::string();
    }

    std::vector<std::pair<std::string,std::string>> availableLanguages() const {
        std::vector<std::pair<std::string,std::string>> out;
        for (auto &p : locales) {
            std::string name = p.first;
            auto itn = p.second.find("LANGUAGE_NAME");
            if (itn != p.second.end()) name = itn->second;
            out.emplace_back(p.first, name);
        }
        return out;
    }
};

// Single global instance convenient for small programs
static inline I18n i18n;
