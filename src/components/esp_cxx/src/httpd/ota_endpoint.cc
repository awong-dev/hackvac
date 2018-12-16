#include "esp_cxx/httpd/ota_endpoint.h"

#include "esp_cxx/logging.h"

namespace esp_cxx {

namespace {
char to_hex(char nibble) {
  static constexpr char hex[16] = {
    '0', '1', '2', '3',
    '4', '5', '6', '7',
    '8', '9', 'a', 'b',
    'c', 'd', 'e', 'f'
  };
  return hex[nibble & 0xf];
}

bool hex_digit(std::string_view input, unsigned char *val) {
  if (isdigit(input[0])) {
    *val = input[0] - '0';
  } else if ('a' <= input[0] && input[0] <= 'f') {
    *val = 10 + input[0] - 'a';
  } else if ('A' <= input[0] && input[0] <= 'F') {
    *val = 10 + input[0] - 'A';
  } else {
    return false;
  }
  *val <<= 4;
  if (isdigit(input[1])) {
    *val |= input[1] - '0';
  } else if ('a' <= input[1] && input[1] <= 'f') {
    *val |= 10 + input[1] - 'a';
  } else if ('A' <= input[1] && input[1] <= 'F') {
    *val |= 10 + input[0] - 'A';
  } else {
    return false;
  }

  return true;
}

}  // namespace

void OtaEndpoint::OnMultipartStart(HttpRequest request, HttpResponse response) {
  if (in_progress_) {
    response.SendError(400, "Another OTA in progress");
    return;
  }
  in_progress_ = true;
}

void OtaEndpoint::OnMultipart(HttpMultipart multipart, HttpResponse response) {
  assert(in_progress_);
  switch (multipart.state()) {
    case HttpMultipart::State::kBegin: {
      ESP_LOGI(kEspCxxTag, "Firmware upload: %s", multipart.var_name().data());
      if (multipart.var_name() == "firmware") {
        // TODO(awong): Maybe pass in content length?
        ota_writer_ = std::make_unique<OtaWriter>();
      }
      break;
    }

    case HttpMultipart::State::kData: {
      if (multipart.var_name() == "md5") {
        static constexpr char kExpectsMd5[] =
            "Expecting md5 field to be 32-digit hex string";
        // Reading the md5 checksum. Data should be 32 hex digits.
        if (multipart.data().size() != 32) {
          response.SendError(400, kExpectsMd5);
          return;
        }
        ESP_LOGI(kEspCxxTag, "Read md5");
        has_expected_md5_ = true;
        for (int i = 0; i < expected_md5_.size(); i++) {
          if (!hex_digit(multipart.data().substr(i*2, 2), &expected_md5_[i])) {
            response.SendError(400, kExpectsMd5);
            // TODO(awong): On error... how do we reset the in_progress_?
            return;
          }
        }
      } else if (multipart.var_name() == "firmware") {
        // This is the firmware blob. Write it!
        ota_writer_->Write(multipart.data());
      }
      break;
    }

    case HttpMultipart::State::kEnd: {
      ESP_LOGI(kEspCxxTag, "Ending multipart: %s", multipart.var_name().data());
      if ("firmware" == multipart.var_name()) {
        ota_writer_->Finish();
      }
      break;
    }

    default:
    case HttpMultipart::State::kRequestEnd:
      in_progress_ = false;
      // TODO(awong): Send an error.
      ESP_LOGI(kEspCxxTag, "Flashing done");
      int status = 400;
      auto& actual_md5 = ota_writer_->md5();
      if (has_expected_md5_ &&
          actual_md5 == expected_md5_) {
        status = 200;
        
        ESP_LOGI(kEspCxxTag, "Setting new OTA to boot.");
        ota_writer_->SetBootPartition();

        // TODO(awong): This should call a shutdown hook.
        ESP_LOGI(kEspCxxTag, "Firmware flashed!");
      }
      // Yay! All good!
      static const std::string_view kFirmwareResponse("uploaded firmware md5: ");
      char md5hex[32];
      response.Send(status, kFirmwareResponse.size() + sizeof(md5hex),
                   HttpResponse::kContentTypePlain, kFirmwareResponse);
      for (int i = 0; i < actual_md5.size(); ++i) {
        uint8_t byte = actual_md5[i];
        md5hex[i*2] = to_hex(byte);
        md5hex[i*2 + 1] = to_hex(byte >> 4);
      }
      response.SendMore({&md5hex[0], sizeof(md5hex)});
      ota_writer_.reset();
  }
}

}  // namespace esp_cxx
