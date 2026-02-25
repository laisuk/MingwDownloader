# MinGW Builds Downloader

[![VirusTotal Scan](https://img.shields.io/badge/VirusTotal-0%2F72-brightgreen?logo=virustotal&logoColor=white)](https://www.virustotal.com/gui/file/52309e7c9c3be17c879a93de6e06e88508411cc21cbdbf981d3971c734982492/detection)
[![Total Downloads](https://img.shields.io/github/downloads/laisuk/MingwDownloader/total.svg)](https://github.com/laisuk/MingwDownloader/releases)

A lightweight native Windows GUI utility for downloading and extracting
prebuilt **MinGW-w64 binaries** from:

[MinGW-w64 binaries (niXman)](https://github.com/niXman/mingw-builds-binaries)

Built with:

- C++17
- FLTK (native GUI)
- libcurl (HTTPS download)
- libarchive (7z extraction)
- nlohmann/json
- Fully static MinGW build
- No external dependencies
- No 7z.exe required

---

![image01](assets/image01.png)

---

## ✨ Features

- Browse official MinGW-w64 releases

- Filter builds by:

    - Architecture (i686 / x86_64)
    - Thread model (posix / win32 / mcf)
    - Exception model (seh / dwarf)
    - CRT (ucrt / msvcrt)

- Download selected archive

- Optional **Download + Extract**

- Extraction to:

      out_dir / artifact_name /

- Progress bar for:

    - Download progress
    - Extraction progress (entry-based counting)

- Cancel support

- Native Windows folder picker (modern COM dialog)

- Fully static portable executable

- Small binary size (~2--3 MB)

- Clean VirusTotal result (0/72)

------------------------------------------------------------------------

## 📦 Why This Tool?

Many MinGW users:

- Do not want to manually browse GitHub releases
- Do not want to manually extract .7z archives
- Want a quick portable way to get a working toolchain

This utility provides a smooth, responsive GUI experience with no
freezing UI.

------------------------------------------------------------------------

## 🧠 Architecture

The application is properly threaded:

- UI thread handles rendering and input
- Worker thread handles:
    - HTTP download
    - Archive entry counting
    - Extraction
- Communication via `Fl::awake()` (FLTK-safe mechanism)

This ensures:

- Smooth UI
- Responsive cancel button
- No "Not Responding" Windows state

------------------------------------------------------------------------

## 🔒 Security & Clean Build

- No external 7z.exe execution
- No runtime unpacking
- No self-modifying behavior
- Fully static binary
- No dynamic loading of SSL backends
- Clean VirusTotal scan

------------------------------------------------------------------------

## 🛠 Build Instructions

Requires:

- vcpkg (x64-mingw-static triplet)
- CMake ≥ 3.23
- MinGW-w64

Install dependencies:

    vcpkg install fltk:x64-mingw-static
    vcpkg install curl[core,schannel]:x64-mingw-static
    vcpkg install libarchive[core,lzma]:x64-mingw-static

Configure:

    cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

Build:

    cmake --build build --config Release

The resulting executable is fully static and portable.

------------------------------------------------------------------------

## 📄 License

This project is released under the MIT License.

MinGW builds are provided by the original maintainers:
https://github.com/niXman/mingw-builds-binaries

------------------------------------------------------------------------

## 👤 Author

Created by Laisuk Lai

Generated on: 2026-02-25 11:17:36 UTC
