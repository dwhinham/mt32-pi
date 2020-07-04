[![mt32-pi CI](https://github.com/dwhinham/mt32-pi/workflows/mt32-pi%20CI/badge.svg)](https://github.com/dwhinham/mt32-pi/actions?query=workflow:"mt32-pi+CI")
[<img width="280rem" align="right" src="https://upload.wikimedia.org/wikipedia/commons/0/05/MT_32.jpg">](https://commons.wikimedia.org/wiki/File:MT_32.jpg)

## 🎹🎶 mt32-pi

A work-in-progress baremetal Roland MT-32 emulator for the Raspberry Pi 3 or above, based on [Munt] and [Circle].
Turn your Raspberry Pi into a dedicated emulation of the [famous multi-timbre sound module](https://en.wikipedia.org/wiki/Roland_MT-32) used by countless classic MS-DOS and Sharp X68000 games, that starts up in seconds!

## Project status

* Tested on Raspberry Pi 4 Model B and Raspberry Pi 3 Model B+.
  + It's _possible_ that the Pi 2 may be good enough, but I don't have one for testing. Can you help?
  + Pi 0 and 1 are unfortunately too slow, even with an overclock.
* PWM headphone jack audio.
  + Quality is known to be poor (aliasing/distortion on quieter sounds).
  + It is not currently known whether this can be improved or not.
* I2S Hi-Fi DAC support is **almost ready** (feel free to open an issue if you have a DAC HAT and want to test!).
* LCD status screen support (for MT-32 SysEx messages and status information) is **in progress**.

## Quick-start guide

* Get the latest build by going to [Actions] --> Click top-most commit in list --> Click `sdcard` in the artifacts list.
  + If you are updating an old version, you can alternatively download the `kernels` archive instead, and just replace the kernels. The other boot files will not change often.
* Extract contents to a blank FAT32-formatted SD card.
* Add `MT32_CONTROL.ROM` and `MT32_PCM.ROM` to the root of the SD card - you have to provide these for copyright reasons.
* Plug a USB MIDI interface into the Pi, and connect some speakers to the headphone jack.
* Connect your vintage PC's MIDI OUT to the Pi's MIDI IN and vice versa.

## MIDI connection examples

The USB MIDI interface connected to the Pi can be any standard class-compliant USB MIDI interface. If it works on Windows or Linux without any drivers, there's a high chance of it working. See [the following section](#confirmed-working-usb-midi-interfaces) for known-good devices.

Connection examples:

``` 
[ Pi ] --> [ USB MIDI ] <===> [ USB MIDI ] <-- [ Modern PC ]
[ Pi ] --> [ USB MIDI ] <===> [ Gameport MIDI cable ] <-- [ Vintage PC ]
[ Pi ] --> [ USB MIDI ] <===> [ Atari ST or other machine with built-in MIDI ]
[ Pi ] --> [ USB MIDI ] <===> [ Synthesizer keyboard or controller ]
```

## Confirmed working USB MIDI interfaces

Any class-compliant USB MIDI interface should work fine - if the interface works on Windows or Linux PCs without requiring any drivers, there's a high chance it will work fine with `mt32-pi` .

**Beware**: cheap no-name interfaces are not recommended; they have reliability issues not unique to this project [[1], [2]].

If you're shopping for a USB MIDI interface, the following devices have been confirmed as working properly by our testers. Feel free to contribute test results with your own MIDI interfaces and we can list known working ones!
 
| Manufacturer | Device                                                           | Comments                                               |
|--------------|------------------------------------------------------------------|--------------------------------------------------------|
| M-Audio      | [Uno](https://m-audio.com/products/view/uno)                     | 1 in, 1 out; male DIN plugs. Tested by @dwhinham.      |
| M-Audio      | [MIDISport 1x1](https://m-audio.com/products/view/midisport-1x1) | 1 in, 1 out; female DIN sockets. Tested by @nswaldman. |

## Disclaimer

This project, just like [Munt], has no affiliation with Roland Corporation. Use of "Roland" or other registered trademarks is purely for informational purposes only, and implies no endorsement by or affiliation with thheir respective owners.

## Acknowledgments

* Many thanks go out to @rc55 and @nswaldman for their encouragement and testing! ❤️
* The [Munt] team for their incredible work reverse-engineering the Roland MT-32 and producing an excellent emulation and well-structured project.
* The [Circle] and [circle-stdlib] projects for providing the best C++ baremetal framework for the Raspberry Pi.

[1]: http://www.arvydas.co.uk/2013/07/cheap-usb-midi-cable-some-self-assembly-may-be-required/
[2]: https://karusisemus.wordpress.com/2017/01/02/cheap-usb-midi-cable-how-to-modify-it/
[Actions]: https://github.com/dwhinham/mt32-pi/actions
[Circle]: https://github.com/rsta2/circle
[circle-stdlib]: https://github.com/smuehlst/circle-stdlib
[Munt]: https://github.com/munt/munt
