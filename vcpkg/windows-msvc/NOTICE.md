# Bundled MSVC dependency notice

These prebuilt x64 Windows libraries were copied from
`D:/lib/vcpkg/installed/x64-windows-static`. They use the MSVC ABI and the
**static Microsoft C/C++ runtime**: `/MT` for Release and `/MTd` for Debug.
They are separate from the MinGW `x86_64-win32-seh-msvcrt` binaries.

The SDK adapter sets `CMAKE_MSVC_RUNTIME_LIBRARY` to match these libraries.
Local builds are verified with Visual Studio 2022 MSVC 19.44.35229.0 (v143).
The retained installed metadata records package ABI hashes, not the exact
original compiler version. Use a compatible x64 MSVC toolset and Windows SDK.

## Included packages

| Package | Installed version | License notice |
| --- | --- | --- |
| curl | 8.14.1 | share/curl/copyright |
| libarchive (LZMA enabled) | 3.7.8#1 | share/libarchive/copyright |
| libjpeg-turbo | 3.1.0#1 | share/libjpeg-turbo/copyright |
| libpng | 1.6.48 | share/libpng/copyright |
| zlib | 1.3.1 | share/zlib/copyright |
| liblzma | 5.8.1 | share/liblzma/copyright |

License paths are relative to `installed/x64-windows-static`. Retain the upstream
notices when redistributing the SDK or application. The application's MIT license
does not replace dependency licenses.

The snapshot includes headers, Release/Debug static libraries, package metadata,
and tools for the retained ports. FLTK 1.4.5 is built separately from source by
the application's CMake FetchContent configuration; legacy FLTK 1.3.11 files
have been removed. JPEG and PNG remain bundled to preserve existing dependencies.
Curl uses Windows Schannel/SSPI; libarchive uses zlib and LZMA. Windows system
DLLs are still required at runtime.

`scripts/buildsystems/vcpkg.cmake` is a project-owned, relocatable CMake SDK adapter,
not a package manager installation. It does not install or update packages.
CMakeLists.txt adds libarchive's private compression dependencies explicitly.
`metadata/` retains source package file lists and installed status, including
versions, features and ABI hashes. `SHA256SUMS.txt` records payload hashes.

To refresh, copy the files enumerated by these six ports' x64-windows-static
installed file lists, preserving the installed layout and notices. Refresh the
metadata and checksums, then verify fresh Release and Debug builds with the
matching static CRT. Package upgrades require rebuilding this binary snapshot.
