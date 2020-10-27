#
# Makefile
#

include Config.mk

.DEFAULT_GOAL=all
.PHONY: circle-stdlib mt32emu all clean veryclean

#
# Configure circle-stdlib
#
.ONESHELL:
$(CIRCLE_STDLIB_CONFIG) $(CIRCLE_CONFIG)&:
ifeq ($(BOARD), pi0)
	@echo -n "Configuring for Raspberry Pi 0/1"
	$(CIRCLESTDLIBHOME)/configure --raspberrypi=1
else ifeq ($(BOARD), pi2)
	@echo -n "Configuring for Raspberry Pi 2"
	$(CIRCLESTDLIBHOME)/configure --raspberrypi=2
else ifeq ($(BOARD), pi3)
	@echo -n "Configuring for Raspberry Pi 3"
	$(CIRCLESTDLIBHOME)/configure --raspberrypi=3
else ifeq ($(BOARD), pi4)
	@echo -n "Configuring for Raspberry Pi 4"
	$(CIRCLESTDLIBHOME)/configure --raspberrypi=4
else ifeq ($(BOARD), pi4-64)
	@echo -n "Configuring for Raspberry Pi 4 (64 bit)"
	$(CIRCLESTDLIBHOME)/configure --raspberrypi=4 --prefix=aarch64-none-elf
else
	$(error Invalid board type "$(BOARD)"; please specify one of [ pi0 | pi2 | pi3 | pi4 | pi4-64 ])
endif

	# Disable delay loop calibration (boot speed improvement)
	echo "DEFINE += -DNO_CALIBRATE_DELAY" >> $(CIRCLE_CONFIG)

	# Enable save/restore floating point registers on interrupt
	echo "DEFINE += -DSAVE_VFP_REGS_ON_IRQ" >> $(CIRCLE_CONFIG)

#
# Build circle-stdlib
#
circle-stdlib: $(CIRCLESTDLIBHOME)/.done

$(CIRCLESTDLIBHOME)/.done: $(CIRCLE_STDLIB_CONFIG)
	@$(MAKE) -C $(CIRCLESTDLIBHOME)
	touch $@
#
# Build mt32emu
#
mt32emu: $(MT32EMUBUILDDIR)/.done

$(MT32EMUBUILDDIR)/.done: $(CIRCLESTDLIBHOME)/.done
	@export CFLAGS="$(CFLAGS_FOR_TARGET)"
	@export CXXFLAGS="$(CFLAGS_FOR_TARGET)"
	@cmake   -B $(MT32EMUBUILDDIR) \
			-DARM_HOME=$(ARM_HOME) \
			-DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi.cmake \
			-DCMAKE_BUILD_TYPE=Release \
			-Dlibmt32emu_C_INTERFACE=FALSE \
			-Dlibmt32emu_SHARED=FALSE \
			$(MT32EMUHOME) \
			>/dev/null
	@cmake --build $(MT32EMUBUILDDIR) >/dev/null
	@touch $@
#
# Build kernel itself
#
all: circle-stdlib mt32emu
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
	@$(RM) $(CIRCLESTDLIBHOME)/.done

	# Clean mt32emu
	@$(RM) -r $(MT32EMUBUILDDIR)
	@$(RM) $(MT32EMUBUILDDIR)/.done
