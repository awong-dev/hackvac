#!/bin/bash

curl -v -N -H "Connection: Upgrade" -H "Upgrade: websocket" -H "Host: $1" -H "Origin: http://$1" http://$1$2
