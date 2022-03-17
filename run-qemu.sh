#!/bin/sh
set -e

dd if=/dev/zero of=sdcard.img count=512 bs=1M
sed -e 's/\s*\([\+0-9a-zA-Z]*\).*/\1/' << FDISK_CMDS | fdisk sdcard.img
o      # create new DOS label
n      # add new partition
       # default - primary
       # default - first partition
       # default - first sector
       # default - last sector
t      # change partition type
c      # W95 FAT32 (LBA)
w      # write partition table and exit
FDISK_CMDS

sudo losetup -f sdcard.img -o 1048576

working_dir=$(pwd)
loop_device=$(losetup | grep $working_dir/sdcard.img | cut -d " " -f 1)

sudo mkfs.vfat $loop_device

mkdir -p tmp
sudo mount $loop_device tmp
sudo cp -r sdcard/* tmp/
sudo cp kernel*.img tmp/
sudo umount tmp
sudo losetup -d $loop_device
rm -rf tmp

#remmina -c vnc://127.0.0.1:5900 &
/home/dale/git/qemu/build/qemu-system-aarch64 \
    -serial stdio \
    -M raspi3b \
    -kernel kernel8.img \
    -drive file=sdcard.img,if=sd,format=raw \
    -device usb-net,netdev=net0 \
    -netdev user,id=net0,hostfwd=tcp::8080-:80 \
    #-s -S
    #-d guest_errors,unimp \
