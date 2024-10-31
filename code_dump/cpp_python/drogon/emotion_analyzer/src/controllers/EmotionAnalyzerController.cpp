
// src/controllers/EmotionAnalyzerController.cpp
#include "../../include/controllers/EmotionAnalyzerController.hpp"

namespace api {

void EmotionAnalyzerController::handleInitialize(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {

  if (req->getMethod() == drogon::HttpMethod::Options) {
    analyzer_->handleOptions(req, std::move(callback));
  } else {
    analyzer_->handleInitialize(req, std::move(callback));
  }
}

void EmotionAnalyzerController::handleChunk(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {

  if (req->getMethod() == drogon::HttpMethod::Options) {
    analyzer_->handleOptions(req, std::move(callback));
  } else {
    analyzer_->handleChunk(req, std::move(callback));
  }
}

} // namespace api
