//
// pisound.h
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

#ifndef _pisound_h
#define _pisound_h

#include <circle/gpiomanager.h>
#include <circle/gpiopin.h>
#include <circle/spimaster.h>
#include <circle/types.h>

#include "ringbuffer.h"

class CPisound
{
public:
	using TMIDIReceiveHandler = void (*)(const u8* pData, size_t nSize);

	CPisound(CSPIMaster* pSPIMaster, CGPIOManager* pGPIOManager, unsigned nSamplerate);
	~CPisound();

	bool Initialize();
	void RegisterMIDIReceiveHandler(TMIDIReceiveHandler pHandler) { m_pReceiveHandler = pHandler; }
	size_t SendMIDI(const u8* pData, size_t nSize);

private:
	u16 Transfer16(u16 nValue) const;
	size_t ReadBytes(u8* pOutBuffer, size_t nSize) const;
	bool ReadInfo();
	void SetOSRPins(unsigned bRatio1, unsigned bRatio2, unsigned bRatio3);
	void SPITask();

	static constexpr size_t MaxSerialNumberStringLength = 11;
	static constexpr size_t MaxIDStringLength = 25;
	static constexpr size_t MaxVersionStringLength = 6;

	CSPIMaster* m_pSPIMaster;
	unsigned m_nSamplerate;

	CGPIOPin m_SPIReset;
	CGPIOPin m_DataAvailable;

	CGPIOPin m_ADCReset;
	CGPIOPin m_OversamplingRatio0;
	CGPIOPin m_OversamplingRatio1;
	CGPIOPin m_OversamplingRatio2;

	TMIDIReceiveHandler m_pReceiveHandler;

	char m_SerialNumber[MaxSerialNumberStringLength];
	char m_ID[MaxIDStringLength];
	char m_FirmwareVersion[MaxVersionStringLength];
	char m_HardwareVersion[MaxVersionStringLength];

	CRingBuffer<u8, 64> m_MIDITxBuffer;

	static void DataAvailableInterruptHandler(void* pUserData);
};

#endif
