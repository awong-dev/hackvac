#include "esp_cxx/task.h"
#include "esp_cxx/httpd/http_server.h"
#include "mongoose.h"

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

static int s_is_connected = 0;
static int s_done = 0;

static void TestHandler(struct mg_connection *nc, int ev, void *ev_data, void* user_data) {
  struct websocket_message *wm = (struct websocket_message *) ev_data;
  (void) nc;

  switch (ev) {
    case MG_EV_CONNECT: {
      int status = *((int *) ev_data);
      if (status != 0) {
        printf("-- Connection error: %d\n", status);
	 }
      break;
    }
    case MG_EV_WEBSOCKET_HANDSHAKE_DONE: {
      printf("-- Connected\n");
      s_is_connected = 1;
      break;
    }
    case MG_EV_POLL: {
      char msg[500];
      int n = 0;
#ifdef _WIN32 /* Windows console input is special. */
      INPUT_RECORD inp[100];
      HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
      DWORD i, num;
      if (!PeekConsoleInput(h, inp, sizeof(inp) / sizeof(*inp), &num)) break;
      for (i = 0; i < num; i++) {
        if (inp[i].EventType == KEY_EVENT &&
            inp[i].Event.KeyEvent.wVirtualKeyCode == VK_RETURN) {
          break;
        }
      }
      if (i == num) break;
      if (!ReadConsole(h, msg, sizeof(msg), &num, NULL)) break;
      /* Un-unicode. This is totally not the right way to do it. */
      for (i = 0; i < num * 2; i += 2) msg[i / 2] = msg[i];
      n = (int) num;
#else /* For everybody else, we just read() stdin. */
      fd_set read_set, write_set, err_set;
      struct timeval timeout = {0, 0};
      FD_ZERO(&read_set);
      FD_ZERO(&write_set);
      FD_ZERO(&err_set);
      FD_SET(0 /* stdin */, &read_set);
      if (select(1, &read_set, &write_set, &err_set, &timeout) == 1) {
        n = read(0, msg, sizeof(msg));
      }
#endif
      if (n <= 0) break;
      while (msg[n - 1] == '\r' || msg[n - 1] == '\n') n--;
      mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, msg, n);
      break;
    }
    case MG_EV_WEBSOCKET_FRAME: {
      printf("%.*s\n", (int) wm->size, wm->data);
      break;
    }
    case MG_EV_CLOSE: {
      if (s_is_connected) printf("-- Disconnected\n");
      s_done = 1;
      break;
    }
  }
}

TEST(Firebase, Connect) {
  esp_cxx::HttpServer server("Firebase test", ":8080");
//  server.EnableWebsockets();
  mg_mgr event_manager;
  mg_mgr_init(&event_manager, nullptr);
  if (mg_connect_ws(&event_manager,
                &TestHandler,
                NULL,
                //"wss://test-ns.firebaseio.com/.ws?v=5&ls=test", 
                //"ws://test-ns.firebaseio.com/.ws?v=5&ls=test", 
			 //"wss://demos.kaazing.com/echo",
                "wss://anger2action-f3698.firebaseio.com/.ws?v=5&ns=anger2action-f3698",
                NULL,
                NULL)) {
    printf("connected.\n");
    while (s_done != 1) {
	 mg_mgr_poll(&event_manager, 1000000);
    }
  }
}

