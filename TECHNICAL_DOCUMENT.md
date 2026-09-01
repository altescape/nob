# NOB Build System: Technical Architecture & Audit

## 1. Overview and Journey
The journey of `nob` began as a C-based declarative build system inspired by Tsoding's NoBuild philosophy, but it has quickly evolved into an advanced, self-aware C++17 build orchestrator. 

**Where we went wrong previously:**
- **Premature Macros:** We originally hallucinated the existence of the `NOB_GO_REBUILD_URSELF` macro inside `nob.hpp`, assuming it was already there during the C-to-C++ port. This led to compilation errors when `nob.cpp` tried to invoke it.
- **Redefinition Conflicts:** We later added a massive, highly capable metaprogramming block at the bottom of the header, completely missing that a simpler `go_rebuild_urself` was already defined in the middle of the file. This caused standard `redefined` compiler errors.
- **Syntax Slippages:** Moving massive blocks of code across a 1800-line header caused a dropped `}` closing brace for `namespace nob`, immediately breaking the build.

**The Current State:**
Despite the bumps, `nob` is now a robust, header-only (`nob.hpp`) C++ build system that natively parses POSIX shell commands, dynamically fetches dependencies, schedules parallel compilation tasks, and modifies its own source code to remember user configurations.

---

## 2. Subsystem Architecture

### A. The Core Metaprogramming & Self-Awareness (`nob::ensure_compiler_configured`)
**The Genius:** The build system is "conscious." When `nob.cpp` runs, it reads its own source code via `__FILE__`. It looks for the magic comment `// NOB_COMPILER: <name>`. If the compiler is missing or uninstalled, it drops into a scanner, finds valid C++ compilers (testing them silently), prompts the user, and **rewrites its own source code on disk** to store the choice.
**Execution:** After editing itself, `nob.exe` recognizes that `nob.cpp` is newer than the executable. It immediately rebuilds itself using the newly discovered compiler, swaps the binary, and restarts the process seamlessly. 

### B. Command Execution and Piping (`nob::Cmd`)
`nob::Cmd` is an abstraction over POSIX `fork`/`execvp` and Windows `CreateProcessA`.
- **String Rendering:** Safely wraps arguments with spaces in quotes.
- **Response Files:** If a command exceeds the Windows 30,000-character limit, it transparently dumps the arguments into a `.rsp` response file and executes `g++ @nob_cmd_1.rsp`. This is a massive feature for linking huge codebases.
- **Piping (`CmdRedirect`):** The system defines a `CmdRedirect` struct containing pointers to `Fd` (File Descriptors / HANDLEs). 
  - On **Windows**, it maps these to `siStartInfo.hStdInput`, `hStdOutput`, and `hStdError`.
  - On **POSIX**, it maps them using `dup2(fd, STDIN_FILENO)`.
  - *How it's used:* You can spawn a process, capture its `stdout` into a pipe, and feed it into the `stdin` of another `nob::Cmd` to create shell-like pipelines natively in C++.

### C. Shell Lexing (`nob::shlex_split`)
Imported directly from the provided C implementation, the `shlex` logic has been modernized to C++ `std::string_view` and `std::vector`. It allows `nob::Cmd` to safely ingest raw terminal strings (e.g., `cmd.append_shlex("-I\"./hello world\" -lm -lc")`) and properly tokenize them respecting quotes and escapes, bypassing the need for manual string slicing.

### D. Job Queue (`nob::JobQueue`)
A thread-pool implementation utilizing `std::thread`, `std::mutex`, and `std::condition_variable`. `nob::project::compile_objects` chunks out compilation tasks and pushes them to the queue. Workers pop tasks and compile concurrently, drastically reducing build times for large C++ projects.

### E. Dependency Fetcher (`nob::fetch`)
Capable of executing `git clone` and `git checkout` to lock dependencies to specific commit hashes. It gracefully falls back between `curl` and `wget` for downloading raw files.

---

## 3. Code Audit: Does it do what we claim?

### The Claims:
1. **"It can self-edit its configuration."** 
   - *Verdict:* **TRUE**. The code successfully opens `std::ifstream`, parses for `// NOB_COMPILER:`, prompts via `std::cin`, and writes back via `std::ofstream`.
2. **"It supports cross-platform piping."** 
   - *Verdict:* **PARTIALLY TRUE**. The underlying OS calls (`CreateProcess` handles and `dup2`) are perfectly implemented in `run_async`. However, there are no high-level wrapper functions (like `pipe()` or `nob::Cmd::pipe_to(Cmd&)`) to easily create the pipes themselves. The user has to manually create the OS-level pipes and pass the raw FDs to `CmdRedirect`.
3. **"It parses POSIX strings safely."** 
   - *Verdict:* **TRUE**. `shlex_split` flawlessly implements the POSIX 2.2.2 and 2.2.3 quoting rules.
4. **"It prevents command-line length limits."**
   - *Verdict:* **TRUE**. The Response File Subsystem kicks in exactly at 30,000 characters.

---

## 4. Shortcomings & What Can Be Done Better

1. **Missing High-Level Pipe API:** 
   While `CmdRedirect` exists, `nob.hpp` doesn't provide a cross-platform `nob::create_pipe(Fd* read, Fd* write)`. Adding this would make chaining commands trivial.
2. **Scanner Brittle on Windows:** 
   The compiler scanner runs `cl.exe /? > nul 2>&1`. If MSVC is installed but `vcvarsall.bat` hasn't been run, `cl.exe` won't be in the PATH, and `nob` won't find it. `nob` should ideally query the Windows Registry or `vswhere.exe` to automatically locate MSVC.
3. **Sub-process Output Interleaving:** 
   When the `JobQueue` runs 8 compilation jobs in parallel, any compiler errors dumped to `stdout`/`stderr` will interleave, creating an unreadable mess. `nob` needs to capture the output of each worker thread into a buffer and print it atomically upon job failure.
4. **No JSON Compilation Database:** 
   Modern IDEs require `compile_commands.json` for IntelliSense and LSP support. `nob` has all the data (compiler paths, flags, source files) but doesn't dump this JSON file. Adding this would make `nob` a first-class citizen for Neovim, VSCode, and CLion.
