#
# Makefile
#

include Config.mk

.DEFAULT_GOAL=all
.PHONY: submodules circle-stdlib mt32emu fluidsynth all clean veryclean

#
# Functions to apply/reverse patches only if not completely applied/reversed already
#
APPLY_PATCH=sh -c '																\
	if patch --dry-run --reverse --force --strip 1 --directory $$1 < $$2 >/dev/null 2>&1; then						\
		echo "Patch $$2 already applied; skipping";											\
	else																	\
		echo "Applying patch $$2 to directory $$1...";											\
		patch --forward --strip 1 --no-backup-if-mismatch -r - --directory $$1 < $$2 || (echo "Patch $$2 failed" >&2 && exit 1);	\
	fi' APPLY_PATCH

REVERSE_PATCH=sh -c '																\
	if patch --dry-run --forward --force --strip 1 --directory $$1 < $$2 >/dev/null 2>&1; then						\
		echo "Patch $$2 already reversed; skipping";											\
	else																	\
		echo "Reversing patch $$2 in directory $$1...";											\
		patch --reverse --strip 1 --no-backup-if-mismatch -r - --directory $$1 < $$2 || (echo "Patch $$2 failed" >&2 && exit 1);	\
	fi' REVERSE_PATCH

#
# Get submodules
#
submodules:
	@git submodule update --init --depth 1
	@git -C external/circle-stdlib submodule update --init --depth 1 libs/circle libs/circle-newlib
	@git -C external/circle-stdlib/libs/circle submodule update --init --depth 1 addon/wlan/hostap

#
# Configure circle-stdlib
#
$(CIRCLE_STDLIB_CONFIG) $(CIRCLE_CONFIG)&:
	@echo "Configuring for Raspberry Pi $(RASPBERRYPI) ($(BITS) bit)"
	$(CIRCLESTDLIBHOME)/configure --raspberrypi=$(RASPBERRYPI) --prefix=$(PREFIX)

# Apply patches
	@${APPLY_PATCH} $(CIRCLEHOME) patches/circle-45-minimal-usb-drivers.patch
	@${APPLY_PATCH} $(CIRCLEHOME) patches/circle-45-cp210x-remove-partnum-check.patch
	@${APPLY_PATCH} $(CIRCLEHOME) patches/circle-45-gzip-kernel.patch

ifeq ($(strip $(GC_SECTIONS)),1)
# Enable function/data sections for circle-stdlib
	@echo "CFLAGS_FOR_TARGET += -ffunction-sections -fdata-sections" >> $(CIRCLE_STDLIB_CONFIG)
endif

# Enable multi-core
	@echo "DEFINE += -DARM_ALLOW_MULTI_CORE" >> $(CIRCLE_CONFIG)

# Disable delay loop calibration (boot speed improvement)
	@echo "DEFINE += -DNO_CALIBRATE_DELAY" >> $(CIRCLE_CONFIG)

# Improve I/O throughput
	@echo "DEFINE += -DNO_BUSY_WAIT" >> $(CIRCLE_CONFIG)

# Enable PWM audio output on GPIO 12/13 for the Pi Zero 2 W
	@echo "DEFINE += -DUSE_PWM_AUDIO_ON_ZERO" >> $(CIRCLE_CONFIG)

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
	@CFLAGS="$(CFLAGS_EXTERNAL)" \
	CXXFLAGS="$(CFLAGS_EXTERNAL)" \
	cmake -B $(MT32EMUBUILDDIR) \
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
	@${APPLY_PATCH} $(FLUIDSYNTHHOME) patches/fluidsynth-2.3.1-circle.patch

	@CFLAGS="$(CFLAGS_EXTERNAL)" \
	cmake -B $(FLUIDSYNTHBUILDDIR) \
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
		 -Denable-openmp=OFF \
		 -Denable-opensles=OFF \
		 -Denable-oss=OFF \
		 -Denable-pipewire=OFF \
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
mrproper: clean
# Reverse patches
	@${REVERSE_PATCH} $(CIRCLEHOME) patches/circle-45-gzip-kernel.patch
	@${REVERSE_PATCH} $(CIRCLEHOME) patches/circle-45-cp210x-remove-partnum-check.patch
	@${REVERSE_PATCH} $(CIRCLEHOME) patches/circle-45-minimal-usb-drivers.patch
	@${REVERSE_PATCH} $(FLUIDSYNTHHOME) patches/fluidsynth-2.3.1-circle.patch

# Clean circle-stdlib
	@if [ -f $(CIRCLE_STDLIB_CONFIG) ]; then $(MAKE) -C $(CIRCLESTDLIBHOME) mrproper; fi
	@$(RM) $(CIRCLESTDLIBHOME)/.done

# Clean mt32emu
	@$(RM) -r $(MT32EMUBUILDDIR)

# Clean FluidSynth
	@$(RM) -r $(FLUIDSYNTHBUILDDIR)
