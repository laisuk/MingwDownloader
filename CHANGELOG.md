# Changelog

All notable changes to this project will be documented in this file.

This project adheres to [Semantic Versioning](https://semver.org/).

------------------------------------------------------------------------

## [1.2.0] - Unreleased

### Added

- Added support for `rt_v14` MinGW Builds releases while retaining `rt_v13` compatibility.

### Changed

- Upgraded FLTK from 1.3.11 to 1.4.5 using CMake FetchContent.
- Improved UI appearance and rendering with FLTK 1.4.5.
- Refactored background download, extraction, and refresh operations for thread-safe FLTK UI updates.
- Improved progress reporting and worker thread lifecycle management.
- Prevented concurrent downloads and duplicate refresh operations.
- Removed 334 obsolete FLTK 1.3.11 artifacts from the bundled MinGW and MSVC SDKs.
- Updated SDK metadata, checksums, and documentation after legacy FLTK cleanup.
- Fixed MSVC static runtime compatibility by configuring FLTK to use `/MT`.
- Verified successful MinGW and MSVC Release builds.

---

## [1.0.1] - 2026-04-06

### Changed

- UI optimization.
- Added FileVersion WinRes

---

## [1.0.0] - 2026-02-25

### Added

- Initial public release
- Native Windows GUI built with FLTK
- GitHub release fetching via libcurl (HTTPS)
- Asset filtering (Architecture, MRT, Exception, CRT, RT)
- Download with progress bar
- Cancel support during download
- In-process archive extraction using libarchive (7z support)
- Extraction progress (entry counting)
- Safe path handling to prevent traversal issues
- Fully static MinGW build (portable single executable)
- Modern Windows folder picker (COM-based dialog)

### Changed

- N/A (Initial release)

------------------------------------------------------------------------
