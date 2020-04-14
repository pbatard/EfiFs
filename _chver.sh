#!/bin/sh
# Changes the version number
# !!!THIS SCRIPT IS FOR INTERNAL DEVELOPER USE ONLY!!!

type -P sed &>/dev/null || { echo "sed command not found. Aborting." >&2; exit 1; }

if [ ! -n "$1" ]; then
  echo "you must provide a version number (eg. 2.1)"
  exit 1
else
  MAJOR=`echo $1 | sed "s/\(.*\)[.].*/\1/"`
  MINOR=`echo $1 | sed "s/.*[.]\(.*\)/\1/"`
fi
case $MAJOR in *[!0-9]*) 
  echo "$MAJOR is not a number"
  exit 1
esac
case $MINOR in *[!0-9]*) 
  echo "$MINOR is not a number"
  exit 1
esac
echo "changing version to $MAJOR.$MINOR"
sed -b -i -e "s/^#define FS_DRIVER_VERSION_MAJOR.*/#define FS_DRIVER_VERSION_MAJOR $MAJOR/" src/driver.h
sed -b -i -e "s/^#define FS_DRIVER_VERSION_MINOR.*/#define FS_DRIVER_VERSION_MINOR $MINOR/" src/driver.h
sed -b -i -e "s/^\([ \t]*\)VERSION_STRING\([ \t]*\)=.*/\1VERSION_STRING\2= $MAJOR.$MINOR/" EfiFsPkg/*.inf
