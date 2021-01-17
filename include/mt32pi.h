//
// mt32pi.h
//
// mt32-pi - A baremetal MIDI synthesizer for Raspberry Pi
// Copyright (C) 2020-2021 Dale Whinham <daleyo@gmail.com>
//
// This file is part of mt32-pi.
//
// mt32-pi is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// mt32-pi is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// mt32-pi. If not, see <http://www.gnu.org/licenses/>.
//

#ifndef _mt32pi_h
#define _mt32pi_h

#include <circle/actled.h>
#include <circle/cputhrottle.h>
#include <circle/devicenameservice.h>
#include <circle/gpiomanager.h>
#include <circle/i2cmaster.h>
#include <circle/interrupt.h>
#include <circle/logger.h>
#include <circle/multicore.h>
#include <circle/sched/scheduler.h>
#include <circle/soundbasedevice.h>
#include <circle/spimaster.h>
#include <circle/timer.h>
#include <circle/types.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/usb/usbmidi.h>

#include "config.h"
#include "control/control.h"
#include "control/mister.h"
#include "event.h"
#include "lcd/synthlcd.h"
#include "midiparser.h"
#include "pisound.h"
#include "power.h"
#include "ringbuffer.h"
#include "synth/mt32romset.h"
#include "synth/mt32synth.h"
#include "synth/soundfontsynth.h"
#include "synth/synth.h"

class CMT32Pi : public CMultiCoreSupport, CPower, CMIDIParser
{
public:
	CMT32Pi(CI2CMaster* pI2CMaster, CSPIMaster* pSPIMaster, CInterruptSystem* pInterrupt, CGPIOManager* pGPIOManager, CSerialDevice* pSerialDevice, CUSBHCIDevice* pUSBHCI);
	virtual ~CMT32Pi() override;

	bool Initialize(bool bSerialMIDIAvailable = true);

	virtual void Run(unsigned nCore) override;

private:
	enum class TLCDLogType
	{
		Startup,
		Error,
		Warning,
		Notice,
	};

	static constexpr size_t MIDIRxBufferSize = 2048;

	// CPower
	virtual void OnEnterPowerSavingMode() override;
	virtual void OnExitPowerSavingMode() override;
	virtual void OnThrottleDetected() override;
	virtual void OnUnderVoltageDetected() override;

	// CMIDIParser
	virtual void OnShortMessage(u32 nMessage) override;
	virtual void OnSysExMessage(const u8* pData, size_t nSize) override;
	virtual void OnUnexpectedStatus() override;
	virtual void OnSysExOverflow() override;

	// Tasks for specific CPU cores
	void MainTask();
	void UITask();
	void AudioTask();

	void UpdateMIDI();
	size_t ReceiveSerialMIDI(u8* pOutData, size_t nSize);
	bool ParseCustomSysEx(const u8* pData, size_t nSize);

	void ProcessEventQueue();
	void ProcessButtonEvent(const TButtonEvent& Event);

	// Actions that can be triggered via events
	void SwitchSynth(TSynth Synth);
	void SwitchMT32ROMSet(TMT32ROMSet ROMSet);
	void NextMT32ROMSet();
	void SwitchSoundFont(size_t nIndex);
	void DeferSwitchSoundFont(size_t nIndex);
	void SetMasterVolume(s32 nVolume);

	void LEDOn();
	void LCDLog(TLCDLogType Type, const char* pFormat...);

	bool InitPCM51xx(u8 nAddress);

	CTimer* m_pTimer;
	CActLED* m_pActLED;

	CI2CMaster* m_pI2CMaster;
	CSPIMaster* m_pSPIMaster;
	CInterruptSystem* m_pInterrupt;
	CGPIOManager* m_pGPIOManager;
	CSerialDevice* m_pSerial;
	CUSBHCIDevice* m_pUSBHCI;

	CSynthLCD* m_pLCD;
	unsigned m_nLCDUpdateTime;

	CControl* m_pControl;

	// MiSTer control interface
	CMisterControl m_MisterControl;
	unsigned m_nMisterUpdateTime;

	// Deferred SoundFont switch
	bool m_bDeferredSoundFontSwitchFlag;
	size_t m_nDeferredSoundFontSwitchIndex;
	unsigned m_nDeferredSoundFontSwitchTime;

	// Serial GPIO MIDI
	bool m_bSerialMIDIAvailable;
	bool m_bSerialMIDIEnabled;

	// USB MIDI
	CUSBMIDIDevice* volatile m_pUSBMIDIDevice;

	bool m_bActiveSenseFlag;
	unsigned m_nActiveSenseTime;

	volatile bool m_bRunning;
	volatile bool m_bUITaskDone;
	bool m_bLEDOn;
	unsigned m_nLEDOnTime;

	// Audio output
	CSoundBaseDevice* m_pSound;

	// Extra devices
	CPisound* m_pPisound;

	// Synthesizers
	u8 m_nMasterVolume;
	CSynthBase* m_pCurrentSynth;
	CMT32Synth* m_pMT32Synth;
	CSoundFontSynth* m_pSoundFontSynth;

	// MIDI receive buffer
	CRingBuffer<u8, MIDIRxBufferSize> m_MIDIRxBuffer;

	// Event handling
	TEventQueue m_EventQueue;

	static void EventHandler(const TEvent& Event);
	static void USBDeviceRemovedHandler(CDevice* pDevice, void* pContext);
	static void USBMIDIPacketHandler(unsigned nCable, u8* pPacket, unsigned nLength);
	static void MIDIReceiveHandler(const u8* pData, size_t nSize);

	static CMT32Pi* s_pThis;
};

#endif
