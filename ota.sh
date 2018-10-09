#!/bin/bash

curl -v -F md5=$(md5 -q "$2") -F firmware=@$2 http://$1/api/firmware
