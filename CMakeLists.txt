cmake_minimum_required(VERSION 3.23)

# -----------------------------
# vcpkg (single owner)
# -----------------------------
if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if (DEFINED ENV{VCPKG_ROOT})
        set(CMAKE_TOOLCHAIN_FILE
                "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
                CACHE STRING "" FORCE)
    else()
        set(CMAKE_TOOLCHAIN_FILE
                "D:/lib/vcpkg/scripts/buildsystems/vcpkg.cmake"
                CACHE STRING "" FORCE)
    endif()
endif()

# Choose triplet by toolchain
if (NOT DEFINED VCPKG_TARGET_TRIPLET)
    if (MSVC)
        set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "")
#        list(APPEND CMAKE_PREFIX_PATH "D:/lib/vcpkg/installed/x64-windows-static")
    else ()
        set(VCPKG_TARGET_TRIPLET "x64-mingw-static" CACHE STRING "")
    endif ()
endif ()

# Help MODULE-mode find_package(LibArchive) on Windows
if (WIN32 AND DEFINED VCPKG_INSTALLED_DIR)
    list(PREPEND CMAKE_PREFIX_PATH "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
endif()

project(MingwDownloader LANGUAGES C CXX)

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
find_package(fltk CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(LibArchive REQUIRED)

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

    # Optional: if you want static MSVC runtime (/MT) in Release
    # (Only do this if you really want "single exe" style for MSVC too)
    # set_property(TARGET MingwDownloader PROPERTY
    #     MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif ()