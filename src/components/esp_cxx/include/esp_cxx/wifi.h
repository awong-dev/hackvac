#ifndef ESPCXX_WIFI_H_
#define ESPCXX_WIFI_H_

#include <string>

#include "esp_cxx/cxx17hack.h"

namespace esp_cxx {

class Wifi {
 public:
  Wifi();
  ~Wifi();

  static constexpr size_t kSsidBytes = 32;
  static constexpr size_t kPasswordBytes = 64;
  
  // Set and get the ssid/password from Nvs.
  static std::optional<std::string> GetSsid();
  static std::optional<std::string> GetPassword();

  // The passed in string_views MUST be null terminated and smaller than
  // kSsidBytes and kPasswordBytes respecitvely.
  static void SetSsid(const std::string& ssid);
  static void SetPassword(const std::string& password);

  // Uses the loaded Ssid and Password to connect to an AP.
  // Returns false if those values are not set or some unexpected
  // error occurs.
  //
  // Will attempt to reconnect if network is disconencted.
  //
  // This is mutually exclusive with CreateSetupNetwork().
  bool ConnectToAP();

  // Creates a new ssid network that clients can connect to for
  // configurating this device.
  //
  // This is mutually exclusive with ConnectToAP().
  bool CreateSetupNetwork(const std::string& setup_ssid,
                          const std::string& setup_password);

  // Stops wifi system after either a ConnectToAP() or CreateSetupNetwork()
  // call.
  void Disconnect();
};

}  // namespace esp_cxx

#endif  // ESPCXX_WIFI_H_
