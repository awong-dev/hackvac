#ifndef EVENT_LOG_H_
#define EVENT_LOG_H_

#include <stddef.h>

struct mg_connection;

namespace hackvac {

void IncrementListeners();
void DecrementListeners();

void EventLogInit();

}  // namespace hackvac

#endif   // EVENT_LOG_H_
