#ifndef REQUEST_PROCESSOR_H_
#define REQUEST_PROCESSOR_H_

#include <memory>
#include <stddef.h>

#include "http_parser.h"

namespace hackvac {

class Route;
class Router;
class RouteHandler;

class RequestProcessor {
  public:
    enum class State {
      kParsingMethodAndUrl,
      kParsingHeaderField,
      kParsingHeaderValue,
      kParsingBody,
      kDone,
      kError,
    };

    static constexpr size_t kUrlMaxLen = 2048;
    static constexpr size_t kHeaderFieldMaxLen = 1024;
    static constexpr size_t kHeaderValueMaxLen = 2048;

    explicit RequestProcessor(Router* router);

    bool IsFinished() const { return state_ == State::kDone || state_ == State::kError; }

    void Reset();
    void OnNetError(int err);
    void OnParseError(http_errno err);
    const http_parser_settings* parser_settings() { return &parser_settings_; }

  private:
    bool PublishMethodAndUrl(http_method method);
    bool PublishHeader();
    bool PublishBodyData(http_parser* parser, const char* at, size_t length);
    bool PublishComplete();

    static RequestProcessor* ToSelf(http_parser* parser);

    // Adapt the http_parser callbacks.
    static int OnMessageBegin(http_parser* parser);
    static int OnUrl(http_parser* parser, const char* at, size_t length);
    static int OnStatus(http_parser* parser, const char* at, size_t length);
    static int OnHeaderField(http_parser* parser, const char* at, size_t length); 
    static int OnHeaderValue(http_parser* parser, const char* at, size_t length);
    static int OnHeadersComplete(http_parser* parser);
    static int OnBody(http_parser* parser, const char* at, size_t length);
    static int OnMessageComplete(http_parser* parser);
    static int OnChunkHeader(http_parser* parser);
    static int OnChunkComplete(http_parser* parser);

    State state_;
    std::unique_ptr<Route> route_;
    Router* router_;

    static const http_parser_settings parser_settings_;

    // Data kept on heap because the stacks on FreeRTOS are small.
    std::unique_ptr<char[]> url_;
    size_t url_len_;

    std::unique_ptr<char[]> header_field_;
    size_t header_field_len_;

    std::unique_ptr<char[]> header_value_;
    size_t header_value_len_;
};

}  // namespace hackvac

#endif  // REQUEST_PROCESSOR_H_

