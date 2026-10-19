cmake_minimum_required(VERSION 3.23)

# Respect a caller-provided toolchain. VCPKG_WDIR is the optional external
# dependency SDK root used by this project.
if (NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_WDIR})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_WDIR}/scripts/buildsystems/vcpkg.cmake"
            CACHE FILEPATH "Dependency toolchain")
endif ()

project(MingwDownloader VERSION 1.2.0.0 LANGUAGES C CXX RC)

# Bundled SDK adapters locate prebuilt libraries; they do not select compilers.
# Detect the CLion-selected compiler first, then choose its matching SDK.
if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if (MSVC)
        set(_dependency_sdk windows-msvc)
    elseif (MINGW)
        set(_dependency_sdk windows-mingw)
    else ()
        message(FATAL_ERROR "Choose an explicit dependency toolchain for this compiler.")
    endif ()
    set(CMAKE_TOOLCHAIN_FILE
            "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/${_dependency_sdk}/scripts/buildsystems/vcpkg.cmake"
            CACHE FILEPATH "Dependency toolchain")
    unset(_dependency_sdk)
endif ()

# Reapply bundled dependency adapters during CMake reloads. Their selection
# may happen after project() on the first configuration.
if (CMAKE_TOOLCHAIN_FILE STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/windows-msvc/scripts/buildsystems/vcpkg.cmake" OR
        CMAKE_TOOLCHAIN_FILE STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/windows-mingw/scripts/buildsystems/vcpkg.cmake")
    include("${CMAKE_TOOLCHAIN_FILE}")
endif ()

# These prebuilt SDKs are x64-specific. Never mix them with another ABI.
if (MINGW_DOWNLOADER_BUNDLED_MINGW AND
(NOT MINGW OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8))
    message(FATAL_ERROR "The bundled MinGW SDK requires x64 MinGW (win32 threads, SEH, MSVCRT).")
endif ()
if (MINGW_DOWNLOADER_BUNDLED_MSVC AND
(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8))
    message(FATAL_ERROR "The bundled MSVC SDK requires an x64 MSVC-compatible compiler.")
endif ()

# Application language and runtime policy.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(MINGW_DOWNLOADER_STATIC_RUNTIME "Prefer static GCC/Stdlib where possible" ON)

# FLTK is built from source with the active CLion toolchain. Only the core
# library is needed: this app does not use FLTK Forms, Images, or OpenGL.
include(FetchContent)
set(FLTK_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_FLUID OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_FLTK_OPTIONS OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_GL OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_FORMS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(fltk
        GIT_REPOSITORY https://github.com/fltk/fltk.git
        GIT_TAG release-1.4.5
        GIT_SHALLOW TRUE)

if (MSVC)
    set(FLTK_MSVC_RUNTIME_DLL OFF CACHE BOOL
            "Use static MSVC runtime for FLTK" FORCE)
endif ()

FetchContent_MakeAvailable(fltk)

# Keep the remaining dependencies in the existing vcpkg SDK.
find_package(CURL CONFIG REQUIRED)
find_package(LibArchive REQUIRED)
find_package(Threads REQUIRED)

# The bundled SDK contains static libarchive. Unlike the full vcpkg wrapper,
# CMake's FindLibArchive does not provide its private compression libraries.
# Supply their imported locations and transitive link requirements explicitly.
if (MINGW_DOWNLOADER_BUNDLED_MINGW OR MINGW_DOWNLOADER_BUNDLED_MSVC)
    find_package(ZLIB REQUIRED)
    find_package(liblzma CONFIG REQUIRED)
    set(_sdk_prefix "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    if (MSVC)
        set(_archive_name archive.lib)
    else ()
        set(_archive_name libarchive.a)
    endif ()
    set_target_properties(LibArchive::LibArchive PROPERTIES
            IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
            IMPORTED_LOCATION_DEBUG "${_sdk_prefix}/debug/lib/${_archive_name}"
            IMPORTED_LOCATION_RELEASE "${_sdk_prefix}/lib/${_archive_name}"
            MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
            MAP_IMPORTED_CONFIG_MINSIZEREL Release)
    set_property(TARGET LibArchive::LibArchive APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES ZLIB::ZLIB liblzma::liblzma)
    unset(_sdk_prefix)
    unset(_archive_name)
endif ()

# Preserve the existing main() entry point and Windows resource icon.
# The Windows GUI subsystem is selected below per compiler.
add_executable(MingwDownloader
        src/main.cpp
        assets/app.rc)

target_compile_definitions(MingwDownloader PRIVATE
        APP_VERSION="${PROJECT_VERSION}"
)

# The project uses a vendored single-header nlohmann/json.hpp.
target_include_directories(MingwDownloader PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

target_link_libraries(MingwDownloader PRIVATE
        fltk::fltk
        CURL::libcurl
        LibArchive::LibArchive
        Threads::Threads)

if (WIN32)
    target_link_libraries(MingwDownloader PRIVATE ole32 shell32)
    target_compile_definitions(MingwDownloader PRIVATE
            UNICODE
            _UNICODE)
endif ()

# These definitions match the project's prebuilt static CURL/NGHTTP2/SSL SDK.
# Revisit them if switching to shared or differently configured dependencies.
if (WIN32 AND (MINGW OR MSVC))
    target_compile_definitions(MingwDownloader PRIVATE
            NGHTTP2_STATICLIB
            CURL_STATICLIB
            NO_SSL_DL)
endif ()

if (MINGW)
    target_compile_options(MingwDownloader PRIVATE
            -Wall -Wextra
            $<$<CONFIG:Release>:-O2>
            $<$<CONFIG:Release>:-ffunction-sections>
            $<$<CONFIG:Release>:-fdata-sections>)

    # Hide the console and discard unused sections in Release builds.
    target_link_options(MingwDownloader PRIVATE
            -mwindows
            $<$<CONFIG:Release>:-Wl,--gc-sections>
            $<$<CONFIG:Release>:-s>)

    # Static compiler runtime for portable MinGW distributions. This does not
    # guarantee that all third-party or operating-system DLLs are absent.
    if (MINGW_DOWNLOADER_STATIC_RUNTIME)
        target_link_options(MingwDownloader PRIVATE
                -static
                -static-libgcc
                -static-libstdc++)
    endif ()
elseif (MSVC)
    # main.cpp defines main(), not WinMain(). Keep the CRT entry-point bridge.
    target_link_options(MingwDownloader PRIVATE
            /SUBSYSTEM:WINDOWS
            /ENTRY:mainCRTStartup)
    # The bundled MSVC SDK adapter selects /MT (Release) and /MTd (Debug).
endif ()
