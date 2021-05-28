//
// mt32pi.cpp
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

#include <circle/i2ssoundbasedevice.h>
#include <circle/memory.h>
#include <circle/pwmsoundbasedevice.h>
#include <circle/serial.h>

#include <cstdarg>

#include "lcd/hd44780.h"
#include "lcd/ssd1306.h"
#include "mt32pi.h"

const char MT32PiName[] = "mt32-pi";

constexpr u32 LCDUpdatePeriodMillis                = 16;
constexpr u32 MisterUpdatePeriodMillis             = 50;
constexpr u32 LEDTimeoutMillis                     = 50;
constexpr u32 ActiveSenseTimeoutMillis             = 330;

constexpr float Sample16BitMax = (1 << 16 - 1) - 1;
constexpr float Sample24BitMax = (1 << 24 - 1) - 1;

enum class TCustomSysExCommand : u8
{
	Reboot           = 0x00,
	SwitchMT32ROMSet = 0x01,
	SwitchSoundFont  = 0x02,
	SwitchSynth      = 0x03,
};

CMT32Pi* CMT32Pi::s_pThis = nullptr;

CMT32Pi::CMT32Pi(CI2CMaster* pI2CMaster, CSPIMaster* pSPIMaster, CInterruptSystem* pInterrupt, CGPIOManager* pGPIOManager, CSerialDevice* pSerialDevice, CUSBHCIDevice* pUSBHCI)
	: CMultiCoreSupport(CMemorySystem::Get()),
	  CMIDIParser(),

	  m_pTimer(CTimer::Get()),
	  m_pActLED(CActLED::Get()),

	  m_pI2CMaster(pI2CMaster),
	  m_pSPIMaster(pSPIMaster),
	  m_pInterrupt(pInterrupt),
	  m_pGPIOManager(pGPIOManager),
	  m_pSerial(pSerialDevice),
	  m_pUSBHCI(pUSBHCI),
	  m_USBFileSystem{},

	  m_pLCD(nullptr),
	  m_nLCDUpdateTime(0),

	  m_pControl(nullptr),
	  m_MisterControl(pI2CMaster, m_EventQueue),
	  m_nMisterUpdateTime(0),

	  m_nDeferredSoundFontSwitchIndex(0),
	  m_nDeferredSoundFontSwitchTime(0),

	  m_bSerialMIDIAvailable(false),
	  m_bSerialMIDIEnabled(false),
	  m_pUSBMIDIDevice(nullptr),
	  m_pUSBMassStorageDevice(nullptr),

	  m_bActiveSenseFlag(false),
	  m_nActiveSenseTime(0),

	  m_bRunning(true),
	  m_bUITaskDone(false),
	  m_bLEDOn(false),
	  m_nLEDOnTime(0),

	  m_pSound(nullptr),
	  m_pPisound(nullptr),

	  m_nMasterVolume(100),
	  m_pCurrentSynth(nullptr),
	  m_pMT32Synth(nullptr),
	  m_pSoundFontSynth(nullptr),
	  m_pPassthroughSynth(nullptr)
{
	s_pThis = this;
}

CMT32Pi::~CMT32Pi()
{
}

bool CMT32Pi::Initialize(bool bSerialMIDIAvailable)
{
	CConfig* const pConfig = CConfig::Get();
	CLogger* const pLogger = CLogger::Get();
        m_bMIDIGPIOThru = CConfig::Get()->MIDIGPIOThru;
	m_bSerialMIDIAvailable = bSerialMIDIAvailable;
	m_bSerialMIDIEnabled = bSerialMIDIAvailable;

	switch (pConfig->LCDType)
	{
		case CConfig::TLCDType::HD44780FourBit:
			m_pLCD = new CHD44780FourBit(pConfig->LCDWidth, pConfig->LCDHeight);
			break;

		case CConfig::TLCDType::HD44780I2C:
			m_pLCD = new CHD44780I2C(m_pI2CMaster, pConfig->LCDI2CLCDAddress, pConfig->LCDWidth, pConfig->LCDHeight);
			break;

		case CConfig::TLCDType::SH1106I2C:
			m_pLCD = new CSH1106(m_pI2CMaster, pConfig->LCDI2CLCDAddress, pConfig->LCDWidth, pConfig->LCDHeight, pConfig->LCDRotation);
			break;

		case CConfig::TLCDType::SSD1306I2C:
			m_pLCD = new CSSD1306(m_pI2CMaster, pConfig->LCDI2CLCDAddress, pConfig->LCDWidth, pConfig->LCDHeight, pConfig->LCDRotation);
			break;

		default:
			break;
	}

	if (m_pLCD)
	{
		if (m_pLCD->Initialize())
			m_pLCD->Print("mt32-pi " MT32_PI_VERSION, 0, 0, false, true);
		else
		{
			pLogger->Write(MT32PiName, LogWarning, "LCD init failed; invalid dimensions?");
			delete m_pLCD;
			m_pLCD = nullptr;
		}
	}

#if !defined(__aarch64__) || !defined(LEAVE_QEMU_ON_HALT)
	// The USB driver is not supported under 64-bit QEMU, so
	// the initialization must be skipped in this case, or an
	// exit happens here under 64-bit QEMU.
	LCDLog(TLCDLogType::Startup, "Init USB");
	if (pConfig->SystemUSB)
	{
		if (!m_pUSBHCI->Initialize())
			return false;

		// Perform an initial Plug and Play update to initialize devices early
		UpdateUSB(true);
	}
#endif

	// Check for Blokas Pisound
	m_pPisound = new CPisound(m_pSPIMaster, m_pGPIOManager, pConfig->AudioSampleRate);
	if (m_pPisound->Initialize())
	{
		pLogger->Write(MT32PiName, LogWarning, "Blokas Pisound detected");
		m_pPisound->RegisterMIDIReceiveHandler(MIDIReceiveHandler);
		m_bSerialMIDIEnabled = false;
	}
	else
	{
		delete m_pPisound;
		m_pPisound = nullptr;
	}

	if (pConfig->AudioOutputDevice == CConfig::TAudioOutputDevice::I2SDAC)
	{
		LCDLog(TLCDLogType::Startup, "Init audio (I2S)");

		// Pisound provides clock
		const bool bSlave = m_pPisound != nullptr;
		m_pSound = new CI2SSoundBaseDevice(m_pInterrupt, pConfig->AudioSampleRate, pConfig->AudioChunkSize, bSlave);
		m_pSound->SetWriteFormat(TSoundFormat::SoundFormatSigned24);

		if (pConfig->AudioI2CDACInit == CConfig::TAudioI2CDACInit::PCM51xx)
			InitPCM51xx(pConfig->AudioI2CDACAddress);
	}
	else
	{
		LCDLog(TLCDLogType::Startup, "Init audio (PWM)");
		m_pSound = new CPWMSoundBaseDevice(m_pInterrupt, pConfig->AudioSampleRate, pConfig->AudioChunkSize);
		m_pSound->SetWriteFormat(TSoundFormat::SoundFormatSigned16);
	}

	if (!m_pSound->AllocateQueueFrames(pConfig->AudioChunkSize))
		pLogger->Write(MT32PiName, LogPanic, "Failed to allocate sound queue");

	LCDLog(TLCDLogType::Startup, "Init controls");
	if (pConfig->ControlScheme == CConfig::TControlScheme::SimpleButtons)
		m_pControl = new CControlSimpleButtons(m_EventQueue);
	else if (pConfig->ControlScheme == CConfig::TControlScheme::SimpleEncoder)
		m_pControl = new CControlSimpleEncoder(m_EventQueue, pConfig->ControlEncoderType);

	if (m_pControl && !m_pControl->Initialize())
	{
		pLogger->Write(MT32PiName, LogWarning, "Control init failed");
		delete m_pControl;
		m_pControl = nullptr;
	}

	LCDLog(TLCDLogType::Startup, "Init mt32emu");
	InitMT32Synth();

	LCDLog(TLCDLogType::Startup, "Init FluidSynth");
	InitSoundFontSynth();

	LCDLog(TLCDLogType::Startup, "Init Passthrough");
	InitPassthroughSynth();

	LCDLog(TLCDLogType::Startup, "Past Passthrough");

	// Set initial synthesizer
	if (pConfig->SystemDefaultSynth == CConfig::TSystemDefaultSynth::MT32)
		m_pCurrentSynth = m_pMT32Synth;
	else if (pConfig->SystemDefaultSynth == CConfig::TSystemDefaultSynth::SoundFont)
		m_pCurrentSynth = m_pSoundFontSynth;
	else if (pConfig->SystemDefaultSynth == CConfig::TSystemDefaultSynth::Passthrough)
	{
		m_pCurrentSynth = m_pPassthroughSynth;
		m_bMIDIGPIOThru = 1;
	}

	LCDLog(TLCDLogType::Startup, "Past set initial synth");
	if (!m_pCurrentSynth)
	{
		pLogger->Write(MT32PiName, LogError, "Preferred synth failed to initialize successfully");

		// Activate any working synth
		if (m_pMT32Synth)
			m_pCurrentSynth = m_pMT32Synth;
		else if (m_pSoundFontSynth)
			m_pCurrentSynth = m_pSoundFontSynth;
		else
		{
			pLogger->Write(MT32PiName, LogError, "No synths available");
			LCDLog(TLCDLogType::Startup, "Synth init failed!");
			return false;
		}
	}

	if (m_pPisound)
		pLogger->Write(MT32PiName, LogNotice, "Using Pisound MIDI interface");
	else if (m_bSerialMIDIEnabled)
		pLogger->Write(MT32PiName, LogNotice, "Using serial MIDI interface");

	CCPUThrottle::Get()->DumpStatus();
	SetPowerSaveTimeout(pConfig->SystemPowerSaveTimeout);

	// Clear LCD
	if (m_pLCD)
		m_pLCD->Clear();

	// Start audio
	m_pSound->Start();

	// Start other cores
	if (!CMultiCoreSupport::Initialize())
		return false;

	return true;
}

bool CMT32Pi::InitMT32Synth()
{
	assert(m_pMT32Synth == nullptr);

	CConfig* const pConfig = CConfig::Get();

	m_pMT32Synth = new CMT32Synth(pConfig->AudioSampleRate, pConfig->MT32EmuGain, pConfig->MT32EmuReverbGain, pConfig->MT32EmuResamplerQuality);
	if (!m_pMT32Synth->Initialize())
	{
		CLogger::Get()->Write(MT32PiName, LogWarning, "mt32emu init failed; no ROMs present?");
		delete m_pMT32Synth;
		m_pMT32Synth = nullptr;
	}

	// Set initial MT-32 channel assignment from config
	if (pConfig->MT32EmuMIDIChannels == CMT32Synth::TMIDIChannels::Alternate)
		m_pMT32Synth->SetMIDIChannels(pConfig->MT32EmuMIDIChannels);

	m_pMT32Synth->SetLCD(m_pLCD);

	return m_pMT32Synth != nullptr;
}

bool CMT32Pi::InitSoundFontSynth()
{
	assert(m_pSoundFontSynth == nullptr);

	CConfig* const pConfig = CConfig::Get();

	m_pSoundFontSynth = new CSoundFontSynth(pConfig->AudioSampleRate, pConfig->FluidSynthGain, pConfig->FluidSynthPolyphony);
	if (!m_pSoundFontSynth->Initialize())
	{
		CLogger::Get()->Write(MT32PiName, LogWarning, "FluidSynth init failed; no SoundFonts present?");
		delete m_pSoundFontSynth;
		m_pSoundFontSynth = nullptr;
	}

	m_pSoundFontSynth->SetLCD(m_pLCD);

	return m_pSoundFontSynth != nullptr;
}

bool CMT32Pi::InitPassthroughSynth()
{
	//assert(m_pPassthroughSynth == nullptr);

	LCDLog(TLCDLogType::Startup, "In Init Passthrough");
	CConfig* const pConfig = CConfig::Get();

	m_pPassthroughSynth = new CPassthroughSynth(pConfig->AudioSampleRate, pConfig->FluidSynthGain, pConfig->FluidSynthPolyphony);

	LCDLog(TLCDLogType::Startup, "Out Init Passthrough");
	m_pPassthroughSynth->SetLCD(m_pLCD);

	return m_pPassthroughSynth != nullptr;
}

void CMT32Pi::MainTask()
{
	CConfig* const pConfig = CConfig::Get();
	CLogger* const pLogger = CLogger::Get();

	pLogger->Write(MT32PiName, LogNotice, "Main task on Core 0 starting up");

	Awaken();

	while (m_bRunning)
	{
		// Process MIDI data
		UpdateMIDI();

		// Update controls
		if (m_pControl)
			m_pControl->Update();

		// Process events
		ProcessEventQueue();

		unsigned ticks = m_pTimer->GetTicks();

		// Update activity LED
		if (m_bLEDOn && (ticks - m_nLEDOnTime) >= MSEC2HZ(LEDTimeoutMillis))
		{
			m_pActLED->Off();
			m_bLEDOn = false;
		}

		// Check for active sensing timeout
		if (m_bActiveSenseFlag && (ticks > m_nActiveSenseTime) && (ticks - m_nActiveSenseTime) >= MSEC2HZ(ActiveSenseTimeoutMillis))
		{
			m_pCurrentSynth->AllSoundOff();
			m_bActiveSenseFlag = false;
			pLogger->Write(MT32PiName, LogNotice, "Active sense timeout - turning notes off");
		}

		// Update power management
		if (m_pCurrentSynth->IsActive())
			Awaken();

		CPower::Update();

		// Check for deferred SoundFont switch
		if (m_bDeferredSoundFontSwitchFlag && (ticks - m_nDeferredSoundFontSwitchTime) >= static_cast<unsigned int>(pConfig->ControlSwitchTimeout) * HZ)
		{
			SwitchSoundFont(m_nDeferredSoundFontSwitchIndex);
			m_bDeferredSoundFontSwitchFlag = false;

			// Trigger an awaken so we don't immediately go to sleep
			Awaken();
		}

		// Check for USB PnP events
		if (CConfig::Get()->SystemUSB)
			UpdateUSB();
	}

	// Stop audio
	m_pSound->Cancel();

	// Wait for UI task to finish
	while (!m_bUITaskDone)
		;
}

void CMT32Pi::UITask()
{
	CLogger::Get()->Write(MT32PiName, LogNotice, "UI task on Core 1 starting up");

	// Display current MT-32 ROM version/SoundFont
	m_pCurrentSynth->ReportStatus();

	const bool bMisterEnabled = CConfig::Get()->ControlMister;

	while (m_bRunning)
	{
		unsigned ticks = m_pTimer->GetTicks();

		// Update LCD
		if (m_pLCD && (ticks - m_nLCDUpdateTime) >= MSEC2HZ(LCDUpdatePeriodMillis))
		{
			if (m_pCurrentSynth == m_pMT32Synth)
				m_pLCD->Update(*m_pMT32Synth);
			else
				m_pLCD->Update(*m_pSoundFontSynth);
			m_nLCDUpdateTime = ticks;
		}

		// Poll MiSTer interface
		if (bMisterEnabled && (ticks - m_nMisterUpdateTime) >= MSEC2HZ(MisterUpdatePeriodMillis))
		{
			TMisterStatus Status{TMisterSynth::Unknown, 0xFF, 0xFF};

			if (m_pCurrentSynth == m_pMT32Synth)
				Status.Synth = TMisterSynth::MT32;
			else if (m_pCurrentSynth == m_pSoundFontSynth)
				Status.Synth = TMisterSynth::SoundFont;

			if (m_pMT32Synth)
				Status.MT32ROMSet = static_cast<u8>(m_pMT32Synth->GetROMSet());

			if (m_pSoundFontSynth)
				Status.SoundFontIndex = m_pSoundFontSynth->GetSoundFontIndex();

			m_MisterControl.Update(Status);
			m_nMisterUpdateTime = ticks;
		}
	}

	// Clear screen
	if (m_pLCD)
		m_pLCD->Clear();

	m_bUITaskDone = true;
}

void CMT32Pi::AudioTask()
{
	CLogger* const pLogger = CLogger::Get();
	pLogger->Write(MT32PiName, LogNotice, "Audio task on Core 2 starting up");

	const bool bUse24Bit    = CConfig::Get()->AudioOutputDevice == CConfig::TAudioOutputDevice::I2SDAC;
	const size_t nQueueSize = m_pSound->GetQueueSizeFrames();
	float FloatBuffer[nQueueSize * 2];
	s16 Int16Buffer[nQueueSize * 2];
	s32 Int32Buffer[nQueueSize * 2];

	while (m_bRunning)
	{
		const size_t nFrames = nQueueSize - m_pSound->GetQueueFramesAvail();
		m_pCurrentSynth->Render(FloatBuffer, nFrames);

		size_t nWriteBytes;
		int nResult;

		if (bUse24Bit)
		{
			nWriteBytes = nFrames * 2 * sizeof(*Int32Buffer);

			// Convert to signed 24-bit integers
			for (size_t i = 0; i < nFrames * 2; ++i)
				Int32Buffer[i] = Utility::Clamp(FloatBuffer[i], -1.0f, 1.0f) * Sample24BitMax;

			nResult = m_pSound->Write(Int32Buffer, nWriteBytes);
		}
		else
		{
			nWriteBytes = nFrames * 2 * sizeof(*Int16Buffer);

			// Convert to signed 16-bit integers
			for (size_t i = 0; i < nFrames * 2; ++i)
				Int16Buffer[i] = Utility::Clamp(FloatBuffer[i], -1.0f, 1.0f) * Sample16BitMax;

			nResult = m_pSound->Write(Int16Buffer, nWriteBytes);
		}

		if (nResult != static_cast<int>(nWriteBytes))
			pLogger->Write(MT32PiName, LogError, "Sound data dropped");
	}
}

void CMT32Pi::Run(unsigned nCore)
{
	// Assign tasks to different CPU cores
	switch (nCore)
	{
		case 0:
			return MainTask();

		case 1:
			return UITask();

		case 2:
			return AudioTask();

		default:
			break;
	}
}

void CMT32Pi::OnEnterPowerSavingMode()
{
	CPower::OnEnterPowerSavingMode();
	m_pSound->Cancel();

	if (m_pLCD)
		m_pLCD->EnterPowerSavingMode();
}

void CMT32Pi::OnExitPowerSavingMode()
{
	CPower::OnExitPowerSavingMode();
	m_pSound->Start();

	if (m_pLCD)
		m_pLCD->ExitPowerSavingMode();
}

void CMT32Pi::OnThrottleDetected()
{
	CPower::OnThrottleDetected();
	LCDLog(TLCDLogType::Warning, "CPU throttl! Chk PSU");
}

void CMT32Pi::OnUnderVoltageDetected()
{
	CPower::OnUnderVoltageDetected();
	LCDLog(TLCDLogType::Warning, "Low voltage! Chk PSU");
}

void CMT32Pi::OnShortMessage(u32 nMessage)
{
	// Active sensing
	if (nMessage == 0xFE)
	{
		m_bActiveSenseFlag = true;
		return;
	}

	// Flash LED
	LEDOn();

	m_pCurrentSynth->HandleMIDIShortMessage(nMessage);

	// Wake from power saving mode if necessary
	Awaken();
}

void CMT32Pi::OnSysExMessage(const u8* pData, size_t nSize)
{
	// Flash LED
	LEDOn();

	// If we don't consume the SysEx message, forward it to the synthesizer
	if (!ParseCustomSysEx(pData, nSize))
		m_pCurrentSynth->HandleMIDISysExMessage(pData, nSize);

	// Wake from power saving mode if necessary
	Awaken();
}

void CMT32Pi::OnUnexpectedStatus()
{
	CMIDIParser::OnUnexpectedStatus();
	LCDLog(TLCDLogType::Error, "Unexp. MIDI status!");
}

void CMT32Pi::OnSysExOverflow()
{
	CMIDIParser::OnSysExOverflow();
	LCDLog(TLCDLogType::Error, "SysEx overflow!");
}

bool CMT32Pi::ParseCustomSysEx(const u8* pData, size_t nSize)
{
	if (nSize < 4)
		return false;

	// 'Educational' manufacturer
	if (pData[1] != 0x7D)
		return false;

	const auto Command = static_cast<TCustomSysExCommand>(pData[2]);

	// Reboot (F0 7D 00 F7)
	if (nSize == 4 && Command == TCustomSysExCommand::Reboot)
	{
		CLogger::Get()->Write(MT32PiName, LogNotice, "Reboot command received");
		m_bRunning = false;
		return true;
	}

	if (nSize != 5)
		return false;

	const u8 nParameter = pData[3];
	switch (Command)
	{
		// Switch MT-32 ROM set (F0 7D 01 xx F7)
		case TCustomSysExCommand::SwitchMT32ROMSet:
		{
			const TMT32ROMSet NewROMSet = static_cast<TMT32ROMSet>(nParameter);
			if (NewROMSet < TMT32ROMSet::Any)
				SwitchMT32ROMSet(NewROMSet);
			return true;
		}

		// Switch SoundFont (F0 7D 02 xx F7)
		case TCustomSysExCommand::SwitchSoundFont:
			SwitchSoundFont(nParameter);
			return true;

		// Switch synthesizer (F0 7D 03 xx F7)
		case TCustomSysExCommand::SwitchSynth:
		{
			SwitchSynth(static_cast<TSynth>(nParameter));
			return true;
		}

		default:
			return false;
	}
}

void CMT32Pi::UpdateUSB(bool bStartup)
{
	if (!m_pUSBHCI->UpdatePlugAndPlay())
		return;

	CLogger* const pLogger = CLogger::Get();
	CUSBBulkOnlyMassStorageDevice* pUSBMassStorageDevice = static_cast<CUSBBulkOnlyMassStorageDevice*>(CDeviceNameService::Get()->GetDevice("umsd1", TRUE));

	if (!m_pUSBMassStorageDevice && pUSBMassStorageDevice)
	{
		// USB disk was attached
		pLogger->Write(MT32PiName, LogNotice, "USB mass storage device attached");

		if (f_mount(&m_USBFileSystem, "USB:", 1) != FR_OK)
			pLogger->Write(MT32PiName, LogError, "Failed to mount USB mass storage device");
		else
		{
			if (!bStartup)
			{
				LCDLog(TLCDLogType::Spinner, "MT-32 ROM rescan");
				if (m_pMT32Synth)
					m_pMT32Synth->GetROMManager().ScanROMs();
				else
					InitMT32Synth();

				LCDLog(TLCDLogType::Spinner, "SoundFont rescan");
				if (m_pSoundFontSynth)
					m_pSoundFontSynth->GetSoundFontManager().ScanSoundFonts();
				else
					InitSoundFontSynth();

				if (m_pSoundFontSynth)
					LCDLog(TLCDLogType::Notice, "%d SoundFonts avail", m_pSoundFontSynth->GetSoundFontManager().GetSoundFontCount());
			}
		}
	}
	else if (m_pUSBMassStorageDevice && !pUSBMassStorageDevice)
	{
		// USB disk was removed
		pLogger->Write(MT32PiName, LogNotice, "USB mass storage device removed");

		f_unmount("USB:");

		// Only need to rescan SoundFonts on storage removal; MT-32 ROMs are kept in memory
		if (m_pSoundFontSynth)
		{
			LCDLog(TLCDLogType::Spinner, "SoundFont rescan");
			m_pSoundFontSynth->GetSoundFontManager().ScanSoundFonts();
			LCDLog(TLCDLogType::Notice, "%d SoundFonts avail", m_pSoundFontSynth->GetSoundFontManager().GetSoundFontCount());
		}
	}
	m_pUSBMassStorageDevice = pUSBMassStorageDevice;

	if (!m_pUSBMIDIDevice && (m_pUSBMIDIDevice = static_cast<CUSBMIDIDevice*>(CDeviceNameService::Get()->GetDevice("umidi1", FALSE))))
	{
		m_pUSBMIDIDevice->RegisterRemovedHandler(USBMIDIDeviceRemovedHandler);
		m_pUSBMIDIDevice->RegisterPacketHandler(USBMIDIPacketHandler);
		pLogger->Write(MT32PiName, LogNotice, "Using USB MIDI interface");
		m_bSerialMIDIEnabled = false;
	}
}

void CMT32Pi::UpdateMIDI()
{
	size_t nBytes;
	u8 Buffer[MIDIRxBufferSize];

	// Read MIDI messages from serial device or ring buffer
	if (m_bSerialMIDIEnabled)
		nBytes = ReceiveSerialMIDI(Buffer, sizeof(Buffer));
	else
		nBytes = m_MIDIRxBuffer.Dequeue(Buffer, sizeof(Buffer));

	if (nBytes == 0)
		return;

	// Process MIDI messages
	ParseMIDIBytes(Buffer, nBytes);

	// Reset the Active Sense timer
	s_pThis->m_nActiveSenseTime = s_pThis->m_pTimer->GetTicks();
}

size_t CMT32Pi::ReceiveSerialMIDI(u8* pOutData, size_t nSize)
{
	// Read serial MIDI data
	int nResult = m_pSerial->Read(pOutData, nSize);

	// No data
	if (nResult == 0)
		return 0;

	// Error
	if (nResult < 0)
	{
		const char* errorString;
		switch (nResult)
		{
			case -SERIAL_ERROR_BREAK:
				errorString = "UART break error!";
				break;

			case -SERIAL_ERROR_OVERRUN:
				errorString = "UART overrun error!";
				break;

			case -SERIAL_ERROR_FRAMING:
				errorString = "UART framing error!";
				break;

			default:
				errorString = "Unknown UART error!";
				break;
		}

		CLogger::Get()->Write(MT32PiName, LogWarning, errorString);
		LCDLog(TLCDLogType::Error, errorString);
		return 0;
	}

	// Replay received MIDI data out via the serial port ('software thru')
	if (m_bMIDIGPIOThru)
	{
		int nSendResult = m_pSerial->Write(pOutData, nResult);
		if (nSendResult != nResult)
		{
			CLogger::Get()->Write(MT32PiName, LogWarning, "received %d bytes, but only sent %d bytes", nResult, nSendResult);
			LCDLog(TLCDLogType::Error, "UART TX error!");
		}
	}

	return static_cast<size_t>(nResult);
}

void CMT32Pi::ProcessEventQueue()
{
	TEvent Buffer[EventQueueSize];
	const size_t nEvents = m_EventQueue.Dequeue(Buffer, sizeof(Buffer));

	// We got some events, wake up
	if (nEvents > 0)
		Awaken();

	for (size_t i = 0; i < nEvents; ++i)
	{
		const TEvent& Event = Buffer[i];

		switch (Event.Type)
		{
			case TEventType::Button:
				ProcessButtonEvent(Event.Button);
				break;

			case TEventType::SwitchSynth:
				SwitchSynth(Event.SwitchSynth.Synth);
				break;

			case TEventType::SwitchMT32ROMSet:
				SwitchMT32ROMSet(Event.SwitchMT32ROMSet.ROMSet);
				break;

			case TEventType::SwitchSoundFont:
				DeferSwitchSoundFont(Event.SwitchSoundFont.Index);
				break;

			case TEventType::AllSoundOff:
				if (m_pMT32Synth)
					m_pMT32Synth->AllSoundOff();
				if (m_pSoundFontSynth)
					m_pSoundFontSynth->AllSoundOff();
				break;

			case TEventType::DisplayImage:
				if (m_pLCD)
					m_pLCD->OnDisplayImage(Event.DisplayImage.Image);
				break;

			case TEventType::Encoder:
				SetMasterVolume(m_nMasterVolume + Event.Encoder.nDelta);
				break;
		}
	}
}

void CMT32Pi::ProcessButtonEvent(const TButtonEvent& Event)
{
	if (Event.Button == TButton::EncoderButton)
	{
		LCDLog(TLCDLogType::Notice, "Enc. button %s", Event.bPressed ? "PRESSED" : "RELEASED");
		return;
	}

	if (!Event.bPressed)
		return;

	if (Event.Button == TButton::Button1)
	{
		// Swap synths
		if (m_pCurrentSynth == m_pMT32Synth)
			SwitchSynth(TSynth::SoundFont);
		else
			SwitchSynth(TSynth::MT32);
	}
	else if (Event.Button == TButton::Button2 && Event.bPressed)
	{
		if (m_pCurrentSynth == m_pMT32Synth)
			NextMT32ROMSet();
		else
		{
			// Next SoundFont
			const size_t nSoundFonts = m_pSoundFontSynth->GetSoundFontManager().GetSoundFontCount();

			if (!nSoundFonts)
				LCDLog(TLCDLogType::Error, "No SoundFonts!");
			else
			{
				size_t nNextSoundFont;
				if (m_bDeferredSoundFontSwitchFlag)
					nNextSoundFont = (m_nDeferredSoundFontSwitchIndex + 1) % nSoundFonts;
				else
				{
					// Current SoundFont was probably on a USB stick that has since been removed
					const size_t nCurrentSoundFont = m_pSoundFontSynth->GetSoundFontIndex();
					if (nCurrentSoundFont > nSoundFonts)
						nNextSoundFont = 0;
					else
						nNextSoundFont = (nCurrentSoundFont + 1) % nSoundFonts;
				}

				DeferSwitchSoundFont(nNextSoundFont);
			}
		}
	}
	else if (Event.Button == TButton::Button3)
	{
		SetMasterVolume(m_nMasterVolume - 1);
	}
	else if (Event.Button == TButton::Button4)
	{
		SetMasterVolume(m_nMasterVolume + 1);
	}
}

void CMT32Pi::SwitchSynth(TSynth NewSynth)
{
	CSynthBase* pNewSynth = nullptr;

	if (NewSynth == TSynth::MT32)
		pNewSynth = m_pMT32Synth;
	else if (NewSynth == TSynth::SoundFont)
		pNewSynth = m_pSoundFontSynth;
	else if (NewSynth == TSynth::Passthrough)
		pNewSynth = m_pPassthroughSynth;

	if (pNewSynth == nullptr)
	{
		LCDLog(TLCDLogType::Warning, "Synth unavailable!");
		return;
	}

	if (pNewSynth == m_pCurrentSynth)
	{
		LCDLog(TLCDLogType::Warning, "Already active!");
		return;
	}

	m_pCurrentSynth->AllSoundOff();
	m_pCurrentSynth = pNewSynth;
        m_bMIDIGPIOThru = CConfig::Get()->MIDIGPIOThru;
        char pMode[32];
        switch (NewSynth)
	{
		case TSynth::MT32:
                        strcpy(pMode,"MT-32 Mode");
        		m_bMIDIGPIOThru = CConfig::Get()->MIDIGPIOThru;
			break;
		case TSynth::SoundFont:
                        strcpy(pMode,"SoundFont Mode");
        		m_bMIDIGPIOThru = CConfig::Get()->MIDIGPIOThru;
			break;
		case TSynth::Passthrough:
                        strcpy(pMode,"Passthrough Mode");
        		m_bMIDIGPIOThru = 1;
			break;
	}
	CLogger::Get()->Write(MT32PiName, LogNotice, "Switching to %s", pMode);
	LCDLog(TLCDLogType::Notice, pMode);
}

void CMT32Pi::SwitchMT32ROMSet(TMT32ROMSet ROMSet)
{
	if (m_pMT32Synth == nullptr)
		return;

	CLogger::Get()->Write(MT32PiName, LogNotice, "Switching to ROM set %d", static_cast<u8>(ROMSet));
	if (m_pMT32Synth->SwitchROMSet(ROMSet) && m_pCurrentSynth == m_pMT32Synth)
		m_pMT32Synth->ReportStatus();
}

void CMT32Pi::NextMT32ROMSet()
{
	if (m_pMT32Synth == nullptr)
		return;

	CLogger::Get()->Write(MT32PiName, LogNotice, "Switching to next ROM set");

	if (m_pMT32Synth->NextROMSet() && m_pCurrentSynth == m_pMT32Synth)
		m_pMT32Synth->ReportStatus();
}

void CMT32Pi::SwitchSoundFont(size_t nIndex)
{
	if (m_pSoundFontSynth == nullptr)
		return;

	CLogger::Get()->Write(MT32PiName, LogNotice, "Switching to SoundFont %d", nIndex);
	if (m_pSoundFontSynth->SwitchSoundFont(nIndex) && m_pCurrentSynth == m_pSoundFontSynth)
		m_pSoundFontSynth->ReportStatus();
}

void CMT32Pi::DeferSwitchSoundFont(size_t nIndex)
{
	if (m_pSoundFontSynth == nullptr)
		return;

	const char* pName = m_pSoundFontSynth->GetSoundFontManager().GetSoundFontName(nIndex);
	LCDLog(TLCDLogType::Notice, "SF %ld: %s", nIndex, pName ? pName : "- N/A -");
	m_nDeferredSoundFontSwitchIndex = nIndex;
	m_nDeferredSoundFontSwitchTime  = CTimer::Get()->GetTicks();
	m_bDeferredSoundFontSwitchFlag  = true;
}

void CMT32Pi::SetMasterVolume(s32 nVolume)
{
	m_nMasterVolume = Utility::Clamp(nVolume, 0, 100);

	if (m_pMT32Synth)
		m_pMT32Synth->SetMasterVolume(m_nMasterVolume);
	if (m_pSoundFontSynth)
		m_pSoundFontSynth->SetMasterVolume(m_nMasterVolume);

	if (m_pCurrentSynth == m_pSoundFontSynth)
		LCDLog(TLCDLogType::Notice, "Volume: %d", m_nMasterVolume);
}

void CMT32Pi::LEDOn()
{
	m_pActLED->On();
	m_nLEDOnTime = m_pTimer->GetTicks();
	m_bLEDOn = true;
}

void CMT32Pi::LCDLog(TLCDLogType Type, const char* pFormat...)
{
	if (!m_pLCD)
		return;

	// Up to 20 characters plus null terminator
	char Buffer[21];

	va_list Args;
	va_start(Args, pFormat);
	vsnprintf(Buffer, sizeof(Buffer), pFormat, Args);
	va_end(Args);

	// LCD task hasn't started yet; print directly
	if (Type == TLCDLogType::Startup)
	{
		m_pLCD->Print("~", 0, 1, false, false);
		m_pLCD->Print(Buffer, 2, 1, true, true);
	}

	// Let LCD task pick up the message in its next update
	else
		m_pLCD->OnSystemMessage(Buffer, Type == TLCDLogType::Spinner);
}

// TODO: Generic configurable DAC init class
bool CMT32Pi::InitPCM51xx(u8 nAddress)
{
	static const u8 initBytes[][2] =
	{
		// Set PLL reference clock to BCK (set SREF to 001b)
		{ 0x0d, 0x10 },

		// Ignore clock halt detection (set IDCH to 1)
		{ 0x25, 0x08 },

		// Disable auto mute
		{ 0x41, 0x04 }
	};

	for (auto& command : initBytes)
	{
		if (m_pI2CMaster->Write(nAddress, &command, sizeof(command)) != sizeof(command))
		{
			CLogger::Get()->Write(MT32PiName, LogWarning, "I2C write error (DAC init sequence)");
			return false;
		}
	}

	return true;
}

void CMT32Pi::EventHandler(const TEvent& Event)
{
	assert(s_pThis != nullptr);

	// Enqueue event
	s_pThis->m_EventQueue.Enqueue(Event);
}

void CMT32Pi::USBMIDIDeviceRemovedHandler(CDevice* pDevice, void* pContext)
{
	assert(s_pThis != nullptr);

	s_pThis->m_pUSBMIDIDevice = nullptr;

	// Re-enable serial MIDI if not in-use by logger and not using Pisound
	if (s_pThis->m_bSerialMIDIAvailable && !s_pThis->m_pPisound)
	{
		CLogger::Get()->Write(MT32PiName, LogNotice, "Using serial MIDI interface");
		s_pThis->m_bSerialMIDIEnabled = true;
	}
}

// The following handlers are called from interrupt context, enqueue into ring buffer for main thread
void CMT32Pi::USBMIDIPacketHandler(unsigned nCable, u8* pPacket, unsigned nLength)
{
	MIDIReceiveHandler(pPacket, nLength);
}

void CMT32Pi::MIDIReceiveHandler(const u8* pData, size_t nSize)
{
	assert(s_pThis != nullptr);

	// Enqueue data into ring buffer
	if (s_pThis->m_MIDIRxBuffer.Enqueue(pData, nSize) != nSize)
	{
		static const char* pErrorString = "MIDI overrun error!";
		CLogger::Get()->Write(MT32PiName, LogWarning, pErrorString);
		s_pThis->LCDLog(TLCDLogType::Error, pErrorString);
	}
}
