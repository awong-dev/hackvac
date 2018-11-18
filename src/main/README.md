## Api paths
/ - reads current status.
/api/firmware - uploads a new firmware.
/api/wificonfig - json: { ssid: '', password: '' }
/api/events - websocket of log events (in json?)

## Next bits of functionality
  * Ensure packet log has source/dest and timing.
  * Hook up HvacSettings with web server, both on reading and writing.
  * Make HvacSettings internal rep equal to the data packet format.
  * Handle room temperature.
  * Implement timings.
  * Set up a generation clock on HvacSettings (perhaps vector for web vs local settings?).
  * Write Hvac control signal handling. Algorithm: periodic send of query and data set.
  * Ensure websocket sends data with minimal drops. Move JSON into there.
  * Extract firmware/config/events API into `esp_cxx` in a bootstrap module.
  * Extract wifi into `esp_cxx`
  * Setup firmware revert to previous successful version, not factory.
