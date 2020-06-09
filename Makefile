#
# Makefile
#

include external/circle-stdlib/Config.mk

CIRCLEHOME = external/circle-stdlib/libs/circle
NEWLIBDIR = external/circle-stdlib/install/$(NEWLIB_ARCH)

OBJS	= main.o kernel.o

include $(CIRCLEHOME)/Rules.mk

CFLAGS += -I "$(NEWLIBDIR)/include" -I $(STDDEF_INCPATH) -I external/circle-stdlib/include
LIBS := "$(NEWLIBDIR)/lib/libm.a" "$(NEWLIBDIR)/lib/libc.a" "$(NEWLIBDIR)/lib/libcirclenewlib.a" \
	  $(CIRCLEHOME)/addon/SDCard/libsdcard.a \
	  $(CIRCLEHOME)/lib/usb/libusb.a \
	  $(CIRCLEHOME)/lib/input/libinput.a \
	  $(CIRCLEHOME)/addon/fatfs/libfatfs.a \
	  $(CIRCLEHOME)/lib/fs/libfs.a \
	  $(CIRCLEHOME)/lib/sched/libsched.a \
	  $(CIRCLEHOME)/lib/libcircle.a

-include $(DEPS)

INCLUDE	+= -I build-munt/include
EXTRALIBS += build-munt/libmt32emu.a
