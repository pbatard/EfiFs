#!/usr/bin/env bash

for f in *.img *.iso; do
  [ -e "$f" ] || continue
  7z a -tzip "${f%.*}.zip" "$f"
done