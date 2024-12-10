#include <drogon/HttpResponse.h>
#include <drogon/HttpTypes.h>
#include <drogon/LocalHostFilter.h>
#pragma one
#include <drogon/HttpController.h>
namespace local {
class ShutdownController : public drogon::HttpController<ShutdownController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(ShutdownController::shutdown, "/local/shutdown", drogon::Post,
                drogon::LocalHostFilter);
  METHOD_LIST_END

  ShutdownController() = default;

  void
  shutdown(const drogon::HttpResponsePtr &req,
           std::function<void(const drogon::HttpResponsePtr &)> &&callback);
};
} // namespace local
