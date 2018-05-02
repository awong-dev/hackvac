#include "request_processor.h"

#include <string.h>
#include "esp_log.h"

#include "router.h"

namespace {

const char TAG[] = "hackvac:request_processor";

static bool AppendString(char buf[], size_t* len, const char* addition,
                         size_t addition_len, size_t max_len) {
  if ((*len + addition_len) >= max_len) {
    ESP_LOGW(TAG, "too long: %.*s%.*s", *len, buf, addition_len, addition);
    return false;
  }
  memcpy(buf + *len, addition, addition_len);
  *len += addition_len;
  return true;
}

}  // namespace

namespace hackvac {

const http_parser_settings RequestProcessor::parser_settings_ = {
  .on_message_begin = &RequestProcessor::OnMessageBegin,
  .on_url = &RequestProcessor::OnUrl,
  .on_status = &RequestProcessor::OnStatus,
  .on_header_field = &RequestProcessor::OnHeaderField,
  .on_header_value = &RequestProcessor::OnHeaderValue,
  .on_headers_complete = &RequestProcessor::OnHeadersComplete,
  .on_body = &RequestProcessor::OnBody,
  .on_message_complete = &RequestProcessor::OnMessageComplete,
  .on_chunk_header = &RequestProcessor::OnChunkHeader,
  .on_chunk_complete = &RequestProcessor::OnChunkComplete,
};

constexpr size_t RequestProcessor::kUrlMaxLen;
constexpr size_t RequestProcessor::kHeaderFieldMaxLen;
constexpr size_t RequestProcessor::kHeaderValueMaxLen;

RequestProcessor::RequestProcessor(Router* router)
  : router_(router),
    url_(new char[kUrlMaxLen]),
    header_field_(new char[kHeaderFieldMaxLen]),
    header_value_(new char[kHeaderValueMaxLen]) {
  Reset();
}

void RequestProcessor::Reset() {
  state_ = State::kParsingMethodAndUrl;
  route_.reset();
  url_len_ = 0;
  header_field_len_ = 0;
  header_value_len_ = 0;
}

void RequestProcessor::OnNetError(int err) {
  state_ = State::kError;
  // TODO(awong): send a 500.
}

void RequestProcessor::OnParseError(http_errno err) {
  state_ = State::kError;
  // TODO(awong): send a 500.
}

bool RequestProcessor::PublishMethodAndUrl(http_method method) {
  ESP_LOGW(TAG, "Here");
  http_parser_url url_parser;
  http_parser_url_init(&url_parser);

  int err = http_parser_parse_url(url_.get(), url_len_, method == HTTP_CONNECT, &url_parser);
  if (err) {
    ESP_LOGW(TAG, "Could not parse url (%zd) %.*s", url_len_, url_len_, &url_[0]);
    // TODO(awong): Setup 500 response here.
    return false;
  }

  // Set the route and publish the parsed url.
  const char* path = &url_[0] + url_parser.field_data[UF_PATH].off;
  size_t path_len = url_parser.field_data[UF_PATH].len;
  route_ = router_->FindRoute(path, path_len);
  if (!route_) {
    ESP_LOGW(TAG, "No route for %.*s", url_len_, &url_[0]);
    // TODO(awong): Setup 500 response here.
    return false;
  }
  route_->OnMethodAndPath(method, path, path_len);
  return true;
}

bool RequestProcessor::PublishHeader() {
  if (header_field_len_ == 0) {
    // There is no header. Do nothing.
    return true;
  }

  // In coming from a prior header value, publish down to route.
  if (route_ &&
      !route_->OnHeader(header_field_.get(), header_field_len_,
                                header_value_.get(), header_value_len_)) {
    ESP_LOGD(TAG, "Route rejected header: %.*s%.*s", header_field_len_,
             header_field_.get(), header_value_len_, header_value_.get());
    return false;
  }
  return true;
}

bool RequestProcessor::PublishBodyData(http_parser* parser, const char* at,
                                       size_t length) {
  if (route_) {
    return route_->OnBodyData(at, length);
  }
  return true;
}

bool RequestProcessor::PublishComplete() {
  if (route_) {
    return route_->OnComplete();
  }
  return true;
}

// static
RequestProcessor* RequestProcessor::ToSelf(http_parser* parser) {
  return static_cast<RequestProcessor*>(parser->data);
}

// static
int RequestProcessor::OnMessageBegin(http_parser* parser) {
  ESP_LOGI(TAG, "Message began");
  RequestProcessor* request_processor = static_cast<RequestProcessor*>(parser->data);
  request_processor->Reset();
  return 0;
}

// static
int RequestProcessor::OnUrl(http_parser* parser, const char* at, size_t length) {
  ESP_LOGI(TAG, "on_url: %.*s", length, at);
  RequestProcessor* request_processor = ToSelf(parser);
  if (!AppendString(request_processor->url_.get(), &request_processor->url_len_, at,
                    length, kUrlMaxLen)) {
    return 1;
  }
  return 0;
}

// static
int RequestProcessor::OnStatus(http_parser* parser, const char* at, size_t length) {
  ESP_LOGW(TAG, "Unexpected Status in request");
  return -1;
}

// static
int RequestProcessor::OnHeaderField(http_parser* parser, const char* at, size_t length) {
  RequestProcessor* request_processor = ToSelf(parser);
  if (request_processor->state_ != State::kParsingHeaderField) {
    if (request_processor->state_ == State::kParsingMethodAndUrl) {
      // Select route when transitioning out of kParsingMethodAndUrl.
      if (!request_processor->PublishMethodAndUrl(static_cast<http_method>(parser->method))) {
        return 1;
      }
    } else if (request_processor->state_ == State::kParsingHeaderValue) {
      if (!request_processor->PublishHeader()) {
        return 1;
      }
    }

    request_processor->state_ = State::kParsingHeaderField;
    request_processor->header_field_len_ = 0;
  }

  if (!AppendString(request_processor->header_field_.get(),
                    &request_processor->header_field_len_, at, length,
                    kHeaderFieldMaxLen)) {
    return 1;
  }

  return 0;
}

//static
int RequestProcessor::OnHeaderValue(http_parser* parser, const char* at, size_t length) {
  RequestProcessor* request_processor = ToSelf(parser);
  if (request_processor->state_ != State::kParsingHeaderValue) {
    request_processor->state_ = State::kParsingHeaderValue;
    request_processor->header_value_len_ = 0;
  }
  if (!AppendString(request_processor->header_value_.get(),
                    &request_processor->header_value_len_, at, length,
                    kHeaderValueMaxLen)) {
    return 1;
  }
  return 0;
}

// static
int RequestProcessor::OnHeadersComplete(http_parser* parser) {
  ESP_LOGI(TAG, "headers complete");
  RequestProcessor* request_processor = ToSelf(parser);
  if (!request_processor->PublishHeader()) {
    return 1;
  }
  return 0;
}

// static
int RequestProcessor::OnBody(http_parser* parser, const char* at, size_t length) {
  RequestProcessor* request_processor = ToSelf(parser);
  request_processor->state_ = State::kParsingBody;
  if (!request_processor->PublishBodyData(parser, at, length)) {
    return 1;
  }
  return 0;
}

// static
int RequestProcessor::OnMessageComplete(http_parser* parser) {
  ESP_LOGI(TAG, "message complete");
  RequestProcessor* request_processor = ToSelf(parser);
  request_processor->state_ = State::kDone;
  if (!request_processor->PublishComplete()) {
    return 1;
  }
  return 0;
}

// static
int RequestProcessor::OnChunkHeader(http_parser* parser) {
  ESP_LOGW(TAG, "Unexpected OnChunkHeader");
  return -1;
}

// static
int RequestProcessor::OnChunkComplete(http_parser* parser) {
  ESP_LOGW(TAG, "Unexpected OnChunkValue");
  return -1;
}
  
}  // namespace hackvac
