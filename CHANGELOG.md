# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Ability to invert the display orientation for SSD1306 (new configuration file option).
- Support for 64 pixel high SSD1306 OLED displays - many thanks to @ctrl_alt_rees for donating a screen!

### Changed

- Kernel size significantly reduced by removing mt32emu ROM loader dependency on C++ iostreams.

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

[unreleased]: https://github.com/dwhinham/mt32-pi/compare/v0.6.2...HEAD
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
