#
# Makefile
#

include Config.mk

.DEFAULT_GOAL=all
.PHONY: all clean veryclean

#
# Configure circle-stdlib
#
.ONESHELL:
$(CIRCLE_STDLIB_CONFIG) $(CIRCLE_CONFIG)&:
ifeq ($(BOARD), pi0)
	echo "Configuring for Raspberry Pi 0/1"
	$(CIRCLESTDLIBHOME)/configure --raspberrypi=1
else ifeq ($(BOARD), pi2)
	echo "Configuring for Raspberry Pi 2"
	$(CIRCLESTDLIBHOME)/configure --raspberrypi=2
else ifeq ($(BOARD), pi0)
	echo "Configuring for Raspberry Pi 3"
	$(CIRCLESTDLIBHOME)/configure --raspberrypi=3
else ifeq ($(BOARD), pi4)
	echo "Configuring for Raspberry Pi 4"
	$(CIRCLESTDLIBHOME)/configure --raspberrypi=4
else ifeq ($(BOARD), pi4)
	echo "Configuring for Raspberry Pi 4"
	$(CIRCLESTDLIBHOME)/configure --raspberrypi=4 --prefix=aarch64-none-elf
else
	$(error Invalid board type "$(BOARD)"; please specify one of [ pi0 | pi2 | pi3 | pi4 | pi4-64 ])
endif

#
# Build circle-stdlib
#
$(CIRCLE_STDLIB_LIBS)&: $(CIRCLE_STDLIB_CONFIG)
	$(MAKE) -C $(CIRCLESTDLIBHOME)

#
# Build mt32emu
#
$(MT32EMULIB): $(CIRCLE_STDLIB_CONFIG)
	export CFLAGS="$(CFLAGS_FOR_TARGET)"
	export CXXFLAGS="$(CFLAGS_FOR_TARGET)"
	cmake   -B $(MT32EMUBUILDDIR) \
			-DARM_HOME=$(ARM_HOME) \
			-DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi.cmake \
			-DCMAKE_BUILD_TYPE=Release \
			-Dlibmt32emu_C_INTERFACE=FALSE \
			-Dlibmt32emu_SHARED=FALSE \
			$(MT32EMUHOME)
	cmake --build $(MT32EMUBUILDDIR)

#
# Build kernel itself
#
all: $(CIRCLE_STDLIB_LIBS) $(MT32EMULIB)
	@$(MAKE) -f Kernel.mk

#
# Clean kernel only
#
clean:
	@$(MAKE) -f Kernel.mk clean

#
# Clean kernel and all dependencies
#
veryclean: clean
	# Clean circle-stdlib
	@$(MAKE) -C $(CIRCLESTDLIBHOME) mrproper

	# Clean mt32emu
	@$(RM) -r $(MT32EMUBUILDDIR)
