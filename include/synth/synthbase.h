//
// synthbase.h
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

#ifndef _synthbase_h
#define _synthbase_h

#include <circle/synchronize.h>
#include <circle/types.h>

class CSynthLCD;

class CSynthBase
{
public:
	CSynthBase(unsigned int nSampleRate)
		: m_Lock(TASK_LEVEL),
		  m_nSampleRate(nSampleRate),
		  m_pLCD(nullptr)
	{
	}

	virtual ~CSynthBase() = default;

	virtual bool Initialize() = 0;
	virtual void HandleMIDIShortMessage(u32 nMessage) = 0;
	virtual void HandleMIDISysExMessage(const u8* pData, size_t nSize) = 0;
	virtual bool IsActive() = 0;
	virtual void AllSoundOff() = 0;
	virtual void SetMasterVolume(u8 nVolume) = 0;
	virtual size_t Render(s16* pOutBuffer, size_t nFrames) = 0;
	virtual size_t Render(float* pOutBuffer, size_t nFrames) = 0;
	virtual u8 GetChannelVelocities(u8* pOutVelocities, size_t nMaxChannels) = 0;
	virtual void ReportStatus() const = 0;
	void SetLCD(CSynthLCD* pLCD) { m_pLCD = pLCD; }

protected:
	CSpinLock m_Lock;
	unsigned int m_nSampleRate;
	CSynthLCD* m_pLCD;
};

#endif
