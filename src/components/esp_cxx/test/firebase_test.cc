#include "esp_cxx/task.h"
#include "esp_cxx/httpd/http_server.h"
#include "mongoose.h"
#include "cJSON.h"

#include <iostream>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

class FirebaseDatabase {
 public:
  FirebaseDatabase() {
  }

  void Connect() {
  }

 private:
};

class FirebaseDatabaseCommand {
 public:
  explicit FirebaseDatabaseCommand(cJSON* raw_json) : raw_json_(raw_json) {
  }
  bool IsRedirect() {
    cJSON* type = cJSON_GetObjectItemCaseSensitive(raw_json_, "t");
    if (!cJSON_IsString(type) || strcmp(type->valuestring, "c") != 0) {
      // TODO(awong): Actually assert.
      std::cout << "boo";
      return false;
    }
    cJSON* data = cJSON_GetObjectItemCaseSensitive(raw_json_, "d");
    cJSON* data_type = cJSON_GetObjectItemCaseSensitive(data, "t");
    if (!cJSON_IsString(data_type) || strcmp(data_type->valuestring, "r") != 0) {
      std::cout << "not stru";
      return false;
    }
    return true;
  }
 private:
  cJSON* raw_json_;
};

TEST(Firebase, Connect) {
  std::string_view response = R"({"t":"c","d":{"t":"r","d":"s-usc1c-nss-226.firebaseio.com"}})";
//  std::string_view response = R"({"t":"c","d":{"t":"h","d":{"ts":1547021870522,"v":"5","h":"s-usc1c-nss-226.firebaseio.com","s":"cQBtTldc3zfMgsY2t3UL2BYJRkpGaeMw"}}})";
  cJSON *json = cJSON_Parse(response.data());
  std::cout << cJSON_Print(json);
  FirebaseDatabaseCommand command(json);
  EXPECT_TRUE(command.IsRedirect());
}

/*
void OnFrame(esp_cxx::WebsocketFrame frame) {
  std::cerr << frame.data();
}

TEST(Firebase, Connect) {
  esp_cxx::WebsocketChannel channel(
      "socket", 
      "wss://test-ns.firebaseio.com/.ws?v=5&ls=test");
  channel.Start(&OnFrame);
  for (;;) sleep(10);
}
*/
