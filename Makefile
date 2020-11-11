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
	@echo "Configuring for Raspberry Pi $(RASPBERRYPI) ($(BITS) bit)"
	$(CIRCLESTDLIBHOME)/configure --raspberrypi=$(RASPBERRYPI) --prefix=$(PREFIX)

	# Enable multi-core
	echo "DEFINE += -DARM_ALLOW_MULTI_CORE" >> $(CIRCLE_CONFIG)

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
	@cmake  -B $(MT32EMUBUILDDIR) \
			$(CMAKE_TOOLCHAIN_FLAGS) \
			-DCMAKE_CXX_FLAGS_RELEASE="-Ofast" \
			-DCMAKE_BUILD_TYPE=Release \
			-Dlibmt32emu_C_INTERFACE=FALSE \
			-Dlibmt32emu_SHARED=FALSE \
			$(MT32EMUHOME) \
			>/dev/null
	@cmake --build $(MT32EMUBUILDDIR)
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
