# Bundled MinGW dependency notice

These binaries were copied from `D:/lib/vcpkg/installed/x64-mingw-static`.
Their supplied build origin is **x86_64-win32-seh-msvcrt**: x64 Windows,
win32 thread model, SEH exceptions, and the MSVCRT C runtime. This is distinct
from the vcpkg triplet name `x64-mingw-static`, which does not specify those ABI
choices. Local Release and Debug builds were verified using MinGW-Builds
GCC 14.2.0 (x86_64-win32-seh-rev2, MSVCRT). The original compiler provenance
is supplied by the repository owner; the installed metadata does not independently
record every compiler option. Use an ABI-compatible toolchain.

## Included packages

| Package                   | Installed version | License notice                |
|---------------------------|-------------------|-------------------------------|
| curl                      | 8.14.1            | share/curl/copyright          |
| libarchive (LZMA enabled) | 3.7.8#1           | share/libarchive/copyright    |
| libjpeg-turbo             | 3.1.0#1           | share/libjpeg-turbo/copyright |
| libpng                    | 1.6.48            | share/libpng/copyright        |
| zlib                      | 1.3.1             | share/zlib/copyright          |
| liblzma                   | 5.8.1             | share/liblzma/copyright       |

License paths are relative to `installed/x64-mingw-static`. Retain these upstream
notices when redistributing the SDK or application. The application's MIT license
does not replace dependency licenses.

The snapshot includes headers, Release/Debug static libraries, package metadata,
and tools for the retained ports. FLTK 1.4.5 is built separately from source by
the application's CMake FetchContent configuration; legacy FLTK 1.3.11 files
have been removed. JPEG and PNG remain bundled to preserve existing dependencies.
Curl uses Windows Schannel/SSPI; libarchive uses zlib and LZMA. Windows system
DLLs are still required at runtime.

`scripts/buildsystems/vcpkg.cmake` is a project-owned, relocatable CMake SDK adapter,
not a full vcpkg installation. It does not install or update packages. The main
CMakeLists adds libarchive's private static compression dependencies explicitly.
`metadata/` retains the source package file lists and installed status (versions,
features and ABI hashes). `SHA256SUMS.txt` records the bundled payload hashes.

To refresh, copy the files enumerated by these six ports' x64-mingw-static
installed file lists, preserving the installed directory layout and notices;
refresh metadata and checksums, then verify fresh Release and Debug builds with
the matching compiler. Package upgrades require rebuilding this binary snapshot.
