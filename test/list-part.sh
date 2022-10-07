#!/bin/bash
# Script to list partitions from generated disk image

set -ex

if [ -z "$1" ]; then
    echo "Missing source filename"
    exit 1
fi

loopdev=$(sudo losetup -f)
sudo losetup -P $loopdev "$1"
sleep 1
sudo blkid -ip $loopdev* > out/blkid.txt
sed -i -e 's/ PART_ENTRY_DISK=".*"//g' -e "s/${loopdev//\//\\\/}/\/dev\/loop5/g" out/blkid.txt
lsblk
cat out/blkid.txt
sudo losetup -d $loopdev

# Check against provided file, if it exists
if [ -n "$2" ]; then
    diff -qs out/blkid.txt $2
fi
