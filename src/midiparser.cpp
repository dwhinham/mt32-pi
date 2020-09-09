//
// midiparser.cpp
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

#include <circle/logger.h>

#include "midiparser.h"

CMIDIParser::CMIDIParser()
	: mState(State::StatusByte),
	  mMessageBuffer{0},
	  mMessageLength(0)
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

		switch (mState)
		{
			// Expecting a status byte
			case State::StatusByte:
				ParseStatusByte(data);
				break;

			// Expecting a data byte
			case State::DataByte:
				// Expected a data byte, but received a status
				if (data & 0x80)
				{
					OnUnexpectedStatus();
					ResetState(true);
					ParseStatusByte(data);
					break;
				}

				mMessageBuffer[mMessageLength++] = data;
				CheckCompleteShortMessage();
				break;

			// Expecting a SysEx data byte or EOX
			case State::SysExByte:
				// Received a status that wasn't EOX
				if (data & 0x80 && data != 0xF7)
				{
					OnUnexpectedStatus();
					ResetState(true);
					ParseStatusByte(data);
					break;
				}

				// Buffer overflow
				if (mMessageLength == sizeof(mMessageBuffer))
				{
					OnSysExOverflow();
					ResetState(true);
					ParseStatusByte(data);
					break;
				}

				mMessageBuffer[mMessageLength++] = data;

				// End of SysEx
				if (data == 0xF7)
				{
					OnSysExMessage(mMessageBuffer, mMessageLength);
					ResetState(true);
				}

				break;
		}
	}
}

void CMIDIParser::OnUnexpectedStatus()
{
	if (mState == State::SysExByte)
		CLogger::Get()->Write("midi", LogWarning, "Received illegal status byte during SysEx message; SysEx ignored");
	else
		CLogger::Get()->Write("midi", LogWarning, "Received illegal status byte when data expected");
}

void CMIDIParser::OnSysExOverflow()
{
	CLogger::Get()->Write("midi", LogWarning, "Buffer overrun when receiving SysEx message; SysEx ignored");
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
				mMessageBuffer[0] = 0;
				return;

			// Start of SysEx message
			case 0xF0:
				mState = State::SysExByte;
				break;

			// Tune Request - single byte, handle immediately and clear running status
			case 0xF6:
				OnShortMessage(nByte);
				mMessageBuffer[0] = 0;
				break;

			// Channel or System Common message
			default:
				mState = State::DataByte;
				break;
		}

		mMessageBuffer[mMessageLength++] = nByte;
	}

	// Data byte, use Running Status if we've stored a status byte
	else if (mMessageBuffer[0])
	{
		mMessageBuffer[1] = nByte;
		mMessageLength = 2;

		// We could have a complete 2-byte message, otherwise wait for third byte
		if (!CheckCompleteShortMessage())
			mState = State::DataByte;
	}
}

bool CMIDIParser::CheckCompleteShortMessage()
{
	u8& status = mMessageBuffer[0];

	// MIDI message is complete if we receive 3 bytes,
	// or 2 bytes if it's a Program Change, Channel Pressure/Aftertouch, Time Code Quarter Frame, or Song Select
	if (mMessageLength == 3 ||
		(mMessageLength == 2 && ((status >= 0xC0 && status <= 0xDF) || status == 0xF1 || status == 0xF3)))
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
	assert(mMessageLength == 2 || mMessageLength == 3);

	u32 message = 0;
	for (size_t i = 0; i < mMessageLength; ++i)
		message |= mMessageBuffer[i] << 8 * i;

	return message;
}

void CMIDIParser::ResetState(bool bClearStatusByte)
{
	if (bClearStatusByte)
		mMessageBuffer[0] = 0;

	mMessageLength = 0;
	mState = State::StatusByte;
}
