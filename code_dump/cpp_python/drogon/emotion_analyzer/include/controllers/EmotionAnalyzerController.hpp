// include/controllers/EmotionAnalyzerController.hpp
#pragma once
#include "EmotionAnalyzer.hpp"
#include <drogon/HttpController.h>

namespace api {

class EmotionAnalyzerController
    : public drogon::HttpController<EmotionAnalyzerController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(EmotionAnalyzerController::handleInitialize,
                "/api/upload/initialize", drogon::Post, drogon::Options);
  ADD_METHOD_TO(EmotionAnalyzerController::handleChunk, "/api/upload/chunk",
                drogon::Post, drogon::Options);
  METHOD_LIST_END

  EmotionAnalyzerController()
      : analyzer_(std::make_shared<EmotionAnalyzer>()) {}

  void handleInitialize(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void
  handleChunk(const drogon::HttpRequestPtr &req,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback);

private:
  std::shared_ptr<EmotionAnalyzer> analyzer_;
};

} // namespace api
