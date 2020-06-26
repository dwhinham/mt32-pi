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

#include <mt32emu/mt32emu.h>

#ifdef BAKE_MT32_ROMS
#include "mt32_control.h"
#include "mt32_pcm.h"
#else
static const char MT32ControlROMName[] = "MT32_CONTROL.ROM";
static const char MT32PCMROMName[] = "MT32_PCM.ROM";
#endif

CKernel *CKernel::pThis = nullptr;

CKernel::CKernel(void)
	: CStdlibApp("mt32"),
	  CPWMSoundBaseDevice(&mInterrupt, 32000, 2),
	  //mScreen(mOptions.GetWidth(), mOptions.GetHeight()),
	  mSerial(&mInterrupt, true),
	  mTimer(&mInterrupt),
	  mLogger(mOptions.GetLogLevel(), &mTimer),
	  mUSBHCI(&mInterrupt, &mTimer),
#ifndef BAKE_MT32_ROMS
	  mEMMC(&mInterrupt, &mTimer, &mActLED),
#endif
	  //mConsole(&mScreen),
	  mConsole(&mSerial),

	  mSerialState(0),
	  mSerialMessage{0},

	  mShouldReboot(false),
	  mLEDOn(false),
	  mLEDOnTime(0),

#ifdef BAKE_MT32_ROMS
	  mControlFile(MT32_CONTROL_ROM, MT32_CONTROL_ROM_len),
	  mPCMFile(MT32_PCM_ROM, MT32_PCM_ROM_len),
#endif
	  mControlROMImage(nullptr),
	  mPCMROMImage(nullptr),
	  mSynth(nullptr)
{
	pThis = this;
	mLowLevel = GetRangeMin();
	mHighLevel = GetRangeMax();
	mNullLevel = (mHighLevel + mLowLevel) / 2;
}

bool CKernel::Initialize(void)
{
	if (!CStdlibApp::Initialize())
	{
		return false;
	}

	// if (!mScreen.Initialize())
	// {
	// 	return false;
	// }

	//if (!mSerial.Initialize(31250))
	if (!mSerial.Initialize(115200))
	{
		return false;
	}

	// CDevice *pTarget = mDeviceNameService.GetDevice(mOptions.GetLogDevice(), false);
	// if (pTarget == 0)
	// {
	// 	pTarget = &mScreen;
	// }

	if (!mLogger.Initialize(&mSerial))
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

	mLogger.Write(GetKernelName(), LogNotice, "Compile time: " __DATE__ " " __TIME__);
	return true;
}

void CKernel::printDebug(const char *fmt, va_list list)
{
	mLogger.WriteV(GetKernelName(), LogNotice, fmt, list);
}

void CKernel::showLCDMessage(const char *message)
{
	mLogger.Write("lcd", LogNotice, message);
}

bool CKernel::onMIDIQueueOverflow()
{
	mLogger.Write("midi", LogError, "MIDI queue overflow");
	return false;
}

bool CKernel::initMT32()
{
#ifndef BAKE_MT32_ROMS
	if (!mControlFile.open(MT32ControlROMName))
	{
		CLogger::Get()->Write(GetKernelName(), LogError, "Could not open %s", MT32ControlROMName);
		return false;
	}

	if (!mPCMFile.open(MT32PCMROMName))
	{
		CLogger::Get()->Write(GetKernelName(), LogError, "Could not open %s", MT32PCMROMName);
		return false;
	}
#endif

	mControlROMImage = MT32Emu::ROMImage::makeROMImage(&mControlFile);
	mPCMROMImage = MT32Emu::ROMImage::makeROMImage(&mPCMFile);
	mSynth = new MT32Emu::Synth(this);
	return mSynth->open(*mControlROMImage, *mPCMROMImage);
}

void CKernel::ledOn()
{
	mActLED.On();
	mLEDOnTime = mTimer.GetTicks();
	mLEDOn = true;
}

unsigned int CKernel::GetChunk(u32 *pBuffer, unsigned nChunkSize)
{
	unsigned nResult = nChunkSize;
	if (!mSynth)
		return nResult;

	// MT32Emu::Bit16s samples[nChunkSize];
	// mSynth->render(samples, nChunkSize / 2);

	float samples[nChunkSize];
	mSynth->render(samples, nChunkSize / 2);

	for (size_t i = 0; i < nChunkSize; ++i)
	{
		// unsigned nValue = samples[i];
		// nValue = (nValue + 0x8000) & 0xFFFF;
		// nValue >>= 4;
		//*pBuffer++ = nValue;

		*pBuffer++ = static_cast<u32>(samples[i] * mHighLevel / 2 + mNullLevel);
	}

	return nResult;
}

CStdlibApp::TShutdownMode CKernel::Run(void)
{
	mLogger.Write(GetKernelName(), LogNotice, "Starting up...");

	if (!initMT32())
		return ShutdownHalt;

	// Start PWM audio
	Start();

	mLogger.Write(GetKernelName(), LogNotice, "Audio output range: lo=%d null=%d hi=%d", mLowLevel, mNullLevel, mHighLevel);

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

	while (true)
	{
		if (mLEDOn && (mTimer.GetTicks() - mLEDOnTime) > 5)
		{
			mActLED.Off();
			mLEDOn = false;
		}

		if (mShouldReboot)
		{
			// Stop audio and reboot
			Cancel();
			return ShutdownReboot;
		}
	}

	// while (true)
	// {
	// 	// Read serial MIDI data
	// 	u8 Buffer[100];
	// 	int nResult = mSerial.Read(Buffer, sizeof Buffer);
	// 	if (nResult <= 0)
	// 	{
	// 		continue;
	// 	}

	// 	// Process MIDI messages
	// 	// See: https://www.midi.org/specifications/item/table-1-summary-of-midi-message
	// 	for (int i = 0; i < nResult; i++)
	// 	{
	// 		u8 uchData = Buffer[i];

	// 		switch (mSerialState)
	// 		{
	// 		case 0:
	// 			// Channel message
	// 			MIDIRestart:
	// 			if (uchData >= 0x80 && uchData <= 0xEF)
	// 			{
	// 				mSerialMessage[mSerialState++] = uchData;
	// 			}
	// 			// // SysEx
	// 			// else if (uchData == 0xF0)
	// 			// {
	// 			// 	mSerialState = 4;
	// 			// 	mSysExMessage.push_back(uchData);
	// 			// }
	// 			break;

	// 		case 1:
	// 		case 2:
	// 			if (uchData & 0x80) // got status when parameter expected
	// 			{
	// 				mSerialState = 0;
	// 				goto MIDIRestart;
	// 			}

	// 			mSerialMessage[mSerialState++] = uchData;

	// 			// MIDI message is complete
	// 			if (mSerialState == 3)
	// 			{
	// 				MIDIPacketHandler(0, mSerialMessage, sizeof mSerialMessage);

	// 				mSerialState = 0;
	// 			}
	// 			break;

	// 		// case 4:
	// 		// 	mSysExMessage.push_back(uchData);
	// 		// 	if (uchData == 0xF7)
	// 		// 	{
	// 		// 		mSynth->playSysex(mSysExMessage.data(), mSysExMessage.size());
	// 		// 		mSysExMessage.clear();
	// 		// 		mSerialState = 0;
	// 		// 	}
	// 		// 	break;

	// 		default:
	// 			assert(0);
	// 			break;
	// 		}
	// 	}
	// }

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
		pThis->mLogger.Write(pThis->GetKernelName(), LogNotice, "midi: Reboot command received");
		pThis->mShouldReboot = true;
		return true;
	}

	return false;
}

void CKernel::MIDIPacketHandler(unsigned nCable, u8 *pPacket, unsigned nLength)
{
	assert(pThis != 0);

	MT32Emu::Bit32u packet = 0;
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
					pThis->mSynth->playSysex(pThis->mSysExMessage.data(), pThis->mSysExMessage.size());
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
		// Flash LED on note on or off
		if (packet & 0x80 == 0x80)
			pThis->ledOn();

		//pThis->mLogger.Write(pThis->GetKernelName(), LogNotice, "midi 0x%08x", packet);
		pThis->mSynth->playMsg(packet);
	}
}
