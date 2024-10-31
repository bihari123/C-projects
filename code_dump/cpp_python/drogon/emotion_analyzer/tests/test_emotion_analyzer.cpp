// tests/test_emotion_analyzer.cpp
#include "../include/controllers/EmotionAnalyzer.hpp"
#include "../include/controllers/EmotionAnalyzerController.hpp"
#include <drogon/drogon_test.h>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

class EmotionAnalyzerTest : public testing::Test {
protected:
  void SetUp() override {
    // Create test upload directory
    std::filesystem::create_directories("uploads");
  }

  void TearDown() override {
    // Clean up test files
    std::filesystem::remove_all("uploads");
  }

  // Helper function to create a test request
  drogon::HttpRequestPtr
  createTestRequest(const std::string &method, const std::string &path,
                    const Json::Value &body = Json::Value()) {

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(method);
    req->setPath(path);

    if (!body.isNull()) {
      req->setBody(body.toStyledString());
      req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    }

    return req;
  }

  // Helper function to read file content
  std::string readFile(const std::string &path) {
    std::ifstream file(path);
    return std::string(std::istreambuf_iterator<char>(file),
                       std::istreambuf_iterator<char>());
  }
};

TEST_F(EmotionAnalyzerTest, Initialize) {
  auto controller = std::make_shared<api::EmotionAnalyzerController>();

  Json::Value body;
  body["fileName"] = "test.srt";
  body["fileSize"] = 1024;

  auto req = createTestRequest("POST", "/api/upload/initialize", body);

  std::string fileId;
  controller->handleInitialize(
      req, [&fileId](const drogon::HttpResponsePtr &resp) {
        ASSERT_EQ(resp->getStatusCode(), drogon::k200OK);

        auto json = *resp->getJsonObject();
        EXPECT_TRUE(json.isMember("fileId"));
        EXPECT_TRUE(json.isMember("fileName"));
        EXPECT_TRUE(json.isMember("fileSize"));
        EXPECT_EQ(json["fileName"].asString(), "test.srt");
        EXPECT_EQ(json["fileSize"].asUInt64(), 1024);

        fileId = json["fileId"].asString();
      });

  EXPECT_FALSE(fileId.empty());
}

TEST_F(EmotionAnalyzerTest, ChunkUpload) {
  auto controller = std::make_shared<api::EmotionAnalyzerController>();

  // Initialize upload
  Json::Value initBody;
  initBody["fileName"] = "test.srt";
  initBody["fileSize"] = 100;

  std::string fileId;
  auto initReq = createTestRequest("POST", "/api/upload/initialize", initBody);

  controller->handleInitialize(initReq,
                               [&fileId](const drogon::HttpResponsePtr &resp) {
                                 auto json = *resp->getJsonObject();
                                 fileId = json["fileId"].asString();
                               });

  ASSERT_FALSE(fileId.empty());

  // Test chunk upload
  std::string testContent = R"
