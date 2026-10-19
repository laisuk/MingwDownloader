cmake_minimum_required(VERSION 3.23)

# Honor explicit external toolchains before compiler detection.
if (NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_WDIR})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_WDIR}/scripts/buildsystems/vcpkg.cmake"
            CACHE FILEPATH "Dependency toolchain")
endif()

project(MingwDownloader VERSION 1.0.2.0 LANGUAGES C CXX RC)

# These adapters only locate prebuilt dependencies. Select after compiler
# detection so Ninja/CLion can choose MSVC without an explicit compiler path.
if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if (MSVC)
        set(_dependency_sdk windows-msvc)
    elseif (MINGW)
        set(_dependency_sdk windows-mingw)
    else()
        message(FATAL_ERROR "Choose an explicit dependency toolchain for this compiler.")
    endif()
    set(CMAKE_TOOLCHAIN_FILE
            "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/${_dependency_sdk}/scripts/buildsystems/vcpkg.cmake"
            CACHE FILEPATH "Dependency toolchain")
    unset(_dependency_sdk)
endif()

# Reapply our dependency-only adapter on reloads too: CMakeSystem.cmake may
# have been created before automatic SDK selection on the first configure.
if (CMAKE_TOOLCHAIN_FILE STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/windows-msvc/scripts/buildsystems/vcpkg.cmake" OR
        CMAKE_TOOLCHAIN_FILE STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/windows-mingw/scripts/buildsystems/vcpkg.cmake")
    include("${CMAKE_TOOLCHAIN_FILE}")
endif()
if (MINGW_DOWNLOADER_BUNDLED_MINGW AND
        (NOT MINGW OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8))
    message(FATAL_ERROR "The bundled MinGW SDK requires x64 MinGW (win32 threads, SEH, MSVCRT).")
endif()
if (MINGW_DOWNLOADER_BUNDLED_MSVC AND
        (NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8))
    message(FATAL_ERROR "The bundled MSVC SDK requires an x64 MSVC-compatible compiler.")
endif()
# -----------------------------
# Language / standard
# -----------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------
# Options
# -----------------------------
option(MINGW_DOWNLOADER_STATIC_RUNTIME "Prefer static GCC/Stdlib where possible" ON)

# -----------------------------
# Packages (from vcpkg)
# -----------------------------
#find_package(fltk CONFIG REQUIRED)
find_package(FLTK CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(LibArchive REQUIRED)

# The SDK contains static libarchive; CMake's FindLibArchive does not add its
# private compression dependencies (the full vcpkg toolchain uses a wrapper).
if (MINGW_DOWNLOADER_BUNDLED_MINGW OR MINGW_DOWNLOADER_BUNDLED_MSVC)
    find_package(ZLIB REQUIRED)
    find_package(liblzma CONFIG REQUIRED)
    set(_sdk_prefix "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    if (MSVC)
        set(_archive_name archive.lib)
    else()
        set(_archive_name libarchive.a)
    endif()
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

# -----------------------------
# Executable
# -----------------------------
add_executable(MingwDownloader
        src/main.cpp
        # add more .cpp here if there are split files
        assets/app.rc
)

# -----------------------------
# Includes
# -----------------------------
# Put single-include json.hpp here:
#   third_party/nlohmann/json.hpp
# or:
#   include/json.hpp
target_include_directories(MingwDownloader PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# -----------------------------
# Link libs
# -----------------------------
target_link_libraries(MingwDownloader PRIVATE
        fltk_images
        fltk_forms
        fltk_gl
        fltk
        CURL::libcurl
        LibArchive::LibArchive
)
if (WIN32)
    target_link_libraries(MingwDownloader PRIVATE ole32 shell32)
endif ()

# -----------------------------
# Threading
# -----------------------------
# (Needed if you use std::thread; harmless otherwise)
# Only prefer pthread on non-Windows
if (NOT WIN32)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
endif ()
find_package(Threads REQUIRED)
target_link_libraries(MingwDownloader PRIVATE
        Threads::Threads
)

# -----------------------------
# Compile definitions (common)
# -----------------------------
target_compile_definitions(MingwDownloader PRIVATE
        UNICODE
        _UNICODE
)

# -----------------------------
# MinGW / GCC
# -----------------------------
if (MINGW)
    target_compile_options(MingwDownloader PRIVATE
            -Wall -Wextra
            $<$<CONFIG:Release>:-O2>
            $<$<CONFIG:Release>:-ffunction-sections>
            $<$<CONFIG:Release>:-fdata-sections>
    )

    target_compile_definitions(MingwDownloader PRIVATE
            NGHTTP2_STATICLIB
            CURL_STATICLIB
            NO_SSL_DL
    )

    # GUI subsystem (no console)
    target_link_options(MingwDownloader PRIVATE -mwindows)

    # Size / dead code stripping (Release)
    target_link_options(MingwDownloader PRIVATE
            $<$<CONFIG:Release>:-Wl,--gc-sections>
            $<$<CONFIG:Release>:-s>
    )

    # Optional: keep it "single exe" friendly
    if (MINGW_DOWNLOADER_STATIC_RUNTIME)
        target_link_options(MingwDownloader PRIVATE
                -static
                -static-libgcc
                -static-libstdc++
        )
    endif ()
endif ()

# -----------------------------
# MSVC
# -----------------------------
if (MSVC)
    target_compile_definitions(MingwDownloader PRIVATE
            NGHTTP2_STATICLIB
            CURL_STATICLIB
            NO_SSL_DL
    )

    # GUI subsystem (no console)
    target_link_options(MingwDownloader PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)

    # The bundled MSVC adapter selects /MT (Release) and /MTd (Debug).
endif ()