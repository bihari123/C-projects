#include "../../include/controllers/ShutdownController.h"
#include <drogon/HttpResponse.h>
#include <functional>
namespace local {
void ShutdownController::shutdown(
    const drogon::HttpResponsePtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {}
} // namespace local
