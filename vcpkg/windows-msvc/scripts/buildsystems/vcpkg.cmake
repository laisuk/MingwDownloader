# Relocatable prebuilt dependency SDK, not a vcpkg package manager installation.
get_filename_component(_msvc_sdk_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "Bundled SDK triplet")
if (NOT VCPKG_TARGET_TRIPLET STREQUAL "x64-windows-static")
    message(FATAL_ERROR "This SDK only contains x64-windows-static dependencies.")
endif()
set(VCPKG_INSTALLED_DIR "${_msvc_sdk_root}/installed" CACHE PATH "Bundled SDK packages")
set(MINGW_DOWNLOADER_BUNDLED_MSVC ON)
# x64-windows-static uses the static CRT in both configurations.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
list(PREPEND CMAKE_PREFIX_PATH "${_msvc_sdk_root}/installed/x64-windows-static")
list(REMOVE_DUPLICATES CMAKE_PREFIX_PATH)
# FindZLIB needs explicit configuration paths outside the full vcpkg toolchain.
set(ZLIB_LIBRARY_RELEASE "${_msvc_sdk_root}/installed/x64-windows-static/lib/zlib.lib" CACHE FILEPATH "Bundled Release zlib" FORCE)
set(ZLIB_LIBRARY_DEBUG "${_msvc_sdk_root}/installed/x64-windows-static/debug/lib/zlibd.lib" CACHE FILEPATH "Bundled Debug zlib" FORCE)
unset(_msvc_sdk_root)
