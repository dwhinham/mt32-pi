#!/bin/bash

BOARD=$1

CIRCLE_STDLIB_HOME=external/circle-stdlib
CIRCLE_BOOT_HOME=$CIRCLE_STDLIB_HOME/libs/circle/boot

if [ "$BOARD" = "pi0" ]; then
    echo "Building for Raspberry Pi 0/1"
elif [ "$BOARD" = "pi2" ]; then
    echo "Building for Raspberry Pi 2"
elif [ "$BOARD" = "pi3" ]; then
    echo "Building for Raspberry Pi 3"
elif [ "$BOARD" = "pi4" ]; then
    echo "Building for Raspberry Pi 4"
elif [ "$BOARD" = "pi4-64" ]; then
    echo "Building for Raspberry Pi 4 (64-bit)"
else
    echo "Please specify target board type: [ pi0 | pi2 | pi3 | pi4 | pi4-64 ]"
    exit 1
fi

#
# Build circle-stdlib
#
pushd $CIRCLE_STDLIB_HOME
if [ ! -f Config.mk ]; then
    if [ "$BOARD" = "pi0" ]; then
        ./configure --raspberrypi=1
    elif [ "$BOARD" = "pi2" ];then
        ./configure --raspberrypi=2
    elif [ "$BOARD" = "pi3" ];then
        ./configure --raspberrypi=3
    elif [ "$BOARD" = "pi4" ]; then
        ./configure --raspberrypi=4
    elif [ "$BOARD" = "pi4-64" ]; then
        ./configure --raspberrypi=4 --prefix=aarch64-none-elf
    fi
fi

# Enable garbage collection
grep -qF "GC_SECTIONS" libs/circle/Config.mk || echo "GC_SECTIONS = 1" >> libs/circle/Config.mk

# Add serial bootloader configuration
grep -qF "SERIALPORT" libs/circle/Config.mk || cat << EOF >> libs/circle/Config.mk
SERIALPORT = /dev/ttyUSB0
FLASHBAUD = 3000000
USERBAUD = 115200
EOF

make -s -j8
popd

#
# Build boot files
#
if [ ! -f sdcard/start.elf ]; then
    make -C $CIRCLE_BOOT_HOME
    cp  $CIRCLE_BOOT_HOME/bcm2711-rpi-4-b.dtb \
        $CIRCLE_BOOT_HOME/bootcode.bin \
        $CIRCLE_BOOT_HOME/COPYING.linux \
        $CIRCLE_BOOT_HOME/fixup4.dat \
        $CIRCLE_BOOT_HOME/fixup.dat \
        $CIRCLE_BOOT_HOME/LICENCE.broadcom \
        $CIRCLE_BOOT_HOME/start4.elf \
        $CIRCLE_BOOT_HOME/start.elf \
        sdcard
fi

# TODO: 64-bit (armstub and config.txt)

#
# Build mt32emu
#

# Extract CFLAGS from circle-stdlib config so that we can build mt32emu with matching flags
CIRCLE_CFLAGS=`grep CFLAGS_FOR_TARGET external/circle-stdlib/Config.mk | cut -d " " -f3-`
export CFLAGS="${CIRCLE_CFLAGS}"
export CXXFLAGS="${CIRCLE_CFLAGS}"

mkdir -p build-munt
pushd build-munt
cmake   ../external/munt/mt32emu \
        -DARM_HOME=$(realpath ~/gcc-arm-9.2-2019.12-x86_64-arm-none-eabi) \
        -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi.cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -Dlibmt32emu_C_INTERFACE=FALSE \
        -Dlibmt32emu_SHARED=FALSE
cmake --build .
popd

#
# Built mt32-pi
#

make
cp kernel*.img sdcard
