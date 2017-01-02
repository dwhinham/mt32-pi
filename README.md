# mt32-pi

A [Buildroot](https://buildroot.org) external tree which which turns your
Raspberry Pi 2 into a dedicated
[Roland MT-32](https://en.wikipedia.org/wiki/Roland_MT-32) emulator based on
[MUNT/libmt32em](https://github.com/munt/munt).

The aim is to have the Raspberry Pi boot into a working state in about 5
seconds.

## How to use it

  * Check out this repo.
  * Copy `MT32_CONTROL.ROM` and `MT32_PCM.ROM` to the `mt32-roms` folder
    (you need to source these copyrighted ROMs yourself).
  * Download and extract Buildroot 2016.11 or later.
  * Build the system (this will take a long time):

        cd /path/to/buildroot
        make BR2_EXTERNAL=/path/to/this/repo raspberrypi2_defconfig
        make
  * After a successful build, an SD card image is located in
    `output/images/sdcard.img`.
  * Copy the image onto an SD card with `dd` (use the correct argument for
    the device representing your SD card reader):

        sudo dd if=output/images/sdcard.img of=/dev/sdc bs=1M
        sync
  * Place the SD card into the Raspberry Pi 2, attach a USB MIDI cable,
    and enjoy!

## TODO

  * There is some kind of delay in the handling of MIDI messages when many
    messages arrive at once (more complex songs). Need to figure out what this
    is being caused by (slow Pi USB MIDI? Would a GPIO MIDI interface be
    better? Threading problem with `mt32d`?).
  * Hardcoded to use (poor) internal sound and first USB MIDI device. Need to
    script hotplugging things properly and select appropriate devices.
  * Maybe try to use `PREEMPT_RT` kernel patch to see if it helps to get the
    latency of `mt32d` down as low as possible with various audio devices.
    N.B.: USB audio devices can achieve lower latency and superior sound
    quality to the internal audio.
  * Provide configurations for external LCD screens such as 3.2" touchscreens
    and various character displays in order to allow the virtual MT-32 to
    output messages and be controlled.
  * Reduce initial build time and image size by disabling unnecessary kernel
    modules.
  * In relation to above, reduce startup time. There's a 2 second `sleep` while
    we wait for the MIDI interfaces to come up before invoking `aconnect`; this
    needs to go away. A 'release' build could also remove network/SSH as this
    increases startup time. 
