#
# Build configuration
#

# Path to ARM toolchain
ARM_HOME?=$(HOME)/gcc-arm-9.2-2019.12-x86_64-arm-none-eabi

# Valid options: pi0, pi2, pi3, pi4, pi4-64
BOARD?=pi4
HDMI_CONSOLE?=0
BAKE_MT32_ROMS?=0

# Serial bootloader config
SERIALPORT?=/dev/ttyUSB0
FLASHBAUD?=3000000
USERBAUD?=115200

# Enable section garbage collection
GC_SECTIONS?=1

CIRCLESTDLIBHOME=$(realpath external/circle-stdlib)
CIRCLE_STDLIB_CONFIG=$(CIRCLESTDLIBHOME)/Config.mk

CIRCLEHOME=$(CIRCLESTDLIBHOME)/libs/circle
CIRCLE_CONFIG=$(CIRCLEHOME)/Config.mk

NEWLIBDIR=$(CIRCLESTDLIBHOME)/install/$(NEWLIB_ARCH)
CIRCLE_STDLIB_LIBS=$(NEWLIBDIR)/lib/libm.a \
				   $(NEWLIBDIR)/lib/libc.a \
				   $(NEWLIBDIR)/lib/libcirclenewlib.a

MT32EMUHOME=$(realpath external/munt/mt32emu)
MT32EMUBUILDDIR=build-munt
MT32EMULIB=$(MT32EMUBUILDDIR)/libmt32emu.a

include $(CIRCLE_STDLIB_CONFIG)
