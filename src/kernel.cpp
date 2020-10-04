//
// kernel.cpp
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

#include "kernel.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <circle/usb/usbmidi.h>
#include <circle/startup.h>

#include "lcd/hd44780.h"
#include "lcd/ssd1306.h"

#ifndef MT32_PI_VERSION
#define MT32_PI_VERSION "<unknown>"
#endif

#define LCD_UPDATE_PERIOD_MILLIS 16
#define LCD_LOG_TIME_MILLIS 2000
#define LED_TIMEOUT_MILLIS 50
#define ACTIVE_SENSE_TIMEOUT_MILLIS 330

CKernel* CKernel::s_pThis = nullptr;

CKernel::CKernel(void)
	: CStdlibApp("mt32-pi"),
	  CMIDIParser(),

	  m_Serial(&mInterrupt, true),
#ifdef HDMI_CONSOLE
	  m_Screen(mOptions.GetWidth(), mOptions.GetHeight()),
#endif

	  m_Timer(&mInterrupt),
	  m_Logger(mOptions.GetLogLevel(), &m_Timer),
	  m_USBHCI(&mInterrupt, &m_Timer),
	  m_EMMC(&mInterrupt, &m_Timer, &mActLED),
	  m_FileSystem{},

	  m_I2CMaster(1, true),
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

bool CKernel::Initialize(void)
{
	if (!CStdlibApp::Initialize())
		return false;

#ifdef HDMI_CONSOLE
	if (!m_Screen.Initialize())
		return false;
#endif

	CDevice* pLogTarget = mDeviceNameService.GetDevice(mOptions.GetLogDevice(), false);

	if (!pLogTarget)
		pLogTarget = &m_Null;

	// Init serial port early if used for logging
	m_bSerialMIDIEnabled = pLogTarget != &m_Serial;
	if (!m_bSerialMIDIEnabled && !m_Serial.Initialize(115200))
		return false;

	if (!m_Logger.Initialize(pLogTarget))
		return false;

	if (!m_Timer.Initialize())
		return false;

	if (!m_EMMC.Initialize())
		return false;

	char const* partitionName = "SD:";

	if (f_mount(&m_FileSystem, partitionName, 1) != FR_OK)
	{
		m_Logger.Write(GetKernelName(), LogError, "Cannot mount partition: %s", partitionName);
		return false;
	}

	// Initialize newlib stdio with a reference to Circle's file system
	CGlueStdioInit(m_FileSystem);

	if (!m_Config.Initialize("mt32-pi.cfg"))
		m_Logger.Write(GetKernelName(), LogWarning, "Unable to find or parse config file; using defaults");

	// Init serial port for GPIO MIDI if not being used for logging
	if (m_bSerialMIDIEnabled && !m_Serial.Initialize(m_Config.m_nMIDIGPIOBaudRate))
		return false;

	if (!m_I2CMaster.Initialize())
		return false;

	if (m_Config.m_LCDType == CConfig::TLCDType::HD44780FourBit)
		m_pLCD = new CHD44780FourBit(m_Config.m_nLCDWidth, m_Config.m_nLCDHeight);
	else if (m_Config.m_LCDType == CConfig::TLCDType::HD44780I2C)
		m_pLCD = new CHD44780I2C(&m_I2CMaster, m_Config.m_nLCDI2CLCDAddress, m_Config.m_nLCDWidth, m_Config.m_nLCDHeight);
	else if (m_Config.m_LCDType == CConfig::TLCDType::SSD1306I2C)
		m_pLCD = new CSSD1306(&m_I2CMaster, m_Config.m_nLCDI2CLCDAddress, m_Config.m_nLCDHeight);

	if (m_pLCD)
	{
		if (m_pLCD->Initialize())
			m_pLCD->Print("mt32-pi " MT32_PI_VERSION, 0, 0, false, true);
		else
		{
			m_Logger.Write(GetKernelName(), LogWarning, "LCD init failed; invalid dimensions?");
			delete m_pLCD;
			m_pLCD = nullptr;
		}
	}

#if !defined(__aarch64__) || !defined(LEAVE_QEMU_ON_HALT)
	// The USB driver is not supported under 64-bit QEMU, so
	// the initialization must be skipped in this case, or an
	// exit happens here under 64-bit QEMU.
	LCDLog("Init USB");
	if (m_Config.m_bMIDIUSB && !m_USBHCI.Initialize())
		return false;
#endif

	if (m_Config.m_AudioOutputDevice == CConfig::TAudioOutputDevice::I2SDAC)
	{
		LCDLog("Init mt32emu (I2S)");
		if (m_Config.m_AudioI2CDACInit == CConfig::TAudioI2CDACInit::PCM51xx)
			InitPCM51xx(m_Config.m_nAudioI2CDACAddress);

		m_pSynth = new CMT32SynthI2S(m_FileSystem, &mInterrupt, m_Config.m_nAudioSampleRate, m_Config.m_MT32EmuResamplerQuality, m_Config.m_nAudioChunkSize);
	}
	else
	{
		LCDLog("Init mt32emu (PWM)");
		m_pSynth = new CMT32SynthPWM(m_FileSystem, &mInterrupt, m_Config.m_nAudioSampleRate, m_Config.m_MT32EmuResamplerQuality, m_Config.m_nAudioChunkSize);
	}

	if (!m_pSynth->Initialize())
		return false;

	// Set initial channel assignment from config
	if (m_Config.m_MT32EmuMIDIChannels == CMT32SynthBase::TMIDIChannels::Alternate)
		m_pSynth->SetMIDIChannels(m_Config.m_MT32EmuMIDIChannels);

	m_Logger.Write(GetKernelName(), LogNotice, "mt32-pi " MT32_PI_VERSION);
	m_Logger.Write(GetKernelName(), LogNotice, "Compile time: " __DATE__ " " __TIME__);
	CCPUThrottle::Get()->DumpStatus();

	return true;
}

// TODO: Generic configurable DAC init class
bool CKernel::InitPCM51xx(u8 nAddress)
{
	unsigned char initBytes[][2] =
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
		if (m_I2CMaster.Write(nAddress, &command, sizeof(command)) != sizeof(command))
		{
			m_Logger.Write(GetKernelName(), LogWarning, "I2C write error (DAC init sequence)");
			return false;
		}
	}

	return true;
}

CStdlibApp::TShutdownMode CKernel::Run(void)
{
	CUSBMIDIDevice* pMIDIDevice = static_cast<CUSBMIDIDevice*>(CDeviceNameService::Get()->GetDevice("umidi1", FALSE));
	if (pMIDIDevice)
	{
		pMIDIDevice->RegisterPacketHandler(USBMIDIPacketHandler);
		m_Logger.Write(GetKernelName(), LogNotice, "Using USB MIDI interface");
		m_bSerialMIDIEnabled = false;
	}
	else
	{
		if (m_bSerialMIDIEnabled)
			m_Logger.Write(GetKernelName(), LogNotice, "Using serial MIDI interface");
		else
		{
			m_Logger.Write(GetKernelName(), LogError, "No USB MIDI device detected and serial port in use - please restart.");
			return ShutdownHalt;
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

		unsigned ticks = m_Timer.GetTicks();

		// Check for active sensing timeout
		if (m_bActiveSenseFlag && (ticks > m_nActiveSenseTime) && (ticks - m_nActiveSenseTime) >= MSEC2HZ(ACTIVE_SENSE_TIMEOUT_MILLIS))
		{
			m_pSynth->AllSoundOff();
			m_bActiveSenseFlag = false;
			m_Logger.Write(GetKernelName(), LogNotice, "Active sense timeout - turning notes off");
		}

		// Update activity LED
		if (m_bLEDOn && (ticks - m_nLEDOnTime) >= MSEC2HZ(LED_TIMEOUT_MILLIS))
		{
			mActLED.Off();
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

			return ShutdownReboot;
		}
	}

	return ShutdownHalt;
}

void CKernel::OnShortMessage(u32 nMessage)
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

void CKernel::OnSysExMessage(const u8* pData, size_t nSize)
{
	// Flash LED
	LEDOn();

	// If we don't consume the SysEx message, forward it to mt32emu
	if (!ParseCustomSysEx(pData, nSize))
		m_pSynth->HandleMIDISysExMessage(pData, nSize);
}

void CKernel::OnUnexpectedStatus()
{
	CMIDIParser::OnUnexpectedStatus();
	LCDLog("Unexp. MIDI status!");
}

void CKernel::OnSysExOverflow()
{
	CMIDIParser::OnSysExOverflow();
	LCDLog("SysEx overflow!");
}

bool CKernel::ParseCustomSysEx(const u8* pData, size_t nSize)
{
	if (nSize < 4)
		return false;

	// 'Educational' manufacturer
	if (pData[1] != 0x7D)
		return false;

	// Reboot (F0 7D 00 F7)
	if (pData[2] == 0x00)
	{
		m_Logger.Write(GetKernelName(), LogNotice, "Reboot command received");
		m_bShouldReboot = true;
		return true;
	}

	// Switch ROM set (F0 7D 01 xx F7)
	else if (pData[2] == 0x01 && nSize == 5)
	{
		u8 romSet = pData[3];
		if (romSet > 2)
			return false;

		m_Logger.Write(GetKernelName(), LogNotice, "Switching to ROM set %d", romSet);
		return m_pSynth->SwitchROMSet(static_cast<CROMManager::TROMSet>(romSet));
	}

	return false;
}

void CKernel::UpdateSerialMIDI()
{
	// Read serial MIDI data
	u8 buffer[SERIAL_BUF_SIZE];
	int nResult = m_Serial.Read(buffer, sizeof(buffer));

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

		m_Logger.Write("serialmidi", LogWarning, errorString);
		LCDLog(errorString);
		return;
	}

	// Replay received MIDI data out via the serial port ('software thru')
	if (m_Config.m_bMIDIGPIOThru)
	{
		int nSendResult = m_Serial.Write(buffer, nResult);
		if (nSendResult != nResult)
		{
			m_Logger.Write("serialmidi", LogWarning, "received %d bytes, but only sent %d bytes", nResult, nSendResult);
			LCDLog("serial TX error");
		}
	}

	// Reset the Active Sense timer
	m_nActiveSenseTime = m_Timer.GetTicks();

	// Process MIDI messages
	ParseMIDIBytes(buffer, nResult);
}

void CKernel::LEDOn()
{
	mActLED.On();
	m_nLEDOnTime = m_Timer.GetTicks();
	m_bLEDOn = true;
}

void CKernel::LCDLog(const char* pMessage)
{
	if (!m_pLCD)
		return;

	m_nLCDLogTime = m_Timer.GetTicks();

	m_pLCD->Print("~", 0, 1);
	m_pLCD->Print(pMessage, 2, 1, true, true);
}

void CKernel::USBMIDIPacketHandler(unsigned nCable, u8* pPacket, unsigned nLength)
{
	assert(s_pThis != nullptr);

	// Reset the Active Sense timer
	s_pThis->m_nActiveSenseTime = s_pThis->m_Timer.GetTicks();

	// Process MIDI messages
	s_pThis->ParseMIDIBytes(pPacket, nLength);
}
