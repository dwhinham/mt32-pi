//
// midiparser.h
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

#ifndef _midiparser_h
#define _midiparser_h

#include <circle/logger.h>
#include <circle/types.h>

class CMIDIParser
{
public:
	CMIDIParser();

	void ParseMIDIBytes(const u8* pData, size_t nSize);

protected:
	virtual void OnShortMessage(u32 nMessage) = 0;
	virtual void OnSysExMessage(const u8* pData, size_t nSize) = 0;

	virtual void OnUnexpectedStatus();
	virtual void OnSysExOverflow();

private:
	enum class TState
	{
		StatusByte,
		DataByte,
		SysExByte
	};

	// Matches mt32emu's SysEx buffer size
	static constexpr size_t SysExBufferSize = 1000;

	void ParseStatusByte(u8 nByte);
	bool CheckCompleteShortMessage();
	u32 PrepareShortMessage() const;
	void ResetState(bool bClearStatusByte);

	TState m_State;
	u8 m_MessageBuffer[SysExBufferSize];
	size_t m_nMessageLength;
};

#endif
