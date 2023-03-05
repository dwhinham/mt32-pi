#
# Build configuration
#

# Valid options: pi3, pi3-64, pi4, pi4-64
BOARD?=pi3-64
HDMI_CONSOLE?=0

# Serial bootloader config
SERIALPORT?=/dev/ttyUSB0
FLASHBAUD?=3000000
USERBAUD?=115200

# Enable section garbage collection
GC_SECTIONS?=1

# Compress the kernel
GZIP_KERNEL?=1

# Toolchain setup
ifeq ($(BOARD), pi2)
RASPBERRYPI=2
BITS=32
CPU_FLAGS=-mcpu=cortex-a7 -marm -mfpu=neon-vfpv4 -mfloat-abi=hard
PREFIX=arm-none-eabi-
KERNEL=kernel7
else ifeq ($(BOARD), pi3)
RASPBERRYPI=3
BITS=32
CPU_FLAGS=-mcpu=cortex-a53 -marm -mfpu=neon-fp-armv8 -mfloat-abi=hard
PREFIX=arm-none-eabi-
KERNEL=kernel8-32
else ifeq ($(BOARD), pi3-64)
RASPBERRYPI=3
BITS=64
CPU_FLAGS=-mcpu=cortex-a53 -mlittle-endian
PREFIX=aarch64-none-elf-
KERNEL=kernel8
else ifeq ($(BOARD), pi4)
RASPBERRYPI=4
BITS=32
CPU_FLAGS=-mcpu=cortex-a72 -marm -mfpu=neon-fp-armv8 -mfloat-abi=hard
PREFIX=arm-none-eabi-
KERNEL=kernel7l
else ifeq ($(BOARD), pi4-64)
RASPBERRYPI=4
BITS=64
CPU_FLAGS=-mcpu=cortex-a72 -mlittle-endian
PREFIX=aarch64-none-elf-
KERNEL=kernel8-rpi4
else
$(error Invalid board type "$(BOARD)"; please specify one of [ pi2 | pi3 | pi3-64 | pi4 | pi4-64 ])
endif

# Compiler flags for external dependencies
CFLAGS_EXTERNAL = $(CPU_FLAGS)
ifeq ($(strip $(GC_SECTIONS)),1)
CFLAGS_EXTERNAL += -ffunction-sections -fdata-sections
endif

ifeq ($(PREFIX), arm-none-eabi-)
CMAKE_TOOLCHAIN_FLAGS=-DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi.cmake
else
CMAKE_TOOLCHAIN_FLAGS=-DCMAKE_TOOLCHAIN_FILE=../cmake/aarch64-none-elf.cmake
endif

# Paths
CIRCLESTDLIBHOME=$(realpath external/circle-stdlib)
CIRCLE_STDLIB_CONFIG=$(CIRCLESTDLIBHOME)/Config.mk

CIRCLEHOME=$(CIRCLESTDLIBHOME)/libs/circle
CIRCLE_CONFIG=$(CIRCLEHOME)/Config.mk

NEWLIB_ARCH=$(firstword $(subst -, ,$(PREFIX)))-none-circle
NEWLIBDIR=$(CIRCLESTDLIBHOME)/install/$(NEWLIB_ARCH)
CIRCLE_STDLIB_LIBS=$(NEWLIBDIR)/lib/libm.a \
		   $(NEWLIBDIR)/lib/libc.a \
		   $(NEWLIBDIR)/lib/libcirclenewlib.a

BOOTHOME=$(CIRCLEHOME)/boot
BOOT_FILES=$(BOOTHOME)/bcm2711-rpi-4-b.dtb \
	   $(BOOTHOME)/bootcode.bin \
	   $(BOOTHOME)/COPYING.linux \
	   $(BOOTHOME)/fixup4.dat \
	   $(BOOTHOME)/fixup.dat \
	   $(BOOTHOME)/LICENCE.broadcom \
	   $(BOOTHOME)/start4.elf \
	   $(BOOTHOME)/start.elf

MT32EMUHOME=$(realpath external/munt/mt32emu)
MT32EMUBUILDDIR=build-munt
MT32EMULIB=$(MT32EMUBUILDDIR)/libmt32emu.a

FLUIDSYNTHHOME=$(realpath external/fluidsynth)
FLUIDSYNTHBUILDDIR=build-fluidsynth
FLUIDSYNTHLIB=$(FLUIDSYNTHBUILDDIR)/src/libfluidsynth.a

INIHHOME=$(realpath external/inih)
