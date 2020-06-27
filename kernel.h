//
// kernel.h
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
#ifndef _kernel_h
#define _kernel_h

#include <circle_stdlib_app.h>
#include <circle/pwmsounddevice.h>

#include <mt32emu/mt32emu.h>

#include <vector>

#ifdef BAKE_MT32_ROMS
#define MT32_ROM_FILE MT32Emu::ArrayFile
#else
#define MT32_ROM_FILE MT32Emu::FileStream
#endif

class CKernel : public CStdlibApp, public CPWMSoundBaseDevice, public MT32Emu::ReportHandler
{
public:
	CKernel(void);

	virtual bool Initialize(void) override;
	TShutdownMode Run(void);

protected:
	//CScreenDevice mScreen;
	CSerialDevice mSerial;
	CTimer mTimer;
	CLogger mLogger;
	CUSBHCIDevice mUSBHCI;
#ifndef BAKE_MT32_ROMS
	CEMMCDevice mEMMC;
#endif
	FATFS mFileSystem;
	CConsole mConsole;

private:
	// CPWMSoundBaseDevice
	virtual unsigned GetChunk(u32 *pBuffer, unsigned nChunkSize);

	// ReportHandler
	virtual void printDebug(const char *fmt, va_list list);
	virtual void showLCDMessage(const char *message);
	virtual bool onMIDIQueueOverflow();

	bool initMT32();

	bool parseSysEx();
	void updateActiveSense();

	void ledOn();

	// MIDI messages
	unsigned mSerialState;
	u8 mSerialMessage[3];
	std::vector<u8> mSysExMessage;
	bool mActiveSenseFlag;
	unsigned mActiveSenseTime;

	bool mShouldReboot;
	bool mLEDOn;
	unsigned mLEDOnTime;

	int mLowLevel;
	int mNullLevel;
	int mHighLevel;

	MT32_ROM_FILE mControlFile;
	MT32_ROM_FILE mPCMFile;
	const MT32Emu::ROMImage *mControlROMImage;
	const MT32Emu::ROMImage *mPCMROMImage;
	MT32Emu::Synth *mSynth;

	static void MIDIPacketHandler(unsigned nCable, u8 *pPacket, unsigned nLength);
	static CKernel *pThis;
};

#endif
