#!/bin/sh
# This script creates a new driver project
TEMPLATE=exfat
TEMPLATE_GUID=25E5B551-F9DD-4D25-A7CD-A1090B558A49
GUID=`curl -ks https://www.guidgen.com | grep YourGuidLabel | sed 's/.*value=\"\(.*\)\".*/\1/'`
cp $TEMPLATE.vcxproj $1.vcxproj
cp $TEMPLATE.vcxproj.filters $1.vcxproj.filters
cp $TEMPLATE.vcxproj.user $1.vcxproj.user
sed -i "s/$TEMPLATE_GUID/$GUID/" $1.vcxproj
sed -i "s/$TEMPLATE/$1/g" $1.vcxproj
sed -i "s/$/\r/" $1.vcxproj
sed -i "s/$TEMPLATE/$1/g" $1.vcxproj.filters
sed -i "s/$/\r/" $1.vcxproj.filters
echo Created project files using GUID $GUID