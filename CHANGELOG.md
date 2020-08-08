# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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

[unreleased]: https://github.com/dwhinham/mt32-pi/compare/v0.3.1...HEAD
[0.3.1]: https://github.com/dwhinham/mt32-pi/compare/v0.3.0..v0.3.1
[0.3.0]: https://github.com/dwhinham/mt32-pi/compare/v0.2.1..v0.3.0
[0.2.1]: https://github.com/dwhinham/mt32-pi/compare/v0.2.0..v0.2.1
[0.2.0]: https://github.com/dwhinham/mt32-pi/compare/v0.1.0..v0.2.0
[0.1.0]: https://github.com/dwhinham/mt32-pi/releases/tag/v0.1.0
