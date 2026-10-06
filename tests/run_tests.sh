#!/usr/bin/env bash

## @file
#  Test script for EfiFs.
#
#  Copyright (c) 2023-2026, Pete Batard <pete@akeo.ie>
#
#  SPDX-License-Identifier: GPL-2.0-or-later
#
##

LINE_LEN=58
TIMEOUT=2m
NUM_PASS=0
NUM_FAIL=0
NUM_ERROR=0
BOOT_DRIVE="FS1:"

fs_names=(bfs btrfs cbfs cpio erofs exfat ext2 f2fs hfs hfsplus iso9660 jfs minix minix2 minix3 newc nilfs2 ntfs odc reiserfs romfs squash4 tar udf ufs1 ufs2 xfs zfs)

# Default to x86_64 testing if no variables set.
if [[ -z "$QEMU_CMD" ]]; then
  QEMU_CMD="qemu-system-x86_64 -M q35 -nodefaults -nographic -serial stdio -net none -L . -drive if=pflash,format=raw,unit=0,file=OVMF.fd,readonly=on -drive format=raw,file=fat:rw:image"
fi
if [[ -z "$UEFI_ARCH" ]]; then
  UEFI_ARCH=x64
fi
# The RISC-V 64 firmware inverts the drives...
if [[ "$UEFI_ARCH" == "riscv64" ]]; then
  BOOT_DRIVE="FS0:"
fi

i=1
for fs_name in ${fs_names[@]}; do
  boot_dir='\EFI\Boot\'
  ext="img"
  if [[ "$fs_name" == "iso9660" ||  "$fs_name" == "udf" ]]; then
    ext="iso"
  elif [[ "$fs_name" == "zfs" ]]; then
    boot_dir='\EFI\@\Boot\'
  fi

  # Only test the file systems for which we have a driver in ./image/
  if [[ ! -f image/${fs_name}_${UEFI_ARCH}.efi ]]; then
    continue
  fi

  # An arguments can be provided to run only a specific test
  if [[ ! -z "$1" && "$1" != "$fs_name" ]]; then
    continue
  fi

  if [[ ! -f tests/${fs_name}.${ext} ]]; then
    7z e -y tests/${fs_name}.zip -otests/ > /dev/null
  fi
  if [[ ! -f tests/${fs_name}.${ext} ]]; then
    echo "Failed to extract tests/${fs_name}.zip"
    NUM_ERROR=$((NUM_ERROR + 1))
    continue
  fi

  test_desc=$(printf "Test #%02d: %s..." $i $fs_name)
  echo -n $test_desc
  num_blanks=$((LINE_LEN - ${#test_desc}))
  if [[ $num_blanks -gt 0 ]]; then
    padding="$(printf '%*s' $num_blanks)"
    echo -n "$padding"
  fi
  i=$((i+1))

  # Run qemu with a timeout
  rm -f output.txt error.txt
  cat << EOF > image/startup.nsh
@echo -off
load FS0:\\${fs_name}_${UEFI_ARCH}.efi
map -r
${BOOT_DRIVE}${boot_dir}boot${UEFI_ARCH}.efi -test
reset -s > NUL
EOF
  timeout --foreground $TIMEOUT bash -c "${QEMU_CMD} -drive format=raw,file=tests/${fs_name}.${ext} 1>output.txt 2>error.txt"

  err=$?
  if [[ $err -eq 124 ]]; then
    echo "[ERROR] (Time out)"
    NUM_ERROR=$((NUM_ERROR + 1))
  elif [[ $err -ne 0 ]]; then
    echo "[ERROR] ($err)"
    cat error.txt
    NUM_ERROR=$((NUM_ERROR + 1))
  else
    if grep -aq 'Test Passed' output.txt; then
      echo "[PASS]"
      NUM_PASS=$((NUM_PASS + 1))
    else
      echo "[FAIL]"
      NUM_FAIL=$((NUM_FAIL + 1))
      echo "Output of failed test was:"
      echo "----------------------------------------------------------------"
      tail -n +3 output.txt
      echo "----------------------------------------------------------------"
    fi
  fi

done

echo
echo "================================================================"
echo "# Test summary for EfiFs"
echo "================================================================"
echo "# TOTAL: $((NUM_PASS + NUM_FAIL + NUM_ERROR))"
echo "# PASS:  $NUM_PASS"
echo "# FAIL:  $NUM_FAIL"
echo "# ERROR: $NUM_ERROR"
echo "================================================================"

if [[ $NUM_FAIL -ne 0 || $NUM_ERROR -ne 0 ]]; then
  exit 1
fi
