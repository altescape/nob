// Free for USE by anyone anywhere
// Inspired by Tsoding https://www.youtube.com/@TsodingDaily

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <cassert>
#include <vector>
#include <string>
#include <string_view>
#include <filesystem>
#include <system_error>
#include <iostream>
#include <atomic>
#include <fstream>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <variant>
#include <iomanip>
#include <charconv>
#include <memory>
#include <stdexcept>

// --- CLI Flags Subsystem ---
namespace flags {

class Parser {
public:
    explicit Parser(std::string program_name) : program_name_(std::move(program_name)) {}

    bool& add_bool(std::string_view name, bool def = false, std::string_view desc = "") {
        return add_flag<bool>(name, def, desc);
    }
    void add_bool_var(bool* var, std::string_view name, std::string_view desc = "") { add_flag_var(var, name, desc); }

    float& add_float(std::string_view name, float def = 0.0f, std::string_view desc = "") {
        return add_flag<float>(name, def, desc);
    }
    void add_float_var(float* var, std::string_view name, std::string_view desc = "") { add_flag_var(var, name, desc); }

    double& add_double(std::string_view name, double def = 0.0, std::string_view desc = "") {
        return add_flag<double>(name, def, desc);
    }
    void add_double_var(double* var, std::string_view name, std::string_view desc = "") { add_flag_var(var, name, desc); }

    uint64_t& add_uint64(std::string_view name, uint64_t def = 0, std::string_view desc = "") {
        return add_flag<uint64_t>(name, def, desc);
    }
    void add_uint64_var(uint64_t* var, std::string_view name, std::string_view desc = "") { add_flag_var(var, name, desc); }

    int64_t& add_int64(std::string_view name, int64_t def = 0, std::string_view desc = "") {
        return add_flag<int64_t>(name, def, desc);
    }
    void add_int64_var(int64_t* var, std::string_view name, std::string_view desc = "") { add_flag_var(var, name, desc); }

    std::string& add_string(std::string_view name, std::string_view def = "", std::string_view desc = "") {
        return add_flag<std::string>(name, std::string(def), desc);
    }
    void add_string_var(std::string* var, std::string_view name, std::string_view desc = "") { add_flag_var(var, name, desc); }

    std::vector<std::string>& add_list(std::string_view name, std::string_view desc = "") {
        return add_flag<std::vector<std::string>>(name, {}, desc);
    }
    void add_list_var(std::vector<std::string>* var, std::string_view name, std::string_view desc = "") { add_flag_var(var, name, desc); }

    bool parse(int argc, char** argv) {
        if (argc > 0) {
            argc--;
            argv++;
        }

        bool parsing_flags = true;

        for (int i = 0; i < argc; ++i) {
            std::string_view arg(argv[i]);

            if (parsing_flags && arg == "--") {
                parsing_flags = false;
                continue;
            }

            if (parsing_flags && arg.rfind("-", 0) == 0) {
                bool ignore = false;
                std::string_view flag_view = arg.substr(1);
                
                // Allow double dash --flag
                if (flag_view.rfind("-", 0) == 0) {
                    flag_view = flag_view.substr(1);
                }

                // Experimental Tsoding Ignore Syntax: -/flag
                if (flag_view.rfind("/", 0) == 0) {
                    ignore = true;
                    flag_view = flag_view.substr(1);
                }

                std::string_view value_view;
                bool has_equals = false;
                auto eq_pos = flag_view.find('=');
                if (eq_pos != std::string_view::npos) {
                    value_view = flag_view.substr(eq_pos + 1);
                    flag_view = flag_view.substr(0, eq_pos);
                    has_equals = true;
                }

                auto it = find_flag(flag_view);
                if (it == flags_.end()) {
                    error_message_ = "Unknown flag: -" + std::string(flag_view);
                    return false;
                }

                if (ignore) {
                    // Skip over the value argument if needed
                    if (!has_equals && !std::holds_alternative<bool*>(it->value)) {
                        if (i + 1 < argc && std::string_view(argv[i+1]).rfind("-", 0) != 0) {
                            i++;
                        }
                    }
                    continue;
                }

                if (std::holds_alternative<bool*>(it->value)) {
                    if (has_equals) {
                        *std::get<bool*>(it->value) = parse_bool(value_view);
                    } else {
                        *std::get<bool*>(it->value) = true;
                    }
                } else {
                    if (!has_equals) {
                        if (i + 1 >= argc || std::string_view(argv[i+1]).rfind("-", 0) == 0) {
                            error_message_ = "Flag -" + std::string(flag_view) + " requires a value";
                            return false;
                        }
                        value_view = argv[++i];
                    }

                    if (!parse_value(*it, value_view)) {
                        return false;
                    }
                }
            } else {
                rest_args_.emplace_back(arg);
            }
        }

        return true;
    }

    const std::vector<std::string>& rest_args() const { return rest_args_; }
    const std::string& error_message() const { return error_message_; }

    void print_help(std::ostream& out = std::cerr) const {
        out << "Usage: " << program_name_ << " [options] [arguments...]\n\n";
        out << "Options:\n";
        for (const auto& flag : flags_) {
            out << "  -" << std::left << std::setw(20) << flag.name;
            if (!flag.desc.empty()) {
                out << flag.desc;
            }
            if (flag.default_str != "\"\"" && flag.default_str != "[]") {
                out << " (default: " << flag.default_str << ")";
            }
            out << "\n";
        }
    }

    void print_error(std::ostream& out = std::cerr) const {
        if (!error_message_.empty()) {
            out << "Error: " << error_message_ << "\n";
        }
    }

private:
    struct FlagDef {
        std::string name;
        std::string desc;
        std::variant<bool*, float*, double*, int64_t*, uint64_t*, std::string*, std::vector<std::string>*> value;
        std::string default_str;
    };

    std::string program_name_;
    std::vector<FlagDef> flags_;
    std::vector<std::string> rest_args_;
    std::string error_message_;
    
    // Type-erased memory storage for dynamic heap allocation of references
    std::vector<std::unique_ptr<void, void(*)(void*)>> storage_;

    template <typename T>
    T& add_flag(std::string_view name, T def, std::string_view desc) {
        T* ptr = new T(std::move(def));
        storage_.emplace_back(ptr, [](void* p) { delete static_cast<T*>(p); });
        add_flag_var(ptr, name, desc);
        return *ptr;
    }

    template <typename T>
    void add_flag_var(T* var, std::string_view name, std::string_view desc) {
        flags_.push_back(FlagDef{
            std::string(name), 
            std::string(desc), 
            var, 
            to_string_impl(*var)
        });
    }

    static std::string to_string_impl(const bool& b) { return b ? "true" : "false"; }
    static std::string to_string_impl(const float& f) { return std::to_string(f); }
    static std::string to_string_impl(const double& d) { return std::to_string(d); }
    static std::string to_string_impl(const uint64_t& u) { return std::to_string(u); }
    static std::string to_string_impl(const int64_t& s) { return std::to_string(s); }
    static std::string to_string_impl(const std::string& s) { return s.empty() ? "\"\"" : s; }
    static std::string to_string_impl(const std::vector<std::string>&) { return "[]"; }

    bool parse_bool(std::string_view v) {
        return v == "true" || v == "1" || v == "yes" || v == "on";
    }

    bool parse_value(FlagDef& flag, std::string_view v) {
        if (std::holds_alternative<float*>(flag.value)) {
            try { *std::get<float*>(flag.value) = std::stof(std::string(v)); } 
            catch(...) { error_message_ = "Invalid float value for flag: " + flag.name; return false; }
        }
        else if (std::holds_alternative<double*>(flag.value)) {
            try { *std::get<double*>(flag.value) = std::stod(std::string(v)); } 
            catch(...) { error_message_ = "Invalid double value for flag: " + flag.name; return false; }
        }
        else if (std::holds_alternative<uint64_t*>(flag.value)) {
            uint64_t val = 0;
            auto [ptr, ec] = std::from_chars(v.data(), v.data() + v.size(), val);
            if (ec != std::errc()) { error_message_ = "Invalid uint64_t for flag: " + flag.name; return false; }
            *std::get<uint64_t*>(flag.value) = val;
        }
        else if (std::holds_alternative<int64_t*>(flag.value)) {
            int64_t val = 0;
            auto [ptr, ec] = std::from_chars(v.data(), v.data() + v.size(), val);
            if (ec != std::errc()) { error_message_ = "Invalid int64_t for flag: " + flag.name; return false; }
            *std::get<int64_t*>(flag.value) = val;
        }
        else if (std::holds_alternative<std::string*>(flag.value)) {
            *std::get<std::string*>(flag.value) = std::string(v);
        }
        else if (std::holds_alternative<std::vector<std::string>*>(flag.value)) {
            std::get<std::vector<std::string>*>(flag.value)->push_back(std::string(v));
        }
        return true;
    }

    std::vector<FlagDef>::iterator find_flag(std::string_view name) {
        for (auto it = flags_.begin(); it != flags_.end(); ++it) {
            if (it->name == name) return it;
        }
        return flags_.end();
    }
};

} // namespace flags

#ifdef _WIN32
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
     // FIX: Prevent Windows macros from corrupting our C++ enums
#    undef ERROR
#    undef WARNING
#    undef INFO
#elif defined(__APPLE__)
#    include <mach-o/dyld.h>
#endif

#ifndef _WIN32
#    include <sys/types.h>
#    include <sys/wait.h>
#    include <unistd.h>
#    include <fcntl.h>
#endif

#ifdef _WIN32
#    define NOB_LINE_END "\r\n"
#else
#    define NOB_LINE_END "\n"
#endif

#define NOB_TODO(message) do { std::fprintf(stderr, "%s:%d: TODO: %s\n", __FILE__, __LINE__, message); std::abort(); } while(0)
#define NOB_UNREACHABLE(message) do { std::fprintf(stderr, "%s:%d: UNREACHABLE: %s\n", __FILE__, __LINE__, message); std::abort(); } while(0)

namespace nob {

// --- Logging Subsystem ---

enum class LogLevel { INFO, WARNING, ERROR, NO_LOGS };

inline LogLevel minimal_log_level = LogLevel::INFO;

inline void log(LogLevel level, const char* fmt, ...) {
    if (level < minimal_log_level) return;
    switch (level) {
        case LogLevel::INFO:    std::fprintf(stderr, "[INFO] "); break;
        case LogLevel::WARNING: std::fprintf(stderr, "[WARNING] "); break;
        case LogLevel::ERROR:   std::fprintf(stderr, "[ERROR] "); break;
        case LogLevel::NO_LOGS: return;
        default:                NOB_UNREACHABLE("nob::log");
    }
    std::va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
    std::fprintf(stderr, "\n");
}

// --- Argument Helper ---

inline const char* shift_args(int& argc, char**& argv) {
    assert(argc > 0);
    const char* result = *argv;
    argc--;
    argv++;
    return result;
}

#ifdef _WIN32
inline std::string win32_error_message(DWORD err) {
    char buf[4096] = {0};
    DWORD size = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
                                NULL, err, LANG_USER_DEFAULT, buf, sizeof(buf), NULL);
    if (size == 0) return "Unknown Win32 Error";
    while (size > 1 && std::isspace(buf[size - 1])) buf[--size] = '\0';
    return std::string(buf);
}
#endif

// --- File System & Path Helpers ---

inline bool mkdir_if_not_exists(const std::string& path) {
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        log(LogLevel::INFO, "directory `%s` already exists", path.c_str());
        return true;
    }
    if (std::filesystem::create_directories(path, ec)) {
        log(LogLevel::INFO, "created directory `%s`", path.c_str());
        return true;
    }
    log(LogLevel::ERROR, "could not create directory `%s`: %s", path.c_str(), ec.message().c_str());
    return false;
}

inline bool copy_file(const std::string& src_path, const std::string& dst_path) {
    log(LogLevel::INFO, "copying %s -> %s", src_path.c_str(), dst_path.c_str());
    std::error_code ec;
    std::filesystem::copy_file(src_path, dst_path, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        log(LogLevel::ERROR, "Could not copy file: %s", ec.message().c_str());
        return false;
    }
    return true;
}

inline bool copy_directory_recursively(const std::string& src_path, const std::string& dst_path) {
    log(LogLevel::INFO, "copying directory %s -> %s", src_path.c_str(), dst_path.c_str());
    std::error_code ec;
    std::filesystem::copy(src_path, dst_path, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        log(LogLevel::ERROR, "Could not copy directory: %s", ec.message().c_str());
        return false;
    }
    return true;
}

inline bool delete_file(const std::string& path) {
    log(LogLevel::INFO, "deleting %s", path.c_str());
    std::error_code ec;
    if (!std::filesystem::remove_all(path, ec)) {
        log(LogLevel::ERROR, "Could not delete %s: %s", path.c_str(), ec.message().c_str());
        return false;
    }
    return true;
}

inline bool rename(const std::string& old_path, const std::string& new_path) {
    log(LogLevel::INFO, "renaming %s -> %s", old_path.c_str(), new_path.c_str());
    std::error_code ec;
    std::filesystem::rename(old_path, new_path, ec);
    if (ec) {
        log(LogLevel::ERROR, "could not rename %s to %s: %s", old_path.c_str(), new_path.c_str(), ec.message().c_str());
        return false;
    }
    return true;
}

inline bool file_exists(const std::string& path) {
    return std::filesystem::exists(path);
}

inline std::string path_name(const std::string& path) {
    return std::filesystem::path(path).filename().string();
}

inline std::string get_current_dir() {
    std::error_code ec;
    auto p = std::filesystem::current_path(ec);
    return ec ? "" : p.string();
}

inline bool set_current_dir(const std::string& path) {
    std::error_code ec;
    std::filesystem::current_path(path, ec);
    if (ec) {
        log(LogLevel::ERROR, "could not set current directory to %s: %s", path.c_str(), ec.message().c_str());
        return false;
    }
    return true;
}

// --- Dependency Tracking Subsystem ---

namespace deps {
    inline std::vector<std::string> parse_gcc_d_file(const std::string& path) {
        std::vector<std::string> dependencies;
        if (!file_exists(path)) return dependencies;
        
        // Using standard C file reading for efficiency
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) return dependencies;
        
        std::fseek(f, 0, SEEK_END);
        long size = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        
        std::string content(size, '\0');
        std::fread(&content[0], 1, size, f);
        std::fclose(f);
        
        size_t i = 0;
        // Skip the target rule (e.g., "build/main.o: ")
        while (i < content.length() && content[i] != ':') i++;
        if (i < content.length()) i++; // skip the colon
        
        std::string current_path;
        while (i < content.length()) {
            if (content[i] == ' ' || content[i] == '\n' || content[i] == '\r') {
                if (!current_path.empty()) {
                    if (current_path != "\\") dependencies.push_back(current_path);
                    current_path.clear();
                }
            } else if (content[i] == '\\' && i + 1 < content.length() && content[i+1] == ' ') {
                // GCC escapes spaces in paths with backslash
                current_path += ' ';
                i++;
            } else {
                current_path += content[i];
            }
            i++;
        }
        
        if (!current_path.empty() && current_path != "\\") {
            dependencies.push_back(current_path);
        }
        
        return dependencies;
    }
} // namespace deps

inline int needs_rebuild(const std::string& output_path, const std::vector<std::string>& input_paths) {
    std::error_code ec;
    auto out_time = std::filesystem::last_write_time(output_path, ec);
    if (ec) {
        if (ec == std::errc::no_such_file_or_directory) return 1; // Output doesn't exist, rebuild
        log(LogLevel::ERROR, "Could not stat %s: %s", output_path.c_str(), ec.message().c_str());
        return -1;
    }
    
    // Check base inputs
    for (const auto& input_path : input_paths) {
        auto in_time = std::filesystem::last_write_time(input_path, ec);
        if (ec) {
            log(LogLevel::ERROR, "Could not stat %s: %s", input_path.c_str(), ec.message().c_str());
            return -1;
        }
        if (in_time > out_time) return 1;
    }
    
    // Check dynamic header dependencies from .d file
    std::string d_file = output_path + ".d";
    if (file_exists(d_file)) {
        // Variable renamed to 'parsed_deps' to prevent shadowing the 'deps' namespace
        std::vector<std::string> parsed_deps = deps::parse_gcc_d_file(d_file);
        for (const auto& dep : parsed_deps) {
            auto dep_time = std::filesystem::last_write_time(dep, ec);
            if (ec) continue; // Dependency might not exist anymore, skip
            if (dep_time > out_time) return 1;
        }
    }
    
    return 0;
}

// --- String Operations ---

inline std::string_view sv_chop_by_delim(std::string_view& sv, char delim) {
    size_t i = sv.find(delim);
    if (i == std::string_view::npos) {
        std::string_view res = sv;
        sv = std::string_view();
        return res;
    }
    std::string_view res = sv.substr(0, i);
    sv.remove_prefix(i + 1);
    return res;
}

inline std::string_view sv_trim_left(std::string_view sv) {
    size_t i = 0;
    while (i < sv.length() && std::isspace(sv[i])) i++;
    return sv.substr(i);
}

inline std::string_view sv_trim_right(std::string_view sv) {
    size_t i = sv.length();
    while (i > 0 && std::isspace(sv[i - 1])) i--;
    return sv.substr(0, i);
}

inline std::string_view sv_trim(std::string_view sv) {
    return sv_trim_right(sv_trim_left(sv));
}

// --- Process Management ---

#ifdef _WIN32
using Proc = HANDLE;
inline const Proc INVALID_PROC = INVALID_HANDLE_VALUE;
using Fd = HANDLE;
inline const Fd INVALID_FD = INVALID_HANDLE_VALUE;
#else
using Proc = int;
inline const Proc INVALID_PROC = -1;
using Fd = int;
inline const Fd INVALID_FD = -1;
#endif

inline Fd fd_open_for_read(const std::string& path) {
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    Fd res = CreateFileA(path.c_str(), GENERIC_READ, 0, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
    if (res == INVALID_HANDLE_VALUE) {
        log(LogLevel::ERROR, "Could not open file %s: %s", path.c_str(), win32_error_message(GetLastError()).c_str());
    }
    return res;
#else
    Fd res = open(path.c_str(), O_RDONLY);
    if (res < 0) log(LogLevel::ERROR, "Could not open file %s: %s", path.c_str(), std::strerror(errno));
    return res;
#endif
}

inline Fd fd_open_for_write(const std::string& path) {
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    Fd res = CreateFileA(path.c_str(), GENERIC_WRITE, 0, &sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (res == INVALID_HANDLE_VALUE) {
        log(LogLevel::ERROR, "Could not open file %s: %s", path.c_str(), win32_error_message(GetLastError()).c_str());
    }
    return res;
#else
    Fd res = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (res < 0) log(LogLevel::ERROR, "could not open file %s: %s", path.c_str(), std::strerror(errno));
    return res;
#endif
}

inline void fd_close(Fd fd) {
    if (fd == INVALID_FD) return;
#ifdef _WIN32
    CloseHandle(fd);
#else
    close(fd);
#endif
}

inline bool create_pipe(Fd* read_fd, Fd* write_fd) {
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(read_fd, write_fd, &sa, 0)) {
        log(LogLevel::ERROR, "Could not create pipe: %s", win32_error_message(GetLastError()).c_str());
        return false;
    }
    return true;
#else
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        log(LogLevel::ERROR, "Could not create pipe: %s", std::strerror(errno));
        return false;
    }
    *read_fd = pipefd[0];
    *write_fd = pipefd[1];
    return true;
#endif
}

inline std::string read_all_from_fd(Fd fd) {
    std::string result;
    char buffer[4096];
#ifdef _WIN32
    DWORD bytes_read;
    while (ReadFile(fd, buffer, sizeof(buffer), &bytes_read, NULL) && bytes_read > 0) {
        result.append(buffer, bytes_read);
    }
#else
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        result.append(buffer, bytes_read);
    }
#endif
    return result;
}

inline bool proc_wait(Proc proc) {
    if (proc == INVALID_PROC) return false;
#ifdef _WIN32
    if (WaitForSingleObject(proc, INFINITE) == WAIT_FAILED) {
        log(LogLevel::ERROR, "could not wait on child process: %s", win32_error_message(GetLastError()).c_str());
        return false;
    }
    DWORD exit_status;
    if (!GetExitCodeProcess(proc, &exit_status)) {
        log(LogLevel::ERROR, "could not get process exit code: %s", win32_error_message(GetLastError()).c_str());
        return false;
    }
    CloseHandle(proc);
    if (exit_status != 0) {
        log(LogLevel::ERROR, "command exited with exit code %lu", exit_status);
        return false;
    }
    return true;
#else
    for (;;) {
        int wstatus = 0;
        if (waitpid(proc, &wstatus, 0) < 0) {
            log(LogLevel::ERROR, "could not wait on command (pid %d): %s", proc, std::strerror(errno));
            return false;
        }
        if (WIFEXITED(wstatus)) {
            int exit_status = WEXITSTATUS(wstatus);
            if (exit_status != 0) {
                log(LogLevel::ERROR, "command exited with exit code %d", exit_status);
                return false;
            }
            break;
        }
        if (WIFSIGNALED(wstatus)) {
            log(LogLevel::ERROR, "command process was terminated by %s", strsignal(WTERMSIG(wstatus)));
            return false;
        }
    }
    return true;
#endif
}

inline bool procs_wait(const std::vector<Proc>& procs) {
    bool success = true;
    for (auto p : procs) success = proc_wait(p) && success;
    return success;
}

// --- Shlex Subsystem ---

    inline std::vector<std::string> shlex_split(std::string_view source) {
        std::vector<std::string> result;
        const char* point = source.data();
        const char* end = source.data() + source.size();
        
        while (true) {
            while (point < end && std::isspace(*point)) point++;
            if (point >= end) break;
            
            std::string current;
            char strlit = 0;
            while (point < end) {
                if (strlit == '\'') {
                    if (*point == '\'') {
                        strlit = 0;
                    } else {
                        current += *point;
                    }
                    point++;
                } else if (strlit == '"') {
                    if (*point == '"') {
                        strlit = 0;
                        point++;
                    } else if (*point == '\\') {
                        point++;
                        if (point >= end) {
                            current += '\\';
                            break;
                        }
                        if (*point == '$' || *point == '`' || *point == '\\' || *point == '\n' || *point == '"') {
                            current += *point;
                        } else {
                            current += '\\';
                            current += *point;
                        }
                        point++;
                    } else {
                        current += *point;
                        point++;
                    }
                } else {
                    if (*point == '"' || *point == '\'') {
                        strlit = *point;
                        point++;
                    } else if (*point == '\\') {
                        point++;
                        if (point < end) {
                            current += *point;
                            point++;
                        }
                    } else if (std::isspace(*point)) {
                        break;
                    } else {
                        current += *point;
                        point++;
                    }
                }
            }
            result.push_back(current);
        }
        return result;
    }

    inline std::string shlex_join(const std::vector<std::string>& strings) {
        std::string result;
        for (size_t i = 0; i < strings.size(); ++i) {
            if (i > 0) result += ' ';
            const std::string& str = strings[i];
            
            if (str.empty()) {
                result += "''";
                continue;
            }
            
            bool safe = true;
            for (char c : str) {
                if (!std::isalnum(c) && strchr("_@%+=:,./-", c) == nullptr) {
                    safe = false;
                    break;
                }
            }
            
            if (safe) {
                result += str;
            } else {
                result += '\'';
                for (char c : str) {
                    if (c == '\'') {
                        result += "'\"'\"'";
                    } else {
                        result += c;
                    }
                }
                result += '\'';
            }
        }
        return result;
    }

// --- Command Execution Subsystem ---
struct CmdRedirect {
    Fd* fdin = nullptr;
    Fd* fdout = nullptr;
    Fd* fderr = nullptr;
};

class Cmd {
public:
    std::vector<std::string> items;

    void append(const std::string& arg) { items.push_back(arg); }

    template<typename... Args>
    void append(const std::string& arg, Args... args) {
        items.push_back(arg);
        if constexpr (sizeof...(args) > 0) {
            append(args...);
        }
    }

    void extend(const Cmd& other) {
        items.insert(items.end(), other.items.begin(), other.items.end());
    }

    void append_shlex(const std::string& command_line) {
        std::vector<std::string> args = shlex_split(command_line);
        for (const auto& arg : args) {
            append(arg);
        }
    }

    bool pipe_to(Cmd& next_cmd) {
        Fd read_fd, write_fd;
        if (!create_pipe(&read_fd, &write_fd)) return false;
        
        CmdRedirect out_redir = {};
        out_redir.fdout = &write_fd;
        Proc p1 = run_async(out_redir);
        fd_close(write_fd);
        
        CmdRedirect in_redir = {};
        in_redir.fdin = &read_fd;
        Proc p2 = next_cmd.run_async(in_redir);
        fd_close(read_fd);
        
        reset();
        next_cmd.reset();
        return proc_wait(p1) && proc_wait(p2);
    }

    void reset() { items.clear(); }

    std::string render() const {
        std::string res;
        for (size_t i = 0; i < items.size(); ++i) {
            if (i > 0) res += " ";
            // Both Windows and POSIX safely accept double quotes for paths with spaces
            if (items[i].find(' ') != std::string::npos) res += "\"" + items[i] + "\"";
            else res += items[i];
        }
        return res;
    }

    Proc run_async(CmdRedirect redirect = {}) const {
        if (items.empty()) {
            log(LogLevel::ERROR, "Could not run empty command");
            return INVALID_PROC;
        }
        std::string rendered = render();
        
        // --- Response File Subsystem ---
        // If command exceeds 30,000 chars, transparently use a response file.
        std::string rsp_file;
        if (rendered.length() > 30000) {
            static std::atomic<int> rsp_counter{0};
            rsp_file = "nob_cmd_" + std::to_string(rsp_counter.fetch_add(1)) + ".rsp";
            std::ofstream out(rsp_file);
            for (size_t i = 1; i < items.size(); ++i) {
                if (items[i].find(' ') != std::string::npos) out << "\"" << items[i] << "\" ";
                else out << items[i] << " ";
            }
            out.close();
            
            // Re-render with just the compiler and the response file
            rendered = items[0] + " @" + rsp_file;
            log(LogLevel::WARNING, "Command exceeded length limits. Using response file: %s", rsp_file.c_str());
        }
        log(LogLevel::INFO, "CMD: %s", rendered.c_str());

#ifdef _WIN32
        STARTUPINFOA siStartInfo;
        ZeroMemory(&siStartInfo, sizeof(siStartInfo));
        siStartInfo.cb = sizeof(STARTUPINFOA);
        siStartInfo.hStdError  = redirect.fderr ? *redirect.fderr : GetStdHandle(STD_ERROR_HANDLE);
        siStartInfo.hStdOutput = redirect.fdout ? *redirect.fdout : GetStdHandle(STD_OUTPUT_HANDLE);
        siStartInfo.hStdInput  = redirect.fdin  ? *redirect.fdin  : GetStdHandle(STD_INPUT_HANDLE);
        siStartInfo.dwFlags   |= STARTF_USESTDHANDLES;

        PROCESS_INFORMATION piProcInfo;
        ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

        // CreateProcessA requires a mutable string buffer
        std::string cmd_str = rendered; 
        if (!CreateProcessA(NULL, cmd_str.data(), NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piProcInfo)) {
            log(LogLevel::ERROR, "Could not create child process: %s", win32_error_message(GetLastError()).c_str());
            return INVALID_PROC;
        }
        CloseHandle(piProcInfo.hThread);
        return piProcInfo.hProcess;
#else
        pid_t cpid = fork();
        if (cpid < 0) {
            log(LogLevel::ERROR, "Could not fork child process: %s", std::strerror(errno));
            return INVALID_PROC;
        }
        if (cpid == 0) {
            if (redirect.fdin && dup2(*redirect.fdin, STDIN_FILENO) < 0) {
                log(LogLevel::ERROR, "Could not setup stdin: %s", std::strerror(errno)); std::exit(1);
            }
            if (redirect.fdout && dup2(*redirect.fdout, STDOUT_FILENO) < 0) {
                log(LogLevel::ERROR, "Could not setup stdout: %s", std::strerror(errno)); std::exit(1);
            }
            if (redirect.fderr && dup2(*redirect.fderr, STDERR_FILENO) < 0) {
                log(LogLevel::ERROR, "Could not setup stderr: %s", std::strerror(errno)); std::exit(1);
            }
            
            std::vector<char*> args;
            if (!rsp_file.empty()) {
                args.push_back(const_cast<char*>(items[0].data()));
                std::string rsp_arg = "@" + rsp_file;
                args.push_back(const_cast<char*>(rsp_arg.data()));
            } else {
                for (auto& item : items) args.push_back(const_cast<char*>(item.data()));
            }
            args.push_back(nullptr);
            
            if (execvp(args[0], args.data()) < 0) {
                log(LogLevel::ERROR, "Could not exec child process: %s", std::strerror(errno));
                std::exit(1);
            }
            NOB_UNREACHABLE("execvp returned");
        }
        return cpid;
#endif
    }

    Proc run_async_and_reset(CmdRedirect redirect = {}) {
        Proc p = run_async(redirect);
        reset();
        return p;
    }

    bool run_sync(CmdRedirect redirect = {}) const {
        Proc p = run_async(redirect);
        if (p == INVALID_PROC) return false;
        return proc_wait(p);
    }

    bool run_sync_and_reset(CmdRedirect redirect = {}) {
        bool result = run_sync(redirect);
        reset();
        if (redirect.fdin)  { fd_close(*redirect.fdin);  *redirect.fdin = INVALID_FD; }
        if (redirect.fdout) { fd_close(*redirect.fdout); *redirect.fdout = INVALID_FD; }
        if (redirect.fderr) { fd_close(*redirect.fderr); *redirect.fderr = INVALID_FD; }
        return result;
    }
};

// --- Fetch / Dependency Subsystem ---

namespace fetch {
    
    // Internal helper to check if a command exists in the system PATH
    inline bool command_exists(const std::string& cmd_name) {
#ifdef _WIN32
        Cmd cmd; cmd.append("where", cmd_name);
#else
        Cmd cmd; cmd.append("command", "-v", cmd_name);
#endif
        // Check command availability using system call
        return std::system(
#ifdef _WIN32
            ("where " + cmd_name + " >nul 2>nul").c_str()
#else
            ("command -v " + cmd_name + " >/dev/null 2>&1").c_str()
#endif
        ) == 0;
    }

    // Clones a git repository and ensures it is locked to a specific ref (commit hash, tag, or branch)
    inline bool git_repo(const std::string& url, const std::string& dest_dir, const std::string& ref = "") {
        // Check for .git directory, not just the folder, to prevent corrupted partial clones
        bool exists = file_exists(dest_dir + "/.git");
        
        if (!exists) {
            log(LogLevel::INFO, "Fetching dependency from %s ...", url.c_str());
            std::filesystem::path dest_path(dest_dir);
            if (dest_path.has_parent_path()) {
                mkdir_if_not_exists(dest_path.parent_path().string());
            }
            Cmd cmd;
            // If we have a specific ref, we cannot reliably shallow clone (server must support it)
            // So we do a standard clone.
            cmd.append("git", "clone", url, dest_dir);
            if (!cmd.run_sync_and_reset()) {
                log(LogLevel::ERROR, "Failed to clone repository: %s", url.c_str());
                return false;
            }
        }

        if (!ref.empty()) {
            Cmd cmd;
            cmd.append("git", "-C", dest_dir, "checkout", ref);
            
            // Suppress the "detached HEAD" warning by redirecting stderr if needed,
            // but running sync is fine. We want to ensure it succeeded.
            if (!cmd.run_sync_and_reset()) {
                log(LogLevel::ERROR, "Failed to checkout ref '%s' in repository '%s'", ref.c_str(), dest_dir.c_str());
                return false;
            }
            log(LogLevel::INFO, "Dependency `%s` is locked to ref `%s`", dest_dir.c_str(), ref.c_str());
        } else if (exists) {
            log(LogLevel::INFO, "Dependency `%s` already exists.", dest_dir.c_str());
        }
        return true;
    }

    // Downloads a single file with dynamic fallback between curl and wget
    inline bool web_file(const std::string& url, const std::string& dest_file) {
        if (file_exists(dest_file)) {
            log(LogLevel::INFO, "File `%s` already exists.", dest_file.c_str());
            return true;
        }
        log(LogLevel::INFO, "Downloading %s -> %s", url.c_str(), dest_file.c_str());
        
        std::filesystem::path dest_path(dest_file);
        if (dest_path.has_parent_path()) {
            mkdir_if_not_exists(dest_path.parent_path().string());
        }

        Cmd cmd;
        if (command_exists("curl")) {
            cmd.append("curl", "-sL", "-o", dest_file, url);
        } else if (command_exists("wget")) {
            cmd.append("wget", "-qO", dest_file, url);
        } else {
            log(LogLevel::ERROR, "Neither 'curl' nor 'wget' found on the system. Cannot download %s", url.c_str());
            return false;
        }

        if (!cmd.run_sync_and_reset()) {
            log(LogLevel::ERROR, "Failed to download file: %s", url.c_str());
            // Clean up the potentially corrupted partial download
            delete_file(dest_file);
            return false;
        }
        return true;
    }

} // namespace fetch

// --- Job Queue Subsystem ---

class JobQueue {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<bool()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::condition_variable wait_condition; // Signals wait_all()
    int active_tasks = 0;                   // Tracks tasks actively being executed
    bool terminate = false;
    bool build_failed = false;

public:
    JobQueue(int num_threads) {
        for (int i = 0; i < num_threads; ++i) {
            workers.emplace_back([this] {
                for (;;) {
                    std::function<bool()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this] { 
                            return this->terminate || !this->tasks.empty(); 
                        });
                        
                        if (this->terminate && this->tasks.empty()) return;
                        
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                        
                        // If a previous task failed, discard remaining tasks but decrement counter
                        if (this->build_failed) {
                            this->active_tasks--;
                            this->wait_condition.notify_all();
                            continue;
                        }
                    }
                    
                    // Execute task outside the lock so other threads aren't blocked
                    bool success = task();
                    
                    {
                        std::lock_guard<std::mutex> lock(this->queue_mutex);
                        if (!success) this->build_failed = true;
                        this->active_tasks--;
                    }
                    this->wait_condition.notify_all(); // Notify wait_all()
                }
            });
        }
    }

    ~JobQueue() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            terminate = true;
        }
        condition.notify_all();
        for (std::thread& worker : workers) worker.join();
    }

    void push(std::function<bool()> task) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            tasks.push(std::move(task));
            active_tasks++; // Increment active task counter
        }
        condition.notify_one();
    }

    bool wait_all() {
        std::unique_lock<std::mutex> lock(queue_mutex);
        // Safely sleep the main thread until active_tasks hits exactly 0
        wait_condition.wait(lock, [this] { return active_tasks == 0; });
        return !build_failed;
    }
};

// --- Temporary Bump Allocator (Thread-Safe) ---

namespace temp {
    inline constexpr std::size_t CAPACITY = 8 * 1024 * 1024;
    
    // Thread-local for parallel build safety
    thread_local inline char buffer[CAPACITY] = {0};
    thread_local inline std::size_t size = 0;

    inline void* alloc(std::size_t req) {
        if (size + req > CAPACITY) return nullptr;
        void* res = &buffer[size];
        size += req;
        return res;
    }

    inline void reset() { size = 0; }
    inline std::size_t save() { return size; }
    inline void rewind(std::size_t checkpoint) { size = checkpoint; }

    inline char* sprintf(const char* format, ...) {
        std::va_list args;
        va_start(args, format);
        int n = std::vsnprintf(nullptr, 0, format, args);
        va_end(args);
        if (n < 0) return nullptr;
        char* result = static_cast<char*>(alloc(n + 1));
        assert(result != nullptr && "Increase temp::CAPACITY");
        va_start(args, format);
        std::vsnprintf(result, n + 1, format, args);
        va_end(args);
        return result;
    }
}

// --- Go Rebuild Urself Technology ---

#ifndef NOB_REBUILD_URSELF
#  if _WIN32
#    if defined(__GNUC__)
#       define NOB_REBUILD_URSELF(cmd, binary_path, source_path) cmd.append("g++", "-std=c++17", "-o", binary_path, source_path)
#    elif defined(__clang__)
#       define NOB_REBUILD_URSELF(cmd, binary_path, source_path) cmd.append("clang++", "-std=c++17", "-o", binary_path, source_path)
#    elif defined(_MSC_VER)
#       define NOB_REBUILD_URSELF(cmd, binary_path, source_path) cmd.append("cl.exe", "/std:c++17", std::string("/Fe:") + binary_path, source_path)
#    endif
#  else
#    define NOB_REBUILD_URSELF(cmd, binary_path, source_path) cmd.append("c++", "-std=c++17", "-o", binary_path, source_path)
#  endif
#endif


// --- Metaprogramming & Compiler Configuration ---

    // PROBES THE SYSTEM FOR A WORKING C++ COMPILER.
    // RETURNS THE NAME OF THE COMPILER CHOSEN BY THE USER.
    inline std::string find_compilers() {
        while (true) {
            std::vector<std::string> candidates = {"g++", "clang++", "cl.exe", "c++"};
            std::vector<std::string> valid;
            
            nob::log(nob::LogLevel::INFO, "No compiler configured. Scanning system...");

#ifdef _WIN32
            nob::Cmd vswhere;
            vswhere.append("cmd.exe", "/c", "\"%ProgramFiles(x86)%\\Microsoft Visual Studio\\Installer\\vswhere.exe\" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath");
            nob::Fd r, w;
            if (nob::create_pipe(&r, &w)) {
                nob::CmdRedirect redir = {};
                redir.fdout = &w;
                nob::Proc p = vswhere.run_async(redir);
                nob::fd_close(w);
                std::string out = nob::read_all_from_fd(r);
                nob::fd_close(r);
                if (nob::proc_wait(p) && !out.empty()) {
                    out.erase(out.find_last_not_of(" \n\r\t") + 1);
                    std::string vcvars = out + "\\VC\\Auxiliary\\Build\\vcvarsall.bat";
                    if (nob::file_exists(vcvars)) {
                        nob::log(nob::LogLevel::INFO, "Found MSVC Installation via vswhere: %s", out.c_str());
                        valid.push_back("MSVC_AUTO:" + vcvars);
                    }
                }
            }
#endif
            
            for (const auto& comp : candidates) {
                std::string probe_cmd;
#ifdef _WIN32
                if (comp == "cl.exe") probe_cmd = "cl.exe /? > nul 2>&1";
                else probe_cmd = comp + " --version > nul 2>&1";
#else
                probe_cmd = comp + " --version > /dev/null 2>&1";
#endif
                int res = std::system(probe_cmd.c_str());
                if (res == 0) {
                    valid.push_back(comp);
                }
            }
            
            if (valid.empty()) {
                nob::log(nob::LogLevel::ERROR, "No C++ compilers found in PATH!");
                
#if _WIN32
                std::cout << "[INFO] Windows detected. To install a compiler, you can run this in PowerShell:\n";
                std::cout << "       winget install MSYS2.MinGW-w64.GCC\n";
#elif __APPLE__
                std::cout << "[INFO] macOS detected. To install a compiler, run this in your terminal:\n";
                std::cout << "       xcode-select --install\n";
#elif __linux__
                std::cout << "[INFO] Linux detected. To install a compiler, run one of the following depending on your distro:\n";
                std::cout << "       Debian/Ubuntu: sudo apt install build-essential\n";
                std::cout << "       Fedora:        sudo dnf install gcc-c++\n";
                std::cout << "       Arch:          sudo pacman -S gcc\n";
#else
                std::cout << "[INFO] Please install a C++ compiler (like g++ or clang++) to proceed.\n";
#endif
                
                std::cout << "\nWould you like to manually enter a compiler command/path? [Y/n]: ";
                std::string ans;
                if (!(std::cin >> ans) || std::cin.eof()) std::exit(1);
                if (ans != "n" && ans != "N") {
                    std::cout << "Compiler command: ";
                    std::string custom;
                    if (!(std::cin >> custom) || std::cin.eof()) std::exit(1);
                    return custom;
                }
                nob::log(nob::LogLevel::ERROR, "Cannot proceed without a compiler. Aborting.");
                std::exit(1);
            }
            
            if (valid.size() == 1) {
                nob::log(nob::LogLevel::INFO, "Found exactly one compiler: %s", valid[0].c_str());
                return valid[0];
            }
            
            std::cout << "[INFO] Found multiple compilers. Please select one:\n";
            for (size_t i = 0; i < valid.size(); ++i) {
                std::cout << "  " << (i + 1) << ". " << valid[i] << "\n";
            }
            std::cout << "  0. Rescan\n";
            std::cout << "Choice: ";
            
            int choice = 0;
            if (!(std::cin >> choice)) {
                if (std::cin.eof()) std::exit(1);
                std::cin.clear();
                std::cin.ignore(10000, '\n');
            }
            
            if (choice == 0) continue;
            
            if (choice >= 1 && choice <= (int)valid.size()) {
                return valid[choice - 1];
            }
            nob::log(nob::LogLevel::ERROR, "Invalid choice, rescanning.");
        }
    }

    // READS SOURCE FILE. IF `// NOB_COMPILER:` IS FOUND, IT VALIDATES IT.
    // IF INVALID OR MISSING, IT SCANS FOR COMPILERS AND EDITS THE SOURCE FILE!
    inline std::string ensure_compiler_configured(const char* source_file) {
        std::ifstream in(source_file);
        if (!in.is_open()) {
            nob::log(nob::LogLevel::ERROR, "Could not open %s for self-analysis.", source_file);
            std::exit(1);
        }
        
        std::vector<std::string> lines;
        std::string line;
        std::string current_compiler = "";
        
        bool has_compiler_line = false;
        if (std::getline(in, line)) {
            lines.push_back(line);
            if (line.find("// NOB_COMPILER: ") == 0) {
                has_compiler_line = true;
                current_compiler = line.substr(17);
                current_compiler.erase(current_compiler.find_last_not_of(" \n\r\t") + 1);
                
                if (current_compiler.rfind("MSVC_AUTO:", 0) != 0) {
                    std::string probe_cmd;
#ifdef _WIN32
                    if (current_compiler.find("cl") != std::string::npos) probe_cmd = current_compiler + " /? > nul 2>&1";
                    else probe_cmd = current_compiler + " --version > nul 2>&1";
#else
                    probe_cmd = current_compiler + " --version > /dev/null 2>&1";
#endif
                    if (std::system(probe_cmd.c_str()) != 0) {
                        nob::log(nob::LogLevel::WARNING, "Saved compiler '%s' is no longer valid.", current_compiler.c_str());
                        current_compiler = "";
                    }
                }
            }
        }
        
        while (std::getline(in, line)) {
            lines.push_back(line);
        }
        in.close();
        
        if (!current_compiler.empty()) {
            return current_compiler;
        }
        
        std::string chosen = find_compilers();
        
        std::ofstream out(source_file);
        if (!out.is_open()) {
            nob::log(nob::LogLevel::ERROR, "Could not open %s for self-editing.", source_file);
            std::exit(1);
        }
        
        out << "// NOB_COMPILER: " << chosen << "\n";
        
        size_t start_idx = has_compiler_line ? 1 : 0;
        for (size_t i = start_idx; i < lines.size(); ++i) {
            out << lines[i] << "\n";
        }
        out.close();
        
        nob::log(nob::LogLevel::INFO, "Successfully self-edited %s with chosen compiler.", source_file);
        return chosen;
    }

    inline bool fetch_file(const std::string& url, const std::string& output_path) {
        nob::log(nob::LogLevel::INFO, "Fetching %s...", output_path.c_str());
        nob::Cmd cmd;
        cmd.append("curl", "-sL", url, "-o", output_path);
        return cmd.run_sync_and_reset();
    }

    inline void go_rebuild_urself(const char* source_file, int argc, char** argv) {
        const char* nob_hpp = "nob.hpp";
        
        if (!nob::file_exists(source_file)) {
            nob::log(nob::LogLevel::WARNING, "Source file '%s' not found.", source_file);
            std::cout << "Would you like me to fetch the latest NOB templates from GitHub to bootstrap this directory? [Y/n]: ";
            std::string ans;
            std::cin >> ans;
            if (ans != "n" && ans != "N") {
                std::string base_url = "https://raw.githubusercontent.com/altescape/nob/main/";
                if (!nob::fetch_file(base_url + "nob.cpp", "nob.cpp") ||
                    !nob::fetch_file(base_url + "nob.hpp", "nob.hpp") ||
                    !nob::fetch_file(base_url + "TECHNICAL_DOCUMENT.md", "TECHNICAL_DOCUMENT.md") ||
                    !nob::fetch_file(base_url + "main.cpp", "main.cpp")) {
                    nob::log(nob::LogLevel::ERROR, "Failed to fetch files from GitHub. Check your internet connection or URL.");
                    std::exit(1);
                }
                nob::log(nob::LogLevel::INFO, "Successfully bootstrapped from GitHub!");
            } else {
                std::exit(1);
            }
        }

        std::string exe_path = argv[0];
#ifdef _WIN32
        if (exe_path.length() < 4 || exe_path.substr(exe_path.length() - 4) != ".exe") {
            exe_path += ".exe";
        }
#endif
        
        int needs = nob::needs_rebuild(exe_path, {source_file, nob_hpp});
        if (needs < 0) {
            nob::log(nob::LogLevel::ERROR, "Could not stat files for self-rebuild.");
            std::exit(1);
        }
        
        if (needs == 0) return;
        
        while (true) {
            nob::log(nob::LogLevel::INFO, "nob system files changed. Rebuilding...");
            
            std::string compiler = ensure_compiler_configured(source_file);
            
            nob::Cmd cmd;
            if (compiler.rfind("MSVC_AUTO:", 0) == 0) {
                std::string vcvars = compiler.substr(10);
                cmd.append_shlex("cmd.exe /c \"\"" + vcvars + "\" x64 && cl.exe " + std::string(source_file) + " /Fe:" + exe_path + " /EHsc /nologo\"");
            } else {
                cmd.append(compiler);
                if (compiler.find("cl") != std::string::npos) {
                    cmd.append(source_file, "/Fe:" + exe_path, "/EHsc", "/nologo");
                } else {
                    cmd.append("-std=c++17", "-Wall", "-Wextra", source_file, "-o", exe_path);
                }
            }
            
            std::string old_exe_path = exe_path + ".old";
            if (nob::file_exists(old_exe_path)) nob::delete_file(old_exe_path);
            if (!nob::rename(exe_path, old_exe_path)) std::exit(1);
            
            if (!cmd.run_sync_and_reset()) {
                nob::rename(old_exe_path, exe_path);
                nob::log(nob::LogLevel::ERROR, "Self-rebuild failed! The saved compiler might be broken or you have syntax errors.");
                std::cout << "Would you like me to wipe the compiler config and try a different compiler? [Y/n]: ";
                std::string ans;
                if (!(std::cin >> ans) || std::cin.eof()) std::exit(1);
                if (ans != "n" && ans != "N") {
                    std::string chosen = find_compilers();
                    std::ifstream in(source_file);
                    std::vector<std::string> lines;
                    std::string line;
                    while (std::getline(in, line)) {
                        if (line.find("// NOB_COMPILER:") != 0) lines.push_back(line);
                    }
                    in.close();
                    std::ofstream out(source_file);
                    out << "// NOB_COMPILER: " << chosen << "\n";
                    for (const auto& l : lines) out << l << "\n";
                    out.close();
                    continue;
                }
                std::exit(1);
            }
            break;
        }
        
        nob::log(nob::LogLevel::INFO, "Self-rebuild successful! Restarting...");
        
        nob::Cmd restart_cmd;
        restart_cmd.append(exe_path);
        for (int i = 1; i < argc; ++i) {
            restart_cmd.append(argv[i]);
        }
        
        if (!restart_cmd.run_sync_and_reset()) std::exit(1);
        std::exit(0);
    }

#define NOB_GO_REBUILD_URSELF(argc, argv) nob::go_rebuild_urself(__FILE__, argc, argv)

// --- CLI Argument Parser Subsystem ---

struct CliArgs {
    bool help = false;
    bool clean = false;
    bool dry_run = false;
    int64_t jobs = 1;
    std::string mode = "debug"; // debug or release
    std::string sysroot;
    std::string prefix;
    std::string toolchain;
    bool shared = false;
    std::vector<std::string> positional;

    static CliArgs parse(int argc, char** argv) {
        flags::Parser parser("nob");
        return parse(parser, argc, argv);
    }

    static CliArgs parse(flags::Parser& parser, int argc, char** argv) {
        CliArgs args;
        parser.add_bool_var(&args.help, "help", "Print this help message and exit");
        parser.add_bool_var(&args.help, "h", ""); 
        parser.add_bool_var(&args.clean, "clean", "Remove build directory and exit");
        parser.add_bool_var(&args.dry_run, "dry-run", "Print commands without executing them");
        parser.add_bool_var(&args.dry_run, "n", "");
        parser.add_int64_var(&args.jobs, "j", "Run <N> jobs in parallel");
        parser.add_string_var(&args.mode, "mode", "Build mode: debug (default) or release");
        parser.add_string_var(&args.sysroot, "sysroot", "Set the logical root directory for headers and libraries");
        parser.add_string_var(&args.prefix, "prefix", "Set the installation prefix (e.g., /usr/local)");
        parser.add_string_var(&args.toolchain, "toolchain", "Set cross-compilation toolchain prefix (e.g., aarch64-linux-gnu-)");
        parser.add_bool_var(&args.shared, "shared", "Build a shared library (DLL)");
        parser.add_bool_var(&args.shared, "s", "");
        
        if (!parser.parse(argc, argv)) {
            parser.print_error();
            parser.print_help();
            std::exit(1);
        }
        
        if (args.jobs < 1) args.jobs = 1;
        args.positional = parser.rest_args();
        return args;
    }

    void print_help(const char* binary_name) const {
        std::cout << "Usage: " << binary_name << " [OPTIONS] [TARGETS...]\n\n"
                  << "Options:\n"
                  << "  -h, --help               Print this help message and exit\n"
                  << "  -j <N>                   Run <N> jobs in parallel (default: 1)\n"
                  << "  --clean                  Remove build directory and exit\n"
                  << "  --dry-run, -n            Print commands without executing them\n"
                  << "  --mode <mode>            Build mode: debug (default) or release\n"
                  << "  --sysroot <dir>          Set the logical root directory for headers and libraries\n"
                  << "  --prefix <dir>           Set the installation prefix (e.g., /usr/local)\n"
                  << "  --toolchain <prefix>     Set cross-compilation toolchain prefix (e.g., aarch64-linux-gnu-)\n"
                  << "  -s, --shared             Build a shared library (DLL/SO) instead of an executable\n";
    }
};

// --- Toolchain & Compilation Subsystem ---

class Toolchain {
public:
    std::string cxx = "c++";
    std::string cc = "cc";
    std::string ar = "ar";
    std::string target_prefix;
    std::string sysroot;
    std::string mode = "debug"; // debug or release
    bool is_msvc = false;

    // Initialize from parsed CLI args and Environment Variables
    void configure(const CliArgs& args) {
        // 1. Environment Variable Overrides
        const char* env_cxx = std::getenv("CXX");
        const char* env_cc = std::getenv("CC");
        
        if (env_cxx) cxx = env_cxx;
        if (env_cc) cc = env_cc;

        // 2. CLI Args Overrides
        if (!args.toolchain.empty()) target_prefix = args.toolchain;
        if (!args.sysroot.empty()) sysroot = args.sysroot;
        if (!args.mode.empty()) mode = args.mode;

#ifdef _MSC_VER
        is_msvc = true;
        // If MSVC and user didn't specify a compiler via ENV, default to MSVC tools
        if (!env_cxx) cxx = "cl.exe";
        if (!env_cc) cc = "cl.exe";
        ar = "lib.exe";
#endif
    }

    std::string get_cxx() const { return target_prefix + cxx; }
    std::string get_cc() const  { return target_prefix + cc; }
    std::string get_ar() const  { return target_prefix + ar; }

    void apply_sysroot(Cmd& cmd) const {
        if (!sysroot.empty()) {
            if (is_msvc) {
                // MSVC doesn't have a direct --sysroot equivalent, relies on /I and /LIBPATH
                log(LogLevel::WARNING, "Sysroot flag is not natively supported on MSVC.");
            } else {
                cmd.append("--sysroot=" + sysroot);
            }
        }
    }

    void apply_mode(Cmd& cmd) const {
        if (is_msvc) {
            // MSVC Flags
            if (mode == "debug") {
                cmd.append("/Zi", "/Od", "/MDd", "/DDEBUG");
            } else {
                cmd.append("/O2", "/MD", "/GL", "/DNDEBUG");
            }
        } else {
            // GCC/Clang Flags
            if (mode == "debug") {
                cmd.append("-g", "-O0", "-DDEBUG");
            } else {
                cmd.append("-O3", "-flto=auto", "-DNDEBUG");
            }
        }
    }

    // High-level compilation routines
    void build_object(Cmd& cmd, const std::string& source, const std::string& output) const {
        cmd.append(get_cxx());
        apply_mode(cmd); // Inject debug/release flags
        apply_sysroot(cmd);
        
        if (is_msvc) {
            cmd.append("/c", source, "/Fo:" + output);
            // MSVC tracking requires parsing stdout of /showIncludes, which is complex.
            // For now, we skip .d files on MSVC.
        } else {
            // GCC/Clang: generate .d file next to the .o file for dependency tracking
            std::string d_file = output + ".d";
            cmd.append("-MMD", "-MF", d_file);
            cmd.append("-c", source, "-o", output, "-fPIC");
        }
    }

    void build_shared_lib(Cmd& cmd, const std::vector<std::string>& objects, const std::string& output) const {
        cmd.append(get_cxx());
        apply_sysroot(cmd);
        if (is_msvc) {
            cmd.append("/LD", "/Fe:" + output);
            for (const auto& obj : objects) cmd.append(obj);
        } else {
            cmd.append("-shared", "-o", output);
            for (const auto& obj : objects) cmd.append(obj);
        }
    }

    void build_static_lib(Cmd& cmd, const std::vector<std::string>& objects, const std::string& output) const {
        cmd.append(get_ar());
        if (is_msvc) {
            cmd.append("/OUT:" + output);
            for (const auto& obj : objects) cmd.append(obj);
        } else {
            cmd.append("rcs", output);
            for (const auto& obj : objects) cmd.append(obj);
        }
    }
};

// --- Library Search Subsystem ---

namespace lib_search {
    inline std::vector<std::string> get_system_search_paths() {
        std::vector<std::string> paths;

#ifdef _WIN32
        const char* lib_env = std::getenv("LIB");
        if (lib_env) {
            std::string env_str(lib_env);
            size_t start = 0, end = 0;
            while ((end = env_str.find(';', start)) != std::string::npos) {
                paths.push_back(env_str.substr(start, end - start));
                start = end + 1;
            }
            paths.push_back(env_str.substr(start));
        }
#else
        // Check standard Linux/macOS paths
        paths.push_back("/usr/lib");
        paths.push_back("/usr/local/lib");
        paths.push_back("/lib");

        // Check environment variable
        const char* lib_env = std::getenv("LIBRARY_PATH");
        if (!lib_env) lib_env = std::getenv("LD_LIBRARY_PATH");
        if (lib_env) {
            std::string env_str(lib_env);
            size_t start = 0, end = 0;
            while ((end = env_str.find(':', start)) != std::string::npos) {
                paths.push_back(env_str.substr(start, end - start));
                start = end + 1;
            }
            paths.push_back(env_str.substr(start));
        }
#endif
        return paths;
    }

    inline std::string find_library(const std::string& name, bool static_lib = false, const std::vector<std::string>& extra_paths = {}) {
        std::vector<std::string> paths = extra_paths;
        std::vector<std::string> sys_paths = get_system_search_paths();
        paths.insert(paths.end(), sys_paths.begin(), sys_paths.end());

#ifdef _WIN32
        std::string filename = name + (static_lib ? ".lib" : ".lib"); // Windows links against .lib for both static and shared imports
#else
        std::string filename = "lib" + name + (static_lib ? ".a" : ".so");
#endif

        for (const auto& path : paths) {
            if (path.empty()) continue;
            std::filesystem::path full_path = std::filesystem::path(path) / filename;
            std::error_code ec;
            if (std::filesystem::exists(full_path, ec)) {
                log(LogLevel::INFO, "Found library '%s' at: %s", name.c_str(), full_path.string().c_str());
                return full_path.string();
            }
        }

        log(LogLevel::WARNING, "Could not find library '%s' in any search paths.", name.c_str());
        return "";
    }
} // namespace lib_search

// --- File Read/Write Subsystem ---

inline bool read_entire_file(const std::string& path, std::string& out_content) {
    // Open in binary mode and immediately seek to the end (ate) to determine size
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        log(LogLevel::ERROR, "Could not open file %s for reading.", path.c_str());
        return false;
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    out_content.resize(size);
    if (!file.read(&out_content[0], size)) {
        log(LogLevel::ERROR, "Failed to read content from %s.", path.c_str());
        return false;
    }
    return true;
}

inline bool write_entire_file(const std::string& path, const void* data, std::size_t size) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        log(LogLevel::ERROR, "Could not open file %s for writing.", path.c_str());
        return false;
    }
    if (!file.write(static_cast<const char*>(data), size)) {
        log(LogLevel::ERROR, "Failed to write content to %s.", path.c_str());
        return false;
    }
    return true;
}

// Convenient overload for writing std::string directly
inline bool write_entire_file(const std::string& path, const std::string& data) {
    return write_entire_file(path, data.data(), data.size());
}

// --- Executable Path Subsystem ---

inline std::string get_executable_path() {
#if defined(_WIN32)
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, buf, MAX_PATH);
    if (len == 0) return "";
    return std::string(buf, len);
#elif defined(__linux__)
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf));
    if (len < 0) return "";
    return std::string(buf, len);
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) return "";
    return std::string(buf); // macOS path might need realpath() to resolve symlinks
#else
    log(LogLevel::WARNING, "get_executable_path() is not implemented for this OS.");
    return "";
#endif
}

// --- Performance Measurement Subsystem ---

class Stopwatch {
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    std::string label;

public:
    // Starts the timer immediately upon creation
    Stopwatch(std::string_view name) : label(name) {
        start_time = std::chrono::high_resolution_clock::now();
    }

    // Automatically logs the elapsed time when it goes out of scope
    ~Stopwatch() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        
        if (duration > 1000) {
            log(LogLevel::INFO, "[TIMER] %s completed in %.2f seconds", label.c_str(), duration / 1000.0);
        } else {
            log(LogLevel::INFO, "[TIMER] %s completed in %lld ms", label.c_str(), static_cast<long long>(duration));
        }
    }

    // Expose raw nanoseconds if a developer needs manual tracking
    static uint64_t nanos_since_epoch() {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    }
};

// --- High-Level Project/Build Abstractions ---

namespace project {

    // Automatically handles object name generation, incremental dependency checking, 
    // and parallel thread-pool compilation for a list of source files.
    inline bool compile_objects(
        const Toolchain& toolchain,
        const std::vector<std::string>& sources,
        const std::string& build_dir,
        const std::vector<std::string>& extra_flags,
        const CliArgs& args,
        std::vector<std::string>& out_objects) 
    {
        JobQueue queue(args.jobs);
        log(LogLevel::INFO, "Compiling %zu files with %d parallel jobs (Mode: %s)...", 
            sources.size(), args.jobs, toolchain.mode.c_str());

        std::string compile_commands = "[\n";
        bool first_cmd = true;

        for (const auto& source : sources) {
            std::string obj_file = build_dir + "/" + path_name(source) + (toolchain.is_msvc ? ".obj" : ".o");
            out_objects.push_back(obj_file);

            Cmd cmd;
            toolchain.build_object(cmd, source, obj_file);
            for (const auto& flag : extra_flags) cmd.append(flag);

            if (!first_cmd) compile_commands += ",\n";
            compile_commands += "  {\n    \"directory\": \".\",\n    \"command\": \"";
            std::string rendered = cmd.render();
            // escape backslashes and quotes for JSON
            for (char c : rendered) {
                if (c == '\\' || c == '"') compile_commands += '\\';
                compile_commands += c;
            }
            compile_commands += "\",\n    \"file\": \"" + source + "\"\n  }";
            first_cmd = false;

            int rebuild = needs_rebuild(obj_file, {source});
            if (rebuild < 0) return false;
            if (rebuild == 0) continue;

            queue.push([cmd_copy = cmd, source, dry_run = args.dry_run]() mutable -> bool {
                if (dry_run) {
                    log(LogLevel::INFO, "[DRY-RUN] %s", cmd_copy.render().c_str());
                    return true;
                }
                
                Fd r, w;
                if (!create_pipe(&r, &w)) {
                    return cmd_copy.run_sync_and_reset();
                }
                
                CmdRedirect redir = {};
                redir.fdout = &w;
                redir.fderr = &w;
                
                Proc p = cmd_copy.run_async(redir);
                fd_close(w);
                
                std::string output = read_all_from_fd(r);
                fd_close(r);
                
                bool success = proc_wait(p);
                cmd_copy.reset();
                
                if (!success) {
                    static std::mutex print_mutex;
                    std::lock_guard<std::mutex> lock(print_mutex);
                    log(LogLevel::ERROR, "Failed to compile %s\n%s", source.c_str(), output.c_str());
                }
                return success;
            });
        }
        
        compile_commands += "\n]\n";
        write_entire_file("compile_commands.json", compile_commands);

        return queue.wait_all();
    }

} // namespace project
    class Project {
    private:
        std::string name;
        std::vector<std::string> sources;
        std::vector<std::string> headers;
        std::string main_file;
        std::string build_dir = "build";
        std::vector<std::string> include_dirs = {".", "runtime", "compiler"};

    public:
        Project(const std::string& proj_name) : name(proj_name) {}

        void add_source_files(const std::vector<std::string>& files) {
            sources.insert(sources.end(), files.begin(), files.end());
        }

        void add_header_files(const std::vector<std::string>& files) {
            headers.insert(headers.end(), files.begin(), files.end());
        }

        void set_main_file(const std::string& file) {
            main_file = file;
        }

        int build(int argc, char** argv) {
            nob::Stopwatch timer(name + " Build System");
            NOB_GO_REBUILD_URSELF(argc, argv);

            // Parse command-line arguments
            nob::CliArgs args = nob::CliArgs::parse(argc, argv);

            if (args.help) {
                args.print_help("nob");
                return 0;
            }

            // Handle clean command
            if (args.clean) {
                nob::log(nob::LogLevel::INFO, "Cleaning build artifacts...");
                if (nob::file_exists(build_dir)) {
                    if (!nob::delete_file(build_dir)) {
                        nob::log(nob::LogLevel::ERROR, "Failed to delete build directory.");
                        return 1;
                    }
                }
                nob::log(nob::LogLevel::INFO, "Clean complete.");
                return 0;
            }

            // Validate project structure
            nob::log(nob::LogLevel::INFO, "Validating project structure...");
            bool all_files_present = true;
            for (const auto& source : sources) {
                if (!nob::file_exists(source)) {
                    nob::log(nob::LogLevel::ERROR, "Missing source file: %s", source.c_str());
                    all_files_present = false;
                }
            }
            for (const auto& header : headers) {
                if (!nob::file_exists(header)) {
                    nob::log(nob::LogLevel::ERROR, "Missing header file: %s", header.c_str());
                    all_files_present = false;
                }
            }
            if (!args.shared && !main_file.empty() && !nob::file_exists(main_file)) {
                nob::log(nob::LogLevel::ERROR, "Missing main entry point file: %s", main_file.c_str());
                all_files_present = false;
            }
            
            if (!all_files_present) {
                nob::log(nob::LogLevel::ERROR, "Project validation failed. Cannot proceed with build.");
                return 1;
            }
            nob::log(nob::LogLevel::INFO, "Project validation complete. All files present.");

            // Create build directory
            if (!nob::mkdir_if_not_exists(build_dir)) {
                nob::log(nob::LogLevel::ERROR, "Failed to create build directory.");
                return 1;
            }

            // Configure toolchain
            nob::Toolchain toolchain;
            toolchain.configure(args);
            
            // Detect architecture and log it
            nob::log(nob::LogLevel::INFO, "Build configuration:");
            nob::log(nob::LogLevel::INFO, "  Compiler: %s", toolchain.get_cxx().c_str());
            nob::log(nob::LogLevel::INFO, "  Mode: %s", toolchain.mode.c_str());
            nob::log(nob::LogLevel::INFO, "  Parallel jobs: %d", args.jobs);
            
        #if defined(__x86_64__) || defined(_M_X64)
            nob::log(nob::LogLevel::INFO, "  Architecture: x86_64");
        #elif defined(__aarch64__) || defined(_M_ARM64)
            nob::log(nob::LogLevel::INFO, "  Architecture: ARM64");
        #elif defined(__arm__)
            nob::log(nob::LogLevel::INFO, "  Architecture: ARM32");
        #elif defined(__riscv)
            nob::log(nob::LogLevel::INFO, "  Architecture: RISC-V");
        #elif defined(__XTENSA__)
            nob::log(nob::LogLevel::INFO, "  Architecture: Xtensa");
        #else
            nob::log(nob::LogLevel::WARNING, "  Architecture: Unknown (may not support inline assembly)");
        #endif

            // Compile object files
            std::vector<std::string> object_files;
            if (sources.empty()) {
                nob::log(nob::LogLevel::INFO, "No source files to compile (header-only project).");
            } else {
                std::vector<std::string> compile_flags = {"-std=c++17", "-Wall", "-Wextra"};
                for (const auto& inc : include_dirs) {
                    compile_flags.push_back("-I");
                    compile_flags.push_back(inc);
                }
                
                if (!nob::project::compile_objects(toolchain, sources, build_dir, compile_flags, args, object_files)) {
                    nob::log(nob::LogLevel::ERROR, "Build failed during object compilation.");
                    return 1;
                }
                nob::log(nob::LogLevel::INFO, "All object files compiled successfully.");
            }

            // Link executable or shared library
            std::string output_path = build_dir + "/" + name;
            if (args.shared) {
                #ifdef _WIN32
                output_path += ".dll";
                #else
                output_path += ".so";
                #endif
            } else {
                #ifdef _WIN32
                output_path += ".exe";
                #endif
            }

            nob::log(nob::LogLevel::INFO, "Linking %s: %s", args.shared ? "shared library" : "executable", output_path.c_str());

            std::vector<std::string> main_deps;
            if (!args.shared && !main_file.empty()) {
                main_deps.push_back(main_file);
            }
            main_deps.insert(main_deps.end(), object_files.begin(), object_files.end());
            
            int rebuild = nob::needs_rebuild(output_path, main_deps);
            if (rebuild < 0) {
                nob::log(nob::LogLevel::ERROR, "Failed to check dependencies for %s", output_path.c_str());
                return 1;
            }
            
            if (rebuild == 0 && !object_files.empty()) {
                nob::log(nob::LogLevel::INFO, "%s is up to date", output_path.c_str());
            } else {
                nob::Cmd cmd;
                if (args.shared) {
                    toolchain.build_shared_lib(cmd, object_files, output_path);
                } else {
                    cmd.append(toolchain.get_cxx());
                    toolchain.apply_mode(cmd);
                    toolchain.apply_sysroot(cmd);
                    
                    cmd.append("-std=c++17", "-Wall", "-Wextra");
                    for (const auto& inc : include_dirs) {
                        cmd.append("-I", inc);
                    }
                    
                    if (!main_file.empty()) {
                        cmd.append(main_file);
                    }
                    
                    for (const auto& obj : object_files) {
                        cmd.append(obj);
                    }
                    
                    if (toolchain.is_msvc) {
                        cmd.append("/Fe:" + output_path);
                    } else {
                        cmd.append("-o", output_path);
                    }

                    #if defined(_WIN32)
                        if (toolchain.is_msvc) {
                            cmd.append("/nologo", "/EHsc");
                        } else {
                            cmd.append("-static-libgcc", "-static-libstdc++");
                        }
                    #endif
                }
                
                if (args.dry_run) {
                    nob::log(nob::LogLevel::INFO, "[DRY-RUN] %s", cmd.render().c_str());
                } else if (!cmd.run_sync_and_reset()) {
                    nob::log(nob::LogLevel::ERROR, "Build failed during linking.");
                    return 1;
                }
            }

            nob::log(nob::LogLevel::INFO, "Build successful!");
            nob::log(nob::LogLevel::INFO, "Output: %s", output_path.c_str());

            // Optionally run the executable if "run" is passed as a positional argument
            if (!args.shared) {
                bool should_run = false;
                for (const auto& arg : args.positional) {
                    if (arg == "run") {
                        should_run = true;
                        break;
                    }
                }

                if (should_run) {
                    nob::Cmd run_cmd;
                    run_cmd.append(output_path);
                    if (!run_cmd.run_sync_and_reset()) {
                        return 1;
                    }
                }
            }

            return 0;
        }
    };

} // namespace nob
