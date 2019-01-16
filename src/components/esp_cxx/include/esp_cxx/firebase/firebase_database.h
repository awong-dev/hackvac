#include <string>

#include "esp_cxx/httpd/websocket.h"
#include "esp_cxx/cpointer.h"

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
  void GetPath(const std::string& path, cJSON** parent, cJSON** node,
               bool create_parent_path = false);
  void OnWsFrame(WebsocketFrame frame);
  void OnCommand(cJSON* command);
  void OnConnectionCommand(cJSON* command);
  void OnDataCommand(cJSON* command);
  void ReplacePath(const char* path, unique_cJSON_ptr new_data);
  void MergePath(const char* path, unique_cJSON_ptr new_data);

  std::string host_;
  std::string database_;
  std::string listen_path_;

  std::string real_host_;
  std::string session_id_;
//  int sever_timestamp_;

  WebsocketChannel websocket_;
  cJSON* root_ = nullptr;
};

}  // namespace esp_cxx
