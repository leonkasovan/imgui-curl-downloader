#include "downloader.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstring>
#include <string>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const char* stateLabel(DownloadState s) {
    switch (s) {
        case DownloadState::Idle:      return "Idle";
        case DownloadState::Running:   return "Running";
        case DownloadState::Paused:    return "Paused";
        case DownloadState::Completed: return "Completed";
        case DownloadState::Failed:    return "Failed";
        case DownloadState::Cancelled: return "Cancelled";
    }
    return "Unknown";
}

static ImVec4 stateColor(DownloadState s) {
    switch (s) {
        case DownloadState::Running:   return {0.2f, 0.8f, 0.2f, 1.0f};
        case DownloadState::Paused:    return {1.0f, 0.8f, 0.0f, 1.0f};
        case DownloadState::Completed: return {0.4f, 0.8f, 1.0f, 1.0f};
        case DownloadState::Failed:    return {1.0f, 0.3f, 0.3f, 1.0f};
        case DownloadState::Cancelled: return {0.6f, 0.6f, 0.6f, 1.0f};
        default:                       return {1.0f, 1.0f, 1.0f, 1.0f};
    }
}

static std::string formatSize(long long bytes) {
    if (bytes < 0)     return "? MB";
    if (bytes < 1024)  return std::to_string(bytes) + " B";
    double kb = bytes / 1024.0;
    if (kb < 1024.0) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f KB", kb);
        return buf;
    }
    double mb = kb / 1024.0;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f MB", mb);
    return buf;
}

static std::string formatSpeed(double bps) {
    if (bps <= 0.0) return "0 KB/s";
    double kbps = bps / 1024.0;
    if (kbps < 1024.0) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f KB/s", kbps);
        return buf;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f MB/s", kbps / 1024.0);
    return buf;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    // ---- GLFW init ----
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(1280, 720, "ImGui Downloader", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // vsync

    // ---- ImGui init ----
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // ---- App state ----
    DownloadManager manager;
    static char urlBuf[2048] = {};
    static char dirBuf[1024] = ".";

    // ---- Main loop ----
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Full-screen window
        {
            int dispW, dispH;
            glfwGetFramebufferSize(window, &dispW, &dispH);
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(static_cast<float>(dispW), static_cast<float>(dispH)));
        }

        ImGui::Begin("Downloader", nullptr,
                     ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoResize   |
                     ImGuiWindowFlags_NoMove     |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);

        // ---- URL input row ----
        ImGui::Text("URL:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 200.0f);
        ImGui::InputText("##url", urlBuf, sizeof(urlBuf));
        ImGui::SameLine();
        ImGui::Text("Dir:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputText("##dir", dirBuf, sizeof(dirBuf));
        ImGui::SameLine();
        if (ImGui::Button("Add") && urlBuf[0] != '\0') {
            manager.addDownload(std::string(urlBuf), std::string(dirBuf));
            urlBuf[0] = '\0';
        }

        ImGui::Separator();

        // ---- Download list ----
        auto& tasks = manager.tasks();
        for (size_t i = 0; i < tasks.size(); ++i) {
            auto& task = tasks[i];
            DownloadState state = task->state.load();

            ImGui::PushID(static_cast<int>(i));

            // Filename header
            ImGui::TextUnformatted(task->filename.c_str());

            // Progress bar with overlay text
            long long dlnow  = task->bytesDownloaded.load();
            long long dltotal = task->totalBytes.load();
            double    speed  = task->speedBps.load();
            float     prog   = static_cast<float>(task->progress.load());

            char overlay[128];
            std::snprintf(overlay, sizeof(overlay), "%s / %s  |  %s",
                          formatSize(dlnow).c_str(),
                          formatSize(dltotal).c_str(),
                          formatSpeed(speed).c_str());
            ImGui::ProgressBar(prog, ImVec2(-1.0f, 0.0f), overlay);

            // State label (colored)
            ImGui::TextColored(stateColor(state), "%s", stateLabel(state));

            if (!task->errorMsg.empty()) {
                ImGui::SameLine();
                ImGui::TextColored({1.0f, 0.3f, 0.3f, 1.0f}, " - %s", task->errorMsg.c_str());
            }

            // Control buttons
            if (state == DownloadState::Running) {
                if (ImGui::Button("Pause"))
                    manager.pauseDownload(i);
                ImGui::SameLine();
            }
            if (state == DownloadState::Paused) {
                if (ImGui::Button("Resume"))
                    manager.resumeDownload(i);
                ImGui::SameLine();
            }
            if (state != DownloadState::Completed && state != DownloadState::Cancelled) {
                if (ImGui::Button("Cancel"))
                    manager.cancelDownload(i);
            }

            ImGui::Separator();
            ImGui::PopID();
        }

        ImGui::End();

        // ---- Render ----
        ImGui::Render();
        int dispW, dispH;
        glfwGetFramebufferSize(window, &dispW, &dispH);
        glViewport(0, 0, dispW, dispH);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // ---- Cleanup ----
    manager.joinAll();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
