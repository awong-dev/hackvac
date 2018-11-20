#include "esp_cxx/httpd/ota_endpoint.h"

namespace esp_cxx {

void OtaEndpoint(const HttpRequest& request, HttpResponse response) {
#if 0
bool hex_digit(const char input[], unsigned char *val) {
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

void HandleFirmware(mg_connection *nc, int event, void *ev_data) {
  ESP_LOGD(kTag, "Got firwamware update");
  bool is_response_sent = false;

  struct OtaContext {
    OtaContext() : has_expected_md5(false), update_partition(nullptr), out_handle(-1) {
      mbedtls_md5_init(&md5_ctx);
      memset(&actual_md5[0], 0xcd, sizeof(actual_md5));
      memset(&expected_md5[0], 0x34, sizeof(expected_md5));
    }

    unsigned char expected_md5[16];
    bool has_expected_md5;
    unsigned char actual_md5[16];
    mbedtls_md5_context md5_ctx;
    const esp_partition_t* update_partition;
    esp_ota_handle_t out_handle;
  };

  switch (event) {
    case MG_EV_HTTP_MULTIPART_REQUEST: {
      ESP_LOGI(kTag, "firmware upload starting");
      nc->user_data = new OtaContext();
      break;
    }
    case MG_EV_HTTP_PART_BEGIN: {
      OtaContext* context =
        static_cast<OtaContext*>(nc->user_data);
      if (!context) return;

      mg_http_multipart_part* multipart =
        static_cast<mg_http_multipart_part*>(ev_data);
      ESP_LOGI(kTag, "Starting multipart: %s", multipart->var_name);
      if (strcmp("firmware", multipart->var_name) == 0) {
        mbedtls_md5_starts(&context->md5_ctx);
        context->update_partition = esp_ota_get_next_update_partition(NULL);
        ESP_ERROR_CHECK(esp_ota_begin(context->update_partition,
                                      OTA_SIZE_UNKNOWN, &context->out_handle));
      }
      break;
    }
    case MG_EV_HTTP_PART_DATA: {
      OtaContext* context =
        static_cast<OtaContext*>(nc->user_data);
      if (!context) return;

      mg_http_multipart_part* multipart =
        static_cast<mg_http_multipart_part*>(ev_data);

      if (strcmp("md5", multipart->var_name) == 0 &&
          multipart->data.len != 0) {
        static constexpr char kExpectsMd5[] =
          "Expecting md5 field to be 32-digit hex string";
        // Reading the md5 checksum. Data should be 32 hex digits.
        if (multipart->data.len != 32) {
          is_response_sent = true;
          mg_send_head(nc, 400, strlen(kExpectsMd5),
                       "Content-Type: text/plain");
          mg_send(nc, kExpectsMd5, strlen(kExpectsMd5));
          goto abort_request;
        }

        ESP_LOGI(kTag, "Read md5");
        context->has_expected_md5 = true;
        for (int i = 0; i < sizeof(context->expected_md5); i++) {
          if (!hex_digit(multipart->data.p + i*2, &context->expected_md5[i])) {
            is_response_sent = true;
            mg_send_head(nc, 400, strlen(kExpectsMd5),
                         "Content-Type: text/plain");
            mg_send(nc, kExpectsMd5, strlen(kExpectsMd5));
            goto abort_request;
          }
        }
      } else if (strcmp("firmware", multipart->var_name) == 0) {
          // This is the firmware blob. Write it! And hash it.
          mbedtls_md5_update(&context->md5_ctx,
                             reinterpret_cast<const unsigned char*>(
                                 multipart->data.p),
                             multipart->data.len); 
          ESP_ERROR_CHECK(esp_ota_write(context->out_handle,
                                        multipart->data.p,
                                        multipart->data.len));
      }
      break;
    }
    case MG_EV_HTTP_PART_END: {
      OtaContext* context = static_cast<OtaContext*>(nc->user_data);
      if (!context) return;
      mg_http_multipart_part* multipart =
          static_cast<mg_http_multipart_part*>(ev_data);
      ESP_LOGI(kTag, "Ending multipart: %s", multipart->var_name);
      if (strcmp("firmware", multipart->var_name) == 0) {
        mbedtls_md5_finish(&context->md5_ctx, &context->actual_md5[0]);
        mbedtls_md5_free(&context->md5_ctx);
      }
      break;
    }
    case MG_EV_HTTP_MULTIPART_REQUEST_END: {
      ESP_LOGI(kTag, "Flashing done");
      OtaContext* context = static_cast<OtaContext*>(nc->user_data);
      if (!context) return;

      int status = 400;
      if (context->has_expected_md5 &&
          memcmp(&context->actual_md5[0], &context->expected_md5[0],
                 sizeof(context->actual_md5)) == 0) {
        status = 200;
        
        ESP_LOGI(kTag, "Setting new OTA to boot.");
        ESP_ERROR_CHECK(esp_ota_end(context->out_handle));
        ESP_ERROR_CHECK(esp_ota_set_boot_partition(context->update_partition));

        // TODO(awong): This should call a shutdown hook.
        SetBootState(BootState::FRESH);
        ESP_LOGI(kTag, "Firmware flashed!");
      }
      // Yay! All good!
      static constexpr char kFirmwareSuccess[] = "uploaded firmware md5: ";
      mg_send_head(nc, status, strlen(kFirmwareSuccess) + 32,
                   "Content-Type: text/plain");
      mg_send(nc, kFirmwareSuccess, strlen(kFirmwareSuccess));
      for (int i = 0; i < sizeof(context->actual_md5); ++i) {
        mg_printf(nc, "%02x", context->actual_md5[i]);
      }
      is_response_sent = true;
      break;
    }
    default:
      static constexpr char kExpectsFileUpload[] =
        "Expecting http multi-part file upload";
      mg_send_head(nc, 400, strlen(kExpectsFileUpload),
                   "Content-Type: text/plain");
      mg_send(nc, kExpectsFileUpload, strlen(kExpectsFileUpload));
      is_response_sent = true;
      break;
  }
abort_request:

  if (is_response_sent) {
    ESP_LOGD(kTag, "Response sent. Cleaning up.");
    OtaContext* context = static_cast<OtaContext*>(nc->user_data);
    delete context;
    nc->user_data = nullptr;
    nc->flags |= MG_F_SEND_AND_CLOSE;
  }
}
#endif
}

}  // namespace esp_cxx
