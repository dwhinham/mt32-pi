# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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

- Update to circle-stdlib v15.1/Circle Step 42.1
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

[unreleased]: https://github.com/dwhinham/mt32-pi/compare/v0.5.0...HEAD
[0.5.0]: https://github.com/dwhinham/mt32-pi/compare/v0.4.0..v0.5.0
[0.4.0]: https://github.com/dwhinham/mt32-pi/compare/v0.3.1..v0.4.0
[0.3.1]: https://github.com/dwhinham/mt32-pi/compare/v0.3.0..v0.3.1
[0.3.0]: https://github.com/dwhinham/mt32-pi/compare/v0.2.1..v0.3.0
[0.2.1]: https://github.com/dwhinham/mt32-pi/compare/v0.2.0..v0.2.1
[0.2.0]: https://github.com/dwhinham/mt32-pi/compare/v0.1.0..v0.2.0
[0.1.0]: https://github.com/dwhinham/mt32-pi/releases/tag/v0.1.0
