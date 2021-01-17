//
// midiparser.cpp
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

#include "midiparser.h"

const char MIDIParserName[] = "midiparser";

CMIDIParser::CMIDIParser()
	: m_State(TState::StatusByte),
	  m_MessageBuffer{0},
	  m_nMessageLength(0)
{
}

void CMIDIParser::ParseMIDIBytes(const u8* pData, size_t nSize)
{
	// Process MIDI messages
	// See: https://www.midi.org/specifications/item/table-1-summary-of-midi-message
	for (size_t i = 0; i < nSize; ++i)
	{
		u8 data = pData[i];

		// System Real-Time message - single byte, handle immediately
		// Can appear anywhere in the stream, even in between status/data bytes
		if (data >= 0xF8)
		{
			// Ignore undefined System Real-Time
			if (data != 0xF9 && data != 0xFD)
				OnShortMessage(data);

			continue;
		}

		switch (m_State)
		{
			// Expecting a status byte
			case TState::StatusByte:
				ParseStatusByte(data);
				break;

			// Expecting a data byte
			case TState::DataByte:
				// Expected a data byte, but received a status
				if (data & 0x80)
				{
					OnUnexpectedStatus();
					ResetState(true);
					ParseStatusByte(data);
					break;
				}

				m_MessageBuffer[m_nMessageLength++] = data;
				CheckCompleteShortMessage();
				break;

			// Expecting a SysEx data byte or EOX
			case TState::SysExByte:
				// Received a status that wasn't EOX
				if (data & 0x80 && data != 0xF7)
				{
					OnUnexpectedStatus();
					ResetState(true);
					ParseStatusByte(data);
					break;
				}

				// Buffer overflow
				if (m_nMessageLength == sizeof(m_MessageBuffer))
				{
					OnSysExOverflow();
					ResetState(true);
					ParseStatusByte(data);
					break;
				}

				m_MessageBuffer[m_nMessageLength++] = data;

				// End of SysEx
				if (data == 0xF7)
				{
					OnSysExMessage(m_MessageBuffer, m_nMessageLength);
					ResetState(true);
				}

				break;
		}
	}
}

void CMIDIParser::OnUnexpectedStatus()
{
	if (m_State == TState::SysExByte)
		CLogger::Get()->Write(MIDIParserName, LogWarning, "Received illegal status byte during SysEx message; SysEx ignored");
	else
		CLogger::Get()->Write(MIDIParserName, LogWarning, "Received illegal status byte when data expected");
}

void CMIDIParser::OnSysExOverflow()
{
	CLogger::Get()->Write(MIDIParserName, LogWarning, "Buffer overrun when receiving SysEx message; SysEx ignored");
}

void CMIDIParser::ParseStatusByte(u8 nByte)
{
	// Is it a status byte?
	if (nByte & 0x80)
	{
		switch (nByte)
		{
			// Invalid End of SysEx or undefined System Common message; ignore and clear running status
			case 0xF4:
			case 0xF5:
			case 0xF7:
				m_MessageBuffer[0] = 0;
				return;

			// Start of SysEx message
			case 0xF0:
				m_State = TState::SysExByte;
				break;

			// Tune Request - single byte, handle immediately and clear running status
			case 0xF6:
				OnShortMessage(nByte);
				m_MessageBuffer[0] = 0;
				break;

			// Channel or System Common message
			default:
				m_State = TState::DataByte;
				break;
		}

		m_MessageBuffer[m_nMessageLength++] = nByte;
	}

	// Data byte, use Running Status if we've stored a status byte
	else if (m_MessageBuffer[0])
	{
		m_MessageBuffer[1] = nByte;
		m_nMessageLength = 2;

		// We could have a complete 2-byte message, otherwise wait for third byte
		if (!CheckCompleteShortMessage())
			m_State = TState::DataByte;
	}
}

bool CMIDIParser::CheckCompleteShortMessage()
{
	u8 status = m_MessageBuffer[0];

	// MIDI message is complete if we receive 3 bytes,
	// or 2 bytes if it's a Program Change, Channel Pressure/Aftertouch, Time Code Quarter Frame, or Song Select
	if (m_nMessageLength == 3 ||
		(m_nMessageLength == 2 && ((status >= 0xC0 && status <= 0xDF) || status == 0xF1 || status == 0xF3)))
	{
		OnShortMessage(PrepareShortMessage());

		// Clear running status if System Common
		ResetState(status >= 0xF1 && status <= 0xF7);
		return true;
	}

	return false;
}

u32 CMIDIParser::PrepareShortMessage() const
{
	assert(m_nMessageLength == 2 || m_nMessageLength == 3);

	u32 message = 0;
	for (size_t i = 0; i < m_nMessageLength; ++i)
		message |= m_MessageBuffer[i] << 8 * i;

	return message;
}

void CMIDIParser::ResetState(bool bClearStatusByte)
{
	if (bClearStatusByte)
		m_MessageBuffer[0] = 0;

	m_nMessageLength = 0;
	m_State = TState::StatusByte;
}
