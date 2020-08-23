#
# Build kernel
#

include Config.mk

OBJS		:=	src/main.o \
				src/kernel.o \
				src/config.o \
				src/mt32synth.o \
				src/control/mcp23017.o \
				src/lcd/hd44780.o \
				src/lcd/hd44780fourbit.o \
				src/lcd/hd44780i2c.o \
				src/lcd/ssd1306.o

EXTRACLEAN	+=	src/*.d src/*.o \
				src/control/*.d src/control/*.o \
				src/lcd/*.d src/lcd/*.o

#
# inih
#
OBJS		+=	$(INIHHOME)/ini.o
INCLUDE		+=	-I $(INIHHOME)
EXTRACLEAN	+=	$(INIHHOME)/ini.d \
				$(INIHHOME)/ini.o

include $(CIRCLEHOME)/Rules.mk

CFLAGS		+=	-Werror -Wextra -Wno-unused-parameter

CFLAGS		+=	-I "$(NEWLIBDIR)/include" \
				-I $(STDDEF_INCPATH) \
				-I $(CIRCLESTDLIBHOME)/include \
				-I include \
				-I .

LIBS 		:=	$(CIRCLE_STDLIB_LIBS) \
				$(CIRCLEHOME)/addon/SDCard/libsdcard.a \
				$(CIRCLEHOME)/lib/usb/libusb.a \
				$(CIRCLEHOME)/lib/input/libinput.a \
				$(CIRCLEHOME)/addon/fatfs/libfatfs.a \
				$(CIRCLEHOME)/lib/fs/libfs.a \
				$(CIRCLEHOME)/lib/sched/libsched.a \
				$(CIRCLEHOME)/lib/libcircle.a

ifeq ($(HDMI_CONSOLE), 1)
DEFINE		+=	-D HDMI_CONSOLE
endif

-include $(DEPS)

INCLUDE		+=	-I $(MT32EMUBUILDDIR)/include
EXTRALIBS	+=	$(MT32EMULIB)

#
# Generate version string from git tag
#
VERSION=$(shell git describe --tags --dirty 2>/dev/null)
ifneq ($(VERSION),)
DEFINE		+=	-D MT32_PI_VERSION=\"$(VERSION)\"
endif

#
# Convert MT32 ROMs to C headers if baking into kernel
#
ifeq ($(BAKE_MT32_ROMS), 1)
MT32_ROM_HEADERS=mt32_control.h mt32_pcm.h
mt32_control.h: MT32_CONTROL.ROM
mt32_pcm.h: MT32_PCM.ROM

$(MT32_ROM_HEADERS):
	xxd -i $< > $@

DEFINE		+=	-D BAKE_MT32_ROMS
EXTRACLEAN	+=	$(MT32_ROM_HEADERS)
endif
