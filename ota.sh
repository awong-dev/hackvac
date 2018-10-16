#!/bin/bash

curl -v -F md5=$(md5 -q "$2") -F firmware=@$2 http://$1/api/firmware

echo ".. Restarting in 5s ..."
sleep 5
curl -v http://$1/api/restart
