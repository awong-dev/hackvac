#include "esp_cxx/httpd/wifi_config_endpoint.h"

#include "esp_cxx/cxx17hack.h"
#include "esp_cxx/logging.h"
#include "esp_cxx/httpd/http_request.h"
#include "esp_cxx/httpd/http_response.h"
#include "esp_cxx/wifi.h"

#include "esp_log.h"
#include "jsmn.h"
#include "mongoose.h"

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

  // TODO(awong): Move to a json response handler.
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

std::string_view TokenToStringView(std::string_view data,
                                   const jsmntok_t& token) {
  return data.substr(token.start, token.end - token.start);
}

bool HandleUpdateEntry(std::string_view data, jsmntok_t field_token,
                       jsmntok_t value_token) {
  static const std::string_view kSsidKey("ssid");
  static const std::string_view kPasswordKey("password");
  std::string_view field = TokenToStringView(data, field_token);

  if (kSsidKey == field) {
    std::string_view value = TokenToStringView(data, value_token);
    // TODO(awong): Rewrite this logic to be less ugly.
    char ssid[kSsidBytes];
    if (value.size() > kSsidBytes - 1) {
      return false;
    }
    memcpy(ssid, value.data(), value.size());
    ssid[value.size()] = '\0';
    SetWifiSsid(ssid);
  } else if (kPasswordKey == field) {
    std::string_view value = TokenToStringView(data, value_token);
    // TODO(awong): Rewrite this logic to be less ugly.
    char password[kPasswordBytes];
    if (value.size() > kPasswordBytes - 1) {
      return false;
    }
    memcpy(password, value.data(), value.size());
    password[value.size()] = '\0';
    SetWifiPassword(password);
  }

  return true;
}

const char* ParseUpdate(std::string_view body) {
  // { 'ssid': 'blah', 'password', 'blee' }
  // Total of 5 tokens. Object, and 4 strings.
  static constexpr unsigned int kMaxTokens = 5;
  jsmntok_t tokens[kMaxTokens];
  jsmn_parser json_parser;
  jsmn_init(&json_parser);
  int num_tokens = jsmn_parse(&json_parser, body.data(), body.size(), &tokens[0],
                              kMaxTokens);
  if (num_tokens < 0) {
    return "Invalid JSON";
  }
  ESP_LOGI(kEspCxxTag, "Num tokens found %d", num_tokens);
  ESP_LOGI(kEspCxxTag, "Received %.*s", body.size(), body.data());

  static constexpr char kExpectedJson[] =
    "Expected format: { \"ssid\": \"abc\", \"password\": \"123\" }. Use double-quotes!";
  if (tokens[0].type != JSMN_OBJECT ||
      tokens[0].size != 2) {
    ESP_LOGI(kEspCxxTag, "Expected object with 2 children. Got type %d children %d",
             tokens[0].type, tokens[0].size);
    return kExpectedJson;
  }

  // All other tokens should be strings and be structured as pairs.
  //
  // JSMN treats 'string' as a JSMN_PRIMITIVE and a "string" as a
  // JSMN_STRING (wtf?). The size field is the number of children
  // so the first should be 1 and the second should be zero.
  for (int i = 0; i < tokens[0].size; i++) {
    int field_start = 1 + 2*i;
    if (tokens[field_start].type != JSMN_STRING ||
        tokens[field_start].size != 1 ||
        tokens[field_start + 1].type != JSMN_STRING ||
        tokens[field_start + 1].size != 0) {
      ESP_LOGW(kEspCxxTag, "Error at field %d", i);
      return kExpectedJson;
    }
  }

  if (!HandleUpdateEntry(body, tokens[1], tokens[2])) {
    return "Unable ot process first field";
  }
  if (!HandleUpdateEntry(body, tokens[3], tokens[4])) {
    return "Unable ot process second field";
  }
  return nullptr;
}

void UpdateWifiConfig(std::string_view body, HttpResponse response) {
  const char* error = ParseUpdate(body);
  if (error == nullptr) {
    // This should send a 200.
    response.SendError(400, "");
  } else {
    response.SendError(400, error);
    //SendResultJson(nc, 400, error);
  }
}
}  // namespace

void WifiConfigEndpoint(const HttpRequest& request, HttpResponse response) {
  if (request.method() == HttpMethod::kGet) {
    SendWifiConfig(std::move(response));
  } else if (request.method() == HttpMethod::kPost) {
    // TODO(awong): ensure it's a json content-type?
    UpdateWifiConfig(request.body(), std::move(response));
  } else {
    response.SendError(400, "Invalid Method");
  }
}

}  // namespace esp_cxx
