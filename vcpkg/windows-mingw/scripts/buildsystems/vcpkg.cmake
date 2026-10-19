# Relocatable prebuilt dependency SDK, not a vcpkg package manager installation.
get_filename_component(_mingw_sdk_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(VCPKG_TARGET_TRIPLET "x64-mingw-static" CACHE STRING "Bundled SDK triplet")
if (NOT VCPKG_TARGET_TRIPLET STREQUAL "x64-mingw-static")
    message(FATAL_ERROR "This SDK only contains x64-mingw-static dependencies.")
endif()
set(VCPKG_INSTALLED_DIR "${_mingw_sdk_root}/installed" CACHE PATH "Bundled SDK packages")
set(MINGW_DOWNLOADER_BUNDLED_MINGW ON)
list(PREPEND CMAKE_PREFIX_PATH "${_mingw_sdk_root}/installed/x64-mingw-static")
list(REMOVE_DUPLICATES CMAKE_PREFIX_PATH)
# FindZLIB needs explicit configuration paths outside the full vcpkg toolchain.
set(ZLIB_LIBRARY_RELEASE "${_mingw_sdk_root}/installed/x64-mingw-static/lib/libzlib.a" CACHE FILEPATH "Bundled Release zlib" FORCE)
set(ZLIB_LIBRARY_DEBUG "${_mingw_sdk_root}/installed/x64-mingw-static/debug/lib/libzlibd.a" CACHE FILEPATH "Bundled Debug zlib" FORCE)
unset(_mingw_sdk_root)
