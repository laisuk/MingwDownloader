#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Progress.H>
#include <FL/Fl_Box.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Native_File_Chooser.H>

#include <curl/curl.h>
#include "json.hpp"

#include <thread>
#include <atomic>
#include <vector>
#include <string>

#include <archive.h>
#include <archive_entry.h>
#include <filesystem>
#include <stdexcept>

using json = nlohmann::json;

// ============================================================
// Data Structures
// ============================================================

enum class Arch { Any, I686, X86_64 };

enum class MRT { Any, Posix, Win32, Mcf };

enum class EXC { Any, Seh, Dwarf };

enum class CRT { Any, Ucrt, Msvcrt };

enum class RT { Any, V13 };

struct AssetInfo {
    Arch arch = Arch::Any;
    MRT mrt = MRT::Any;
    EXC exc = EXC::Any;
    CRT crt = CRT::Any;
    RT rt = RT::Any;
};

static bool has_token(const std::string &s, const char *tok) {
    return s.find(tok) != std::string::npos;
}

static AssetInfo parse_asset_name(const std::string &name) {
    AssetInfo info{};

    // Arch (prefix)
    if (name.rfind("i686-", 0) == 0) info.arch = Arch::I686;
    else if (name.rfind("x86_64-", 0) == 0) info.arch = Arch::X86_64;

    // MRT
    if (has_token(name, "-posix-")) info.mrt = MRT::Posix;
    else if (has_token(name, "-win32-")) info.mrt = MRT::Win32;
    else if (has_token(name, "-mcf-")) info.mrt = MRT::Mcf;

    // EXC
    if (has_token(name, "-seh-")) info.exc = EXC::Seh;
    else if (has_token(name, "-dwarf-")) info.exc = EXC::Dwarf;

    // CRT
    if (has_token(name, "-ucrt-")) info.crt = CRT::Ucrt;
    else if (has_token(name, "-msvcrt-")) info.crt = CRT::Msvcrt;

    // RT
    if (has_token(name, "-rt_v13-") || has_token(name, "-rt_v13.")) info.rt = RT::V13;

    return info;
}

struct Filters {
    Arch arch = Arch::Any;
    MRT mrt = MRT::Any;
    EXC exc = EXC::Any;
    CRT crt = CRT::Any;
    RT rt = RT::Any;
};

static Filters gFilters{};

struct Asset {
    std::string name;
    long long size = 0;
    std::string url;
    AssetInfo info; // ✅ add
};

struct Release {
    std::string tag;
    std::string published_at;
    std::vector<Asset> assets;
};

static std::vector<Release> gReleases;

// ============================================================
// UI Globals
// ============================================================

static Fl_Choice *gRelease = nullptr;
static Fl_Hold_Browser *gAssets = nullptr;
static Fl_Progress *gProgress = nullptr;
static Fl_Box *gStatus = nullptr;

static std::atomic<bool> gCancel{false};
static std::atomic<int> gLastCurlResult{0}; // stores CURLcode
static std::atomic gProgressValue{0.0}; // progress %
static std::atomic<int> gRefreshStage{0};
// 0 = none
// 1 = network error
// 2 = JSON parse error
// 3 = success (releases already in gReleases)

static std::atomic<bool> gDoExtract{false};
static std::atomic<int> gExtractOk{0}; // 0=none, 1=ok, -1=fail
static std::string gExtractErr;

static std::atomic<int> gExtractTotal{0};
static std::atomic<int> gExtractDone{0};
static std::atomic<int> gUiMode{0}; // 0=download, 1=extract

// ============================================================
// Utility
// ============================================================

static void set_status(const std::string &s) {
    gStatus->copy_label(s.c_str());
    gStatus->redraw();
}

static size_t write_callback(void *contents, const size_t size, const size_t nMemB, void *user_p) {
    const size_t total = size * nMemB;
    const auto s = static_cast<std::string *>(user_p);
    s->append(static_cast<char *>(contents), total);
    return total;
}

// ============================================================
// Fetch GitHub Releases
// ============================================================

std::string fetch_releases_json() {
    CURL *curl = curl_easy_init();
    if (!curl) return {};

    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL,
                     "https://api.github.com/repos/niXman/mingw-builds-binaries/releases");
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "mingw-downloader-fltk");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    const CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        return {};

    return response;
}

// ============================================================
// JSON Parse
// ============================================================

bool parse_releases(const std::string &data) {
    gReleases.clear();

    try {
        json j = json::parse(data);

        for (auto &r: j) {
            Release rel;
            rel.tag = r.value("tag_name", "");
            rel.published_at = r.value("published_at", "");

            if (r.contains("assets") && r["assets"].is_array()) {
                for (auto &a: r["assets"]) {
                    Asset asset;
                    asset.name = a.value("name", "");
                    asset.size = a.value("size", 0LL);
                    asset.url = a.value("browser_download_url", "");
                    asset.info = parse_asset_name(asset.name);

                    if (!asset.name.empty())
                        rel.assets.push_back(std::move(asset));
                }
            }

            if (!rel.tag.empty())
                gReleases.push_back(std::move(rel));
        }
    } catch (...) {
        return false;
    }

    return true;
}

// ============================================================
// UI Populate
// ============================================================

static Fl_Choice *gArch = nullptr;
static Fl_Choice *gMrt = nullptr;
static Fl_Choice *gExc = nullptr;
static Fl_Choice *gCrt = nullptr;
static Fl_Choice *gRt = nullptr;

static std::vector<int> gAssetIndexMap; // list row -> release.assets[index]

template<typename T>
static bool match_filter(T want, T got) {
    return want == T::Any || want == got;
}

static bool asset_matches(const Asset &a) {
    return match_filter(gFilters.arch, a.info.arch)
           && match_filter(gFilters.mrt, a.info.mrt)
           && match_filter(gFilters.exc, a.info.exc)
           && match_filter(gFilters.crt, a.info.crt)
           && match_filter(gFilters.rt, a.info.rt);
}

static void rebuild_asset_list_for_release(const int r_idx) {
    gAssets->clear();
    gAssetIndexMap.clear();

    if (r_idx < 0 || r_idx >= static_cast<int>(gReleases.size())) return;

    const auto &rel = gReleases[r_idx];
    for (int i = 0; i < static_cast<int>(rel.assets.size()); ++i) {
        if (const auto &a = rel.assets[i]; asset_matches(a)) {
            gAssets->add(a.name.c_str());
            gAssetIndexMap.push_back(i);
        }
    }
}

// -------

void populate_release_choice() {
    gRelease->clear();

    for (auto &r: gReleases) {
        std::string label = r.tag + "  (" +
                            (r.published_at.size() >= 10 ? r.published_at.substr(0, 10) : "") + ")";
        gRelease->add(label.c_str());
    }

    if (!gReleases.empty()) {
        gRelease->value(0);
        rebuild_asset_list_for_release(0);
    }
}

static void on_release_changed(Fl_Widget *, void *) {
    rebuild_asset_list_for_release(gRelease->value());
}

static void on_filters_changed(Fl_Widget *, void *) {
    // Arch
    switch (gArch->value()) {
        case 0: gFilters.arch = Arch::Any;
            break;
        case 1: gFilters.arch = Arch::I686;
            break;
        case 2: gFilters.arch = Arch::X86_64;
            break;
        default: ;
    }
    // MRT
    switch (gMrt->value()) {
        case 0: gFilters.mrt = MRT::Any;
            break;
        case 1: gFilters.mrt = MRT::Posix;
            break;
        case 2: gFilters.mrt = MRT::Win32;
            break;
        case 3: gFilters.mrt = MRT::Mcf;
            break;
        default: ;
    }
    // EXC
    switch (gExc->value()) {
        case 0: gFilters.exc = EXC::Any;
            break;
        case 1: gFilters.exc = EXC::Seh;
            break;
        case 2: gFilters.exc = EXC::Dwarf;
            break;
        default: ;
    }
    // CRT
    switch (gCrt->value()) {
        case 0: gFilters.crt = CRT::Any;
            break;
        case 1: gFilters.crt = CRT::Ucrt;
            break;
        case 2: gFilters.crt = CRT::Msvcrt;
            break;
        default: ;
    }
    // RT
    switch (gRt->value()) {
        case 0: gFilters.rt = RT::Any;
            break;
        case 1: gFilters.rt = RT::V13;
            break;
        default: ;
    }

    rebuild_asset_list_for_release(gRelease->value());
}

static void on_reset_filters(Fl_Widget *, void *) {
    gFilters = Filters{};
    gArch->value(0);
    gMrt->value(0);
    gExc->value(0);
    gCrt->value(0);
    gRt->value(0);
    rebuild_asset_list_for_release(gRelease->value());
}

// ============================================================
// Download with Progress
// ============================================================

static void awake_download_done(void *) {
    if (const int res = gLastCurlResult.load(); res == CURLE_OK) set_status("Download complete.");
    else set_status("Download failed or cancelled.");
    gProgress->value(0);
    gProgress->redraw();
}

static void awake_update_progress(void *) {
    gProgress->value(gProgressValue.load());
    gProgress->redraw();
}

static void awake_update_extract_progress(void *) {
    const int total = gExtractTotal.load();
    const int done = gExtractDone.load();

    if (total <= 0) {
        // Unknown total => show 0 (or you can "pulse" if you implement a timer)
        gProgress->value(0);
    } else {
        const double percent = static_cast<double>(done) / static_cast<double>(total) * 100.0;
        gProgress->value(percent);
    }
    gProgress->redraw();
}

static int progress_callback(void *,
                             const curl_off_t total, const curl_off_t now,
                             curl_off_t, curl_off_t) {
    if (gCancel) return 1; // abort

    if (total > 0) {
        gProgressValue = static_cast<double>(now) / static_cast<double>(total) * 100.0;
        Fl::awake(awake_update_progress);
    }
    return 0;
}

static int count_archive_entries(const std::string &archivePath, std::string &err) {
    archive *ar = archive_read_new();
    if (!ar) {
        err = "libarchive init failed";
        return -1;
    }

    archive_read_support_format_7zip(ar);
    archive_read_support_format_zip(ar);
    archive_read_support_filter_all(ar);

    int r = archive_read_open_filename(ar, archivePath.c_str(), 10240);
    if (r != ARCHIVE_OK) {
        err = archive_error_string(ar) ? archive_error_string(ar) : "open archive failed";
        archive_read_free(ar);
        return -1;
    }

    int count = 0;
    archive_entry *entry = nullptr;
    while ((r = archive_read_next_header(ar, &entry)) == ARCHIVE_OK) {
        ++count;
        archive_read_data_skip(ar);
    }

    if (r != ARCHIVE_EOF) {
        err = archive_error_string(ar) ? archive_error_string(ar) : "count header failed";
        archive_read_free(ar);
        return -1;
    }

    archive_read_close(ar);
    archive_read_free(ar);
    return count;
}

#ifdef _WIN32
#include <windows.h>

static std::string pick_output_dir() {
    std::string result;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return {};

    IFileDialog *pfd = nullptr;

    hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr,
                          CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(&pfd));
    if (SUCCEEDED(hr)) {
        DWORD options;
        hr = pfd->GetOptions(&options);
        if (FAILED(hr)) {
            pfd->Release();
            CoUninitialize();
            return {};
        }

        hr = pfd->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        if (FAILED(hr)) {
            pfd->Release();
            CoUninitialize();
            return {};
        }

        hr = pfd->Show(nullptr);
        if (SUCCEEDED(hr)) {
            IShellItem *psi = nullptr;
            if (SUCCEEDED(pfd->GetResult(&psi))) {
                PWSTR path = nullptr;
                if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                    char buffer[MAX_PATH];
                    WideCharToMultiByte(CP_UTF8, 0,
                                        path, -1,
                                        buffer, MAX_PATH,
                                        nullptr, nullptr);
                    result = buffer;
                    CoTaskMemFree(path);
                }
                psi->Release();
            }
        }
        pfd->Release();
    }

    CoUninitialize();
    return result;
}
#endif

// ------ Extraction ------

static int copy_archive_data(archive *ar, archive *aw) {
    const void *buff = nullptr;
    size_t size = 0;
    la_int64_t offset = 0;

    for (;;) {
        int r = archive_read_data_block(ar, &buff, &size, &offset);
        if (r == ARCHIVE_EOF) return ARCHIVE_OK;
        if (r != ARCHIVE_OK) return r;

        r = archive_write_data_block(aw, buff, size, offset);
        if (r != ARCHIVE_OK) return r;
    }
}

static std::filesystem::path safe_join(const std::filesystem::path &base,
                                       const std::filesystem::path &rel) {
    auto out = (base / rel).lexically_normal();
    const auto baseN = base.lexically_normal();

    // Block traversal: ensure normalized output starts with base
    const auto baseStr = baseN.native();
    if (const auto outStr = out.native(); outStr.size() < baseStr.size() || outStr.compare(0, baseStr.size(), baseStr)
                                          != 0) {
        throw std::runtime_error("Blocked path traversal in archive entry");
    }
    return out;
}

static bool extract_archive_to_dir(const std::string &archivePath,
                                   const std::string &outDir,
                                   std::string &err) {
    gExtractDone = 0;
    Fl::awake(awake_update_extract_progress);

    namespace fs = std::filesystem;
    try {
        int doneCount = 0;
        const fs::path base(outDir);
        fs::create_directories(base);

        archive *ar = archive_read_new();
        archive *aw = archive_write_disk_new();
        if (!ar || !aw) {
            err = "libarchive init failed";
            if (ar) archive_read_free(ar);
            if (aw) archive_write_free(aw);
            return false;
        }

        archive_read_support_format_7zip(ar);
        archive_read_support_format_zip(ar);
        archive_read_support_filter_all(ar);

        archive_write_disk_set_options(aw,
                                       ARCHIVE_EXTRACT_TIME |
                                       ARCHIVE_EXTRACT_PERM |
                                       ARCHIVE_EXTRACT_ACL |
                                       ARCHIVE_EXTRACT_FFLAGS);
        archive_write_disk_set_standard_lookup(aw);

        int r = archive_read_open_filename(ar, archivePath.c_str(), 10240);
        if (r != ARCHIVE_OK) {
            err = archive_error_string(ar) ? archive_error_string(ar) : "open archive failed";
            archive_read_free(ar);
            archive_write_free(aw);
            return false;
        }

        archive_entry *entry = nullptr;
        while ((r = archive_read_next_header(ar, &entry)) == ARCHIVE_OK) {
            const char *p = archive_entry_pathname(entry);
            if (!p || !*p) {
                archive_read_data_skip(ar);
                continue;
            }

            fs::path rel(p);

            // block absolute paths
            if (rel.is_absolute()) {
                archive_read_data_skip(ar);
                continue;
            }

            // build safe output path
            fs::path full = safe_join(base, rel);
            archive_entry_set_pathname(entry, full.string().c_str());

            r = archive_write_header(aw, entry);
            if (r == ARCHIVE_OK) {
                r = copy_archive_data(ar, aw);
                if (r != ARCHIVE_OK) {
                    err = archive_error_string(ar) ? archive_error_string(ar) : "extract data failed";
                    archive_read_free(ar);
                    archive_write_free(aw);
                    return false;
                }
            } else {
                // header write failed; skip data to continue
                archive_read_data_skip(ar);
            }

            archive_write_finish_entry(aw);
            // --- added ---
            ++doneCount;
            gExtractDone = doneCount;
            Fl::awake(awake_update_extract_progress);
        }

        if (r != ARCHIVE_EOF) {
            err = archive_error_string(ar) ? archive_error_string(ar) : "read header failed";
            archive_read_free(ar);
            archive_write_free(aw);
            return false;
        }

        archive_read_close(ar);
        archive_read_free(ar);
        archive_write_close(aw);
        archive_write_free(aw);
        return true;
    } catch (const std::exception &ex) {
        err = ex.what();
        return false;
    }
}

static void awake_extract_done(void *) {
    if (gExtractOk.load() == 1) {
        set_status("Extract complete.");
    } else if (gExtractOk.load() == -1) {
        set_status(std::string("Extract failed: " + gExtractErr));
    }
}

// ------ Extraction End ------
// Note: Do not set const for pointer
static size_t file_write_cb(void *ptr, const size_t size, const size_t nMemBytes, void *userdata) {
    const auto fp = static_cast<FILE *>(userdata);
    return fwrite(ptr, size, nMemBytes, fp);
}

static std::string gUiText;

static void awake_set_status(void *) {
    set_status(gUiText);
}

static void post_status(const char *s) {
    gUiText = s;
    Fl::awake(awake_set_status);
}

void download_file(const std::string &url, const std::string &outPath) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        gLastCurlResult = static_cast<int>(CURLE_FAILED_INIT);
        Fl::awake(awake_download_done);
        return;
    }

    FILE *fp = fopen(outPath.c_str(), "wb");
    if (!fp) {
        curl_easy_cleanup(curl);
        gLastCurlResult = static_cast<int>(CURLE_WRITE_ERROR);
        Fl::awake(awake_download_done);
        return;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "mingw-downloader-fltk");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // write
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, file_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

    // progress + cancel
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    const CURLcode res = curl_easy_perform(curl);

    fclose(fp);
    curl_easy_cleanup(curl);

    gLastCurlResult = static_cast<int>(res);
    Fl::awake(awake_download_done);

    // Extract
    // Optional extract: out_dir / artifact_name /
    if (res == CURLE_OK && gDoExtract.load()) {
        namespace fs = std::filesystem;

        const fs::path ap(outPath);
        const fs::path outDir = ap.parent_path();
        const fs::path artifactName = ap.stem();
        const fs::path extractDir = outDir / artifactName;

        // ---- PASS 1: COUNT ENTRIES ----
        post_status("Counting archive entries...");
        gProgress->value(0);
        gProgress->redraw();

        std::string c_err;

        if (const int total = count_archive_entries(ap.string(), c_err); total > 0) {
            gExtractTotal = total;
            gExtractDone = 0;
            Fl::awake(awake_update_extract_progress);
        } else {
            // fallback if count fails
            gExtractTotal = 0;
        }

        // ---- PASS 2: EXTRACT ----
        post_status("Extracting...");

        if (std::string err; extract_archive_to_dir(ap.string(), extractDir.string(), err)) {
            gExtractOk = 1;
        } else {
            gExtractOk = -1;
            gExtractErr = err;
        }

        Fl::awake(awake_extract_done);
    }
}

// ============================================================
// Button Callbacks
// ============================================================
static void awake_refresh_done(void *) {
    const int st = gRefreshStage.load();

    if (st == 1) {
        set_status("Network error.");
        return;
    }
    if (st == 2) {
        set_status("JSON parse error.");
        return;
    }
    if (st == 3) {
        populate_release_choice();
        set_status("Releases loaded.");
    }
}

static void on_refresh(Fl_Widget *, void *) {
    set_status("Fetching releases...");
    gProgress->value(0);

    std::thread([] {
        const std::string data = fetch_releases_json();
        if (data.empty()) {
            gRefreshStage = 1;
            Fl::awake(awake_refresh_done);
            return;
        }

        if (const bool ok = parse_releases(data); !ok) {
            gRefreshStage = 2;
            Fl::awake(awake_refresh_done);
            return;
        }

        gRefreshStage = 3;
        Fl::awake(awake_refresh_done);
    }).detach();
}

static void start_download(const bool extract_after) {
    const int r_idx = gRelease->value();
    const int a_row = gAssets->value();

    if (r_idx < 0 || a_row <= 0) {
        fl_alert("Select release and asset first.");
        return;
    }

    // If you have filters enabled, use gAssetIndexMap:
    int real_idx = a_row - 1;
    if (!gAssetIndexMap.empty()) {
        if (static_cast<size_t>(a_row - 1) >= gAssetIndexMap.size()) {
            fl_alert("Invalid selection.");
            return;
        }
        real_idx = gAssetIndexMap[static_cast<size_t>(a_row - 1)];
    }

    const auto &asset = gReleases[r_idx].assets[static_cast<size_t>(real_idx)];

    const std::string outDir = pick_output_dir();
    if (outDir.empty()) {
        set_status("Cancelled.");
        return;
    }

    std::string outPath = outDir;
    if (!outPath.empty() && outPath.back() != '\\' && outPath.back() != '/')
        outPath += "\\";
    outPath += asset.name;

    gCancel = false;
    gDoExtract = extract_after;
    gExtractOk = 0;
    gExtractErr.clear();

    set_status(extract_after ? "Downloading (then extract)..." : "Downloading...");
    gProgress->value(0);

    std::thread([asset, outPath] {
        download_file(asset.url, outPath);
    }).detach();
}

static void on_download(Fl_Widget *, void *) {
    start_download(false);
}

static void on_download_extract(Fl_Widget *, void *) {
    start_download(true);
}

static void on_cancel(Fl_Widget *, void *) {
    gCancel = true;
    set_status("Cancel requested...");
}

// ============================================================
// Main
// ============================================================
#ifdef _WIN32
#include <FL/x.H>
#endif

static void center_window(Fl_Window &win) {
    int sx, sy, sw, sh;
    Fl::screen_xywh(sx, sy, sw, sh);
    win.position(sx + (sw - win.w()) / 2,
                 sy + (sh - win.h()) / 2);
}

int main() {
    Fl::scheme("gtk+");
    Fl::set_color(FL_BACKGROUND_COLOR, 245, 245, 245);

    Fl::lock();
    curl_global_init(CURL_GLOBAL_DEFAULT);

    constexpr int W = 860;
    constexpr int H = 480;
    Fl_Window win(W, H, "MinGW Builds Downloader");

    // ---- layout constants ----
    constexpr int M = 12; // outer margin
    constexpr int GAP = 10; // spacing
    constexpr int ROW1_H = 28;
    constexpr int FILTER_LABEL_H = 16;
    constexpr int FILTER_BOX_H = 26;
    constexpr int FILTER_TOTAL_H = FILTER_LABEL_H + FILTER_BOX_H + 4;
    constexpr int BTN_W = 110;

    constexpr int x0 = M;
    constexpr int y0 = M;

    // =========================
    // Row 1: Release + Refresh
    // =========================
    constexpr int releaseLabelW = 60;
    constexpr int releaseX = x0 + releaseLabelW;
    constexpr int releaseY = y0;
    constexpr int releaseW = W - M - releaseX - GAP - BTN_W;
    constexpr int releaseH = ROW1_H;

    gRelease = new Fl_Choice(releaseX, releaseY, releaseW, releaseH, "Release:");
    gRelease->align(FL_ALIGN_LEFT);
    gRelease->callback(on_release_changed);

    auto *btnRefresh = new Fl_Button(releaseX + releaseW + GAP, releaseY, BTN_W, releaseH, "Refresh");
    btnRefresh->callback(on_refresh);

    // =========================
    // Row 2: Filters + Reset
    // =========================
    constexpr int filterY = releaseY + releaseH + 14;
    constexpr int resetW = 80;

    constexpr int filterAreaW = W - 2 * M - resetW - GAP;
    constexpr int colW = (filterAreaW - 4 * GAP) / 5;

    int fx = x0;

    auto make_filter = [&](Fl_Choice *&c, const char *label, const char *items) {
        // Create label manually (cleaner than FLTK auto top-align)
        auto *lbl = new Fl_Box(fx, filterY, colW, FILTER_LABEL_H, label);
        lbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        c = new Fl_Choice(fx, filterY + FILTER_LABEL_H + 2,
                          colW, FILTER_BOX_H);
        c->add(items);
        c->value(0);
        c->callback(on_filters_changed);

        fx += colW + GAP;
    };

    make_filter(gArch, "Arch", "Any|i686|x86_64");
    make_filter(gMrt, "MRT", "Any|posix|win32|mcf");
    make_filter(gExc, "EXC", "Any|seh|dwarf");
    make_filter(gCrt, "CRT", "Any|ucrt|msvcrt");
    make_filter(gRt, "RT", "Any|rt_v13");

    auto *btnReset = new Fl_Button(W - M - resetW, filterY + FILTER_LABEL_H + 2, resetW, FILTER_BOX_H, "Reset");
    btnReset->callback(on_reset_filters);

    // =========================
    // Assets list (fills)
    // =========================
    constexpr int listY = filterY + FILTER_TOTAL_H + 12;
    constexpr int listH = 280;
    gAssets = new Fl_Hold_Browser(x0, listY, W - 2 * M, listH);
    gAssets->textfont(FL_HELVETICA);
    gAssets->textsize(16);

    // =========================
    // Bottom row: buttons + progress
    // =========================
    constexpr int bottomY = listY + listH + 10;
    constexpr int btnH = 30;

    auto *btnDownload = new Fl_Button(x0, bottomY, 160, btnH, "Download");
    btnDownload->callback(on_download);

    auto *btnDownloadExtract = new Fl_Button(x0 + 160 + GAP, bottomY, 180, btnH, "Download + Extract");
    btnDownloadExtract->callback(on_download_extract);

    auto *btnCancel = new Fl_Button(x0 + 160 + GAP + 180 + GAP, bottomY, 90, btnH, "Cancel");
    btnCancel->callback(on_cancel);

    // progress starts after buttons
    constexpr int progX = x0 + 160 + GAP + 180 + GAP + 90 + GAP;
    constexpr int progW = W - M - progX;
    gProgress = new Fl_Progress(progX, bottomY, progW, btnH);
    gProgress->minimum(0);
    gProgress->maximum(100);

    // =========================
    // Status bar
    // =========================
    constexpr int statusY = bottomY + btnH + 8;
    gStatus = new Fl_Box(x0, statusY, W - 2 * M, 24, "Ready.");
    gStatus->box(FL_THIN_DOWN_BOX);
    gStatus->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    win.end();
    win.resizable(win);
    center_window(win);
    win.show();

#ifdef _WIN32
    if (HICON hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(101))) {
        const HWND hwnd = fl_xid(&win);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
        SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
    }
#endif

    const int result = Fl::run();
    curl_global_cleanup();
    return result;
}
