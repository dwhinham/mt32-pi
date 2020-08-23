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
#include <circle/sched/scheduler.h>
#include <circle_stdlib_app.h>

#include <vector>

#include "control/mcp23017.h"
#include "lcd/clcd.h"
#include "config.h"
#include "mt32synth.h"

class CKernel : public CStdlibApp
{
public:
	CKernel(void);

	virtual bool Initialize(void) override;
	TShutdownMode Run(void);

protected:
	CCPUThrottle mCPUThrottle;
	CNullDevice mNull;
	CSerialDevice mSerial;
#ifdef HDMI_CONSOLE
	CScreenDevice mScreen;
#endif
	CTimer mTimer;
	CLogger mLogger;
	CScheduler mScheduler;
	CUSBHCIDevice mUSBHCI;
	CEMMCDevice mEMMC;
	FATFS mFileSystem;

	CI2CMaster mI2CMaster;
	CCharacterLCD* mLCD;
	CMCP23017* mControl;

private:
	bool InitPCM51xx(u8 pAddress);

	bool ParseSysEx();

	void UpdateSerialMIDI();
	void UpdateActiveSense();
	void LEDOn();
	void LCDLog(const char* pMessage);

	// Configuration
	CConfig mConfig;

	unsigned mLCDLogTime;
	unsigned mLCDUpdateTime;

	// Serial GPIO MIDI
	bool mSerialMIDIEnabled;
	unsigned mSerialMIDIState;
	u8 mSerialMIDIMessage[3];

	std::vector<u8> mSysExMessage;
	bool mActiveSenseFlag;
	unsigned mActiveSenseTime;

	bool mShouldReboot;
	bool mLEDOn;
	unsigned mLEDOnTime;

	// Synthesizer
	CMT32SynthBase* mSynth;

	static void MIDIPacketHandler(unsigned nCable, u8 *pPacket, unsigned nLength);
	static void LCDMessageHandler(const char* pMessage);
	static CKernel *pThis;
};

#endif
