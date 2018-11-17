#ifndef EVENT_LOG_H_
#define EVENT_LOG_H_

#include <stddef.h>
#include <functional>
#include <memory>

namespace hackvac {

void IncrementListeners();
void DecrementListeners();

void EventLogInit();

}  // namespace hackvac

#endif   // EVENT_LOG_H_
