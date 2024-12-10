// include/controllers/EmotionAnalyzer.hpp
#pragma once
#include <drogon/HttpController.h>
#include <fstream>
#include <json/json.h>
#include <memory>
#include <mutex>
#include <python3.12/Python.h>
#include <string>
#include <unordered_map>

namespace api {

struct UploadSession {
  std::string fileId;
  std::string fileName;
  size_t totalSize;
  size_t uploadedSize;
  std::shared_ptr<std::ofstream> file;
  int64_t lastActivity;
};

class EmotionAnalyzer {
public:
  EmotionAnalyzer();
  ~EmotionAnalyzer();

  void handleInitialize(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void
  handleChunk(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void handleOptions(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);

private:
  static constexpr size_t MAX_FILE_SIZE = 500 * 1024 * 1024; // 500MB
  static constexpr const char *UPLOAD_DIR = "uploads";

  // Python-related members
  struct PythonState {
    PyObject *module;
    PyObject *func;
    PyGILState_STATE gilState;

    PythonState() : module(nullptr), func(nullptr) {}
  };

  std::unordered_map<std::string, UploadSession> sessions_;
  std::mutex sessionsMutex_;
  std::unique_ptr<std::thread> cleanupThread_;
  bool running_;

  // Thread-local Python state
  static thread_local std::unique_ptr<PythonState> pythonState_;

  std::string generateFileId();
  void initPythonState();
  std::string analyzeContent(const std::string &content);
  void cleanupSessions();
  void addCorsHeaders(const drogon::HttpResponsePtr &resp);
  Json::Value createErrorResponse(const std::string &message);
  Json::Value createSuccessResponse(const std::string &message = "");
};

} // namespace api
