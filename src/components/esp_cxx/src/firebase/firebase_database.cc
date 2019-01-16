#include "esp_cxx/firebase/firebase_database.h"

#include "cJSON.h"

namespace esp_cxx {

FirebaseDatabase::FirebaseDatabase(
    const std::string& host,
    const std::string& database,
    const std::string& listen_path,
    EventManager* event_manager)
  : host_(host),
    database_(database),
    listen_path_(listen_path),
    websocket_(event_manager,
               "wss://" + host_ + "/.ws?v=5&ns=" + database_) {
}

FirebaseDatabase::~FirebaseDatabase() {
}

void FirebaseDatabase::Connect() {
  websocket_.Connect<FirebaseDatabase, &FirebaseDatabase::OnWsFrame>(this);
  // TODO(awong): Schedule periodic keepalive ping.
}

void FirebaseDatabase::Publish(const std::string& path,
                               cJSON* new_value) {
}

cJSON* FirebaseDatabase::Get(const std::string& path) {
  cJSON* parent;
  cJSON* node;
  GetPath(path, &parent, &node);
  return node;
}

void FirebaseDatabase::GetPath(const std::string& path, cJSON** parent, cJSON** node,
                               bool create_parent_path) {
  *parent = nullptr;
  *node = nullptr;
  if (path.empty()) {
    return;
  }

  unique_C_ptr<char> path_copy(strdup(path.c_str()));
  static constexpr char kPathSeparator[] = "/";
  for (const char* key = strtok(path_copy.get(), kPathSeparator),
                 * last_key = nullptr;
       key;
       last_key = key,
       key = strtok(NULL, kPathSeparator)) {
    // Special case root node.
    if (key == path_copy.get() && strlen(key) != 0) {
      *node = root_;
      continue;
    }

    // Node is from last run. If it was nullptr, then the path
    // does not exist.
    if (!node) {
      if (!create_parent_path) {
        // Unless the parent path is being created, bail
        // as all other iterations are just nullptr.
        break;
      }

      // If node is null, just start creating the parents.
      *parent = cJSON_AddObjectToObject(*parent, last_key);
    } else {
      *parent = *node;
      *node = cJSON_GetObjectItemCaseSensitive(*parent, key);
    }
  }
}

void FirebaseDatabase::OnWsFrame(WebsocketFrame frame) {
  switch(frame.opcode()) {
    case WebsocketOpcode::kBinary:
      break;

    case WebsocketOpcode::kText: {
                                   /*
      cJSON *json = cJSON_Parse(frame.data().data());
      // TODO(awong): Delete the json obj?
      OnUpdate(json);
      // This should be json?
      // */
      break;
    }

    case WebsocketOpcode::kPong:
    case WebsocketOpcode::kPing:
      // TODO(awong): Send pong.
      break;

    case WebsocketOpcode::kClose:
      // TODO(awong): Reconnect.
      break;

    case WebsocketOpcode::kContinue:
      // TODO(awong): Shouldn't be here. The mongose implementation is supposed to reconstruct.
      break;
  }
}

void FirebaseDatabase::OnCommand(cJSON* command) {
  // Find the envelope.
  // Dispatch update.
  cJSON* type = cJSON_GetObjectItemCaseSensitive(command, "t");
  if (cJSON_IsString(type)) {
    cJSON* data = cJSON_GetObjectItemCaseSensitive(command, "d");

    // Type has 2 possibilities:
    //   c = connection oriented command like server information or
    //       redirect info.
    //   d = data commands such as publishing new database entries.
    if (strcmp(type->valuestring, "c") != 0) {
      OnConnectionCommand(data);
    } else if (strcmp(type->valuestring, "d") != 0) {
      OnDataCommand(data);
    }
  }
}

void FirebaseDatabase::OnConnectionCommand(cJSON* command) {
  cJSON* type = cJSON_GetObjectItemCaseSensitive(command, "t");
  cJSON* data = cJSON_GetObjectItemCaseSensitive(command, "d");
  cJSON* host = cJSON_GetObjectItemCaseSensitive(data, "h");

  // Two types of connection requests
  //   h - host data
  //   r - redirect.
  if (cJSON_IsString(type) && cJSON_IsString(host) && host->valuestring != nullptr) {
    real_host_ = host->valuestring;
    if (strcmp(type->valuestring, "h") != 0) {
      // TODO(ajwong): Print other data? maybe?
    } else if (strcmp(type->valuestring, "r") != 0) {
      // TODO(awong): Reconnect.
    }
  }
}

void FirebaseDatabase::OnDataCommand(cJSON* command) {
  cJSON* request_id = cJSON_GetObjectItemCaseSensitive(command, "r");
  cJSON* action = cJSON_GetObjectItemCaseSensitive(command, "a");
  cJSON* body = cJSON_GetObjectItemCaseSensitive(command, "b");
  cJSON* path = cJSON_GetObjectItemCaseSensitive(body, "p");
//  cJSON* hash = cJSON_GetObjectItemCaseSensitive(body, "h");

  if (cJSON_IsNumber(request_id) &&
      cJSON_IsString(action) && action->valuestring != nullptr &&
      cJSON_IsObject(body)) {
    // TODO(awong): Match the request_id? Do we even care to track?
    // We can do skipped messages I guess.
    // There are 2 action types received:
    //   d - a JSON tree is being replaced.
    //   m - a JSON tree should be merged. [ TODO(awong): what does this mean? Don't delete? ]
    unique_cJSON_ptr new_data(cJSON_DetachItemFromObjectCaseSensitive(body, "d"));
    if (strcmp(action->valuestring, "d") != 0) {
      ReplacePath(path->valuestring, std::move(new_data));
    } if (strcmp(action->valuestring, "m") != 0) {
      MergePath(path->valuestring, std::move(new_data));
    }
  }
}

void FirebaseDatabase::ReplacePath(const char* path, unique_cJSON_ptr new_data) {
  cJSON* parent;
  cJSON* node;
  GetPath(path, &parent, &node, true);
  if (!cJSON_IsObject(parent)) {
    // TODO(awong): Log warning.
    return;
  }
  if (node) {
    cJSON_ReplaceItemViaPointer(parent, node, new_data.release());
  } else {
    // TODO(awong): Uh oh. need a key.
    cJSON_AddItemToObject(parent, "key", new_data.release());
  }
}

void FirebaseDatabase::MergePath(const char* path, unique_cJSON_ptr new_data) {
  cJSON* parent;
  cJSON* node;
  GetPath(path, &parent, &node, true);
  if (!cJSON_IsObject(parent)) {
    // TODO(awong): Log warning.
    return;
  }

  if (!node) {
    // TODO(awong): Uh oh. Also need key.
    cJSON_AddItemToObject(parent, "key", new_data.release());
    return;
  }
  // TODO(awong): Walk the 2 trees and merge. *sigh*
}

}  // namespace esp_cxx
