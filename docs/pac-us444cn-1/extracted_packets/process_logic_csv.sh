#!/bin/bash

f=$(mktemp)
outfile=$(basename "$1")

cat "$1" | sort -n > "$f"
./extract_packets.rb "$f" packets-"$outfile"
rm $f
