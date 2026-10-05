#!/bin/bash
# This assumes that the EFI/Boot structure is under a ./boot directory
cbfstool /root/cbfs.img create -m x86 -s 4M
cd boot/
# Find all files and add them recursively into the target ROM
find . -type f | while read -r file; do
  # Strip the leading "./" from the path string
  cbfs_name="${file#./}"
  echo "Adding $file as flat CBFS name: $cbfs_name"
  # Inject it into the ROM image using standard type "raw"
  cbfstool /root/cbfs.img add -f "$file" -n "$cbfs_name" -t raw
done
