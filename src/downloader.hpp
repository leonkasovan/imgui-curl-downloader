#pragma once
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

enum class DownloadState { Idle, Running, Paused, Completed, Failed, Cancelled };

struct DownloadTask {
    std::string url;
    std::string outputPath;
    std::string filename;
    std::atomic<DownloadState> state{DownloadState::Idle};
    std::atomic<double> progress{0.0};   // 0.0 – 1.0
    std::atomic<double> speedBps{0.0};
    std::atomic<long long> bytesDownloaded{0};
    std::atomic<long long> totalBytes{-1};
    std::string errorMsg;
    std::mutex pauseMutex;
    std::condition_variable pauseCV;
    std::thread thread;

    DownloadTask(const DownloadTask&) = delete;
    DownloadTask& operator=(const DownloadTask&) = delete;

    explicit DownloadTask(std::string u, std::string out);
    ~DownloadTask();
};

class DownloadManager {
public:
    void addDownload(const std::string& url, const std::string& outputDir = ".");
    void pauseDownload(size_t idx);
    void resumeDownload(size_t idx);
    void cancelDownload(size_t idx);
    std::vector<std::unique_ptr<DownloadTask>>& tasks() { return tasks_; }
    void joinAll();
private:
    std::vector<std::unique_ptr<DownloadTask>> tasks_;
    std::mutex mutex_;
};
