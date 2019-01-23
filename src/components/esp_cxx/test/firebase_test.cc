#include "esp_cxx/firebase/firebase_database.h"
#include "esp_cxx/httpd/event_manager.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace {
const std::string_view kOverwriteResponse = R"(
{ "t":"d",
  "d":{
    "b":{
      "p":"test",
      "d":{
        "int":314159,
        "string":"hi",
        "isTrue":true,
        "array": [ 1, "two", 3 ],
        "obj": { "a": 1 }
      }
    },
  "a":"d"
  }
})";

}  // namespace

namespace esp_cxx {
class Firebase : public ::testing::Test {
 protected:
  EventManager event_manager_;
  FirebaseDatabase database_{"fake.host", "a_database", "/listen/path", &event_manager_};
};

TEST_F(Firebase, Get) {
  EXPECT_TRUE(database_.Get(""));
  EXPECT_TRUE(database_.Get("/"));
  cJSON* root = database_.Get("/");
  EXPECT_TRUE(cJSON_IsObject(root));
  EXPECT_EQ(0, cJSON_GetArraySize(root));
}

TEST_F(Firebase, PathUpdate) {
  // Create dummy data.
  WebsocketFrame frame(kOverwriteResponse, WebsocketOpcode::kText);
  database_.OnWsFrame(frame);
  ASSERT_TRUE(database_.Get("/test")) << "Path root should exist";

  // Check an int value.
  cJSON* item = database_.Get("/test/int");
  ASSERT_TRUE(cJSON_IsNumber(item));
  EXPECT_EQ(314159, item->valueint);

  // Check an string value.
  item = database_.Get("/test/string");
  ASSERT_TRUE(cJSON_IsString(item));
  EXPECT_STREQ("hi", item->valuestring);

  // Check an bool value.
  item = database_.Get("/test/isTrue");
  ASSERT_TRUE(cJSON_IsBool(item));
  EXPECT_TRUE(cJSON_IsTrue(item));

  // Check an array value.
  item = database_.Get("/test/array");
  ASSERT_TRUE(cJSON_IsArray(item));
  EXPECT_EQ(3, cJSON_GetArraySize(item));

  // Check a nested field.
  item = database_.Get("/test/obj");
  ASSERT_TRUE(cJSON_IsObject(item));

//  std::string_view response = R"({"t":"c","d":{"t":"r","d":"s-usc1c-nss-226.firebaseio.com"}})";
//  std::string_view response = R"({"t":"c","d":{"t":"h","d":{"ts":1547021870522,"v":"5","h":"s-usc1c-nss-226.firebaseio.com","s":"cQBtTldc3zfMgsY2t3UL2BYJRkpGaeMw"}}})";
//  std::cout << cJSON_Print(database_.Get("/"));
//  FirebaseDatabaseCommand command(json);
//  EXPECT_TRUE(command.IsRedirect());
}

}  // namespace esp_cxx
