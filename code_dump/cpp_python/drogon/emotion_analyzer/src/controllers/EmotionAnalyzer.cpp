// src/controllers/EmotionAnalyzer.cpp
#include "../../include/controllers/EmotionAnalyzer.hpp"
#include <chrono>
#include <drogon/HttpResponse.h>
#include <filesystem>
#include <fstream>
#include <json/json.h>
#include <random>
#include <sstream>
#include <trantor/utils/Logger.h>

namespace api {

thread_local std::unique_ptr<EmotionAnalyzer::PythonState>
    EmotionAnalyzer::pythonState_;

EmotionAnalyzer::EmotionAnalyzer() : running_(true) {
  // Initialize Python
  if (Py_IsInitialized() == 0) {
    Py_Initialize();
    PyEval_InitThreads();
    PyEval_SaveThread();
  }

  // Create upload directory
  std::filesystem::create_directories(UPLOAD_DIR);

  // Start cleanup thread
  cleanupThread_ = std::make_unique<std::thread>([this]() {
    while (running_) {
      cleanupSessions();
      std::this_thread::sleep_for(std::chrono::minutes(5));
    }
  });
}

EmotionAnalyzer::~EmotionAnalyzer() {
  running_ = false;
  if (cleanupThread_ && cleanupThread_->joinable()) {
    cleanupThread_->join();
  }

  if (pythonState_) {
    if (pythonState_->func) {
      Py_DECREF(pythonState_->func);
    }
    if (pythonState_->module) {
      Py_DECREF(pythonState_->module);
    }
    PyGILState_Release(pythonState_->gilState);
  }
}

void EmotionAnalyzer::initPythonState() {
  if (!pythonState_) {
    pythonState_ = std::make_unique<PythonState>();
    pythonState_->gilState = PyGILState_Ensure();

    // Import emotion_analyzer module
    pythonState_->module = PyImport_ImportModule("emotion_analyzer");
    if (!pythonState_->module) {
      LOG_ERROR << "Module emotion_analyzer not found ";
      PyErr_Print();
      throw std::runtime_error("Failed to import emotion_analyzer module");
    }
    LOG_TRACE << "Python module found";
    // Get analyze_emotions function
    pythonState_->func =
        PyObject_GetAttrString(pythonState_->module, "analyze_emotions");
    if (!pythonState_->func) {
      PyErr_Print();
      Py_DECREF(pythonState_->module);
      throw std::runtime_error("Failed to get analyze_emotions function");
    }
  }
}

std::string EmotionAnalyzer::analyzeContent(const std::string &content) {
  initPythonState();

  try {
    // Convert content to Python string directly (not bytes)
    PyObject *pyStr = PyUnicode_FromString(content.c_str());
    if (!pyStr) {
      PyErr_Print();
      throw std::runtime_error("Failed to create Python string");
    }

    // Create arguments tuple
    PyObject *args = PyTuple_New(1);
    if (!args) {
      Py_DECREF(pyStr);
      throw std::runtime_error("Failed to create arguments tuple");
    }

    // Set the string object in the tuple
    PyTuple_SET_ITEM(args, 0, pyStr); // This steals the reference to pyStr

    // Call the Python function
    PyObject *result = PyObject_CallObject(pythonState_->func, args);
    Py_DECREF(args);

    if (!result) {
      PyErr_Print();
      throw std::runtime_error("Failed to call analyze_emotions");
    }

    // Convert result to string
    const char *resultStr = PyUnicode_AsUTF8(result);
    if (!resultStr) {
      Py_DECREF(result);
      throw std::runtime_error("Failed to convert result to UTF-8");
    }

    std::string analysis(resultStr);
    Py_DECREF(result);
    return analysis;

  } catch (const std::exception &e) {
    if (PyErr_Occurred()) {
      PyErr_Print();
    }
    throw;
  }
}
Json::Value EmotionAnalyzer::createErrorResponse(const std::string &message) {
  Json::Value response;
  response["status"] = "error";
  response["error"] = message;
  return response;
}

Json::Value EmotionAnalyzer::createSuccessResponse(const std::string &message) {
  Json::Value response;
  response["status"] = "success";
  if (!message.empty()) {
    response["message"] = message;
  }
  return response;
}

void EmotionAnalyzer::handleInitialize(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {

  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
  addCorsHeaders(resp);

  try {
    auto jsonPtr = req->getJsonObject();
    if (!jsonPtr) {
      auto errorResp = createErrorResponse("Invalid JSON request");
      resp->setStatusCode(drogon::k400BadRequest);
      resp->setBody(errorResp.toStyledString());
      callback(resp);
      return;
    }

    const auto &json = *jsonPtr;
    std::string fileName = json["fileName"].asString();
    size_t fileSize = json["fileSize"].asUInt64();

    if (fileSize > MAX_FILE_SIZE) {
      auto errorResp = createErrorResponse("File too large");
      resp->setStatusCode(drogon::k400BadRequest);
      resp->setBody(errorResp.toStyledString());
      callback(resp);
      return;
    }

    std::string fileId = generateFileId();

    std::lock_guard<std::mutex> lock(sessionsMutex_);
    auto filePath = std::filesystem::path(UPLOAD_DIR) / fileId;
    auto file = std::make_shared<std::ofstream>(filePath, std::ios::binary);

    UploadSession session{
        fileId,
        fileName,
        fileSize,
        0,
        file,
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())};

    sessions_[fileId] = session;

    Json::Value response = createSuccessResponse();
    response["fileId"] = fileId;
    response["fileName"] = fileName;
    response["fileSize"] = Json::UInt64(fileSize);

    resp->setBody(response.toStyledString());
    callback(resp);
  } catch (const std::exception &e) {
    auto errorResp = createErrorResponse(e.what());
    resp->setStatusCode(drogon::k500InternalServerError);
    resp->setBody(errorResp.toStyledString());
    callback(resp);
  }
}

void EmotionAnalyzer::handleChunk(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {

  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
  addCorsHeaders(resp);

  try {
    std::string fileId = req->getHeader("x-file-id");
    if (fileId.empty()) {
      auto errorResp = createErrorResponse("Missing file ID");
      resp->setStatusCode(drogon::k400BadRequest);
      resp->setBody(errorResp.toStyledString());
      callback(resp);
      return;
    }

    auto body = req->getBody();
    std::string chunkData(body.data(), body.length());

    std::lock_guard<std::mutex> lock(sessionsMutex_);
    auto it = sessions_.find(fileId);
    if (it == sessions_.end()) {
      auto errorResp = createErrorResponse("Invalid session");
      resp->setStatusCode(drogon::k400BadRequest);
      resp->setBody(errorResp.toStyledString());
      callback(resp);
      return;
    }

    auto &session = it->second;
    if (session.uploadedSize + chunkData.length() > session.totalSize) {
      auto errorResp = createErrorResponse("File size exceeded");
      resp->setStatusCode(drogon::k400BadRequest);
      resp->setBody(errorResp.toStyledString());
      callback(resp);
      return;
    }

    session.file->write(chunkData.c_str(), chunkData.length());
    session.uploadedSize += chunkData.length();
    session.lastActivity =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    float progress = (float)session.uploadedSize / session.totalSize * 100;

    Json::Value response = createSuccessResponse();
    response["received"] = true;
    response["progress"] = (int)progress;
    response["uploadedSize"] = Json::UInt64(session.uploadedSize);
    response["totalSize"] = Json::UInt64(session.totalSize);

    if (session.uploadedSize >= session.totalSize) {
      session.file->close();

      auto filePath = std::filesystem::path(UPLOAD_DIR) / fileId;
      std::ifstream inFile(filePath, std::ios::binary);
      std::string content((std::istreambuf_iterator<char>(inFile)),
                          std::istreambuf_iterator<char>());

      try {
        std::string analysis = analyzeContent(content);
        response["status"] = "done";
        response["message"] = "Upload and analysis complete";

        Json::Value analysisJson;
        Json::Reader reader;
        if (reader.parse(analysis, analysisJson)) {
          response["analysis"] = analysisJson;
        } else {
          response["analysis"] = analysis;
        }
      } catch (const std::exception &e) {
        response["status"] = "error";
        response["message"] =
            std::string("Error analyzing content: ") + e.what();
      }

      sessions_.erase(fileId);
    } else {
      response["status"] = progress < 30   ? "analyzing"
                           : progress < 60 ? "processing"
                                           : "finalizing";
      response["message"] = "chunk upload successful";
    }

    resp->setBody(response.toStyledString());
    callback(resp);
  } catch (const std::exception &e) {
    auto errorResp = createErrorResponse(e.what());
    resp->setStatusCode(drogon::k400BadRequest);
    resp->setBody(errorResp.toStyledString());
    callback(resp);
  }
}

void EmotionAnalyzer::handleOptions(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {

  auto resp = drogon::HttpResponse::newHttpResponse();
  addCorsHeaders(resp);
  resp->setStatusCode(drogon::k204NoContent);
  callback(resp);
}

void EmotionAnalyzer::addCorsHeaders(const drogon::HttpResponsePtr &resp) {
  resp->addHeader("Access-Control-Allow-Origin", "*");
  resp->addHeader("Access-Control-Allow-Methods", "POST, PUT, OPTIONS");
  resp->addHeader("Access-Control-Allow-Headers",
                  "Content-Type, X-File-Id, X-Chunk-Index");
}

std::string EmotionAnalyzer::generateFileId() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 15);

  const char *hex = "0123456789abcdef";
  std::string fileId;
  fileId.reserve(32);

  for (int i = 0; i < 32; ++i) {
    fileId += hex[dis(gen)];
  }

  return fileId;
}

void EmotionAnalyzer::cleanupSessions() {
  const int64_t timeout = 3600; // 1 hour timeout
  auto now =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

  std::lock_guard<std::mutex> lock(sessionsMutex_);
  for (auto it = sessions_.begin(); it != sessions_.end();) {
    if (now - it->second.lastActivity > timeout) {
      it->second.file->close();
      auto filePath = std::filesystem::path(UPLOAD_DIR) / it->first;
      std::filesystem::remove(filePath);
      it = sessions_.erase(it);
    } else {
      ++it;
    }
  }
}

} // namespace api
