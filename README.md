# NOB: Shape-Shifting C++17 Build System

**NOB** (No Build) is an ultra-fast, self-aware, zero-dependency C++17 build orchestrator. 
Inspired by Tsoding's NoBuild philosophy, NOB gives you 100% imperative control over your build pipeline natively in C++.

No Makefiles. No CMake lists. Just pure, compiled C++.

## Features

- **Header Only (`nob.hpp`)**: Drop it into your project and you're done. No installation.
- **Universal Teleportation**: Run an empty `nob` executable in a blank directory, and it will fetch its own source code from GitHub and seamlessly bootstrap a new project.
- **Self-Replicating & Self-Healing**: `nob` dynamically finds compilers (`g++`, `clang++`, MSVC), locks onto one, modifies its own source code to remember it, and instantly rebuilds itself.
- **Multi-Core Concurrency**: Fully asynchronous `JobQueue` to compile hundreds of files in parallel, utilizing every CPU core.
- **Zero-Dependency POSIX Pipelines**: Native C++ abstractions for `fork`/`execvp` on Linux and `CreateProcess` on Windows.
- **LSP Intelligence**: Automatically dumps `compile_commands.json` for instant IntelliSense in Neovim/VSCode.

## Getting Started

1. Copy `nob.hpp`, `nob.cpp`, and `main.cpp` into your project.
2. Compile the orchestrator:
   - Linux/Mac: `g++ -std=c++17 nob.cpp -o nob`
   - Windows: `cl.exe /std:c++17 /EHsc nob.cpp`
3. Run the build orchestrator: `./nob`

### Teleportation Bootstrap (Magic Mode)
If you already have a `nob` executable but your directory is empty:
Just run `./nob`. It will detect the void, download the `nob.hpp` header from GitHub, compile itself seamlessly over its own binary, and spawn your project.

## Technical Architecture
Read the [TECHNICAL_DOCUMENT.md](TECHNICAL_DOCUMENT.md) for a deep dive into how `nob` bypasses OS locks, shapes-shifts, and implements its neural persistence layer.
