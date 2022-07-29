//
// mt32pi.h
//
// mt32-pi - A baremetal MIDI synthesizer for Raspberry Pi
// Copyright (C) 2020-2023 Dale Whinham <daleyo@gmail.com>
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
#include <circle/bcm54213.h>
#include <circle/bcmrandom.h>
#include <circle/cputhrottle.h>
#include <circle/devicenameservice.h>
#include <circle/gpiomanager.h>
#include <circle/i2cmaster.h>
#include <circle/interrupt.h>
#include <circle/logger.h>
#include <circle/multicore.h>
#include <circle/net/netsubsystem.h>
#include <circle/sched/scheduler.h>
#include <circle/sound/soundbasedevice.h>
#include <circle/spimaster.h>
#include <circle/timer.h>
#include <circle/types.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/usb/usbmassdevice.h>
#include <circle/usb/usbmidi.h>
#include <circle/usb/usbserial.h>
#include <fatfs/ff.h>
#include <wlan/bcm4343.h>
#include <wlan/hostap/wpa_supplicant/wpasupplicant.h>

#include "config.h"
#include "control/control.h"
#include "control/mister.h"
#include "event.h"
#include "lcd/ui.h"
#include "midiparser.h"
#include "net/applemidi.h"
#include "net/ftpdaemon.h"
#include "net/udpmidi.h"
#include "pisound.h"
#include "power.h"
#include "ringbuffer.h"
#include "synth/mt32romset.h"
#include "synth/mt32synth.h"
#include "synth/oplsynth.h"
#include "synth/soundfontsynth.h"
#include "synth/synth.h"

//#define MONITOR_TEMPERATURE

class CMT32Pi : CMultiCoreSupport, CPower, CMIDIParser, CAppleMIDIHandler, CUDPMIDIHandler
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
		Spinner,
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

	// CAppleMIDIHandler
	virtual void OnAppleMIDIDataReceived(const u8* pData, size_t nSize) override { ParseMIDIBytes(pData, nSize); };
	virtual void OnAppleMIDIConnect(const CIPAddress* pIPAddress, const char* pName) override;
	virtual void OnAppleMIDIDisconnect(const CIPAddress* pIPAddress, const char* pName) override;

	// CUDPMIDIHandler
	virtual void OnUDPMIDIDataReceived(const u8* pData, size_t nSize) override { ParseMIDIBytes(pData, nSize); };

	// Initialization
	bool InitNetwork();
	bool InitMT32Synth();
	bool InitSoundFontSynth();
	bool InitOPLSynth();

	// Tasks for specific CPU cores
	void MainTask();
	void UITask();
	void AudioTask();

	void UpdateUSB(bool bStartup = false);
	void UpdateNetwork();
	void UpdateMIDI();
	void PurgeMIDIBuffers();
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

	const char* GetNetworkDeviceShortName() const;
	void LEDOn();
	void LCDLog(TLCDLogType Type, const char* pFormat...);

	CLogger* volatile m_pLogger;
	CConfig* volatile m_pConfig;

	CTimer* m_pTimer;
	CActLED* m_pActLED;

	CI2CMaster* m_pI2CMaster;
	CSPIMaster* m_pSPIMaster;
	CInterruptSystem* m_pInterrupt;
	CGPIOManager* m_pGPIOManager;
	CSerialDevice* m_pSerial;
	CUSBHCIDevice* m_pUSBHCI;
	FATFS m_USBFileSystem;
	bool m_bUSBAvailable;

	// Networking
	CNetSubSystem* m_pNet;
	CNetDevice* m_pNetDevice;
	CBcm4343Device m_WLAN;
	CWPASupplicant m_WPASupplicant;
	bool m_bNetworkReady;
	CAppleMIDIParticipant* m_pAppleMIDIParticipant;
	CUDPMIDIReceiver* m_pUDPMIDIReceiver;
	CFTPDaemon* m_pFTPDaemon;

	CBcmRandomNumberGenerator m_Random;

	CLCD* m_pLCD;
	unsigned m_nLCDUpdateTime;
	CUserInterface m_UserInterface;
#ifdef MONITOR_TEMPERATURE
	unsigned m_nTempUpdateTime;
#endif

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

	// USB devices
	CUSBMIDIDevice* m_pUSBMIDIDevice;
	CUSBSerialDevice* m_pUSBSerialDevice;
	CUSBBulkOnlyMassStorageDevice* volatile m_pUSBMassStorageDevice;

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
	COPLSynth* m_pOPLSynth;

	// MIDI receive buffer
	CRingBuffer<u8, MIDIRxBufferSize> m_MIDIRxBuffer;

	// Event handling
	TEventQueue m_EventQueue;

	static void EventHandler(const TEvent& Event);
	static void USBMIDIDeviceRemovedHandler(CDevice* pDevice, void* pContext);
	static void USBMIDIPacketHandler(unsigned nCable, u8* pPacket, unsigned nLength);
	static void IRQMIDIReceiveHandler(const u8* pData, size_t nSize);

	static void PanicHandler();

	static CMT32Pi* s_pThis;
};

#endif
