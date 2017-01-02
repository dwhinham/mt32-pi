#!/bin/sh

set -u
set -e

# Add a console on tty1
if [ -e ${TARGET_DIR}/etc/inittab ]; then
    grep -qE '^tty1::' ${TARGET_DIR}/etc/inittab || \
	sed -i '/GENERIC_SERIAL/a\
tty1::respawn:/sbin/getty -L  tty1 0 vt100 # HDMI console' ${TARGET_DIR}/etc/inittab
fi

# Override Raspberry Pi config.txt
echo -n "Copying Raspberry Pi boot config..."
cp ${BR2_EXTERNAL_MT32_PI_PATH}/rpi-bootconfig.txt ${BINARIES_DIR}/rpi-firmware/config.txt
echo " OK!"

# Copy MT-32 ROMs into place
echo -n "Copying MT-32 ROMs..."
if [ ! -f ${BR2_EXTERNAL_MT32_PI_PATH}/mt32-roms/MT32_CONTROL.ROM ]; then
    echo "\nMT32_CONTROL.ROM is missing. Please place it into the mt32-roms directory."
    exit 1
fi

if [ ! -f ${BR2_EXTERNAL_MT32_PI_PATH}/mt32-roms/MT32_PCM.ROM ]; then
    echo "\nMT32_PCM.ROM is missing. Please place it into the mt32-roms directory."
    exit 1
fi

mkdir -p ${TARGET_DIR}/usr/share/mt32-rom-data
cp ${BR2_EXTERNAL_MT32_PI_PATH}/mt32-roms/*.ROM ${TARGET_DIR}/usr/share/mt32-rom-data/
echo " OK!"