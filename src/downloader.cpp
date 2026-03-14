#include "downloader.hpp"

#include <curl/curl.h>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string extractFilename(const std::string& url) {
    auto pos = url.rfind('/');
    if (pos != std::string::npos && pos + 1 < url.size())
        return url.substr(pos + 1);
    return "download";
}

// Write callback: appends received data to the output file stream
static size_t writeCallback(void* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* ofs = static_cast<std::ofstream*>(userdata);
    const size_t bytes = size * nmemb;
    ofs->write(static_cast<const char*>(ptr), static_cast<std::streamsize>(bytes));
    return bytes;
}

// Progress callback: updates progress/speed, handles pause and cancel
struct ProgressData {
    DownloadTask* task;
};

static int progressCallback(void* userdata,
                             curl_off_t dltotal, curl_off_t dlnow,
                             curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
    auto* pd = static_cast<ProgressData*>(userdata);
    DownloadTask* task = pd->task;

    // Check for cancellation
    if (task->state.load() == DownloadState::Cancelled)
        return 1;  // non-zero aborts curl

    // Handle pause: block until resumed or cancelled
    {
        std::unique_lock<std::mutex> lock(task->pauseMutex);
        task->pauseCV.wait(lock, [task] {
            auto s = task->state.load();
            return s != DownloadState::Paused;
        });
    }

    // After unblocking, check again for cancellation
    if (task->state.load() == DownloadState::Cancelled)
        return 1;

    // Update progress metrics
    task->bytesDownloaded.store(static_cast<long long>(dlnow));
    task->totalBytes.store(static_cast<long long>(dltotal));
    if (dltotal > 0)
        task->progress.store(static_cast<double>(dlnow) / static_cast<double>(dltotal));

    return 0;
}

// ---------------------------------------------------------------------------
// DownloadTask
// ---------------------------------------------------------------------------

DownloadTask::DownloadTask(std::string u, std::string out)
    : url(std::move(u)), outputPath(std::move(out)) {
    filename = extractFilename(url);
}

DownloadTask::~DownloadTask() {
    // Signal cancellation so the thread unblocks if paused
    state.store(DownloadState::Cancelled);
    pauseCV.notify_all();
    if (thread.joinable())
        thread.join();
}

// ---------------------------------------------------------------------------
// Thread worker
// ---------------------------------------------------------------------------

static void downloadThread(DownloadTask* task) {
    task->state.store(DownloadState::Running);

    fs::path outFile = fs::path(task->outputPath) / task->filename;

    std::ofstream ofs(outFile, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        task->errorMsg = "Cannot open output file: " + outFile.string();
        task->state.store(DownloadState::Failed);
        return;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        task->errorMsg = "curl_easy_init() failed";
        task->state.store(DownloadState::Failed);
        return;
    }

    ProgressData pd{task};

    curl_easy_setopt(curl, CURLOPT_URL, task->url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &pd);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ofs);

    CURLcode res = curl_easy_perform(curl);

    double speed = 0.0;
    curl_easy_getinfo(curl, CURLINFO_SPEED_DOWNLOAD, &speed);
    task->speedBps.store(speed);

    curl_easy_cleanup(curl);
    ofs.close();

    if (task->state.load() == DownloadState::Cancelled) {
        // Remove partial file on cancel
        std::error_code ec;
        fs::remove(outFile, ec);
        return;
    }

    if (res != CURLE_OK) {
        task->errorMsg = curl_easy_strerror(res);
        task->state.store(DownloadState::Failed);
    } else {
        task->progress.store(1.0);
        task->state.store(DownloadState::Completed);
    }
}

// ---------------------------------------------------------------------------
// DownloadManager
// ---------------------------------------------------------------------------

void DownloadManager::addDownload(const std::string& url, const std::string& outputDir) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto task = std::make_unique<DownloadTask>(url, outputDir);
    task->thread = std::thread(downloadThread, task.get());
    tasks_.push_back(std::move(task));
}

void DownloadManager::pauseDownload(size_t idx) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (idx < tasks_.size() && tasks_[idx]->state.load() == DownloadState::Running)
        tasks_[idx]->state.store(DownloadState::Paused);
    // No need to notify pauseCV — the thread will block on next progress callback
}

void DownloadManager::resumeDownload(size_t idx) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (idx < tasks_.size() && tasks_[idx]->state.load() == DownloadState::Paused) {
        tasks_[idx]->state.store(DownloadState::Running);
        tasks_[idx]->pauseCV.notify_all();
    }
}

void DownloadManager::cancelDownload(size_t idx) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (idx < tasks_.size()) {
        auto& task = tasks_[idx];
        auto s = task->state.load();
        if (s != DownloadState::Completed && s != DownloadState::Cancelled) {
            task->state.store(DownloadState::Cancelled);
            task->pauseCV.notify_all();
        }
    }
}

void DownloadManager::joinAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& task : tasks_) {
        if (task->thread.joinable())
            task->thread.join();
    }
}
