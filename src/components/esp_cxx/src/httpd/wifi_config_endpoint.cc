#include "esp_cxx/httpd/wifi_config_endpoint.h"

#include "esp_cxx/httpd/http_request.h"
#include "esp_cxx/httpd/http_response.h"

#include "mongoose.h"
#include "esp_log.h"

// TODO(awong): THIS IS INCORRECT. Migrate wifi over first.
static constexpr size_t kSsidBytes = 16;
static constexpr size_t kPasswordBytes = 16;
static constexpr const char kContentTypeJson[] = "Content-Type: application/json";

namespace esp_cxx {

namespace {
void SendWifiConfig(HttpResponse response) {
  static const std::experimental::string_view kConfigStart("{ 'ssid': '");
  static const std::experimental::string_view kConfigMid("', 'password': '");
  static const std::experimental::string_view kConfigEnd("' }");
  char ssid[kSsidBytes];
  size_t ssid_len = sizeof(ssid);
  char password[kPasswordBytes];
  size_t password_len = sizeof(password);
  /*
  if (!GetWifiSsid(&ssid[0], &ssid_len)) {
    constexpr char kNotSet[] = "(not set)";
    strcpy(ssid, kNotSet);
    ssid_len = strlen(kNotSet);
  } else {
    ssid_len--;  // null terminator.
  }
  if (!GetWifiPassword(&password[0], &password_len)) {
    constexpr char kNotSet[] = "(not set)";
    strcpy(password, kNotSet);
    password_len = strlen(kNotSet);
  } else {
    password_len--;  // null terminator.
  }
  */
  response.SendHead(200,
                    kConfigStart.size() + ssid_len + kConfigMid.size() +
                    password_len + kConfigEnd.size(),
                    kContentTypeJson);
  response.Send(kConfigStart);
  response.Send({ssid, ssid_len});
  response.Send(kConfigMid);
  response.Send({password, password_len});
  response.Send(kConfigEnd);
}

void UpdateWifiConfig(const char* body, HttpResponse response) {
}
}  // namespace

void WifiConfigEndpoint(const HttpRequest& request, HttpResponse response) {
  if (request.method() == HttpMethod::kGet) {
    SendWifiConfig(std::move(response));
  } else if (request.method() == HttpMethod::kPost) {
    // TODO(awong): This needs the post body.
    UpdateWifiConfig("", std::move(response));
  } else {
    // TODO(awong): What to do here?
  }
}

}  // namespace esp_cxx
