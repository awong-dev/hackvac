#include <stdint.h>
#include <stdlib.h>

int mg_ssl_if_mbed_random(void *ctx, unsigned char *buf, size_t len) {
  while (len > 0) {
#ifndef FAKE_ESP_IDF
    uint32_t r = esp_random(); /* Uses hardware RNG. */
#else
#warning BAD RANDOM
#warning BAD RANDOM
#warning BAD RANDOM
#warning BAD RANDOM
#warning BAD RANDOM
    uint32_t r = rand(); 
#endif
    for (int i = 0; i < 4 && len > 0; i++, len--) {
      *buf++ = (uint8_t) r;
      r >>= 8;
    }
  }
  (void) ctx;
  return 0;
}
