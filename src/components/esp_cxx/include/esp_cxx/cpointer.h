#ifndef ESPCXX_CPOINTER_H_
#define ESPCXX_CPOINTER_H_

#include "cJSON.h"

namespace esp_cxx {

struct free_deleter{
  template <typename T>
    void operator()(T *p) const {
      std::free(const_cast<std::remove_const_t<T>*>(p));
    }
};

template <typename T>
using unique_C_ptr=std::unique_ptr<T,free_deleter>;
static_assert(sizeof(char *)==
              sizeof(unique_C_ptr<char>),""); // ensure no overhead

struct cJSON_deleter{
  void operator()(cJSON *obj) const {
    cJSON_Delete(obj);
  }
};

using unique_cJSON_ptr=std::unique_ptr<cJSON, cJSON_deleter>;
static_assert(sizeof(cJSON *)==
              sizeof(unique_cJSON_ptr),""); // ensure no overhead

}  // namespace esp_cxx

#endif  // ESPCXX_CPOINTER_H_
