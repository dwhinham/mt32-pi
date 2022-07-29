//
// mt32pi.cpp
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

#include <circle/memory.h>
#include <circle/serial.h>
#include <circle/sound/hdmisoundbasedevice.h>
#include <circle/sound/i2ssoundbasedevice.h>
#include <circle/sound/pwmsoundbasedevice.h>

#include <cstdarg>

#include "lcd/drivers/hd44780.h"
#include "lcd/drivers/ssd1306.h"
#include "lcd/ui.h"
#include "mt32pi.h"

#define MT32_PI_NAME "mt32-pi"
const char MT32PiName[] = MT32_PI_NAME;
const char MT32PiFullName[] = MT32_PI_NAME " " MT32_PI_VERSION;

const char WLANFirmwarePath[] = "SD:firmware/";
const char WLANConfigFile[]   = "SD:wpa_supplicant.conf";

constexpr u32 LCDUpdatePeriodMillis                = 16;
constexpr u32 MisterUpdatePeriodMillis             = 50;
constexpr u32 LEDTimeoutMillis                     = 50;
constexpr u32 ActiveSenseTimeoutMillis             = 330;

constexpr float Sample24BitMax = (1 << 24 - 1) - 1;

enum class TCustomSysExCommand : u8
{
	Reboot                = 0x00,
	SwitchMT32ROMSet      = 0x01,
	SwitchSoundFont       = 0x02,
	SwitchSynth           = 0x03,
	SetMT32ReversedStereo = 0x04,
};

CMT32Pi* CMT32Pi::s_pThis = nullptr;

CMT32Pi::CMT32Pi(CI2CMaster* pI2CMaster, CSPIMaster* pSPIMaster, CInterruptSystem* pInterrupt, CGPIOManager* pGPIOManager, CSerialDevice* pSerialDevice, CUSBHCIDevice* pUSBHCI)
	: CMultiCoreSupport(CMemorySystem::Get()),
	  CMIDIParser(),

	  m_pLogger(CLogger::Get()),
	  m_pConfig(CConfig::Get()),

	  m_pTimer(CTimer::Get()),
	  m_pActLED(CActLED::Get()),

	  m_pI2CMaster(pI2CMaster),
	  m_pSPIMaster(pSPIMaster),
	  m_pInterrupt(pInterrupt),
	  m_pGPIOManager(pGPIOManager),
	  m_pSerial(pSerialDevice),
	  m_pUSBHCI(pUSBHCI),
	  m_USBFileSystem{},
	  m_bUSBAvailable(false),

	  m_pNet(nullptr),
	  m_pNetDevice(nullptr),
	  m_WLAN(WLANFirmwarePath),
	  m_WPASupplicant(WLANConfigFile),
	  m_bNetworkReady(false),
	  m_pAppleMIDIParticipant(nullptr),
	  m_pUDPMIDIReceiver(nullptr),
	  m_pFTPDaemon(nullptr),

	  m_pLCD(nullptr),
	  m_nLCDUpdateTime(0),
#ifdef MONITOR_TEMPERATURE
	  m_nTempUpdateTime(0),
#endif

	  m_pControl(nullptr),
	  m_MisterControl(pI2CMaster, m_EventQueue),
	  m_nMisterUpdateTime(0),

	  m_bDeferredSoundFontSwitchFlag(false),
	  m_nDeferredSoundFontSwitchIndex(0),
	  m_nDeferredSoundFontSwitchTime(0),

	  m_bSerialMIDIAvailable(false),
	  m_bSerialMIDIEnabled(false),
	  m_pUSBMIDIDevice(nullptr),
	  m_pUSBSerialDevice(nullptr),
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
	  m_pOPLSynth(nullptr),
	  m_pOPNSynth(nullptr)
{
	s_pThis = this;
}

CMT32Pi::~CMT32Pi()
{
}

bool CMT32Pi::Initialize(bool bSerialMIDIAvailable)
{
	m_bSerialMIDIAvailable = bSerialMIDIAvailable;
	m_bSerialMIDIEnabled = bSerialMIDIAvailable;

	switch (m_pConfig->LCDType)
	{
		case CConfig::TLCDType::HD44780FourBit:
			m_pLCD = new CHD44780FourBit(m_pConfig->LCDWidth, m_pConfig->LCDHeight);
			break;

		case CConfig::TLCDType::HD44780I2C:
			m_pLCD = new CHD44780I2C(m_pI2CMaster, m_pConfig->LCDI2CLCDAddress, m_pConfig->LCDWidth, m_pConfig->LCDHeight);
			break;

		case CConfig::TLCDType::SH1106I2C:
			m_pLCD = new CSH1106(m_pI2CMaster, m_pConfig->LCDI2CLCDAddress, m_pConfig->LCDWidth, m_pConfig->LCDHeight, m_pConfig->LCDRotation);
			break;

		case CConfig::TLCDType::SSD1306I2C:
			m_pLCD = new CSSD1306(m_pI2CMaster, m_pConfig->LCDI2CLCDAddress, m_pConfig->LCDWidth, m_pConfig->LCDHeight, m_pConfig->LCDRotation, m_pConfig->LCDMirror);
			break;

		default:
			break;
	}

	if (m_pLCD)
	{
		if (m_pLCD->Initialize())
		{
			m_pLogger->RegisterPanicHandler(PanicHandler);

			// Splash screen
			if (m_pLCD->GetType() == CLCD::TType::Graphical && !m_pConfig->SystemVerbose)
				m_pLCD->DrawImage(TImage::MT32PiLogo, true);
			else
			{
				const u8 nOffsetX = CUserInterface::CenterMessageOffset(*m_pLCD, MT32PiFullName);
				m_pLCD->Print(MT32PiFullName, nOffsetX, 0, false, true);
			}
		}
		else
		{
			m_pLogger->Write(MT32PiName, LogWarning, "LCD init failed; invalid dimensions?");
			delete m_pLCD;
			m_pLCD = nullptr;
		}
	}

#if !defined(__aarch64__) || !defined(LEAVE_QEMU_ON_HALT)
	// The USB driver is not supported under 64-bit QEMU, so
	// the initialization must be skipped in this case, or an
	// exit happens here under 64-bit QEMU.
	LCDLog(TLCDLogType::Startup, "Init USB");
	if (m_pConfig->SystemUSB && m_pUSBHCI->Initialize())
	{
		m_bUSBAvailable = true;

		// Perform an initial Plug and Play update to initialize devices early
		UpdateUSB(true);
	}
#endif

	LCDLog(TLCDLogType::Startup, "Init Network");
	InitNetwork();

	// Check for Blokas Pisound, but only when not using 4-bit HD44780 (GPIO pin conflict)
	if (m_pConfig->LCDType != CConfig::TLCDType::HD44780FourBit)
	{
		m_pPisound = new CPisound(m_pSPIMaster, m_pGPIOManager, m_pConfig->AudioSampleRate);
		if (m_pPisound->Initialize())
		{
			m_pLogger->Write(MT32PiName, LogWarning, "Blokas Pisound detected");
			m_pPisound->RegisterMIDIReceiveHandler(IRQMIDIReceiveHandler);
			m_bSerialMIDIEnabled = false;
		}
		else
		{
			delete m_pPisound;
			m_pPisound = nullptr;
		}
	}

	// Queue size of just one chunk
	unsigned int nQueueSize = m_pConfig->AudioChunkSize;
	TSoundFormat Format = TSoundFormat::SoundFormatSigned24;

	switch (m_pConfig->AudioOutputDevice)
	{
		case CConfig::TAudioOutputDevice::PWM:
			LCDLog(TLCDLogType::Startup, "Init audio (PWM)");
			m_pSound = new CPWMSoundBaseDevice(m_pInterrupt, m_pConfig->AudioSampleRate, m_pConfig->AudioChunkSize);
			break;

		case CConfig::TAudioOutputDevice::HDMI:
		{
			LCDLog(TLCDLogType::Startup, "Init audio (HDMI)");

			// Chunk size must be a multiple of 384
			const unsigned int nChunkSize = Utility::RoundToNearestMultiple(m_pConfig->AudioChunkSize, IEC958_SUBFRAMES_PER_BLOCK);
			nQueueSize = nChunkSize;

			m_pSound = new CHDMISoundBaseDevice(m_pInterrupt, m_pConfig->AudioSampleRate, nChunkSize);
			break;
		}

		case CConfig::TAudioOutputDevice::I2S:
		{
			LCDLog(TLCDLogType::Startup, "Init audio (I2S)");

			// Pisound provides clock
			const bool bSlave = m_pPisound != nullptr;

			// Don't probe if using Pisound
			CI2CMaster* const pI2CMaster = bSlave ? nullptr : m_pI2CMaster;

			m_pSound = new CI2SSoundBaseDevice(m_pInterrupt, m_pConfig->AudioSampleRate, m_pConfig->AudioChunkSize, bSlave, pI2CMaster);
			Format = TSoundFormat::SoundFormatSigned24_32;

			break;
		}
	}

	m_pSound->SetWriteFormat(Format);
	if (!m_pSound->AllocateQueueFrames(nQueueSize))
		m_pLogger->Write(MT32PiName, LogPanic, "Failed to allocate sound queue");

	LCDLog(TLCDLogType::Startup, "Init controls");
	if (m_pConfig->ControlScheme == CConfig::TControlScheme::SimpleButtons)
		m_pControl = new CControlSimpleButtons(m_EventQueue);
	else if (m_pConfig->ControlScheme == CConfig::TControlScheme::SimpleEncoder)
		m_pControl = new CControlSimpleEncoder(m_EventQueue, m_pConfig->ControlEncoderType, m_pConfig->ControlEncoderReversed);

	if (m_pControl && !m_pControl->Initialize())
	{
		m_pLogger->Write(MT32PiName, LogWarning, "Control init failed");
		delete m_pControl;
		m_pControl = nullptr;
	}

	LCDLog(TLCDLogType::Startup, "Init mt32emu");
	InitMT32Synth();

	LCDLog(TLCDLogType::Startup, "Init FluidSynth");
	InitSoundFontSynth();

	LCDLog(TLCDLogType::Startup, "Init ADLMIDI");
	InitOPLSynth();

	LCDLog(TLCDLogType::Startup, "Init OPNMIDI");
	InitOPNSynth();

	// Set initial synthesizer
	if (m_pConfig->SystemDefaultSynth == CConfig::TSystemDefaultSynth::MT32)
		m_pCurrentSynth = m_pMT32Synth;
	else if (m_pConfig->SystemDefaultSynth == CConfig::TSystemDefaultSynth::SoundFont)
		m_pCurrentSynth = m_pSoundFontSynth;
	else if (m_pConfig->SystemDefaultSynth == CConfig::TSystemDefaultSynth::OPL)
		m_pCurrentSynth = m_pOPLSynth;
	else if (m_pConfig->SystemDefaultSynth == CConfig::TSystemDefaultSynth::OPN)
		m_pCurrentSynth = m_pOPNSynth;

	if (!m_pCurrentSynth)
	{
		m_pLogger->Write(MT32PiName, LogError, "Preferred synth failed to initialize successfully");

		// Activate any working synth
		if (m_pMT32Synth)
			m_pCurrentSynth = m_pMT32Synth;
		else if (m_pSoundFontSynth)
			m_pCurrentSynth = m_pSoundFontSynth;
		else if (m_pOPLSynth)
			m_pCurrentSynth = m_pOPLSynth;
		else if (m_pOPNSynth)
			m_pCurrentSynth = m_pOPNSynth;
		else
		{
			m_pLogger->Write(MT32PiName, LogPanic, "No synths available; ROMs/SoundFonts not found");
			return false;
		}
	}

	if (m_pPisound)
		m_pLogger->Write(MT32PiName, LogNotice, "Using Pisound MIDI interface");
	else if (m_bSerialMIDIEnabled)
		m_pLogger->Write(MT32PiName, LogNotice, "Using serial MIDI interface");

	CCPUThrottle::Get()->DumpStatus();
	SetPowerSaveTimeout(m_pConfig->SystemPowerSaveTimeout);

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

bool CMT32Pi::InitNetwork()
{
	assert(m_pNet == nullptr);

	TNetDeviceType NetDeviceType = NetDeviceTypeUnknown;

	if (m_pConfig->NetworkMode == CConfig::TNetworkMode::WiFi)
	{
		m_pLogger->Write(MT32PiName, LogNotice, "Initializing Wi-Fi");

		if (m_WLAN.Initialize() && m_WPASupplicant.Initialize())
			NetDeviceType = NetDeviceTypeWLAN;
		else
			m_pLogger->Write(MT32PiName, LogError, "Failed to initialize Wi-Fi");
	}
	else if (m_pConfig->NetworkMode == CConfig::TNetworkMode::Ethernet)
	{
		m_pLogger->Write(MT32PiName, LogNotice, "Initializing Ethernet");
		NetDeviceType = NetDeviceTypeEthernet;
	}

	if (NetDeviceType != NetDeviceTypeUnknown)
	{
		if (m_pConfig->NetworkDHCP)
			m_pNet = new CNetSubSystem(0, 0, 0, 0, m_pConfig->NetworkHostname, NetDeviceType);
		else
			m_pNet = new CNetSubSystem(
				m_pConfig->NetworkIPAddress.Get(),
				m_pConfig->NetworkSubnetMask.Get(),
				m_pConfig->NetworkDefaultGateway.Get(),
				m_pConfig->NetworkDNSServer.Get(),
				m_pConfig->NetworkHostname,
				NetDeviceType
			);

		if (!m_pNet->Initialize(false))
		{
			m_pLogger->Write(MT32PiName, LogError, "Failed to initialize network subsystem");
			delete m_pNet;
			m_pNet = nullptr;
		}

		m_pNetDevice = CNetDevice::GetNetDevice(NetDeviceType);
	}

	return m_pNet != nullptr;
}

bool CMT32Pi::InitMT32Synth()
{
	assert(m_pMT32Synth == nullptr);

	m_pMT32Synth = new CMT32Synth(m_pConfig->AudioSampleRate, m_pConfig->MT32EmuGain, m_pConfig->MT32EmuReverbGain, m_pConfig->MT32EmuResamplerQuality);
	if (!m_pMT32Synth->Initialize())
	{
		m_pLogger->Write(MT32PiName, LogWarning, "mt32emu init failed; no ROMs present?");
		delete m_pMT32Synth;
		m_pMT32Synth = nullptr;
		return false;
	}

	// Set initial MT-32 channel assignment from config
	if (m_pConfig->MT32EmuMIDIChannels == CMT32Synth::TMIDIChannels::Alternate)
		m_pMT32Synth->SetMIDIChannels(m_pConfig->MT32EmuMIDIChannels);

	// Set MT-32 reversed stereo option from config
	m_pMT32Synth->SetReversedStereo(m_pConfig->MT32EmuReversedStereo);

	m_pMT32Synth->SetUserInterface(&m_UserInterface);

	return true;
}

bool CMT32Pi::InitSoundFontSynth()
{
	assert(m_pSoundFontSynth == nullptr);

	m_pSoundFontSynth = new CSoundFontSynth(m_pConfig->AudioSampleRate);
	if (!m_pSoundFontSynth->Initialize())
	{
		m_pLogger->Write(MT32PiName, LogWarning, "FluidSynth init failed; no SoundFonts present?");
		delete m_pSoundFontSynth;
		m_pSoundFontSynth = nullptr;
		return false;
	}

	m_pSoundFontSynth->SetUserInterface(&m_UserInterface);

	return true;
}

bool CMT32Pi::InitOPLSynth()
{
	assert(m_pOPLSynth == nullptr);

	m_pOPLSynth = new COPLSynth(m_pConfig->AudioSampleRate);
	if (!m_pOPLSynth->Initialize())
	{
		m_pLogger->Write(MT32PiName, LogWarning, "ADLMIDI init failed; no banks present?");
		delete m_pOPLSynth;
		m_pOPLSynth = nullptr;
		return false;
	}

	m_pOPLSynth->SetUserInterface(&m_UserInterface);

	return true;
}

bool CMT32Pi::InitOPNSynth()
{
	assert(m_pOPNSynth == nullptr);

	m_pOPNSynth = new COPNSynth(m_pConfig->AudioSampleRate);
	if (!m_pOPNSynth->Initialize())
	{
		m_pLogger->Write(MT32PiName, LogWarning, "OPNMIDI init failed; no banks present?");
		delete m_pOPNSynth;
		m_pOPNSynth = nullptr;
		return false;
	}

	m_pOPNSynth->SetUserInterface(&m_UserInterface);

	return true;
}

void CMT32Pi::MainTask()
{
	CScheduler* const pScheduler = CScheduler::Get();

	m_pLogger->Write(MT32PiName, LogNotice, "Main task on Core 0 starting up");

	Awaken();

	while (m_bRunning)
	{
		// Process MIDI data
		UpdateMIDI();

		// Process network packets
		UpdateNetwork();

		// Update controls
		if (m_pControl)
			m_pControl->Update();

		// Process events
		ProcessEventQueue();

		const unsigned int nTicks = m_pTimer->GetTicks();

		// Update activity LED
		if (m_bLEDOn && (nTicks - m_nLEDOnTime) >= MSEC2HZ(LEDTimeoutMillis))
		{
			m_pActLED->Off();
			m_bLEDOn = false;
		}

		// Check for active sensing timeout
		if (m_bActiveSenseFlag && (nTicks > m_nActiveSenseTime) && (nTicks - m_nActiveSenseTime) >= MSEC2HZ(ActiveSenseTimeoutMillis))
		{
			m_pCurrentSynth->AllSoundOff();
			m_bActiveSenseFlag = false;
			m_pLogger->Write(MT32PiName, LogNotice, "Active sense timeout - turning notes off");
		}

		// Update power management
		if (m_pCurrentSynth->IsActive())
			Awaken();

#ifdef MONITOR_TEMPERATURE
		if (nTicks - m_nTempUpdateTime >= MSEC2HZ(5000))
		{
			const unsigned int nTemp = CCPUThrottle::Get()->GetTemperature();
			m_pLogger->Write(MT32PiName, LogDebug, "Temperature: %dC", nTemp);
			LCDLog(TLCDLogType::Notice, "Temp: %dC", nTemp);
			m_nTempUpdateTime = nTicks;
		}
#endif

		CPower::Update();

		// Check for deferred SoundFont switch
		if (m_bDeferredSoundFontSwitchFlag)
		{
			// Delay switch if scrolling a long SoundFont name
			if (m_UserInterface.IsScrolling())
				m_nDeferredSoundFontSwitchTime = nTicks;
			else if ((nTicks - m_nDeferredSoundFontSwitchTime) >= static_cast<unsigned int>(m_pConfig->ControlSwitchTimeout) * HZ)
			{
				SwitchSoundFont(m_nDeferredSoundFontSwitchIndex);
				m_bDeferredSoundFontSwitchFlag = false;

				// Trigger an awaken so we don't immediately go to sleep
				Awaken();
			}
		}

		// Check for USB PnP events
		UpdateUSB();

		// Allow other tasks to run
		pScheduler->Yield();
	}


	// Stop audio
	m_pSound->Cancel();

	// Wait for UI task to finish
	while (!m_bUITaskDone)
		;
}

void CMT32Pi::UITask()
{
	m_pLogger->Write(MT32PiName, LogNotice, "UI task on Core 1 starting up");

	const bool bMisterEnabled = m_pConfig->ControlMister;

	// Nothing for this core to do; bail out
	if (!(m_pLCD || bMisterEnabled))
	{
		m_bUITaskDone = true;
		return;
	}

	// Display current MT-32 ROM version/SoundFont
	m_pCurrentSynth->ReportStatus();

	while (m_bRunning)
	{
		const unsigned int nTicks = CTimer::GetClockTicks();

		// Update LCD
		if (m_pLCD && (nTicks - m_nLCDUpdateTime) >= Utility::MillisToTicks(LCDUpdatePeriodMillis))
		{
			m_UserInterface.Update(*m_pLCD, *m_pCurrentSynth, nTicks);
			m_nLCDUpdateTime = nTicks;
		}

		// Poll MiSTer interface
		if (bMisterEnabled && (nTicks - m_nMisterUpdateTime) >= Utility::MillisToTicks(MisterUpdatePeriodMillis))
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
			m_nMisterUpdateTime = nTicks;
		}
	}

	// Clear screen
	if (m_pLCD)
		m_pLCD->Clear();

	m_bUITaskDone = true;
}

void CMT32Pi::AudioTask()
{
	m_pLogger->Write(MT32PiName, LogNotice, "Audio task on Core 2 starting up");

	constexpr u8 nChannels = 2;

	// Circle's "fast path" for I2S 24-bit really expects 32-bit samples
	const bool bI2S = m_pConfig->AudioOutputDevice == CConfig::TAudioOutputDevice::I2S;
	const bool bReversedStereo = m_pConfig->AudioReversedStereo;
	const u8 nBytesPerSample = bI2S ? sizeof(s32) : (sizeof(s8) * 3);
	const u8 nBytesPerFrame = 2 * nBytesPerSample;

	const size_t nQueueSizeFrames = m_pSound->GetQueueSizeFrames();

	// Extra byte so that we can write to the 24-bit buffer with overlapping 32-bit writes (efficiency)
	float FloatBuffer[nQueueSizeFrames * nChannels];
	s8 IntBuffer[nQueueSizeFrames * nBytesPerFrame + bI2S ? 0 : 1];

	while (m_bRunning)
	{
		const size_t nFrames = nQueueSizeFrames - m_pSound->GetQueueFramesAvail();
		const size_t nWriteBytes = nFrames * nBytesPerFrame;

		m_pCurrentSynth->Render(FloatBuffer, nFrames);

		if (bReversedStereo)
		{
			// Convert to signed 24-bit integers with channel swap
			for (size_t i = 0; i < nFrames * nChannels; i += nChannels)
			{
				s32* const pLeftSample = reinterpret_cast<s32*>(IntBuffer + i * nBytesPerSample);
				s32* const pRightSample = reinterpret_cast<s32*>(IntBuffer + (i + 1) * nBytesPerSample);
				*pLeftSample = FloatBuffer[i + 1] * Sample24BitMax;
				*pRightSample = FloatBuffer[i] * Sample24BitMax;
			}
		}
		else
		{
			// Convert to signed 24-bit integers
			for (size_t i = 0; i < nFrames * nChannels; ++i)
			{
				s32* const pSample = reinterpret_cast<s32*>(IntBuffer + i * nBytesPerSample);
				*pSample = FloatBuffer[i] * Sample24BitMax;
			}
		}

		const int nResult = m_pSound->Write(IntBuffer, nWriteBytes);
		if (nResult != static_cast<int>(nWriteBytes))
			m_pLogger->Write(MT32PiName, LogError, "Sound data dropped");
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
	m_UserInterface.EnterPowerSavingMode();
}

void CMT32Pi::OnExitPowerSavingMode()
{
	CPower::OnExitPowerSavingMode();
	m_pSound->Start();
	m_UserInterface.ExitPowerSavingMode();
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

	// Flash LED for channel messages
	if ((nMessage & 0xFF) < 0xF0)
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
	if (m_pConfig->SystemVerbose)
		LCDLog(TLCDLogType::Warning, "Unexp. MIDI status!");
}

void CMT32Pi::OnSysExOverflow()
{
	CMIDIParser::OnSysExOverflow();
	LCDLog(TLCDLogType::Error, "SysEx overflow!");
}

void CMT32Pi::OnAppleMIDIConnect(const CIPAddress* pIPAddress, const char* pName)
{
	if (!m_pLCD)
		return;

	LCDLog(TLCDLogType::Notice, "%s connected!", pName);
}

void CMT32Pi::OnAppleMIDIDisconnect(const CIPAddress* pIPAddress, const char* pName)
{
	if (!m_pLCD)
		return;

	LCDLog(TLCDLogType::Notice, "%s disconnected!", pName);
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
		m_pLogger->Write(MT32PiName, LogNotice, "Reboot command received");
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

		// Swap MT-32 stereo channels (F0 7D 04 xx F7)
		case TCustomSysExCommand::SetMT32ReversedStereo:
		{
			if (m_pMT32Synth)
				m_pMT32Synth->SetReversedStereo(nParameter);
			return true;
		}

		default:
			return false;
	}
}

void CMT32Pi::UpdateUSB(bool bStartup)
{
	if (!m_bUSBAvailable || !m_pUSBHCI->UpdatePlugAndPlay())
		return;

	Awaken();

	CUSBBulkOnlyMassStorageDevice* pUSBMassStorageDevice = static_cast<CUSBBulkOnlyMassStorageDevice*>(CDeviceNameService::Get()->GetDevice("umsd1", TRUE));

	if (!m_pUSBMassStorageDevice && pUSBMassStorageDevice)
	{
		// USB disk was attached
		m_pLogger->Write(MT32PiName, LogNotice, "USB mass storage device attached");

		if (f_mount(&m_USBFileSystem, "USB:", 1) != FR_OK)
			m_pLogger->Write(MT32PiName, LogError, "Failed to mount USB mass storage device");
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
		m_pLogger->Write(MT32PiName, LogNotice, "USB mass storage device removed");

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
		m_pUSBMIDIDevice->RegisterRemovedHandler(USBMIDIDeviceRemovedHandler, &m_pUSBMIDIDevice);
		m_pUSBMIDIDevice->RegisterPacketHandler(USBMIDIPacketHandler);
		m_pLogger->Write(MT32PiName, LogNotice, "Using USB MIDI interface");
		m_bSerialMIDIEnabled = false;
	}

	if (!m_pUSBSerialDevice && (m_pUSBSerialDevice = static_cast<CUSBSerialDevice*>(CDeviceNameService::Get()->GetDevice("utty1", FALSE))))
	{
		m_pUSBSerialDevice->SetBaudRate(m_pConfig->MIDIUSBSerialBaudRate);
		m_pUSBSerialDevice->RegisterRemovedHandler(USBMIDIDeviceRemovedHandler, &m_pUSBSerialDevice);
		m_pLogger->Write(MT32PiName, LogNotice, "Using USB serial interface");
		m_bSerialMIDIEnabled = false;
	}
}

void CMT32Pi::UpdateNetwork()
{
	if (!m_pNet)
		return;

	bool bNetIsRunning = m_pNet->IsRunning();
	if (m_pConfig->NetworkMode == CConfig::TNetworkMode::Ethernet)
		bNetIsRunning &= m_pNetDevice->IsLinkUp();
	else if (m_pConfig->NetworkMode == CConfig::TNetworkMode::WiFi)
		bNetIsRunning &= m_WPASupplicant.IsConnected();

	if (!m_bNetworkReady && bNetIsRunning)
	{
		m_bNetworkReady = true;

		CString IPString;
		m_pNet->GetConfig()->GetIPAddress()->Format(&IPString);

		m_pLogger->Write(MT32PiName, LogNotice, "Network up and running at: %s", static_cast<const char *>(IPString));
		LCDLog(TLCDLogType::Notice, "%s: %s", GetNetworkDeviceShortName(), static_cast<const char*>(IPString));

		if (m_pConfig->NetworkRTPMIDI && !m_pAppleMIDIParticipant)
		{
			m_pAppleMIDIParticipant = new CAppleMIDIParticipant(&m_Random, this);
			if (!m_pAppleMIDIParticipant->Initialize())
			{
				m_pLogger->Write(MT32PiName, LogError, "Failed to init AppleMIDI receiver");
				delete m_pAppleMIDIParticipant;
				m_pAppleMIDIParticipant = nullptr;
			}
			else
				m_pLogger->Write(MT32PiName, LogNotice, "AppleMIDI receiver initialized");
		}

		if (m_pConfig->NetworkUDPMIDI && !m_pUDPMIDIReceiver)
		{
			m_pUDPMIDIReceiver = new CUDPMIDIReceiver(this);
			if (!m_pUDPMIDIReceiver->Initialize())
			{
				m_pLogger->Write(MT32PiName, LogError, "Failed to init UDP MIDI receiver");
				delete m_pUDPMIDIReceiver;
				m_pUDPMIDIReceiver = nullptr;
			}
			else
				m_pLogger->Write(MT32PiName, LogNotice, "UDP MIDI receiver initialized");
		}

		if (m_pConfig->NetworkFTPServer && !m_pFTPDaemon)
		{
			m_pFTPDaemon = new CFTPDaemon(m_pConfig->NetworkFTPUsername, m_pConfig->NetworkFTPPassword);
			if (!m_pFTPDaemon->Initialize())
			{
				m_pLogger->Write(MT32PiName, LogError, "Failed to init FTP daemon");
				delete m_pFTPDaemon;
				m_pFTPDaemon = nullptr;
			}
			else
				m_pLogger->Write(MT32PiName, LogNotice, "FTP daemon initialized");
		}
	}
	else if (m_bNetworkReady && !bNetIsRunning)
	{
		m_bNetworkReady = false;
		m_pLogger->Write(MT32PiName, LogNotice, "Network disconnected.");
		LCDLog(TLCDLogType::Notice, "%s disconnected!", GetNetworkDeviceShortName());

	}
}

void CMT32Pi::UpdateMIDI()
{
	size_t nBytes;
	u8 Buffer[MIDIRxBufferSize];

	// Read MIDI messages from serial device or ring buffer
	if (m_bSerialMIDIEnabled)
		nBytes = ReceiveSerialMIDI(Buffer, sizeof(Buffer));
	else if (m_pUSBSerialDevice)
	{
		const int nResult = m_pUSBSerialDevice->Read(Buffer, sizeof(Buffer));
		nBytes = nResult > 0 ? static_cast<size_t>(nResult) : 0;
	}
	else
		nBytes = m_MIDIRxBuffer.Dequeue(Buffer, sizeof(Buffer));

	if (nBytes == 0)
		return;

	// Process MIDI messages
	ParseMIDIBytes(Buffer, nBytes);

	// Reset the Active Sense timer
	s_pThis->m_nActiveSenseTime = s_pThis->m_pTimer->GetTicks();
}

void CMT32Pi::PurgeMIDIBuffers()
{
	size_t nBytes;
	u8 Buffer[MIDIRxBufferSize];

	// Process MIDI messages from all devices/ring buffers, but ignore note-ons
	while (m_bSerialMIDIEnabled && (nBytes = ReceiveSerialMIDI(Buffer, sizeof(Buffer))) > 0)
		ParseMIDIBytes(Buffer, nBytes, true);

	while (m_pUSBSerialDevice && (nBytes = m_pUSBSerialDevice->Read(Buffer, sizeof(Buffer))) > 0)
		ParseMIDIBytes(Buffer, nBytes, true);

	while ((nBytes = m_MIDIRxBuffer.Dequeue(Buffer, sizeof(Buffer))) > 0)
		ParseMIDIBytes(Buffer, nBytes, true);
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
		if (m_pConfig->SystemVerbose)
		{
			const char* pErrorString;
			switch (nResult)
			{
				case -SERIAL_ERROR_BREAK:
					pErrorString = "UART break error!";
					break;

				case -SERIAL_ERROR_OVERRUN:
					pErrorString = "UART overrun error!";
					break;

				case -SERIAL_ERROR_FRAMING:
					pErrorString = "UART framing error!";
					break;

				default:
					pErrorString = "Unknown UART error!";
					break;
			}

			m_pLogger->Write(MT32PiName, LogWarning, pErrorString);
			LCDLog(TLCDLogType::Warning, pErrorString);
		}

		return 0;
	}

	// Replay received MIDI data out via the serial port ('software thru')
	if (m_pConfig->MIDIGPIOThru)
	{
		int nSendResult = m_pSerial->Write(pOutData, nResult);
		if (nSendResult != nResult)
		{
			m_pLogger->Write(MT32PiName, LogError, "received %d bytes, but only sent %d bytes", nResult, nSendResult);
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
				m_UserInterface.DisplayImage(Event.DisplayImage.Image);
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

	if (Event.Button == TButton::Button1 && !Event.bRepeat)
	{
		// Swap synths
		if (m_pCurrentSynth == m_pMT32Synth)
			SwitchSynth(TSynth::SoundFont);
		else if (m_pCurrentSynth == m_pSoundFontSynth)
			SwitchSynth(TSynth::OPL);
		else if (m_pCurrentSynth == m_pOPLSynth)
			SwitchSynth(TSynth::OPN);
		else
			SwitchSynth(TSynth::MT32);
	}
	else if (Event.Button == TButton::Button2 && !Event.bRepeat)
	{
		if (m_pCurrentSynth == m_pMT32Synth)
			NextMT32ROMSet();
		else if (m_pCurrentSynth == m_pSoundFontSynth)
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
	const char* pModeString;

	switch (NewSynth)
	{
		case TSynth::MT32:
			pNewSynth = m_pMT32Synth;
			pModeString = "MT-32 mode";
			break;

		case TSynth::SoundFont:
			pNewSynth = m_pSoundFontSynth;
			pModeString = "SoundFont mode";
			break;

		case TSynth::OPL:
			pNewSynth = m_pOPLSynth;
			pModeString = "OPL mode";
			break;

		case TSynth::OPN:
			pNewSynth = m_pOPNSynth;
			pModeString = "OPN mode";
			break;
	}

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
	m_pLogger->Write(MT32PiName, LogNotice, "Switching to %s", pModeString);
	LCDLog(TLCDLogType::Notice, pModeString);
}

void CMT32Pi::SwitchMT32ROMSet(TMT32ROMSet ROMSet)
{
	if (m_pMT32Synth == nullptr)
		return;

	m_pLogger->Write(MT32PiName, LogNotice, "Switching to ROM set %d", static_cast<u8>(ROMSet));
	if (m_pMT32Synth->SwitchROMSet(ROMSet) && m_pCurrentSynth == m_pMT32Synth)
		m_pMT32Synth->ReportStatus();
}

void CMT32Pi::NextMT32ROMSet()
{
	if (m_pMT32Synth == nullptr)
		return;

	m_pLogger->Write(MT32PiName, LogNotice, "Switching to next ROM set");

	if (m_pMT32Synth->NextROMSet() && m_pCurrentSynth == m_pMT32Synth)
		m_pMT32Synth->ReportStatus();
}

void CMT32Pi::SwitchSoundFont(size_t nIndex)
{
	if (m_pSoundFontSynth == nullptr)
		return;

	m_pLogger->Write(MT32PiName, LogNotice, "Switching to SoundFont %d", nIndex);
	if (m_pSoundFontSynth->SwitchSoundFont(nIndex))
	{
		// Handle any MIDI data that has been queued up while busy
		PurgeMIDIBuffers();

		if (m_pCurrentSynth == m_pSoundFontSynth)
			m_pSoundFontSynth->ReportStatus();
	}
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
	if (m_pOPLSynth)
		m_pOPLSynth->SetMasterVolume(m_nMasterVolume);
	if (m_pOPNSynth)
		m_pOPNSynth->SetMasterVolume(m_nMasterVolume);

	if (m_pCurrentSynth != m_pMT32Synth)
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

	char Buffer[256];

	va_list Args;
	va_start(Args, pFormat);
	vsnprintf(Buffer, sizeof(Buffer), pFormat, Args);
	va_end(Args);

	// LCD task hasn't started yet; print directly
	if (Type == TLCDLogType::Startup)
	{
		if (m_pLCD->GetType() == CLCD::TType::Graphical && !m_pConfig->SystemVerbose)
			return;

		u8 nOffsetX = CUserInterface::CenterMessageOffset(*m_pLCD, Buffer);
		m_pLCD->Print(Buffer, nOffsetX, 1, true, true);
	}

	// Let LCD task pick up the message in its next update
	else
		m_UserInterface.ShowSystemMessage(Buffer, Type == TLCDLogType::Spinner);
}

const char* CMT32Pi::GetNetworkDeviceShortName() const
{
	switch (m_pConfig->NetworkMode)
	{
		case CConfig::TNetworkMode::Ethernet:	return "Ether";
		case CConfig::TNetworkMode::WiFi:	return "WiFi";
		default:				return "None";
	}
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

	void** pDevicePointer = reinterpret_cast<void**>(pContext);
	*pDevicePointer = nullptr;

	// Re-enable serial MIDI if not in-use by logger and no other MIDI devices available
	if (s_pThis->m_bSerialMIDIAvailable && !(s_pThis->m_pUSBMIDIDevice || s_pThis->m_pUSBSerialDevice || s_pThis->m_pPisound))
	{
		s_pThis->m_pLogger->Write(MT32PiName, LogNotice, "Using serial MIDI interface");
		s_pThis->m_bSerialMIDIEnabled = true;
	}
}

// The following handlers are called from interrupt context, enqueue into ring buffer for main thread
void CMT32Pi::USBMIDIPacketHandler(unsigned nCable, u8* pPacket, unsigned nLength)
{
	IRQMIDIReceiveHandler(pPacket, nLength);
}

void CMT32Pi::IRQMIDIReceiveHandler(const u8* pData, size_t nSize)
{
	assert(s_pThis != nullptr);

	// Enqueue data into ring buffer
	if (s_pThis->m_MIDIRxBuffer.Enqueue(pData, nSize) != nSize)
	{
		static const char* pErrorString = "MIDI overrun error!";
		s_pThis->m_pLogger->Write(MT32PiName, LogWarning, pErrorString);
		s_pThis->LCDLog(TLCDLogType::Error, pErrorString);
	}
}

void CMT32Pi::PanicHandler()
{
	if (!s_pThis || !s_pThis->m_pLCD)
		return;

	// Kill UI task
	s_pThis->m_bRunning = false;
	while (!s_pThis->m_bUITaskDone)
		;

	const char* pGuru = "Guru Meditation:";
	u8 nOffsetX = CUserInterface::CenterMessageOffset(*s_pThis->m_pLCD, pGuru);
	s_pThis->m_pLCD->Clear(true);
	s_pThis->m_pLCD->Print(pGuru, nOffsetX, 0, true, true);

	char Buffer[LOGGER_BUFSIZE];
	s_pThis->m_pLogger->Read(Buffer, sizeof(Buffer), false);

	// Find last newline
	char* pMessageStart = strrchr(Buffer, '\n');
	if (!pMessageStart)
		return;
	*pMessageStart = '\0';

	// Find second-last newline
	pMessageStart = strrchr(Buffer, '\n');
	if (!pMessageStart)
		return;

	// Skip past timestamp and log source, kill color control characters
	pMessageStart = strstr(pMessageStart, ": ") + 2;
	char* pMessageEnd = strstr(pMessageStart, "\x1B[0m");
	*pMessageEnd = '\0';

	const size_t nMessageLength = strlen(pMessageStart);
	size_t nCurrentScrollOffset = 0;
	const char* pGuruFlash = pGuru;
	bool bFlash = false;

	unsigned int nTicks = CTimer::GetClockTicks();
	unsigned int nPanicStart = nTicks;
	unsigned int nFlashTime = nTicks;
	unsigned int nScrollTime = nTicks;

	const u8 nWidth = s_pThis->m_pLCD->Width();
	const u8 nHeight = s_pThis->m_pLCD->Height();

	// TODO: API for getting width in pixels/characters for a string
	const bool bGraphical = s_pThis->m_pLCD->GetType() == CLCD::TType::Graphical;
	const size_t nCharWidth = bGraphical ? 20 : nWidth;

	while (true)
	{
		s_pThis->m_pLCD->Clear(false);
		nTicks = CTimer::GetClockTicks();

		if (Utility::TicksToMillis(nTicks - nFlashTime) > 1000)
		{
			bFlash = !bFlash;
			nFlashTime = nTicks;
		}

		if (nMessageLength > nCharWidth)
		{
			if (nMessageLength - nCurrentScrollOffset > nCharWidth)
			{
				const unsigned int nTimeout = nCurrentScrollOffset == 0 ? 1500 : 175;
				if (Utility::TicksToMillis(nTicks - nScrollTime) >= nTimeout)
				{
					++nCurrentScrollOffset;
					nScrollTime = nTicks;
				}
			}
			else if (Utility::TicksToMillis(nTicks - nScrollTime) >= 3000)
			{
				nCurrentScrollOffset = 0;
				nScrollTime = nTicks;
			}
		}

		if (Utility::TicksToMillis(nTicks - nPanicStart) > 2 * 60 * 1000)
			break;

		if (!bGraphical)
			pGuruFlash = bFlash ? "" : pGuru;

		u8 nOffsetX = CUserInterface::CenterMessageOffset(*s_pThis->m_pLCD, pGuruFlash);
		s_pThis->m_pLCD->Print(pGuruFlash, nOffsetX, 0, true, false);
		s_pThis->m_pLCD->Print(pMessageStart + nCurrentScrollOffset, 0, 1, true, false);

		if (bGraphical && bFlash)
		{
			s_pThis->m_pLCD->DrawFilledRect(0, 0, nWidth - 1, 1);
			s_pThis->m_pLCD->DrawFilledRect(0, nHeight - 1, nWidth - 1, nHeight - 2);
			s_pThis->m_pLCD->DrawFilledRect(0, 0, 1, nHeight - 1);
			s_pThis->m_pLCD->DrawFilledRect(nWidth - 1, 0, nWidth - 2, nHeight - 1);
		}

		s_pThis->m_pLCD->Flip();
	}

	s_pThis->m_pLCD->Clear(true);
	const char* pMessage = "System halted";
	nOffsetX = CUserInterface::CenterMessageOffset(*s_pThis->m_pLCD, pMessage);
	s_pThis->m_pLCD->Print(pMessage, nOffsetX, 0, true, true);
	pMessage = "Please reboot";
	nOffsetX = CUserInterface::CenterMessageOffset(*s_pThis->m_pLCD, pMessage);
	s_pThis->m_pLCD->Print(pMessage, nOffsetX, 1, true, true);
}
