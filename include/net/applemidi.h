//
// applemidi.h
//
// mt32-pi - A baremetal MIDI synthesizer for Raspberry Pi
// Copyright (C) 2020-2022 Dale Whinham <daleyo@gmail.com>
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

#ifndef _applemidi_h
#define _applemidi_h

#include <circle/bcmrandom.h>
#include <circle/net/ipaddress.h>
#include <circle/net/socket.h>
#include <circle/sched/task.h>

class CAppleMIDIHandler
{
public:
	virtual void OnAppleMIDIDataReceived(const u8* pData, size_t nSize) = 0;
	virtual void OnAppleMIDIConnect(const CIPAddress* pIPAddress, const char* pName) = 0;
	virtual void OnAppleMIDIDisconnect(const CIPAddress* pIPAddress, const char* pName) = 0;
};

class CAppleMIDIParticipant : protected CTask
{
public:
	CAppleMIDIParticipant(CBcmRandomNumberGenerator* pRandom, CAppleMIDIHandler* pHandler);
	virtual ~CAppleMIDIParticipant() override;

	bool Initialize();

	virtual void Run() override;

private:
	void ControlInvitationState();
	void MIDIInvitationState();
	void ConnectedState();
	void Reset();

	bool SendPacket(CSocket* pSocket, CIPAddress* pIPAddress, u16 nPort, const void* pData, size_t nSize);
	bool SendAcceptInvitationPacket(CSocket* pSocket, CIPAddress* pIPAddress, u16 nPort);
	bool SendRejectInvitationPacket(CSocket* pSocket, CIPAddress* pIPAddress, u16 nPort, u32 nInitiatorToken);
	bool SendSyncPacket(u64 nTimestamp1, u64 nTimestamp2);
	bool SendFeedbackPacket();

	CBcmRandomNumberGenerator* m_pRandom;

	// UDP sockets
	CSocket* m_pControlSocket;
	CSocket* m_pMIDISocket;

	// Foreign peers
	CIPAddress m_ForeignControlIPAddress;
	CIPAddress m_ForeignMIDIIPAddress;
	u16 m_nForeignControlPort;
	u16 m_nForeignMIDIPort;

	// Connected peer
	CIPAddress m_InitiatorIPAddress;
	u16 m_nInitiatorControlPort;
	u16 m_nInitiatorMIDIPort;

	// Socket receive buffers
	u8 m_ControlBuffer[FRAME_BUFFER_SIZE];
	u8 m_MIDIBuffer[FRAME_BUFFER_SIZE];

	int m_nControlResult;
	int m_nMIDIResult;

	// Callback handler
	CAppleMIDIHandler* m_pHandler;

	// Participant state machine
	enum class TState
	{
		ControlInvitation,
		MIDIInvitation,
		Connected
	};

	TState m_State;

	u32 m_nInitiatorToken = 0;
	u32 m_nInitiatorSSRC = 0;
	u32 m_nSSRC = 0;
	u32 m_nLastMIDISequenceNumber = 0;

	u64 m_nOffsetEstimate = 0;
	u64 m_nLastSyncTime = 0;

	u16 m_nSequence = 0;
	u16 m_nLastFeedbackSequence = 0;
	u64 m_nLastFeedbackTime = 0;
};

#endif
