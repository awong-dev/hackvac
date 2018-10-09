#!/bin/bash

md5=$(md5 $2)
curl -v -H "md5: ${md5}" -F firmware=@$2 http://$1/api/firmware
