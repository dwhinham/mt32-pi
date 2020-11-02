//
// mt32pi.h
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

#ifndef _mt32pi_h
#define _mt32pi_h

#include <circle/actled.h>
#include <circle/cputhrottle.h>
#include <circle/devicenameservice.h>
#include <circle/i2cmaster.h>
#include <circle/soundbasedevice.h>
#include <circle/interrupt.h>
#include <circle/logger.h>
#include <circle/multicore.h>
#include <circle/sched/scheduler.h>
#include <circle/timer.h>
#include <circle/types.h>
#include <circle/usb/usbhcidevice.h>

#include "config.h"
#include "lcd/mt32lcd.h"
#include "midiparser.h"
#include "synth/mt32synth.h"

class CMT32Pi : public CMultiCoreSupport, CMIDIParser
{
public:
	CMT32Pi(CI2CMaster* pI2CMaster, CInterruptSystem* pInterrupt, CSerialDevice* pSerialDevice, CUSBHCIDevice* pUSBHCI);
	virtual ~CMT32Pi() override;

	bool Initialize(bool bSerialMIDIEnabled = true);

	virtual void Run(unsigned nCore) override;

private:
	enum class TLCDLogType
	{
		Startup,
		Error,
		Warning,
		Notice,
	};

	// CMIDIParser
	virtual void OnShortMessage(u32 nMessage) override;
	virtual void OnSysExMessage(const u8* pData, size_t nSize) override;
	virtual void OnUnexpectedStatus() override;
	virtual void OnSysExOverflow() override;

	// Tasks for specific CPU cores
	void MainTask();
	void UITask();
	void AudioTask();

	bool ParseCustomSysEx(const u8* pData, size_t nSize);
	void UpdateSerialMIDI();

	void LEDOn();
	void LCDLog(TLCDLogType Type, const char* pMessage);

	bool InitPCM51xx(u8 nAddress);

	CTimer* m_pTimer;
	CActLED* m_pActLED;

	CI2CMaster* m_pI2CMaster;
	CInterruptSystem* m_pInterrupt;
	CSerialDevice* m_pSerial;
	CUSBHCIDevice* m_pUSBHCI;

	CMT32LCD* m_pLCD;
	unsigned m_nLCDUpdateTime;

	// Serial GPIO MIDI
	bool m_bSerialMIDIEnabled;

	bool m_bActiveSenseFlag;
	unsigned m_nActiveSenseTime;

	volatile bool m_bRunning;
	volatile bool m_bUITaskDone;
	bool m_bLEDOn;
	unsigned m_nLEDOnTime;

	// Audio output
	CSoundBaseDevice* m_pSound;

	// Synthesizers
	CMT32Synth* m_pMT32Synth;

	static void USBMIDIPacketHandler(unsigned nCable, u8* pPacket, unsigned nLength);

	static CMT32Pi* s_pThis;
};

#endif
