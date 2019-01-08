#include <stdlib.h>

void * esp_mbedtls_mem_calloc(size_t count, size_t size) {
  return calloc(count, size);
}
void esp_mbedtls_mem_free(void* p) {
  free(p);
}
