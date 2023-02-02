//
// kernel.cpp
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

#include "config.h"
#include "kernel.h"

#ifndef MT32_PI_VERSION
#define MT32_PI_VERSION "<unknown>"
#endif

CKernel::CKernel(void)
	: CStdlibApp("mt32-pi"),

	  m_Serial(&mInterrupt, true),
#ifdef HDMI_CONSOLE
	  m_Screen(mOptions.GetWidth(), mOptions.GetHeight()),
#endif

	  m_Timer(&mInterrupt),
	  m_Logger(mOptions.GetLogLevel(), &m_Timer),
	  m_USBHCI(&mInterrupt, &m_Timer, true),
	  m_EMMC(&mInterrupt, &m_Timer, &mActLED),
	  m_SDFileSystem{},

	  m_I2CMaster(1, true),
	  m_SPIMaster(&mInterrupt),
	  m_GPIOManager(&mInterrupt),

	  m_MT32Pi(&m_I2CMaster, &m_SPIMaster, &mInterrupt, &m_GPIOManager, &m_Serial, &m_USBHCI)
{
}

bool CKernel::Initialize(void)
{
	if (!CStdlibApp::Initialize())
		return false;

#ifdef HDMI_CONSOLE
	if (!m_Screen.Initialize())
		return false;
#endif

	const char* pLogDeviceName = mOptions.GetLogDevice();
	const bool bSerialMIDIAvailable = strcmp(pLogDeviceName, "ttyS1") != 0;

	// Init serial port early if used for logging
	if (!bSerialMIDIAvailable && !m_Serial.Initialize(115200))
		return false;

	CDevice* pLogTarget = mDeviceNameService.GetDevice(pLogDeviceName, false);

	if (!pLogTarget)
		pLogTarget = &mNullDevice;

	if (!m_Logger.Initialize(pLogTarget))
		return false;

	if (!m_Timer.Initialize())
		return false;

	if (!m_EMMC.Initialize())
		return false;

	if (f_mount(&m_SDFileSystem, "SD:", 1) != FR_OK)
	{
		m_Logger.Write(GetKernelName(), LogError, "Failed to mount SD card");
		return false;
	}

	// Load configuration file
	if (!m_Config.Initialize("mt32-pi.cfg"))
		m_Logger.Write(GetKernelName(), LogWarning, "Unable to find or parse config file; using defaults");

	// Init serial port for MIDI with preferred baud rate if not used for logging
	if (bSerialMIDIAvailable && !m_Serial.Initialize(m_Config.MIDIGPIOBaudRate))
		return false;

	// Init I2C; don't bother with Initialize() as it only sets the clock to 100/400KHz
	m_I2CMaster.SetClock(m_Config.SystemI2CBaudRate);

	// Init SPI
	if (!m_SPIMaster.Initialize())
		return false;

	// Init GPIO manager
	if (!m_GPIOManager.Initialize())
		return false;

	// Init custom memory allocator
	if (!m_Allocator.Initialize())
		return false;

	if (!m_MT32Pi.Initialize(bSerialMIDIAvailable))
		return false;

	return true;
}

CStdlibApp::TShutdownMode CKernel::Run(void)
{
	m_Logger.Write(GetKernelName(), LogNotice, "mt32-pi " MT32_PI_VERSION);
	m_Logger.Write(GetKernelName(), LogNotice, "Compile time: " __DATE__ " " __TIME__);

	m_MT32Pi.Run(0);

	return ShutdownReboot;
}
