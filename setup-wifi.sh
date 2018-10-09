#!/bin/bash
set -x

curl -v -H "Content-Type: application/json" -d "{\"ssid\": \"$2\", \"password\": \"$3\"}" http://$1/api/wificonfig
