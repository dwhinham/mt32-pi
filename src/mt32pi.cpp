//
// mt32pi.cpp
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

#include <circle/memory.h>
#include <circle/serial.h>
#include <circle/usb/usbmidi.h>

#include "lcd/hd44780.h"
#include "lcd/ssd1306.h"
#include "mt32pi.h"

const char MT32PiName[] = "mt32-pi";

#define LCD_UPDATE_PERIOD_MILLIS 16
#define LCD_LOG_TIME_MILLIS 2000
#define LED_TIMEOUT_MILLIS 50
#define ACTIVE_SENSE_TIMEOUT_MILLIS 330

CMT32Pi* CMT32Pi::s_pThis = nullptr;

CMT32Pi::CMT32Pi(CI2CMaster* pI2CMaster, CInterruptSystem* pInterrupt, CSerialDevice* pSerialDevice, CUSBHCIDevice* pUSBHCI, FATFS* pFilesystem)
	: CMultiCoreSupport(CMemorySystem::Get()),
	  CMIDIParser(),

	  m_pConfig(CConfig::Get()),
	  m_pLogger(CLogger::Get()),
	  m_pTimer(CTimer::Get()),
	  m_pActLED(CActLED::Get()),

	  m_pFileSystem(pFilesystem),
	  m_pI2CMaster(pI2CMaster),
	  m_pInterrupt(pInterrupt),
	  m_pSerial(pSerialDevice),
	  m_pUSBHCI(pUSBHCI),

	  m_pLCD(nullptr),
	  m_nLCDLogTime(0),
	  m_nLCDUpdateTime(0),

	  m_bSerialMIDIEnabled(false),

	  m_bActiveSenseFlag(false),
	  m_nActiveSenseTime(0),

	  m_bShouldReboot(false),
	  m_bLEDOn(false),
	  m_nLEDOnTime(0),

	  m_pSynth(nullptr)
{
	s_pThis = this;
}

CMT32Pi::~CMT32Pi()
{
}

bool CMT32Pi::Initialize(bool bSerialMIDIEnabled)
{
	CConfig& config = *m_pConfig;
	CLogger& logger = *m_pLogger;

	if (!CMultiCoreSupport::Initialize())
		return false;

	m_bSerialMIDIEnabled = bSerialMIDIEnabled;

	if (config.m_LCDType == CConfig::TLCDType::HD44780FourBit)
		m_pLCD = new CHD44780FourBit(config.m_nLCDWidth, config.m_nLCDHeight);
	else if (config.m_LCDType == CConfig::TLCDType::HD44780I2C)
		m_pLCD = new CHD44780I2C(m_pI2CMaster, config.m_nLCDI2CLCDAddress, config.m_nLCDWidth, config.m_nLCDHeight);
	else if (config.m_LCDType == CConfig::TLCDType::SSD1306I2C)
		m_pLCD = new CSSD1306(m_pI2CMaster, config.m_nLCDI2CLCDAddress, config.m_nLCDHeight, config.m_nLCDRotation);

	if (m_pLCD)
	{
		if (m_pLCD->Initialize())
			m_pLCD->Print("mt32-pi " MT32_PI_VERSION, 0, 0, false, true);
		else
		{
			logger.Write(MT32PiName, LogWarning, "LCD init failed; invalid dimensions?");
			delete m_pLCD;
			m_pLCD = nullptr;
		}
	}

#if !defined(__aarch64__) || !defined(LEAVE_QEMU_ON_HALT)
	// The USB driver is not supported under 64-bit QEMU, so
	// the initialization must be skipped in this case, or an
	// exit happens here under 64-bit QEMU.
	LCDLog("Init USB");
	if (config.m_bMIDIUSB && !m_pUSBHCI->Initialize())
		return false;
#endif

	if (config.m_AudioOutputDevice == CConfig::TAudioOutputDevice::I2SDAC)
	{
		LCDLog("Init mt32emu (I2S)");
		if (config.m_AudioI2CDACInit == CConfig::TAudioI2CDACInit::PCM51xx)
			InitPCM51xx(config.m_nAudioI2CDACAddress);

		m_pSynth = new CMT32SynthI2S(m_pInterrupt, config.m_nAudioSampleRate, config.m_MT32EmuResamplerQuality, config.m_nAudioChunkSize);
	}
	else
	{
		LCDLog("Init mt32emu (PWM)");
		m_pSynth = new CMT32SynthPWM(m_pInterrupt, config.m_nAudioSampleRate, config.m_MT32EmuResamplerQuality, config.m_nAudioChunkSize);
	}

	if (!m_pSynth->Initialize())
		return false;

	// Set initial channel assignment from config
	if (config.m_MT32EmuMIDIChannels == CMT32SynthBase::TMIDIChannels::Alternate)
		m_pSynth->SetMIDIChannels(config.m_MT32EmuMIDIChannels);

	logger.Write(MT32PiName, LogNotice, "mt32-pi " MT32_PI_VERSION);
	logger.Write(MT32PiName, LogNotice, "Compile time: " __DATE__ " " __TIME__);
	CCPUThrottle::Get()->DumpStatus();

	return true;
}

void CMT32Pi::Run(unsigned nCore)
{
	CLogger& logger = *m_pLogger;

	if (nCore > 0)
	{
		logger.Write(MT32PiName, LogNotice, "Core %d standing by", nCore);
		while (true)
			;
		return;
	}

	CUSBMIDIDevice* pMIDIDevice = static_cast<CUSBMIDIDevice*>(CDeviceNameService::Get()->GetDevice("umidi1", FALSE));
	if (pMIDIDevice)
	{
		pMIDIDevice->RegisterPacketHandler(USBMIDIPacketHandler);
		logger.Write(MT32PiName, LogNotice, "Using USB MIDI interface");
		m_bSerialMIDIEnabled = false;
	}
	else
	{
		if (m_bSerialMIDIEnabled)
			logger.Write(MT32PiName, LogNotice, "Using serial MIDI interface");
		else
		{
			logger.Write(MT32PiName, LogError, "No USB MIDI device detected and serial port in use - please restart.");
			return;
		}
	}

	// Attach LCD to MT32 synth
	if (m_pLCD)
		m_pSynth->SetLCD(m_pLCD);

	// Start audio
	m_pSynth->Start();
	LCDLog("Ready.");

	while (true)
	{
		// Update serial GPIO MIDI
		if (m_bSerialMIDIEnabled)
			UpdateSerialMIDI();

		unsigned ticks = m_pTimer->GetTicks();

		// Check for active sensing timeout
		if (m_bActiveSenseFlag && (ticks > m_nActiveSenseTime) && (ticks - m_nActiveSenseTime) >= MSEC2HZ(ACTIVE_SENSE_TIMEOUT_MILLIS))
		{
			m_pSynth->AllSoundOff();
			m_bActiveSenseFlag = false;
			logger.Write(MT32PiName, LogNotice, "Active sense timeout - turning notes off");
		}

		// Update activity LED
		if (m_bLEDOn && (ticks - m_nLEDOnTime) >= MSEC2HZ(LED_TIMEOUT_MILLIS))
		{
			m_pActLED->Off();
			m_bLEDOn = false;
		}

		// Update LCD
		if (m_pLCD)
		{
			if (m_nLCDLogTime)
			{
				if ((ticks - m_nLCDLogTime) >= MSEC2HZ(LCD_LOG_TIME_MILLIS))
				{
					m_nLCDLogTime = 0;
					m_pLCD->Clear();
				}
			}
			else if ((ticks - m_nLCDUpdateTime) >= MSEC2HZ(LCD_UPDATE_PERIOD_MILLIS))
			{
				m_pLCD->Update(*m_pSynth);
				m_nLCDUpdateTime = ticks;
			}
		}

		if (m_bShouldReboot)
		{
			// Stop audio and reboot
			m_pSynth->Cancel();

			// Clear screen
			if (m_pLCD)
				m_pLCD->Clear();

			// FIXME: needs to tell kernel what to do
			return;// ShutdownReboot;
		}
	}
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

	m_pSynth->HandleMIDIShortMessage(nMessage);
}

void CMT32Pi::OnSysExMessage(const u8* pData, size_t nSize)
{
	// Flash LED
	LEDOn();

	// If we don't consume the SysEx message, forward it to mt32emu
	if (!ParseCustomSysEx(pData, nSize))
		m_pSynth->HandleMIDISysExMessage(pData, nSize);
}

void CMT32Pi::OnUnexpectedStatus()
{
	CMIDIParser::OnUnexpectedStatus();
	LCDLog("Unexp. MIDI status!");
}

void CMT32Pi::OnSysExOverflow()
{
	CMIDIParser::OnSysExOverflow();
	LCDLog("SysEx overflow!");
}

bool CMT32Pi::ParseCustomSysEx(const u8* pData, size_t nSize)
{
	if (nSize < 4)
		return false;

	// 'Educational' manufacturer
	if (pData[1] != 0x7D)
		return false;

	// Reboot (F0 7D 00 F7)
	if (pData[2] == 0x00)
	{
		m_pLogger->Write(MT32PiName, LogNotice, "Reboot command received");
		m_bShouldReboot = true;
		return true;
	}

	// Switch ROM set (F0 7D 01 xx F7)
	else if (pData[2] == 0x01 && nSize == 5)
	{
		u8 romSet = pData[3];
		if (romSet > 2)
			return false;

		m_pLogger->Write(MT32PiName, LogNotice, "Switching to ROM set %d", romSet);
		return m_pSynth->SwitchROMSet(static_cast<CROMManager::TROMSet>(romSet));
	}

	return false;
}

void CMT32Pi::UpdateSerialMIDI()
{
	// Read serial MIDI data
	u8 buffer[SERIAL_BUF_SIZE];
	int nResult = m_pSerial->Read(buffer, sizeof(buffer));

	// No data
	if (nResult == 0)
		return;

	// Error
	if (nResult < 0)
	{
		const char* errorString;
		switch (nResult)
		{
			case -SERIAL_ERROR_BREAK:
				errorString = "break error";
				break;

			case -SERIAL_ERROR_OVERRUN:
				errorString = "overrun error";
				break;

			case -SERIAL_ERROR_FRAMING:
				errorString = "framing error";
				break;

			default:
				errorString = "unknown error";
				break;
		}

		m_pLogger->Write("serialmidi", LogWarning, errorString);
		LCDLog(errorString);
		return;
	}

	// Replay received MIDI data out via the serial port ('software thru')
	if (m_pConfig->m_bMIDIGPIOThru)
	{
		int nSendResult = m_pSerial->Write(buffer, nResult);
		if (nSendResult != nResult)
		{
			m_pLogger->Write("serialmidi", LogWarning, "received %d bytes, but only sent %d bytes", nResult, nSendResult);
			LCDLog("serial TX error");
		}
	}

	// Reset the Active Sense timer
	m_nActiveSenseTime = m_pTimer->GetTicks();

	// Process MIDI messages
	ParseMIDIBytes(buffer, nResult);
}

void CMT32Pi::LEDOn()
{
	m_pActLED->On();
	m_nLEDOnTime = m_pTimer->GetTicks();
	m_bLEDOn = true;
}

void CMT32Pi::LCDLog(const char* pMessage)
{
	if (!m_pLCD)
		return;

	m_nLCDLogTime = m_pTimer->GetTicks();

	m_pLCD->Print("~", 0, 1);
	m_pLCD->Print(pMessage, 2, 1, true, true);
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
			m_pLogger->Write(MT32PiName, LogWarning, "I2C write error (DAC init sequence)");
			return false;
		}
	}

	return true;
}

void CMT32Pi::USBMIDIPacketHandler(unsigned nCable, u8* pPacket, unsigned nLength)
{
	assert(s_pThis != nullptr);

	// Reset the Active Sense timer
	s_pThis->m_nActiveSenseTime = s_pThis->m_pTimer->GetTicks();

	// Process MIDI messages
	s_pThis->ParseMIDIBytes(pPacket, nLength);
}
