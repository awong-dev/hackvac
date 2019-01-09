#include <stdint.h>

#ifdef FAKE_ESP_IDF
#include <stdlib.h>
#include <stdio.h>
#endif

int mg_ssl_if_mbed_random(void *ctx, unsigned char *buf, size_t len) {
#ifndef FAKE_ESP_IDF
  while (len > 0) {
    uint32_t r = esp_random(); /* Uses hardware RNG. */
    for (int i = 0; i < 4 && len > 0; i++, len--) {
      *buf++ = (uint8_t) r;
      r >>= 8;
    }
  }
#else
    static FILE* dev_rand = NULL;
    if (!dev_rand) {
      dev_rand = fopen("/dev/random", "rb");
    } else {
      if (fread(buf, len, 1, dev_rand) != 1) {
        return -1;
      }
    }
#endif
  (void) ctx;
  return 0;
}
