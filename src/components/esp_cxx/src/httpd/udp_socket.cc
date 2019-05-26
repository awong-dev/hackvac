#include "esp_cxx/httpd/udp_socket.h"

#include "esp_cxx/httpd/mongoose_event_manager.h"

#include "esp_cxx/logging.h"

namespace esp_cxx {

UdpSocket::UdpSocket(MongooseEventManager *event_manager,
                     std::string_view udp_url,
                     std::function<void(std::string_view)> on_packet)
  : event_manager_(event_manager),
    udp_url_(udp_url),
    on_packet_(std::move(on_packet)) {
}

void UdpSocket::Connect() {
  connection_ = mg_connect(event_manager_->underlying_manager(), udp_url_.c_str(), &OnEventThunk, this);
}

void UdpSocket::Send(std::string_view data) {
  if (!connection_) {
    ESP_LOGW(kEspCxxTag, "UDP Connetion failed. droping data");
    return;
  }

  mg_send(connection_, data.data(), data.size());
}

void UdpSocket::OnEvent(struct mg_connection *nc, int event, void *ev_data) {
  switch (event) {
    case MG_EV_CONNECT: {
      int status = *((int *) ev_data);
      if (status != 0) {
        ESP_LOGW(kEspCxxTag, "UDP Connect error: %d", status);
      }
      break;
    }

    case MG_EV_RECV: {
      on_packet_({nc->recv_mbuf.buf, nc->recv_mbuf.len});
      break;
    }

    case MG_EV_CLOSE: {
      connection_ = nullptr;
      break;
    }
  }
}

}  // namespace esp_cxx
