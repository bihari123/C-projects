// src/main.cpp
#include "../include/controllers/EmotionAnalyzer.hpp"
#include <drogon/drogon.h>
#include <filesystem>
#include <iostream>

int main() {
  // Create required directories
  std::filesystem::create_directories("uploads");
  std::filesystem::create_directories("logs");

  // Load configuration
  try {
    // Configure Drogon app
    drogon::app()
        .setLogPath("./logs")
        .setLogLevel(trantor::Logger::kInfo)
        .setUploadPath("./uploads")
        .setDocumentRoot("./uploads")
        .setThreadNum(16)
        .setMaxConnectionNum(100000)
        .setMaxConnectionNumPerIP(0)
        .setIdleConnectionTimeout(60)
        // .setMaxBodySize(500 * 1024 * 1024) // 500MB
        .addListener("0.0.0.0", 8080);

    // Initialize Python paths
    auto sysPath = std::filesystem::current_path().string();
    setenv("PYTHONPATH", sysPath.c_str(), 1);

    LOG_INFO << "Server configuration completed";
    LOG_INFO << "Server running on http://0.0.0.0:8080";
    LOG_INFO << "Upload directory: " << std::filesystem::absolute("uploads");
    LOG_INFO << "Python path: " << sysPath;

    // Start the server
    drogon::app().run();

  } catch (const std::exception &e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
