//
// pisound.cpp
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

#include <circle/logger.h>
#include <circle/timer.h>
#include <circle/util.h>

#include <cstdio>

#include "pisound.h"

constexpr u8 SPIChipSelect        = 0;
constexpr u8 SPIDelayMicroseconds = 10;
constexpr u32 SPIClockSpeed       = 150000;
constexpr u8 SPITransferSize      = 4;

constexpr u8 GPIOButton = 17;

constexpr u8 GPIOADCReset           = 12;
constexpr u8 GPIOOversamplingRatio0 = 13;
constexpr u8 GPIOOversamplingRatio1 = 26;
constexpr u8 GPIOOversamplingRatio2 = 16;

constexpr u8 GPIOSPIReset         = 24;
constexpr u8 GPIOSPIDataAvailable = 25;

const char PisoundName[] = "pisound";

// Based on: https://github.com/raspberrypi/linux/blob/rpi-5.4.y/sound/soc/bcm/pisound.c
//           https://github.com/raspberrypi/linux/blob/rpi-5.4.y/arch/arm/boot/dts/overlays/pisound-overlay.dts

CPisound::CPisound(CSPIMaster* pSPIMaster, CGPIOManager* pGPIOManager, unsigned nSamplerate)
	: m_pSPIMaster(pSPIMaster),
	  m_nSamplerate(nSamplerate),

	  m_SPIReset(GPIOSPIReset, TGPIOMode::GPIOModeOutput),
	  m_DataAvailable(GPIOSPIDataAvailable, TGPIOMode::GPIOModeInput, pGPIOManager),

	  m_ADCReset(GPIOADCReset, TGPIOMode::GPIOModeOutput),
	  m_OversamplingRatio0(GPIOOversamplingRatio0, TGPIOMode::GPIOModeOutput),
	  m_OversamplingRatio1(GPIOOversamplingRatio1, TGPIOMode::GPIOModeOutput),
	  m_OversamplingRatio2(GPIOOversamplingRatio2, TGPIOMode::GPIOModeOutput),

	  m_pReceiveHandler(nullptr),

	  m_SerialNumber{0},
	  m_ID{0},
	  m_FirmwareVersion{0},

	  // Assume hardware version 1.0
	  m_HardwareVersion{"1.0"}
{
}

bool CPisound::Initialize()
{
	assert(m_pSPIMaster != nullptr);

	CLogger* const pLogger = CLogger::Get();

	// Set the oversampling ratio pins
	switch (m_nSamplerate)
	{
	case 48000:
		SetOSRPins(HIGH, LOW, LOW);
		break;

	case 96000:
		SetOSRPins(HIGH, LOW, HIGH);
		break;

	case 192000:
		SetOSRPins(HIGH, HIGH, HIGH);
		break;

	default:
		return false;
	}

	// Setup SPI
	m_pSPIMaster->SetCSHoldTime(SPIDelayMicroseconds);
	m_pSPIMaster->SetClock(SPIClockSpeed);

	// Reset the SPI device
	m_SPIReset.Write(LOW);
	CTimer::SimpleMsDelay(1);
	m_SPIReset.Write(HIGH);
	CTimer::SimpleMsDelay(64);

	// Read information from board
	if (!ReadInfo())
		return false;

	// Attach receive interrupt
	m_DataAvailable.ConnectInterrupt(DataAvailableInterruptHandler, this);
	m_DataAvailable.EnableInterrupt(TGPIOInterrupt::GPIOInterruptOnRisingEdge);

	// Flash the LEDs
	Transfer16(0xF008);

	pLogger->Write(PisoundName, LogNotice, "Serial number: %s", m_SerialNumber);
	pLogger->Write(PisoundName, LogNotice, "ID: %s", m_ID);
	pLogger->Write(PisoundName, LogNotice, "Firmware version: %s", m_FirmwareVersion);
	pLogger->Write(PisoundName, LogNotice, "Hardware version: %s", m_HardwareVersion);

	return true;
}

u16 CPisound::Transfer16(u16 nTxValue) const
{
	u8 RxBuffer[2];
	u8 TxBuffer[2];
	TxBuffer[0] = nTxValue >> 8;
	TxBuffer[1] = nTxValue & 0xFF;

	if (m_pSPIMaster->WriteRead(SPIChipSelect, TxBuffer, RxBuffer, sizeof(RxBuffer)) < 0)
		return 0;

	return (RxBuffer[0] << 8) | RxBuffer[1];
}

size_t CPisound::ReadBytes(u8* pOutBuffer, size_t nSize) const
{
	size_t nBytesRead;
	size_t nBytesAvailable;

	u16 nRx = Transfer16(0);
	if (!(nRx >> 8))
		return 0;

	nBytesAvailable = nRx & 0xFF;
	if (nBytesAvailable > nSize)
		return 0;

	for (nBytesRead = 0; nBytesRead < nBytesAvailable; ++nBytesRead)
	{
		nRx = Transfer16(0);
		if (!(nRx >> 8))
			return 0;

		pOutBuffer[nBytesRead] = nRx & 0xFF;
	}

	return nBytesRead;
}

bool CPisound::ReadInfo()
{
	u16 nRx = Transfer16(0);
	if (!(nRx >> 8))
		return false;

	u8 nCount = nRx & 0xFF;
	u8 RxBuffer[256 + 1];

	for (u8 i = 0; i < nCount; ++i)
	{
		memset(RxBuffer, 0, sizeof(RxBuffer));
		size_t nResult = ReadBytes(RxBuffer, sizeof(RxBuffer) - 1);
		if (!nResult)
			return false;

		switch (i)
		{
		case 0:
		case 3:
			if (nResult != 2)
				return false;

			snprintf(i == 0 ? m_FirmwareVersion : m_HardwareVersion, MaxVersionStringLength, "%x.%02x", RxBuffer[0], RxBuffer[1]);
			break;

		case 1:
			if (nResult >= sizeof(m_SerialNumber))
				return false;

			memcpy(m_SerialNumber, RxBuffer, sizeof(m_SerialNumber));
			break;

		case 2:
		{
			if (nResult * 2 >= sizeof(m_ID))
				return false;

			char* pCurrentChar = m_ID;
			for (size_t j = 0; j < nResult; ++j)
				pCurrentChar += sprintf(pCurrentChar, "%02x", RxBuffer[j]);
			*pCurrentChar = '\0';

			break;
		}

		default:
			break;
		}
	}

	return true;
}

void CPisound::SetOSRPins(unsigned bRatio1, unsigned bRatio2, unsigned bRatio3)
{
	m_ADCReset.Write(LOW);
	m_OversamplingRatio0.Write(bRatio1);
	m_OversamplingRatio1.Write(bRatio2);
	m_OversamplingRatio2.Write(bRatio3);
	m_ADCReset.Write(HIGH);
}

void CPisound::DataAvailableInterruptHandler(void* pUserData)
{
	CPisound* pThis = static_cast<CPisound*>(pUserData);
	assert(pThis && pThis->m_pReceiveHandler);

	do
	{
		size_t nMIDIBytes = 0;
		u8 MIDIBuffer[SPITransferSize / 2];

		u8 RxBuffer[SPITransferSize];
		memset(RxBuffer, 0, sizeof(RxBuffer));

		// Extract MIDI bytes from SPI packet
		pThis->m_pSPIMaster->Read(SPIChipSelect, RxBuffer, sizeof(RxBuffer));
		for (size_t i = 0; i < sizeof(RxBuffer); i += 2)
		{
			if (RxBuffer[i])
				MIDIBuffer[nMIDIBytes++] = RxBuffer[i + 1];
		}

		// Pass MIDI bytes on to handler
		if (nMIDIBytes)
			pThis->m_pReceiveHandler(MIDIBuffer, nMIDIBytes);
	} while (pThis->m_DataAvailable.Read() == HIGH);
}
