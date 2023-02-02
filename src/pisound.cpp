//
// pisound.cpp
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

#include <circle/logger.h>
#include <circle/timer.h>
#include <circle/util.h>
#include <circle/sched/scheduler.h>

#include <cstdio>

#include "pisound.h"

LOGMODULE("pisound");

constexpr u8 SPIChipSelect        = 0;
// FIXME: Linux driver works at 150000Hz, but here we get data corruption hence lower clock speed - investigate
constexpr u32 SPIClockSpeed       = 15000;
constexpr u8 SPITransferSize      = 4;

constexpr u8 GPIOButton = 17;

constexpr u8 GPIOADCReset           = 12;
constexpr u8 GPIOOversamplingRatio0 = 13;
constexpr u8 GPIOOversamplingRatio1 = 26;
constexpr u8 GPIOOversamplingRatio2 = 16;

constexpr u8 GPIOSPIReset         = 24;
constexpr u8 GPIOSPIDataAvailable = 25;

// Based on: https://github.com/raspberrypi/linux/blob/rpi-5.4.y/sound/soc/bcm/pisound.c
//           https://github.com/raspberrypi/linux/blob/rpi-5.4.y/arch/arm/boot/dts/overlays/pisound-overlay.dts

CPisound::CPisound(CSPIMasterDMA* pSPIMaster, CGPIOManager* pGPIOManager, unsigned nSamplerate)
	: CTask(TASK_STACK_SIZE, true),
	  m_Lock(IRQ_LEVEL),
	  m_pSPIMaster(pSPIMaster),
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

CPisound::~CPisound()
{
	// Reset GPIO pins to default boot-up state
	m_SPIReset.SetMode(TGPIOMode::GPIOModeInputPullDown);
	m_DataAvailable.SetMode(TGPIOMode::GPIOModeInputPullDown);
	m_ADCReset.SetMode(TGPIOMode::GPIOModeInputPullDown);
	m_OversamplingRatio0.SetMode(TGPIOMode::GPIOModeInputPullDown);
	m_OversamplingRatio1.SetMode(TGPIOMode::GPIOModeInputPullDown);
	m_OversamplingRatio2.SetMode(TGPIOMode::GPIOModeInputPullDown);

	Terminate();
}

bool CPisound::Initialize()
{
	assert(m_pSPIMaster != nullptr);

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

	LOGNOTE("Serial number: %s", m_SerialNumber);
	LOGNOTE("ID: %s", m_ID);
	LOGNOTE("Firmware version: %s", m_FirmwareVersion);
	LOGNOTE("Hardware version: %s", m_HardwareVersion);

	Start();

	return true;
}

size_t CPisound::SendMIDI(const u8* pData, size_t nSize)
{
	// Wake up task
	if (IsSuspended())
		Resume();

	return m_MIDITxBuffer.Enqueue(pData, nSize);
}

u16 CPisound::Transfer16(u16 nTxValue) const
{
	u8 SPIRxBuffer[2];
	u8 SPITxBuffer[2];
	SPITxBuffer[0] = nTxValue >> 8;
	SPITxBuffer[1] = nTxValue & 0xFF;

	if (m_pSPIMaster->WriteReadSync(SPIChipSelect, SPITxBuffer, SPIRxBuffer, sizeof(SPIRxBuffer)) < 0)
		return 0;

	return (SPIRxBuffer[0] << 8) | SPIRxBuffer[1];
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
	u8 SPIRxBuffer[256 + 1];

	for (u8 i = 0; i < nCount; ++i)
	{
		memset(SPIRxBuffer, 0, sizeof(SPIRxBuffer));
		size_t nResult = ReadBytes(SPIRxBuffer, sizeof(SPIRxBuffer) - 1);
		if (!nResult)
			return false;

		switch (i)
		{
		case 0:
		case 3:
			if (nResult != 2)
				return false;

			snprintf(i == 0 ? m_FirmwareVersion : m_HardwareVersion, MaxVersionStringLength, "%x.%02x", SPIRxBuffer[0], SPIRxBuffer[1]);
			break;

		case 1:
			if (nResult >= sizeof(m_SerialNumber))
				return false;

			memcpy(m_SerialNumber, SPIRxBuffer, sizeof(m_SerialNumber));
			break;

		case 2:
		{
			if (nResult * 2 >= sizeof(m_ID))
				return false;

			char* pCurrentChar = m_ID;
			for (size_t j = 0; j < nResult; ++j)
				pCurrentChar += sprintf(pCurrentChar, "%02x", SPIRxBuffer[j]);
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
	if (pThis->IsSuspended())
		pThis->Resume();
}

void CPisound::DoTransfer()
{
	bool bHadData = false;

	m_Lock.Acquire();

	do
	{
		size_t nMIDIRxBytes = 0;
		size_t nMIDITxBytes = 0;
		u8 MIDIRxBuffer[SPITransferSize / 2];
		u8 MIDITxBuffer[SPITransferSize / 2];

		u8 SPIRxBuffer[SPITransferSize];
		u8 SPITxBuffer[SPITransferSize];

		bHadData = false;
		memset(SPIRxBuffer, 0, sizeof(SPIRxBuffer));
		memset(SPITxBuffer, 0, sizeof(SPITxBuffer));

		// Prepare outgoing MIDI SPI packet
		nMIDITxBytes = m_MIDITxBuffer.Dequeue(MIDITxBuffer, sizeof(MIDITxBuffer));
		for (size_t i = 0; i < nMIDITxBytes; ++i)
		{
			SPITxBuffer[i * 2] = 0x0F;
			SPITxBuffer[i * 2 + 1] = MIDITxBuffer[i];
		}

		// Extract incoming MIDI bytes from SPI packet
		if (m_pSPIMaster->WriteReadSync(SPIChipSelect, SPITxBuffer, SPIRxBuffer, SPITransferSize) != SPITransferSize)
		{
			LOGERR("SPI transfer failed");
			continue;
		}

		for (size_t i = 0; i < SPITransferSize; i += 2)
		{
			if (SPIRxBuffer[i])
			{
				MIDIRxBuffer[nMIDIRxBytes++] = SPIRxBuffer[i + 1];
				bHadData = true;
			}
		}

		// Pass MIDI bytes on to handler
		if (nMIDIRxBytes && m_pReceiveHandler)
			m_pReceiveHandler(MIDIRxBuffer, nMIDIRxBytes);
	} while (bHadData || !m_MIDITxBuffer.IsEmpty() || m_DataAvailable.Read() == HIGH);

	m_Lock.Release();
}

void CPisound::Run()
{
	CScheduler* const pScheduler = CScheduler::Get();

	while (true)
	{
		DoTransfer();
		Suspend();

		// Allow other tasks to run
		pScheduler->Yield();
	}
}
