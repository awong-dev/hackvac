#ifndef ESPCXX_HTTPD_HTTP_RESPONSE_H_
#define ESPCXX_HTTPD_HTTP_RESPONSE_H_

#include "esp_cxx/cxx17hack.h"

struct mg_connection;

namespace esp_cxx {

// Copyable wrapper for the mg_connection class used to send HTTP responses.
class HttpResponse {
 public:
  explicit HttpResponse(mg_connection* connection);

  // Returns true if Send() or SendError() has been called.
  bool HasSentHeaders();

  // Sends a response back. This and SendError() are mutually exclusive.
  // If |content_length| is negative, chunked encoding is used to send
  // streaming amounts of data back. Otherwise, the total sum of bytes in
  // |body| and subseqeunt SendMore() calls should add up to |content_length|.
  void Send(int status_code, int64_t content_length,
            const char* extra_headers, std::string_view body);

  // Sends more bytes back down the channel. Should be called after Send().
  void SendMore(std::string_view data);

  // Sends an HTTP error code back. This and Send() are mutually exclusive.
  // If |text| is nullptr, then a default error message for the status code
  // will be returned.
  void SendError(int status_code, const char* text = nullptr);

 private:
  mg_connection* connection_;  // not owned.
};

}  // namespace esp_cxx

#endif  // ESPCXX_HTTPD_HTTP_RESPONSE_H_
