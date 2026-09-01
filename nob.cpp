// NOB_COMPILER: g++
#define NOB_IMPLEMENTATION
#include "nob.hpp"

int main(int argc, char** argv) {
    // 1. BOOTSTRAP: REBUILD OURSELVES IF THIS FILE WAS MODIFIED
    NOB_GO_REBUILD_URSELF(argc, argv);

    // 2. PARSE CLI ARGUMENTS (OPTIONAL)
    nob::CliArgs args = nob::CliArgs::parse(argc, argv);
    
    // Check for intelligent upgrade flag
    if (args.has_flag("upgrade")) {
        nob::upgrade(__FILE__);
        return 0;
    }
    
    // Check for updates in the background on every run
    nob::check_for_updates_async();
    
    // 3. CONFIGURE THE COMPILER (DETECTS CXX/CC ENVIRONMENTS)
    nob::Toolchain toolchain;
    toolchain.configure(args);
    
    // 4. TSODING PHILOSOPHY: TOTAL IMPERATIVE CONTROL
    nob::Cmd cmd;
    
    // CHECK IF MAIN.CPP HAS BEEN UPDATED SINCE WE LAST BUILT MAIN.EXE
    if (nob::needs_rebuild("main.exe", {"main.cpp"})) {
        nob::log(nob::LogLevel::INFO, "Compiling main.cpp...");
        
        // APPEND ARGUMENTS MANUALLY
        cmd.append(toolchain.get_cxx());
        
        // ADD FLAGS BASED ON COMPILER
        if (toolchain.is_msvc) {
            cmd.append("/EHsc", "main.cpp", "/Fe:main.exe");
        } else {
            cmd.append("-Wall", "-Wextra", "main.cpp", "-o", "main.exe");
        }
        
        // EXECUTE SYNCHRONOUSLY AND CHECK FOR FAILURE
        if (!cmd.run_sync_and_reset()) {
            nob::log(nob::LogLevel::ERROR, "Failed to compile main.cpp");
            return 1;
        }
    } else {
        nob::log(nob::LogLevel::INFO, "main.exe is up to date");
    }

    return 0;
}
