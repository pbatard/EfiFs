#!/bin/bash
grub_include=`dirname $(readlink -f $0)`/grub/include/grub
shopt -s nocasematch
case "$1" in
  "x64")     arch=x86_64;;
  "x86")     arch=i386;;
  "ia32")    arch=i386;;
  "arm")     arch=arm;;
  "arm64")   arch=arm64;;
  "aa64")    arch=arm64;;
  "aarch64") arch=arm64;;
  "riscv64") arch=riscv64;;
  "loongarch64") arch=loongarch64;;
  *) echo "Unsupported arch"; exit 1;;
esac
 rm -f $grub_include/cpu
 ln -vs $grub_include/$arch $grub_include/cpu
