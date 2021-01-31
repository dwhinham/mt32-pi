#
# Build kernel
#

include Config.mk

OBJS		:=	src/config.o \
				src/control/control.o \
				src/control/mister.o \
				src/control/rotaryencoder.o \
				src/control/simplebuttons.o \
				src/control/simpleencoder.o \
				src/kernel.o \
				src/lcd/hd44780.o \
				src/lcd/hd44780fourbit.o \
				src/lcd/hd44780i2c.o \
				src/lcd/sh1106.o \
				src/lcd/ssd1306.o \
				src/lcd/synthlcd.o \
				src/main.o \
				src/midiparser.o \
				src/mt32pi.o \
				src/pisound.o \
				src/power.o \
				src/rommanager.o \
				src/soundfontmanager.o \
				src/synth/mt32synth.o \
				src/synth/soundfontsynth.o \
				src/zoneallocator.o

EXTRACLEAN	+=	src/*.d src/*.o \
				src/control/*.d src/control/*.o \
				src/lcd/*.d src/lcd/*.o \
				src/synth/*.d src/synth/*.o

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

INCLUDE		+=	-I $(FLUIDSYNTHBUILDDIR)/include \
				-I $(FLUIDSYNTHHOME)/include
EXTRALIBS	+=	$(FLUIDSYNTHLIB)

#
# Generate version string from git tag
#
VERSION=$(shell git describe --tags --dirty --always 2>/dev/null)
ifneq ($(VERSION),)
DEFINE		+=	-D MT32_PI_VERSION=\"$(VERSION)\"
endif
