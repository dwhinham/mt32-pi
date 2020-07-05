//
// kernel.h
//
// mt32-pi - A bare-metal Roland MT-32 emulator for Raspberry Pi
// Copyright (C) 2020  Dale Whinham <daleyo@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef _kernel_h
#define _kernel_h

#include <circle/cputhrottle.h>
#include <circle/i2cmaster.h>
#include <circle/i2ssoundbasedevice.h>
#include <circle_stdlib_app.h>

#include <vector>

#include "mt32synth.h"

class CKernel : public CStdlibApp
{
public:
	CKernel(void);

	virtual bool Initialize(void) override;
	TShutdownMode Run(void);

protected:
	CCPUThrottle mCPUThrottle;
#ifdef HDMI_CONSOLE
	CScreenDevice mScreen;
#else
	CSerialDevice mSerial;
#endif
	CConsole mConsole;
	CTimer mTimer;
	CLogger mLogger;
	CUSBHCIDevice mUSBHCI;
#ifndef BAKE_MT32_ROMS
	CEMMCDevice mEMMC;
#endif
	FATFS mFileSystem;

	CI2CMaster mI2CMaster;

private:
	bool InitPCM5242();

	bool ParseSysEx();

	void UpdateActiveSense();
	void LEDOn();

	// MIDI messages
	unsigned mSerialState;
	u8 mSerialMessage[3];
	std::vector<u8> mSysExMessage;
	bool mActiveSenseFlag;
	unsigned mActiveSenseTime;

	bool mShouldReboot;
	bool mLEDOn;
	unsigned mLEDOnTime;

	// Synthesizer
	CMT32SynthBase* mSynth;

	static void MIDIPacketHandler(unsigned nCable, u8 *pPacket, unsigned nLength);
	static CKernel *pThis;
};

#endif
