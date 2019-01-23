#include <string>

#include "esp_cxx/httpd/websocket.h"
#include "esp_cxx/cpointer.h"

#include "gtest/gtest_prod.h"
#include "esp_cxx/cxx17hack.h"

struct cJSON;

namespace esp_cxx {
class EventManager;

class FirebaseDatabase {
 public:
  //  wss://anger2action-f3698.firebaseio.com/.ws?v=5&ns=anger2action-f3698
  FirebaseDatabase(
      const std::string& host,
      const std::string& database,
      const std::string& listen_path,
      EventManager* event_manager);
  ~FirebaseDatabase();

  // Connects 
  void Connect();

  void Publish(const std::string& path, cJSON* new_value);
  cJSON* Get(const std::string& path);

 private:
  FRIEND_TEST(Firebase, PathUpdate);
  FRIEND_TEST(Firebase, MergeUpdate);
  FRIEND_TEST(Firebase, OverwriteUpdate);

  void GetPath(const std::string& path, cJSON** parent, cJSON** node,
               bool create_parent_path = false,
               std::optional<std::string*> last_key = {});

  // Handle incoming websocket frames and errors.
  void OnWsFrame(WebsocketFrame frame);

  // Entrypoint to firebase protocol parsing. Takes the full JSON command
  // from the server.
  void OnCommand(cJSON* command);

  // Handles commands with an envelope of of type "c".
  void OnConnectionCommand(cJSON* command);

  // Handles commands with an envelope of of type "d".
  void OnDataCommand(cJSON* command);
  
  // Sets |new_data| at |path| in the stored json tree.
  void ReplacePath(const char* path, unique_cJSON_ptr new_data);

  // Merges |new_data| into the existing tree at |path| in the stored json tree.
  void MergePath(const char* path, unique_cJSON_ptr new_data);

  std::string host_;
  std::string database_;
  std::string listen_path_;

  std::string real_host_;
  std::string session_id_;
//  int sever_timestamp_;

  WebsocketChannel websocket_;
  unique_cJSON_ptr root_;
};

}  // namespace esp_cxx
