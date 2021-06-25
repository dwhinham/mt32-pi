#
# Makefile
#

include Config.mk

.DEFAULT_GOAL=all
.PHONY: circle-stdlib mt32emu fluidsynth all clean veryclean

#
# Configure circle-stdlib
#
.ONESHELL:
$(CIRCLE_STDLIB_CONFIG) $(CIRCLE_CONFIG)&:
	@echo "Configuring for Raspberry Pi $(RASPBERRYPI) ($(BITS) bit)"
	$(CIRCLESTDLIBHOME)/configure --raspberrypi=$(RASPBERRYPI) --prefix=$(PREFIX)

	# Apply patches
	@patch -N -p1 --no-backup-if-mismatch -r - -d $(CIRCLEHOME) < patches/circle-44-minimal-usb-drivers.patch

ifeq ($(strip $(GC_SECTIONS)),1)
	# Enable function/data sections for circle-stdlib
	echo "CFLAGS_FOR_TARGET += -ffunction-sections -fdata-sections" >> $(CIRCLE_STDLIB_CONFIG)
endif

	# Enable multi-core
	echo "DEFINE += -DARM_ALLOW_MULTI_CORE" >> $(CIRCLE_CONFIG)

	# Disable delay loop calibration (boot speed improvement)
	echo "DEFINE += -DNO_CALIBRATE_DELAY" >> $(CIRCLE_CONFIG)

	# Improve I/O throughput
	echo "DEFINE += -DNO_BUSY_WAIT" >> $(CIRCLE_CONFIG)

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
# Build FluidSynth
#
fluidsynth: $(FLUIDSYNTHBUILDDIR)/.done

$(FLUIDSYNTHBUILDDIR)/.done: $(CIRCLESTDLIBHOME)/.done
	@patch -N -p1 --no-backup-if-mismatch -r - -d $(FLUIDSYNTHHOME) < patches/fluidsynth-2.2.1-circle.patch

	@export CFLAGS="$(CFLAGS_FOR_TARGET)"
	@cmake  -B $(FLUIDSYNTHBUILDDIR) \
			$(CMAKE_TOOLCHAIN_FLAGS) \
			-DCMAKE_C_FLAGS_RELEASE="-Ofast -fopenmp-simd" \
			-DCMAKE_BUILD_TYPE=Release \
			-DBUILD_SHARED_LIBS=OFF \
			-Denable-aufile=OFF \
			-Denable-dbus=OFF \
			-Denable-dsound=OFF \
			-Denable-floats=ON \
			-Denable-ipv6=OFF \
			-Denable-jack=OFF \
			-Denable-ladspa=OFF \
			-Denable-libinstpatch=OFF \
			-Denable-libsndfile=OFF \
			-Denable-midishare=OFF \
			-Denable-network=OFF \
			-Denable-oboe=OFF \
			-Denable-opensles=OFF \
			-Denable-oss=OFF \
			-Denable-pkgconfig=OFF \
			-Denable-pulseaudio=OFF \
			-Denable-readline=OFF \
			-Denable-sdl2=OFF \
			-Denable-threads=OFF \
			-Denable-waveout=OFF \
			-Denable-winmidi=OFF \
			$(FLUIDSYNTHHOME) \
			>/dev/null
	@cmake --build $(FLUIDSYNTHBUILDDIR) --target libfluidsynth
	@touch $@

#
# Build kernel itself
#
all: circle-stdlib mt32emu fluidsynth
	@$(MAKE) -f Kernel.mk $(KERNEL).img $(KERNEL).hex

#
# Clean kernel only
#
clean:
	@$(MAKE) -f Kernel.mk clean

#
# Clean kernel and all dependencies
#
veryclean: clean
	# Reverse patches
	@patch -R -N -p1 --no-backup-if-mismatch -r - -d $(CIRCLEHOME) < patches/circle-44-minimal-usb-drivers.patch
	@patch -R -N -p1 --no-backup-if-mismatch -r - -d $(FLUIDSYNTHHOME) < patches/fluidsynth-2.2.1-circle.patch

	# Clean circle-stdlib
	@$(MAKE) -C $(CIRCLESTDLIBHOME) mrproper
	@$(RM) $(CIRCLESTDLIBHOME)/.done

	# Clean mt32emu
	@$(RM) -r $(MT32EMUBUILDDIR)

	# Clean FluidSynth
	@$(RM) -r $(FLUIDSYNTHBUILDDIR)
