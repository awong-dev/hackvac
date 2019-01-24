#include "esp_cxx/firebase/firebase_database.h"
#include "esp_cxx/httpd/event_manager.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace {
const std::string_view kFullUpdateResponse = R"(
{ "t":"d",
  "d":{
    "b":{
      "p":"test",
      "d":{
        "num":314159,
        "string":"hi",
        "isTrue":true,
        "array": { "0": 1, "1": "two", "3": 3 },
        "obj": { "a": 1, "b": 2 }
      }
    },
  "a":"d"
  }
})";

const std::string_view kMergeResponse = R"(
{ "t":"d",
  "d":{
    "b":{
      "p":"test",
      "d":{
        "num": 10,
        "array": null,
        "obj": { "a": null }
      }
    },
  "a":"m"
  }
})";

const std::string_view kOverwriteResponse = R"(
{ "t":"d",
  "d":{
    "b":{
      "p":"test",
      "d":{
        "num": 3
      }
    },
  "a":"d"
  }
})";

const std::string_view kOverwriteRootResponse = R"(
{ "t":"d",
  "d":{
    "b":{
      "p":"",
      "d":{
        "moo": "cow"
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
  database_.OnWsFrame(WebsocketFrame(kFullUpdateResponse, WebsocketOpcode::kText));
  ASSERT_TRUE(database_.Get("/test")) << "Path root should exist";

  // Check an num value.
  cJSON* item = database_.Get("/test/num");
  ASSERT_TRUE(cJSON_IsNumber(item));
  EXPECT_EQ(314159, item->valueint);
  EXPECT_FLOAT_EQ(314159, item->valuedouble);

  // Check an string value.
  item = database_.Get("/test/string");
  ASSERT_TRUE(cJSON_IsString(item));
  EXPECT_STREQ("hi", item->valuestring);

  // Check an bool value.
  item = database_.Get("/test/isTrue");
  ASSERT_TRUE(cJSON_IsBool(item));
  EXPECT_TRUE(cJSON_IsTrue(item));

  // Check an array value. Firebase represents these as
  // objects indexed by their number.
  item = database_.Get("/test/array");
  ASSERT_TRUE(cJSON_IsObject(item));
  EXPECT_EQ(3, cJSON_GetArraySize(item));

  // Check a nested field.
  item = database_.Get("/test/obj");
  ASSERT_TRUE(cJSON_IsObject(item));
  EXPECT_EQ(2, cJSON_GetArraySize(item));
}

TEST_F(Firebase, MergeUpdate) {
  // Create dummy data.
  database_.OnWsFrame(WebsocketFrame(kFullUpdateResponse, WebsocketOpcode::kText));
  ASSERT_EQ(5, cJSON_GetArraySize(database_.Get("/test")));
  ASSERT_EQ(2, cJSON_GetArraySize(database_.Get("/test/obj")));

  // Do a merge update.
  database_.OnWsFrame(WebsocketFrame(kMergeResponse, WebsocketOpcode::kText));
  cJSON* item = database_.Get("/test");
  ASSERT_TRUE(cJSON_IsObject(item));

  // This merge erases one top level field.
  EXPECT_EQ(4, cJSON_GetArraySize(item)) << cJSON_Print(item);
  EXPECT_FALSE(database_.Get("/test/array"));

  // Check the new value actaully propagated.
  item = database_.Get("/test/num");
  ASSERT_TRUE(cJSON_IsNumber(item));
  EXPECT_FLOAT_EQ(10, item->valuedouble);

  // Oddly on a nested object, it overwrites the whole thing
  // rather than just the changed fields.
  EXPECT_EQ(0, cJSON_GetArraySize(database_.Get("/test/obj")));
}

TEST_F(Firebase, OverwriteUpdate) {
  // Create dummy data.
  database_.OnWsFrame(WebsocketFrame(kFullUpdateResponse, WebsocketOpcode::kText));
  ASSERT_EQ(5, cJSON_GetArraySize(database_.Get("/test")));

  // Do an overwrite update.
  database_.OnWsFrame(WebsocketFrame(kOverwriteResponse, WebsocketOpcode::kText));
  cJSON* item = database_.Get("/test");
  ASSERT_TRUE(cJSON_IsObject(item));
  EXPECT_EQ(1, cJSON_GetArraySize(item));

  item = database_.Get("/test/num");
  ASSERT_TRUE(item);
  EXPECT_EQ(3, item->valueint);
  EXPECT_FLOAT_EQ(3, item->valuedouble);

  // Now overwrite root.
  database_.OnWsFrame(WebsocketFrame(kOverwriteRootResponse, WebsocketOpcode::kText));
  item = database_.Get("/");
  ASSERT_TRUE(cJSON_IsObject(item));
  EXPECT_EQ(1, cJSON_GetArraySize(item));
  item = database_.Get("/moo");

  ASSERT_TRUE(cJSON_IsString(item));
  EXPECT_STREQ("cow", item->valuestring);
}

}  // namespace esp_cxx
