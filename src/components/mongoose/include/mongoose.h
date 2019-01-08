#ifndef ESP_MONGOOSE_WRAPPER_H_
#define ESP_MONGOOSE_WRAPPER_H_

// Handle to ensure all users of his component have the same configuration
// for mongoose. This ideally would be done by having a setting in
// components.mk that allowed setting CFLAGS/CXXFLAGS/CPPFLAGS for all users
// of the component, but that does not exist.
//
// KEEP THIS IN SYNC WITH components.mk OR THERE WILL BE UNDEFINED BEHAVIOR.
#define MG_ENABLE_HTTP_STREAMING_MULTIPART 1
#define MG_ENABLE_HTTP_WEBSOCKET 1
#define MG_ENABLE_CALLBACK_USERDATA 1
#define MG_ENABLE_BROADCAST 1
#define MG_ENABLE_SSL 1
#define MG_SSL_IF MG_SSL_IF_MBEDTLS

#include "../src/mongoose.h"

#endif  // ESP_MONGOOSE_WRAPPER_H_
