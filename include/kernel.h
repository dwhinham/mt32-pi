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

#include "config.h"
#include "lcd/mt32lcd.h"
#include "midiparser.h"
#include "mt32synth.h"

class CKernel : public CStdlibApp, CMIDIParser
{
public:
	CKernel(void);

	virtual bool Initialize(void) override;
	TShutdownMode Run(void);

protected:
	CCPUThrottle m_CPUThrottle;
	CNullDevice m_Null;
	CSerialDevice m_Serial;
#ifdef HDMI_CONSOLE
	CScreenDevice m_Screen;
#endif
	CTimer m_Timer;
	CLogger m_Logger;
	CScheduler m_Scheduler;
	CUSBHCIDevice m_USBHCI;
	CEMMCDevice m_EMMC;
	FATFS m_FileSystem;

	CI2CMaster m_I2CMaster;
	CMT32LCD* m_pLCD;

private:
	bool InitPCM51xx(u8 nAddress);

	// CMIDIParser
	virtual void OnShortMessage(u32 nMessage) override;
	virtual void OnSysExMessage(const u8* pData, size_t nSize) override;
	virtual void OnUnexpectedStatus() override;
	virtual void OnSysExOverflow() override;

	bool ParseCustomSysEx(const u8* pData, size_t nSize);
	void UpdateSerialMIDI();

	void LEDOn();
	void LCDLog(const char* pMessage);

	// Configuration
	CConfig m_Config;

	unsigned m_nLCDLogTime;
	unsigned m_nLCDUpdateTime;

	// Serial GPIO MIDI
	bool m_bSerialMIDIEnabled;

	bool m_bActiveSenseFlag;
	unsigned m_nActiveSenseTime;

	bool m_bShouldReboot;
	bool m_bLEDOn;
	unsigned m_nLEDOnTime;

	// Synthesizer
	CMT32SynthBase* m_pSynth;

	static void USBMIDIPacketHandler(unsigned nCable, u8* pPacket, unsigned nLength);

	static CKernel* s_pThis;
};

#endif
