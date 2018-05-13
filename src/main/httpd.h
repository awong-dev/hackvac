#ifndef HTTPD_H_
#define HTTPD_H_

#include <stddef.h>

namespace hackvac {

void HttpdTask(void *pvParameters);

// Publishes an event to all listening websockets
void HttpdPublishEvent(void* data, size_t len);

}  // namespace hackvac

#endif  // HTTPD_H_
