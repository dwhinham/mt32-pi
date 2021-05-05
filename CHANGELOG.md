# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Basic networking support (Wi-Fi or Ethernet) - read the new `[network]` section of the configuration file to learn how to enable and set it up.
  * In order to use Wi-Fi, firmware must be copied to the root of the SD card (new `firmware` directory in release package), and SSID/password must be added to `wpa_supplicant.conf` (example file in release package).
- Support for receiving MIDI over the network via Wi-Fi or Ethernet using the RTP-MIDI/AppleMIDI protocol.
  * macOS users can use this feature without any additional software (see [Apple documentation](https://support.apple.com/en-gb/guide/audio-midi-setup/ams1012/mac)).
  * Windows users can use [rtpMIDI by Tobias Erichsen](https://www.tobias-erichsen.de/software/rtpmidi/rtpmidi-tutorial.html).
  * Linux users can use [rtpmidid by David Moreno](https://github.com/davidmoreno/rtpmidid).
- Ability to configure FluidSynth reverb/chorus effects (new configuration file options).
  * Defaults can be set in the `[fluidsynth]` section.
  * Additional sections can be added to create "profiles" that override the defaults for specific SoundFonts.
  * An example section has been added for GeneralUser GS (assumed to be at index 0) as recommended by S. Christian Collins.
- The rotary encoder direction can now be reversed (new configuration file option).
- Ability to use the HDMI port for digital audio (new configuration value for `output_device`). This allows you to use inexpensive HDMI audio extractors or VGA+audio dongles to get better audio quality out of the Raspberry Pi (compared to the headphone jack).
- Additional boot files for the Compute Module 4 and Raspberry Pi 400 are now included.
  * The Raspberry Pi 400 is untested.

### Changed

- Update to libmt32emu v2.5.0.
- Update to FluidSynth v2.2.0.
- The activity LED no longer illuminates for System Common or System Real-Time messages.

### Fixed

- USB plug/unplug events now bring mt32-pi out of power saving mode.
- Some USB MIDI devices that violate the USB specification were unusable (discussion #102) - a workaround in the USB driver has been implemented. Thanks to @fabbrimichele for reporting and @rsta2 for the fix!
- Hang on "Init USB" when no USB controller is present (e.g. a Compute Module 4-based system with no external XHCI controller). Huge thanks to Serdaco for donating the CM4 and I/O board for testing!
- Boot failure on the 1GB Raspberry Pi 4/CM4 because of a bug in the memory allocator.

## [0.9.1] - 2021-03-20

### Fixed

- The `gpio_baud_rate` option was broken in v0.9.0. Thanks to Scandy for the quick report and testing the fix!

## [0.9.0] - 2021-03-19

### Added

- The number of seconds to wait before a SoundFont begins loading after using the switch button can now be adjusted (new configuration file option).
- MT-32 ROMs and SoundFonts can now be loaded from USB mass storage devices.
  * This feature should be considered **unstable/experimental** - some USB disk/Raspberry Pi combinations can encounter freezes/crashes.
  * SoundFonts will be rescanned and the indices updated if a USB storage device is inserted or removed.
  * USB storage devices must be FAT32 formatted.
  * MT-32 ROMs must be located under a `roms` directory on the root of the device.
  * SoundFonts must be located under a `soundfonts` directory on the root of the device.
  * Only one USB storage device is supported at a time.
  * The SoundFont index continues counting after the last SD card SoundFont index. If there are no SoundFonts on the SD card, the first USB storage SoundFont starts at index zero.
  * Special thanks to @rsta2 for providing important USB driver fixes and mmmonkey Pete for donating a Raspberry Pi 3B for testing!
- Ability to set master volume gain and reverb gain for mt32emu (new configuration file options).

### Changed

- Update to circle-stdlib v15.6/Circle Step 43.3.
- Update to inih r52.
- Update to FluidSynth v2.1.8.
- Update ARM toolchains to 10.2-2020.11.
- Config file options are now case-insensitive.
- The `usb` configuration option has been moved to the `[system]` section. Please update your configuration file if you use this option.

### Fixed

- Large files mistakenly placed in the `roms` directory could cause mt32-pi to crash on startup (issue #93).

### Removed

- Old ROM loading behavior now removed. If you have `MT32_CONTROL.ROM`/`MT32_PCM.ROM` files in the root of your SD card, please move them to the `roms` subdirectory otherwise they will fail to load.
- Unused USB drivers removed (kernel size reduced).
- Unused C standard library functions now removed by linker (kernel size reduced).

## [0.8.5] - 2021-02-10

### Fixed

- 128x32 SSD1306 displays were broken (flickering) since 0.8.4.

## [0.8.4] - 2021-02-07

### Added

- USB plug & play support for MIDI interfaces.
  * You can now connect/disconnect USB MIDI devices at runtime and they will be used instead of the GPIO MIDI interface when present.
- Support for SH1106 OLED displays - many thanks to @arananet for donating a screen!

### Changed

- Update to libmt32emu v2.4.2.
- Update to FluidSynth v2.1.7.
- Rotary encoder now has an acceleration curve applied to it when turned quickly.
- The SoundFont name is now displayed before loading when using a button to cycle through them.

### Fixed

- Some USB MIDI devices were not being detected at startup.
- "LCD-Auto" mode for MiSTer was broken because of a bug in the SSD1306 framebuffer difference-checking code.
- Rapidly-changing panning values [could cause pops/clicks](https://github.com/FluidSynth/fluidsynth/issues/768) in FluidSynth - many thanks to @Asbrandt for reporting and @derselbst for the quick fix!
  * This was particularly noticable in the Descent Level 1 soundtrack's bassline, for example.
  * A temporary patch has been applied until FluidSynth v2.1.8 is released.

## [0.8.3] - 2021-01-16

### Added

- FluidSynth is now reset when a "GM Mode On" or "GS Reset" SysEx message is received.
- FluidSynth is now reconfigured when GS "Use For Rhythm Part" SysEx messages are received for songs that use multiple drum kits.

### Fixed

- Switching MT-32 ROM sets via button press would fail if the next ROM set wasn't present. For example, if an MT-32 "new" set wasn't present, then pressing the button would not switch from "old" to CM-32L as expected.

## [0.8.2] - 2021-01-06

### Changed

- Update to FluidSynth v2.1.6.

### Fixed

- Missing part level bar segments and "glitchy" rendering when using HD44780 LCDs.
- Missing part level bar "bases" when using HD44780 LCDs.
- Backlight would not be turned off/on for supported I²C HD44780 LCDs when entering/exiting power saving mode.

## [0.8.1] - 2021-01-03

### Fixed

- Boot failure on Raspberry Pi 4 due to inability to allocate heap.

## [0.8.0] - 2021-01-03

### Added

- Support for the SSD1309 using the `ssd1306_i2c` driver - many thanks to @flynnsbit for testing!
- Support for the [Blokas Pisound](https://blokas.io/pisound/) - many thanks to @sigkill for providing the device for development!
- Support for configuration using [MiSTer FPGA](https://github.com/MiSTer-devel/Main_MiSTer/wiki)'s OSD via an I²C control interface in certain cores - many thanks to @sorgelig for collaborating and implementing the MiSTer side!
- Support for physical buttons and rotary encoders with two "simple" control schemes (new configuration file option).
  * `simple_buttons` allows connecting 4 buttons for switching synth, switching MT-32 ROM/SoundFont, decreasing and increasing volume.
  * `simple_encoder` allows connecting 2 buttons and a rotary encoder (with button). Volume is adjusted by turning the encoder, encoder button will be enabled in a future release.
  * Menu system/additional button features will come in future releases; this is just to get basic functionality implemented.
  * Details on how to wire these controls will be made available in the wiki.
- Animated loading "spinner" for when large SoundFonts are being loaded.
  * This will probably be replaced with a progress bar in the future.
- Support for Roland SC-55 "graphics" messages when using a graphical display.
  * A good example of this is the Roland "Star Games" demo MIDI (`STARGAME.MID`).
- Ability to set master volume gain for FluidSynth (new configuration file option).

### Changed

- SD card I/O speed increased by up to ~180% for cards that support High Speed mode (almost all cards available for purchase today).
  * This should dramatically reduce SoundFont loading times.
- MT-32 ROM/SoundFont switch messages are now only shown when the appropriate synth is active.
- A message is now displayed on the LCD for a second just before entering power saving mode.

### Fixed

- Switching MT-32 ROM sets whilst MIDI was playing could cause a crash.
- Switching SoundFonts too often (especially large ones, and especially while MIDI is playing) could result in a crash due to an out-of-memory condition.
  * This has been solved with some upstream fixes by [the FluidSynth team](https://github.com/FluidSynth/fluidsynth/issues/727) combined with a custom memory allocator that should prevent fragmentation. Many thanks to @derselbst and @jjceresa for their help!
- Power saving mode would never be entered whilst in SoundFont mode if the user had switched SoundFonts.

## [0.7.1] - 2020-11-26

### Added

- "Loading" message when switching SoundFonts.

### Changed

- Update to GeneralUser GS v1.511.
  * This is a currently-unpublished version from the author, who has made several important fixes to the SoundFont to improve compatibility with FluidSynth v2.1.0+.
- Update to circle-stdlib v15.5/Circle Step 43.2.
- Part level meters in SoundFont mode now have "bases".

### Fixed

- Incorrect `-dirty` suffix on version string on clean builds (again!).
- Crash when using USB MIDI interface in SoundFont mode, or when resuming from power saving mode.
- Hang on startup if unsupported LCD size is set in configuration file.

## [0.7.0] - 2020-11-22

### Added

- FluidSynth synthesizer engine v2.1.5 for using SoundFonts for General MIDI support and more.
  * You can switch between MT-32 and SoundFont mode at runtime using SysEx.
  * SoundFonts can be switched at runtime using SysEx, with some caveats for large SoundFonts (see FAQ section of README).
  * New configuration file options for changing default synthesizer and SoundFont.
- GeneralUser GS v1.471 included as default SoundFont - many thanks to S. Christian Collins for kindly giving permission!
- Ability to invert the display orientation for SSD1306 (new configuration file option).
- Support for 64 pixel high SSD1306 OLED displays - many thanks to @ctrl_alt_rees for donating a screen!
- Ability to set I2C clock speed (new configuration file option).
  * This is useful for allowing larger displays to refresh at a faster speed; see config file for details.
- Power saving mode with configurable timeout (new configuration file option).
  * The CPU clock speed will be lowered, audio device stopped, and LCD backlight turned off (when possible) after a configurable number of seconds to save energy.
  * Any MIDI activity will instantly bring the system out of power saving mode.
- Undervoltage/throttling detection - `mt32-pi` will now warn the user when the firmware detects an undervoltage/CPU throttling condition.

### Changed

- Update to circle-stdlib v15.4.
- New multi-core architecture; audio rendering and LCD updates moved to their own dedicated CPU cores, leaving MIDI and interrupt processing on the primary core.
- Kernels for Pi 3 and 4 are now compiled for AArch64 (64-bit) for better performance.
  * When upgrading, you **must** replace the `config.txt` file on your SD card and add the new `armstub8-rpi4.bin` file for Pi 4.
  * You should remove old `kernel*.img` files from your SD card.
  * It's recommended that you clear your SD card (except for your `roms` directory) and reconfigure `mt32-pi` for this release.
- Synth engines now compiled with more aggressive optimizations enabled to benefit from ARM NEON instructions.
- Due to the above three changes, it's possible that Raspberry Pi 2 may be more usable with this release, but this is **untested**.
- Kernel size significantly reduced by removing mt32emu ROM loader dependency on C++ iostreams.
- Improved layout of 4-line HD44780 LCD.
  * MT-32 status line moved to bottom row.
  * Part levels now 3 rows high.
- Default sample rate and chunk size reduced to 48000Hz and 256 samples.

### Fixed

- SSD1306 text alignment was off by one pixel.
- SSD1306 part level meters' top pixels would flicker with long sustained notes.
- Part level meters would suddenly snap to a lower level if a quiet note followed a loud note instead of falling off.
- Possibility of overflowing text in HD44780 Print() function.

## [0.6.2] - 2020-10-18

### Fixed

- USB MIDI was broken since 0.6.0.

### Changed

- SSD1306 font updated to perfectly match the original MT-32's Sanyo DM2011 font.

## [0.6.1] - 2020-10-13

### Changed

- Update to libmt32emu v2.4.1.
- Update to circle-stdlib v15.3/Circle Step 43.1.
- Update to inih r52.
- Boot speed improved by another ~0.3 seconds when `usb = on` thanks to improvements in Circle Step 43.1.
  * If updating from an old version, make sure you replace `config.txt` to benefit from a couple of other boot optimizations.

### Fixed

- Correct version string is now extracted from ROM 2.04 when switching to it.
- The `none` option for `i2c_dac_init` was broken in v0.6.0.
- Spacing between status row text and part level meters on SSD1306 improved.

## [0.6.0] - 2020-10-04

### Added

- Ability to configure initial MIDI channel assignment (new configuration file option).
- Ability to set custom baud rates for GPIO MIDI (new configuration file option).
  * This could be useful for those wanting to use [SoftMPU](http://bjt42.github.io/softmpu/)'s serial MIDI mode.
- Multiple ROM sets can now be used and switched between using custom SysEx commands. See new `README.md` section and config file for full details.
  * Please move your `MT32_CONTROL.ROM` and `MT32_PCM.ROM` into a new subdirectory called `roms`.
  * For now, the old locations are still checked as a fallback, but this may be removed in a later version.

### Changed

- Update to circle-stdlib v15.2/Circle Step 43.
- Boot speed improved by ~0.5 seconds by using `start_cd.elf`/`start4cd.elf` and `fixup_cd.dat`/`fixup4cd.dat`.
  * If updating from an old version, make sure you replace `config.txt` and add the new `*.elf` and `*.dat` files when updating your SD card to benefit from this. 
- LCD/OLED part level meters moved to the upper row(s).
- Config file parsing now more efficient.

### Fixed

- Left/right channels were backwards when using PWM (headphone jack) - thanks @ctrl_alt_rees and YouTube viewers.

## [0.5.0] - 2020-09-19

### Added

- Program change messages are now shown on the LCD.

### Changed

- Activity LED now flashes when other types of messages are received (e.g. SysEx or control changes).

### Fixed

- Complete rewrite of the MIDI parser code.
  * USB and GPIO MIDI streams are now handled by a single code path.
  * Corrupt SysEx messages (e.g. from cheap/no-name MIDI interfaces) will no longer cause `mt32-pi` to hang (issue #25).
  * 2-byte Running Status messages are now handled correctly (issue #26).
- Active Sensing race condition fixed and timeout increased to 330ms as recommended by the MIDI 1.0 Specification.

## [0.4.0] - 2020-09-02

### Added

- Software "MIDI thru" for GPIO interface (new configuration file option).

### Changed

- Update to circle-stdlib v15.1/Circle Step 42.1.
- Enhanced error reporting for GPIO MIDI parser.

### Fixed

- Hanging/missing notes when GPIO MIDI interface used with some modern MIDI sources that transmit using Running Status optimizations.
  * Many thanks to @thorr2, @nswaldman, @icb-, @glaucon1984, @Braincell1973, @Higgy69, and @olliraa for patiently reporting, testing and brainstorming the cause of this critical issue.
- MIDI messages could be passed to `mt32emu` with the wrong length.

## [0.3.1] - 2020-08-08

### Added

- Missing sample `[lcd]` config section from `mt32-pi.cfg`.

### Fixed

- Incorrect `-dirty` suffix on version string on clean builds.
- Possibility of overflowing text in SSD1306 Print() function.
- Rhythm part on SSD1306 was inverted on activity instead of being blocked out.

## [0.3.0] - 2020-08-03

### Added

- Initial support for HD44780 (4-bit direct and I2C) character LCDs and SSD1306 128x32 graphical OLEDs.
  * Configuration file changes required - please check documentation.

## [0.2.1] - 2020-07-14

### Fixed

- Crash when using `resampler_quality = none` - thanks @nswaldman.

## [0.2.0] - 2020-07-14

### Added

- Configuration file system - see available options inside `mt32-pi.cfg`.
- Support for PCM5xxx series DACs connected via I2S - tested with PCM5242 and PCM5122. See `README.md` and config file for details.

## [0.1.0] - 2020-07-10

### Added

- Initial version.

[unreleased]: https://github.com/dwhinham/mt32-pi/compare/v0.9.1...HEAD
[0.9.1]: https://github.com/dwhinham/mt32-pi/compare/v0.9.0..v0.9.1
[0.9.0]: https://github.com/dwhinham/mt32-pi/compare/v0.8.5..v0.9.0
[0.8.5]: https://github.com/dwhinham/mt32-pi/compare/v0.8.4..v0.8.5
[0.8.4]: https://github.com/dwhinham/mt32-pi/compare/v0.8.3..v0.8.4
[0.8.3]: https://github.com/dwhinham/mt32-pi/compare/v0.8.2..v0.8.3
[0.8.2]: https://github.com/dwhinham/mt32-pi/compare/v0.8.1..v0.8.2
[0.8.1]: https://github.com/dwhinham/mt32-pi/compare/v0.8.0..v0.8.1
[0.8.0]: https://github.com/dwhinham/mt32-pi/compare/v0.7.1..v0.8.0
[0.7.1]: https://github.com/dwhinham/mt32-pi/compare/v0.7.0..v0.7.1
[0.7.0]: https://github.com/dwhinham/mt32-pi/compare/v0.6.2..v0.7.0
[0.6.2]: https://github.com/dwhinham/mt32-pi/compare/v0.6.1..v0.6.2
[0.6.1]: https://github.com/dwhinham/mt32-pi/compare/v0.6.0..v0.6.1
[0.6.0]: https://github.com/dwhinham/mt32-pi/compare/v0.5.0..v0.6.0
[0.5.0]: https://github.com/dwhinham/mt32-pi/compare/v0.4.0..v0.5.0
[0.4.0]: https://github.com/dwhinham/mt32-pi/compare/v0.3.1..v0.4.0
[0.3.1]: https://github.com/dwhinham/mt32-pi/compare/v0.3.0..v0.3.1
[0.3.0]: https://github.com/dwhinham/mt32-pi/compare/v0.2.1..v0.3.0
[0.2.1]: https://github.com/dwhinham/mt32-pi/compare/v0.2.0..v0.2.1
[0.2.0]: https://github.com/dwhinham/mt32-pi/compare/v0.1.0..v0.2.0
[0.1.0]: https://github.com/dwhinham/mt32-pi/releases/tag/v0.1.0
