cmake_minimum_required(VERSION 3.23)

# -----------------------------
# vcpkg (single owner)
# -----------------------------
set(CMAKE_TOOLCHAIN_FILE "D:/lib/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "" FORCE)

set(VCPKG_TARGET_TRIPLET "x64-mingw-static"
        CACHE STRING "" FORCE)

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
        ole32
        shell32
)

# -----------------------------
# Threading
# -----------------------------
# (Needed if you use std::thread; harmless otherwise)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(MingwDownloader PRIVATE
        Threads::Threads
)

find_package(LibArchive REQUIRED)
target_link_libraries(MingwDownloader PRIVATE LibArchive::LibArchive)

# -----------------------------
# Compile definitions / warnings
# -----------------------------
target_compile_definitions(MingwDownloader PRIVATE
        UNICODE
        _UNICODE
)

if (MINGW)
    target_compile_options(MingwDownloader PRIVATE
            -Wall -Wextra
    )

    # Try to keep it "single exe" friendly: static libgcc/libstdc++
    # (This does not guarantee fully static if some deps are dynamic,
    # but for x64-mingw-static vcpkg triplet it usually works well.)
    if (MINGW_DOWNLOADER_STATIC_RUNTIME)
        target_link_options(MingwDownloader PRIVATE
                -static-libgcc
                -static-libstdc++
        )
    endif()
endif()

# -----------------------------
# Windows subsystem (no console)
# -----------------------------
if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
            NGHTTP2_STATICLIB
            CURL_STATICLIB
            NO_SSL_DL
    )

    target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Release>:-O2>
            $<$<CONFIG:Release>:-ffunction-sections>
            $<$<CONFIG:Release>:-fdata-sections>
    )

    # Keep these as link options (cleaner than mixing into libraries)
    target_link_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Release>:-Wl,--gc-sections>
            $<$<CONFIG:Release>:-s>
            -mwindows
            -static
    )
endif()