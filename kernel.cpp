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
#include <math.h>
#include <stdlib.h>
#include <time.h>

#include <circle/usb/usbmidi.h>
#include <circle/startup.h>

#define LED_TIMEOUT_MILLIS 50
#define ACTIVE_SENSE_TIMEOUT_MILLIS 300

#define SAMPLE_RATE 96000
#define CHUNK_SIZE 512				// Min = 32 for I2S

#define I2C_MASTER_DEVICE	1		// 0 on Raspberry Pi 1 Rev. 1 boards, 1 otherwise
#define I2C_MASTER_CONFIG	0		// 0 or 1 on Raspberry Pi 4, 0 otherwise
#define I2C_FAST_MODE		TRUE	// standard mode (100 Kbps) or fast mode (400 Kbps)

#define I2C_DAC_PCM5242		0
#define I2C_DAC_ADDRESS		0x4C	// standard mode (100 Kbps) or fast mode (400 Kbps)

CKernel *CKernel::pThis = nullptr;

CKernel::CKernel(void)
	: CStdlibApp("mt32"),

#ifdef HDMI_CONSOLE
	  mScreen(mOptions.GetWidth(), mOptions.GetHeight()),
	  mConsole(&mScreen),
#else
	  mSerial(&mInterrupt, true),
	  mConsole(&mSerial),
#endif

	  mTimer(&mInterrupt),
	  mLogger(mOptions.GetLogLevel(), &mTimer),
	  mUSBHCI(&mInterrupt, &mTimer),
#ifndef BAKE_MT32_ROMS
	  mEMMC(&mInterrupt, &mTimer, &mActLED),
#endif

	  mI2CMaster(I2C_MASTER_DEVICE, I2C_FAST_MODE, I2C_MASTER_CONFIG),

	  mSerialState(0),
	  mSerialMessage{0},
	  mActiveSenseFlag(false),
	  mActiveSenseTime(0),

	  mShouldReboot(false),
	  mLEDOn(false),
	  mLEDOnTime(0),

	  mSynth(nullptr)
{
	pThis = this;
}

bool CKernel::Initialize(void)
{
	if (!CStdlibApp::Initialize())
	{
		return false;
	}

#ifdef HDMI_CONSOLE
	if (!mScreen.Initialize())
	{
		return false;
	}
#else
	if (!mSerial.Initialize(115200))
	{
		return false;
	}
#endif

	// CDevice *pTarget = mDeviceNameService.GetDevice(mOptions.GetLogDevice(), false);
	// if (pTarget == 0)
	// {
	// 	pTarget = &mScreen;
	// }

	if (!mLogger.Initialize(
#ifdef HDMI_CONSOLE
		&mScreen
#else
		&mSerial
#endif
	))
	{
		return false;
	}

	if (!mTimer.Initialize())
	{
		return false;
	}

#ifndef BAKE_MT32_ROMS
	if (!mEMMC.Initialize())
	{
		return false;
	}

	char const *partitionName = "SD:";

	if (f_mount(&mFileSystem, partitionName, 1) != FR_OK)
	{
		mLogger.Write(GetKernelName(), LogError, "Cannot mount partition: %s", partitionName);
		return false;
	}
#endif

#if !defined(__aarch64__) || !defined(LEAVE_QEMU_ON_HALT)
	// The USB driver is not supported under 64-bit QEMU, so
	// the initialization must be skipped in this case, or an
	// exit happens here under 64-bit QEMU.
	if (!mUSBHCI.Initialize())
	{
		return false;
	}
#endif

	if (!mConsole.Initialize())
	{
		return false;
	}

	// Initialize newlib stdio with a reference to Circle's file system and console
	CGlueStdioInit(mFileSystem, mConsole);

	if (!mI2CMaster.Initialize())
	{
		return false;
	}

#if I2C_DAC_PCM5242
	InitPCM5242();
	mSynth = new CMT32SynthI2S(&mInterrupt, SAMPLE_RATE, CHUNK_SIZE);
#else
	mSynth = new CMT32SynthPWM(&mInterrupt, SAMPLE_RATE, CHUNK_SIZE);
#endif
	if (!mSynth->Initialize())
	{
		return false;
	}

	mLogger.Write(GetKernelName(), LogNotice, "Compile time: " __DATE__ " " __TIME__);
	CCPUThrottle::Get()->DumpStatus();

	return true;
}

// TODO: Generic configurable DAC init class
// TODO: Probably works on PCM5122 too
bool CKernel::InitPCM5242()
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
		if (mI2CMaster.Write(I2C_DAC_ADDRESS, &command, sizeof(command)) != sizeof(command))
		{
			CLogger::Get()->Write(GetKernelName(), LogWarning, "I2C write error (DAC init sequence)");
			return false;
		}
	}

	return true;
}

void CKernel::ledOn()
{
	mActLED.On();
	mLEDOnTime = mTimer.GetTicks();
	mLEDOn = true;
}

CStdlibApp::TShutdownMode CKernel::Run(void)
{
	mLogger.Write(GetKernelName(), LogNotice, "Starting up...");

	CUSBMIDIDevice *pMIDIDevice = static_cast<CUSBMIDIDevice *>(CDeviceNameService::Get()->GetDevice("umidi1", FALSE));
	if (pMIDIDevice)
	{
		pMIDIDevice->RegisterPacketHandler(MIDIPacketHandler);
		mLogger.Write(GetKernelName(), LogNotice, "Using USB MIDI interface");
	}
	else
	{
		//mLogger.Write(GetKernelName(), LogNotice, "Using serial MIDI interface");
		mLogger.Write(GetKernelName(), LogError, "No USB MIDI device detected - please connect and restart.");
		return ShutdownHalt;
	}

	// Start audio
	//mSynth->Start();

	while (true)
	{
		unsigned ticks = mTimer.GetTicks();

		// Update activity LED
		if (mLEDOn && (ticks - mLEDOnTime) >= MSEC2HZ(LED_TIMEOUT_MILLIS))
		{
			mActLED.Off();
			mLEDOn = false;
		}

		// Check for active sensing timeout (300 milliseconds)
		// Based on http://midi.teragonaudio.com/tech/midispec/sense.htm
		if (mActiveSenseFlag && (ticks - mActiveSenseTime) >= MSEC2HZ(ACTIVE_SENSE_TIMEOUT_MILLIS))
		{
			mSynth->AllSoundOff();
			mActiveSenseFlag = false;
			mLogger.Write(GetKernelName(), LogNotice, "Active sense timeout - turning notes off");
		}

		if (mShouldReboot)
		{
			// Stop audio and reboot
			//mSynth->Cancel();
			return ShutdownReboot;
		}
	}

	return ShutdownHalt;
}

bool CKernel::parseSysEx()
{
	if (mSysExMessage.size() < 4)
		return false;

	// 'Educational' manufactuer
	if (mSysExMessage[1] != 0x7D)
		return false;

	// Reboot (F0 7F 00 F7)
	if (mSysExMessage[2] == 0x00)
	{
		mLogger.Write(GetKernelName(), LogNotice, "midi: Reboot command received");
		mShouldReboot = true;
		return true;
	}

	return false;
}

void CKernel::updateActiveSense()
{
	//mLogger.Write(GetKernelName(), LogNotice, "Active sense");
	mActiveSenseTime = mTimer.GetTicks();
	mActiveSenseFlag = true;
}

void CKernel::MIDIPacketHandler(unsigned nCable, u8 *pPacket, unsigned nLength)
{
	assert(pThis != 0);
	pThis->mActiveSenseTime = pThis->mTimer.GetTicks();

	u32 packet = 0;
	for (size_t i = 0; i < nLength; ++i)
	{
		// SysEx message
		if (pThis->mSysExMessage.size() || pPacket[i] == 0xF0)
		{
			pThis->mSysExMessage.push_back(pPacket[i]);
			if (pPacket[i] == 0xF7)
			{
				// If we don't consume the SysEx message, forward it to mt32emu
				if (!pThis->parseSysEx())
					pThis->mSynth->HandleMIDISysExMessage(pThis->mSysExMessage.data(), pThis->mSysExMessage.size());
				pThis->mSysExMessage.clear();
				packet = 0;
			}
		}
		// Channel message
		else
			packet |= pPacket[i] << 8 * i;
	}

	if (packet)
	{
		// Active sensing
		if (packet == 0xFE)
		{
			pThis->mActiveSenseFlag = true;
			return;
		}

		// Flash LED on note on or off
		if ((packet & 0x80) == 0x80)
			pThis->ledOn();

		//pThis->mLogger.Write(pThis->GetKernelName(), LogNotice, "midi 0x%08x", packet);
		pThis->mSynth->HandleMIDIControlMessage(packet);
	}
}
