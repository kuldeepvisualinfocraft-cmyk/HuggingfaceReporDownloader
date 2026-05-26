#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <array>
#include <map>
#include <regex>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

// ─── Windows console setup ────────────────────────────────────────────────────

void enable_ansi() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(hOut, &mode)) {
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, mode);
    }
#endif
}

// ─── ANSI helpers ─────────────────────────────────────────────────────────────

struct Color {
    static std::string reset()   { return "\033[0m";  }
    static std::string red()     { return "\033[31m"; }
    static std::string green()   { return "\033[32m"; }
    static std::string yellow()  { return "\033[33m"; }
    static std::string blue()    { return "\033[34m"; }
    static std::string cyan()    { return "\033[36m"; }
    static std::string bold()    { return "\033[1m";  }
};

#define PRINT_COLOR(color, msg)  std::cout << color << msg << Color::reset()

// ─── Utility ──────────────────────────────────────────────────────────────────

std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string exec(const std::string& cmd) {
    std::array<char, 4096> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd.c_str(), "r"), _pclose);
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

int exec_status(const std::string& cmd) {
    return system(cmd.c_str());
}

std::string to_lower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), ::tolower);
    return out;
}

// ─── Minimal JSON parser ──────────────────────────────────────────────────────

class Json {
public:
    using Object = std::map<std::string, Json>;
    using Array = std::vector<Json>;

    enum Type { NUL, STR, NUM, OBJ, ARR, BOOL };

    Type type = NUL;
    std::string str_val;
    double num_val = 0;
    bool bool_val = false;
    Object obj_val;
    Array arr_val;

    Json() = default;

    static Json parse(const std::string& input) {
        Parser p(input);
        return p.parse_value();
    }

    const Json& operator[](const std::string& key) const {
        static const Json null_json;
        auto it = obj_val.find(key);
        return it != obj_val.end() ? it->second : null_json;
    }

    const Json& operator[](size_t idx) const {
        static const Json null_json;
        return idx < arr_val.size() ? arr_val[idx] : null_json;
    }

    size_t size() const {
        if (type == OBJ) return obj_val.size();
        if (type == ARR) return arr_val.size();
        return 0;
    }

    bool is_valid() const { return type != NUL; }

private:
    struct Parser {
        const std::string& s;
        size_t pos = 0;

        Parser(const std::string& str) : s(str) {}

        void skip_ws() {
            while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r'))
                pos++;
        }

        char peek() { skip_ws(); return pos < s.size() ? s[pos] : '\0'; }
        char next() { skip_ws(); return pos < s.size() ? s[pos++] : '\0'; }

        Json parse_value() {
            char c = peek();
            if (c == '"')  return parse_string();
            if (c == '{')  return parse_object();
            if (c == '[')  return parse_array();
            if (c == 't' || c == 'f') return parse_bool();
            if (c == 'n')  { parse_null(); return Json(); }
            if (c == '-' || (c >= '0' && c <= '9')) return parse_number();
            throw std::runtime_error(std::string("Unexpected char: ") + c);
        }

        Json parse_string() {
            Json j;
            j.type = STR;
            next(); // "
            while (pos < s.size() && s[pos] != '"') {
                if (s[pos] == '\\') {
                    pos++;
                    switch (s[pos]) {
                        case '"':  j.str_val += '"';  break;
                        case '\\': j.str_val += '\\'; break;
                        case '/':  j.str_val += '/';  break;
                        case 'b':  j.str_val += '\b'; break;
                        case 'f':  j.str_val += '\f'; break;
                        case 'n':  j.str_val += '\n'; break;
                        case 'r':  j.str_val += '\r'; break;
                        case 't':  j.str_val += '\t'; break;
                        default:   j.str_val += s[pos]; break;
                    }
                    pos++;
                } else {
                    j.str_val += s[pos++];
                }
            }
            if (pos < s.size()) pos++; // "
            return j;
        }

        Json parse_number() {
            Json j;
            j.type = NUM;
            size_t start = pos;
            if (s[pos] == '-') pos++;
            while (pos < s.size() && isdigit(s[pos])) pos++;
            if (pos < s.size() && s[pos] == '.') { pos++; while (pos < s.size() && isdigit(s[pos])) pos++; }
            if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
                pos++; if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) pos++;
                while (pos < s.size() && isdigit(s[pos])) pos++;
            }
            j.num_val = std::stod(s.substr(start, pos - start));
            return j;
        }

        Json parse_object() {
            Json j;
            j.type = OBJ;
            next(); // {
            if (peek() == '}') { next(); return j; }
            while (true) {
                skip_ws();
                auto key = parse_string();
                skip_ws();
                if (next() != ':') throw std::runtime_error("Expected ':'");
                j.obj_val[key.str_val] = parse_value();
                skip_ws();
                char c = next();
                if (c == '}') break;
                if (c != ',') throw std::runtime_error("Expected ',' or '}'");
            }
            return j;
        }

        Json parse_array() {
            Json j;
            j.type = ARR;
            next(); // [
            if (peek() == ']') { next(); return j; }
            while (true) {
                j.arr_val.push_back(parse_value());
                skip_ws();
                char c = next();
                if (c == ']') break;
                if (c != ',') throw std::runtime_error("Expected ',' or ']'");
            }
            return j;
        }

        Json parse_bool() {
            Json j;
            j.type = BOOL;
            if (s.substr(pos, 4) == "true")  { j.bool_val = true;  pos += 4; }
            else if (s.substr(pos, 5) == "false") { j.bool_val = false; pos += 5; }
            else throw std::runtime_error("Expected bool");
            return j;
        }

        void parse_null() {
            if (s.substr(pos, 4) == "null") pos += 4;
            else throw std::runtime_error("Expected null");
        }
    };
};

// ─── Hugging Face URL parsing ─────────────────────────────────────────────────

struct RepoInfo {
    std::string type;    // "models" or "datasets"
    std::string owner;
    std::string repo;
    bool valid = false;
};

RepoInfo parse_hf_url(const std::string& url) {
    RepoInfo info;
    std::string u = trim(url);

    // Strip trailing slashes
    while (!u.empty() && u.back() == '/') u.pop_back();

    // Handle different URL formats:
    // https://huggingface.co/owner/repo
    // https://huggingface.co/datasets/owner/repo
    // huggingface.co/owner/repo
    // owner/repo

    std::regex patterns[] = {
        // Full URL with datasets
        std::regex(R"(^(?:https?://)?huggingface\.co/datasets/([^/]+)/([^/?#]+))", std::regex::icase),
        // Full URL with models (default)
        std::regex(R"(^(?:https?://)?huggingface\.co/([^/]+)/([^/?#]+))", std::regex::icase),
        // Short form: owner/repo
        std::regex(R"(^([^/]+)/([^/]+)$)"),
    };

    std::smatch m;
    if (std::regex_search(u, m, patterns[0])) {
        info.type = "datasets";
        info.owner = m[1];
        info.repo = m[2];
        info.valid = true;
    } else if (std::regex_search(u, m, patterns[1])) {
        info.type = "models";
        info.owner = m[1];
        info.repo = m[2];
        info.valid = true;
    } else if (std::regex_search(u, m, patterns[2])) {
        info.type = "models";
        info.owner = m[1];
        info.repo = m[2];
        info.valid = true;
    }

    return info;
}

// ─── Hugging Face API ─────────────────────────────────────────────────────────

std::vector<std::string> list_hf_files(const RepoInfo& info) {
    std::string api_url = "https://huggingface.co/api/" + info.type + "/"
                          + info.owner + "/" + info.repo;

    std::string curl_cmd = "curl.exe -s --max-time 30 \"" + api_url + "\" 2>nul";
    std::string json_str = exec(curl_cmd);

    if (json_str.empty()) {
        PRINT_COLOR(Color::red(), "[ERROR] Failed to fetch from API or empty response.\n");
        return {};
    }

    Json root;
    try {
        root = Json::parse(json_str);
    } catch (const std::exception& e) {
        PRINT_COLOR(Color::red(), std::string("[ERROR] Failed to parse API response: ") + e.what() + "\n");
        return {};
    }

    if (!root.is_valid() || root.type != Json::OBJ) {
        PRINT_COLOR(Color::red(), "[ERROR] Invalid API response format.\n");
        return {};
    }

    // Check for error message
    if (root["error"].is_valid()) {
        PRINT_COLOR(Color::red(), std::string("[ERROR] API error: ") + root["error"].str_val + "\n");
        return {};
    }

    // Get safe name from API if available
    // Not needed here but useful info

    // Extract siblings
    Json siblings = root["siblings"];
    if (!siblings.is_valid() || siblings.type != Json::ARR) {
        PRINT_COLOR(Color::red(), "[ERROR] No 'siblings' array found in API response.\n");
        return {};
    }

    std::vector<std::string> files;
    for (size_t i = 0; i < siblings.size(); i++) {
        Json sibling = siblings[i];
        Json rfilename = sibling["rfilename"];
        if (rfilename.is_valid() && rfilename.type == Json::STR) {
            files.push_back(rfilename.str_val);
        }
    }

    return files;
}

// ─── Download ─────────────────────────────────────────────────────────────────

bool download_file(const std::string& url, const std::string& dest_path) {
    std::string dest_dir = fs::path(dest_path).parent_path().string();
    if (!dest_dir.empty() && !fs::exists(dest_dir)) {
        fs::create_directories(dest_dir);
    }

    fs::path dest(dest_path);
    // Quote paths that may contain spaces
    std::string curl_cmd = "curl.exe -L -# -o \"" + dest.string() + "\" \"" + url + "\" 2>&1";

    int ret = exec_status(curl_cmd);
    return ret == 0;
}

bool download_repo(const RepoInfo& info, const std::vector<std::string>& files) {
    std::string folder_name = info.repo;
    fs::path base_dir = fs::current_path() / folder_name;

    if (fs::exists(base_dir)) {
        PRINT_COLOR(Color::yellow(), "[WARN] Folder '" + folder_name + "' already exists.\n");
        std::cout << "  Overwrite? (y/N): ";
        std::string resp;
        std::getline(std::cin, resp);
        if (to_lower(trim(resp)) != "y") {
            PRINT_COLOR(Color::yellow(), "Download cancelled.\n");
            return false;
        }
    }

    fs::create_directories(base_dir);
    std::cout << "\n";

    size_t total = files.size();
    size_t success = 0;
    size_t failed = 0;

    for (size_t i = 0; i < total; i++) {
        const auto& file = files[i];

        std::string file_url = "https://huggingface.co/" + info.owner + "/" + info.repo
                               + "/resolve/main/" + file;

        // Use URL-encoded version for special chars
        // curl handles this fine as long as we pass the URL properly

        fs::path dest_path = base_dir / file;
        fs::path parent_dir = dest_path.parent_path();

        if (!fs::exists(parent_dir)) {
            fs::create_directories(parent_dir);
        }

        printf("\r[%zu/%zu] Downloading: %-60s", i + 1, total, file.c_str());

        bool ok = download_file(file_url, dest_path.string());

        if (ok) {
            success++;
        } else {
            failed++;
            // Print error on next line
            printf("\n");
            PRINT_COLOR(Color::red(), "  [FAILED] " + file + "\n");
        }
    }

    printf("\n");
    std::cout << "\n";

    PRINT_COLOR(Color::cyan(), "═══════════════════════════════════════\n");
    std::cout << "  Repository: " << info.owner << "/" << info.repo << "\n";
    std::cout << "  Location:   " << base_dir.string() << "\n";
    std::cout << "  Total:      " << total << " files\n";
    PRINT_COLOR(Color::green(), "  Succeeded:  " + std::to_string(success) + "\n");
    if (failed > 0) {
        PRINT_COLOR(Color::red(), "  Failed:     " + std::to_string(failed) + "\n");
    }
    PRINT_COLOR(Color::cyan(), "═══════════════════════════════════════\n");

    return failed == 0;
}

// ─── UI ───────────────────────────────────────────────────────────────────────

void print_banner() {
    std::cout << Color::cyan() << Color::bold()
              << "  _  _    _    ____    _    _   _  ____  \n"
              << " | || |  / \\  / ___|  / \\  | \\ | |/ ___| \n"
              << " | || |_/ _ \\| |  _  / _ \\ |  \\| | |  _  \n"
              << " |__   _/ ___ \\ |_| |/ ___ \\| |\\  | |_| |\n"
              << "    |_/_/   \\_\\____/_/   \\_\\_| \\_|\\____|\n"
              << Color::reset()
              << "    Hugging Face Repository Downloader\n\n";
}

void print_menu() {
    std::cout << Color::bold() << "  Main Menu\n" << Color::reset();
    std::cout << "  " << Color::green() << "[1]" << Color::reset() << " Enter Hugging Face URL\n";
    std::cout << "  " << Color::green() << "[2]" << Color::reset() << " Start Download\n";
    std::cout << "  " << Color::green() << "[3]" << Color::reset() << " Change Output Directory\n";
    std::cout << "  " << Color::green() << "[4]" << Color::reset() << " Exit\n\n";
}

int main() {
    enable_ansi();

    std::string hf_url;
    RepoInfo repo_info;
    std::vector<std::string> file_list;
    std::string output_dir = fs::current_path().string();
    bool has_files = false;

    while (true) {
        system("cls");
        print_banner();
        std::cout << "  Output directory: " << Color::yellow() << output_dir << Color::reset() << "\n\n";

        if (!hf_url.empty()) {
            std::cout << "  URL: " << Color::cyan() << hf_url << Color::reset() << "\n";
            if (repo_info.valid) {
                std::cout << "  Repo: " << Color::green() << repo_info.owner << "/" << repo_info.repo
                          << " (" << repo_info.type << ")" << Color::reset() << "\n";
                if (has_files) {
                    std::cout << "  Files: " << Color::green() << file_list.size() << " found" << Color::reset() << "\n";
                }
            }
            std::cout << "\n";
        }

        print_menu();

        std::cout << "  Enter choice [1-4]: ";
        std::string choice;
        std::getline(std::cin, choice);
        choice = trim(choice);

        if (choice == "1") {
            std::cout << "\n  Enter Hugging Face repo URL:\n";
            std::cout << "  " << Color::blue() << ">" << Color::reset() << " ";
            std::getline(std::cin, hf_url);
            hf_url = trim(hf_url);

            if (hf_url.empty()) {
                PRINT_COLOR(Color::red(), "\n  [ERROR] URL cannot be empty.\n");
                std::cout << "  Press Enter to continue...";
                std::cin.get();
                continue;
            }

            repo_info = parse_hf_url(hf_url);
            if (!repo_info.valid) {
                PRINT_COLOR(Color::red(), "\n  [ERROR] Invalid Hugging Face URL.\n");
                std::cout << "  Expected format: https://huggingface.co/owner/repo\n";
                std::cout << "  Press Enter to continue...";
                std::cin.get();
                continue;
            }

            PRINT_COLOR(Color::green(), "\n  [OK] Repo parsed: " + repo_info.owner + "/" + repo_info.repo + "\n");

            // Auto-fetch file list
            std::cout << "  Fetching file list from API...\n";
            file_list = list_hf_files(repo_info);

            if (file_list.empty()) {
                PRINT_COLOR(Color::red(), "\n  [ERROR] No files found or repo is private/doesn't exist.\n");
                repo_info.valid = false;
                has_files = false;
                std::cout << "  Press Enter to continue...";
                std::cin.get();
                continue;
            }

            has_files = true;
            PRINT_COLOR(Color::green(), "\n  [OK] Found " + std::to_string(file_list.size()) + " files.\n");

            // Show first few files as preview
            size_t preview = std::min(file_list.size(), size_t(5));
            for (size_t i = 0; i < preview; i++) {
                std::cout << "       " << file_list[i] << "\n";
            }
            if (file_list.size() > preview) {
                std::cout << "       ... and " << (file_list.size() - preview) << " more\n";
            }

            std::cout << "\n  Press Enter to continue...";
            std::cin.get();

        } else if (choice == "2") {
            if (!repo_info.valid || !has_files || file_list.empty()) {
                PRINT_COLOR(Color::red(), "\n  [ERROR] No repository loaded. Enter a URL first (option 1).\n");
                std::cout << "  Press Enter to continue...";
                std::cin.get();
                continue;
            }

            fs::current_path(output_dir);
            download_repo(repo_info, file_list);

            std::cout << "\n  Press Enter to continue...";
            std::cin.get();

        } else if (choice == "3") {
            std::cout << "\n  Current output directory: " << Color::yellow() << output_dir << Color::reset() << "\n";
            std::cout << "  Enter new output directory:\n";
            std::cout << "  " << Color::blue() << ">" << Color::reset() << " ";
            std::getline(std::cin, output_dir);
            output_dir = trim(output_dir);

            if (output_dir.empty()) {
                output_dir = fs::current_path().string();
            } else if (!fs::exists(output_dir)) {
                PRINT_COLOR(Color::yellow(), "\n  [WARN] Directory doesn't exist. It will be created on download.\n");
                std::cout << "  Press Enter to continue...";
                std::cin.get();
            } else {
                PRINT_COLOR(Color::green(), "\n  [OK] Output directory set.\n");
                std::cout << "  Press Enter to continue...";
                std::cin.get();
            }

        } else if (choice == "4") {
            std::cout << "\n";
            PRINT_COLOR(Color::green(), "  Goodbye!\n");
            break;

        } else {
            PRINT_COLOR(Color::red(), "\n  [ERROR] Invalid choice. Please enter 1-4.\n");
            std::cout << "  Press Enter to continue...";
            std::cin.get();
        }
    }

    return 0;
}
