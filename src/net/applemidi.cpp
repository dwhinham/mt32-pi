//
// applemidi.cpp
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

#include <circle/logger.h>
#include <circle/macros.h>
#include <circle/net/in.h>
#include <circle/net/netsubsystem.h>
#include <circle/sched/scheduler.h>
#include <circle/timer.h>
#include <circle/util.h>

#include "net/applemidi.h"
#include "net/byteorder.h"

// #define APPLEMIDI_DEBUG

constexpr u16 ControlPort = 5004;
constexpr u16 MIDIPort    = ControlPort + 1;

constexpr u16 AppleMIDISignature = 0xFFFF;
constexpr u8 AppleMIDIVersion    = 2;

constexpr u8 RTPMIDIPayloadType = 0x61;
constexpr u8 RTPMIDIVersion     = 2;

// Arbitrary value
constexpr size_t MaxNameLength = 256;

// Timeout period for invitation (5 seconds in 100 microsecond units)
constexpr unsigned int InvitationTimeout = 5 * 10000;

// Timeout period for sync packets (60 seconds in 100 microsecond units)
constexpr unsigned int SyncTimeout = 60 * 10000;

// Receiver feedback packet frequency (1 second in 100 microsecond units)
constexpr unsigned int ReceiverFeedbackPeriod = 1 * 10000;

const char AppleMIDIName[] = "applemidi";

constexpr u16 CommandWord(const char Command[2]) { return Command[0] << 8 | Command[1]; }

enum TAppleMIDICommand : u16
{
	Invitation         = CommandWord("IN"),
	InvitationAccepted = CommandWord("OK"),
	InvitationRejected = CommandWord("NO"),
	Sync               = CommandWord("CK"),
	ReceiverFeedback   = CommandWord("RS"),
	EndSession         = CommandWord("BY"),
};

struct TAppleMIDISession
{
	u16 nSignature;
	u16 nCommand;
	u32 nVersion;
	u32 nInitiatorToken;
	u32 nSSRC;
	char Name[MaxNameLength];
}
PACKED;

// The Name field is optional
constexpr size_t NamelessSessionPacketSize = sizeof(TAppleMIDISession) - sizeof(TAppleMIDISession::Name);

struct TAppleMIDISync
{
	u16 nSignature;
	u16 nCommand;
	u32 nSSRC;
	u8 nCount;
	u8 Padding[3];
	u64 Timestamps[3];
}
PACKED;

struct TAppleMIDIReceiverFeedback
{
	u16 nSignature;
	u16 nCommand;
	u32 nSSRC;
	u32 nSequence;
}
PACKED;

struct TRTPMIDI
{
	u16 nFlags;
	u16 nSequence;
	u32 nTimestamp;
	u32 nSSRC;
}
PACKED;

u64 GetSyncClock()
{
	static const u64 nStartTime = CTimer::GetClockTicks();
	const u64 nMicrosSinceEpoch = CTimer::GetClockTicks();

	// Units of 100 microseconds
	return (nMicrosSinceEpoch - nStartTime ) / 100;
}

bool ParseInvitationPacket(const u8* pBuffer, size_t nSize, TAppleMIDISession* pOutPacket)
{
	const TAppleMIDISession* const pInPacket = reinterpret_cast<const TAppleMIDISession*>(pBuffer);

	if (nSize < NamelessSessionPacketSize)
		return false;

	const u16 nSignature = ntohs(pInPacket->nSignature);
	if (nSignature != AppleMIDISignature)
		return false;

	const u16 nCommand = ntohs(pInPacket->nCommand);
	if (nCommand != Invitation)
		return false;

	const u32 nVersion = ntohl(pInPacket->nVersion);
	if (nVersion != AppleMIDIVersion)
		return false;

	pOutPacket->nSignature = nSignature;
	pOutPacket->nCommand = nCommand;
	pOutPacket->nVersion = nVersion;
	pOutPacket->nInitiatorToken = ntohl(pInPacket->nInitiatorToken);
	pOutPacket->nSSRC = ntohl(pInPacket->nSSRC);

	if (nSize > NamelessSessionPacketSize)
		strncpy(pOutPacket->Name, pInPacket->Name, sizeof(pOutPacket->Name));
	else
		strncpy(pOutPacket->Name, "<unknown>", sizeof(pOutPacket->Name));

	return true;
}

bool ParseEndSessionPacket(const u8* pBuffer, size_t nSize, TAppleMIDISession* pOutPacket)
{
	const TAppleMIDISession* const pInPacket = reinterpret_cast<const TAppleMIDISession*>(pBuffer);

	if (nSize < NamelessSessionPacketSize)
		return false;

	const u16 nSignature = ntohs(pInPacket->nSignature);
	if (nSignature != AppleMIDISignature)
		return false;

	const u16 nCommand = ntohs(pInPacket->nCommand);
	if (nCommand != EndSession)
		return false;

	const u32 nVersion = ntohl(pInPacket->nVersion);
	if (nVersion != AppleMIDIVersion)
		return false;

	pOutPacket->nSignature = nSignature;
	pOutPacket->nCommand = nCommand;
	pOutPacket->nVersion = nVersion;
	pOutPacket->nInitiatorToken = ntohl(pInPacket->nInitiatorToken);
	pOutPacket->nSSRC = ntohl(pInPacket->nSSRC);

	return true;
}

bool ParseSyncPacket(const u8* pBuffer, size_t nSize, TAppleMIDISync* pOutPacket)
{
	const TAppleMIDISync* const pInPacket = reinterpret_cast<const TAppleMIDISync*>(pBuffer);

	if (nSize < sizeof(TAppleMIDISync))
		return false;

	const u32 nSignature = ntohs(pInPacket->nSignature);
	if (nSignature != AppleMIDISignature)
		return false;

	const u32 nCommand = ntohs(pInPacket->nCommand);
	if (nCommand != Sync)
		return false;

	pOutPacket->nSignature = nSignature;
	pOutPacket->nCommand = nCommand;
	pOutPacket->nSSRC = ntohl(pInPacket->nSSRC);
	pOutPacket->nCount = pInPacket->nCount;
	pOutPacket->Timestamps[0] = ntohll(pInPacket->Timestamps[0]);
	pOutPacket->Timestamps[1] = ntohll(pInPacket->Timestamps[1]);
	pOutPacket->Timestamps[2] = ntohll(pInPacket->Timestamps[2]);

	return true;
}

u8 ParseMIDIDeltaTime(const u8* pBuffer)
{
	u8 nLength = 0;
	u32 nDeltaTime = 0;

	while (nLength < 4)
	{
		nDeltaTime <<= 7;
		nDeltaTime |= pBuffer[nLength] & 0x7F;

		// Upper bit not set; end of timestamp
		if ((pBuffer[nLength++] & (1 << 7)) == 0)
			break;
	}

	return nLength;
}

size_t ParseSysExCommand(const u8* pBuffer, size_t nSize, CAppleMIDIHandler* pHandler)
{
	size_t nBytesParsed = 1;
	const u8 nHead = pBuffer[0];
	u8 nTail = 0;

	while (nBytesParsed < nSize && !(nTail == 0xF0 || nTail == 0xF7 || nTail == 0xF4))
		nTail = pBuffer[nBytesParsed++];

	size_t nReceiveLength = nBytesParsed;

	// First segmented SysEx packet
	if (nHead == 0xF0 && nTail == 0xF0)
	{
#ifdef APPLEMIDI_DEBUG
		CLogger::Get()->Write(AppleMIDIName, LogNotice, "Received segmented SysEx (first)");
#endif
		--nReceiveLength;
	}

	// Middle segmented SysEx packet
	else if (nHead == 0xF7 && nTail == 0xF0)
	{
#ifdef APPLEMIDI_DEBUG
		CLogger::Get()->Write(AppleMIDIName, LogNotice, "Received segmented SysEx (middle)");
#endif
		++pBuffer;
		nBytesParsed -= 2;
	}

	// Last segmented SysEx packet
	else if (nHead == 0xF7 && nTail == 0xF7)
	{
#ifdef APPLEMIDI_DEBUG
		CLogger::Get()->Write(AppleMIDIName, LogNotice, "Received segmented SysEx (last)");
#endif
		++pBuffer;
		--nReceiveLength;
	}

	// Cancelled segmented SysEx packet
	else if (nHead == 0xF7 && nTail == 0xF4)
	{
#ifdef APPLEMIDI_DEBUG
		CLogger::Get()->Write(AppleMIDIName, LogNotice, "Received cancelled SysEx");
#endif
		nReceiveLength = 1;
	}

#ifdef APPLEMIDI_DEBUG
	else
	{
		CLogger::Get()->Write(AppleMIDIName, LogNotice, "Received complete SysEx");
	}
#endif

	pHandler->OnAppleMIDIDataReceived(pBuffer, nReceiveLength);

	return nBytesParsed;
}

size_t ParseMIDICommand(const u8* pBuffer, size_t nSize, u8& nRunningStatus, CAppleMIDIHandler* pHandler)
{
	size_t nBytesParsed = 0;
	u8 nByte = pBuffer[0];

	// System Real-Time message - single byte, handle immediately
	// Can appear anywhere in the stream, even in between status/data bytes
	if (nByte >= 0xF8)
	{
		// Ignore undefined System Real-Time
		if (nByte != 0xF9 && nByte != 0xFD)
			pHandler->OnAppleMIDIDataReceived(&nByte, 1);

		return 1;
	}

	// Is it a status byte?
	if (nByte & 0x80)
	{
		// Update running status if non Real-Time System status
		if (nByte < 0xF0)
			nRunningStatus = nByte;
		else
			nRunningStatus = 0;

		++nBytesParsed;
	}
	else
	{
		// First byte not a status byte and no running status - invalid
		if (!nRunningStatus)
			return 0;

		// Use running status
		nByte = nRunningStatus;
	}

	// Channel messages
	if (nByte < 0xF0)
	{
		// How many data bytes?
		switch (nByte & 0xF0)
		{
			case 0x80:				// Note off
			case 0x90:				// Note on
			case 0xA0:				// Polyphonic key pressure/aftertouch
			case 0xB0:				// Control change
			case 0xE0:				// Pitch bend
				nBytesParsed += 2;
				break;

			case 0xC0:				// Program change
			case 0xD0:				// Channel pressure/aftertouch
				nBytesParsed += 1;
				break;
		}

		// Handle command
		pHandler->OnAppleMIDIDataReceived(pBuffer, nBytesParsed);
		return nBytesParsed;
	}

	// System common commands
	switch (nByte)
	{
		case 0xF0:					// Start of System Exclusive
		case 0xF7:					// End of Exclusive
			return ParseSysExCommand(pBuffer, nSize, pHandler);

		case 0xF1:					// MIDI Time Code Quarter Frame
		case 0xF3:					// Song Select
			++nBytesParsed;
			break;

		case 0xF2:					// Song Position Pointer
			nBytesParsed += 2;
			break;
	}

	pHandler->OnAppleMIDIDataReceived(pBuffer, nBytesParsed);
	return nBytesParsed;
}

bool ParseMIDICommandSection(const u8* pBuffer, size_t nSize, CAppleMIDIHandler* pHandler)
{
	// Must have at least a header byte and a single status byte
	if (nSize < 2)
		return false;

	size_t nMIDICommandsProcessed = 0;
	size_t nBytesRemaining = nSize - 1;
	u8 nRunningStatus = 0;

	const u8 nMIDIHeader = pBuffer[0];
	const u8* pMIDICommands = pBuffer + 1;

	// Lower 4 bits of the header is length
	u16 nMIDICommandLength = nMIDIHeader & 0x0F;

	// If B flag is set, length value is 12 bits
	if (nMIDIHeader & (1 << 7))
	{
		nMIDICommandLength <<= 8;
		nMIDICommandLength |= pMIDICommands[0];
		++pMIDICommands;
		--nBytesRemaining;
	}

	if (nMIDICommandLength > nBytesRemaining)
	{
		CLogger::Get()->Write(AppleMIDIName, LogError, "Invalid MIDI command length");
		return false;
	}

	// Begin decoding the command list
	while (nMIDICommandLength)
	{
		// If Z flag is set, first list entry is a delta time
		if (nMIDICommandsProcessed || nMIDIHeader & (1 << 5))
		{
			const u8 nBytesParsed = ParseMIDIDeltaTime(pMIDICommands);
			nMIDICommandLength -= nBytesParsed;
			pMIDICommands += nBytesParsed;
		}

		if (nMIDICommandLength)
		{
			const size_t nBytesParsed = ParseMIDICommand(pMIDICommands, nMIDICommandLength, nRunningStatus, pHandler);
			nMIDICommandLength -= nBytesParsed;
			pMIDICommands += nBytesParsed;
			++nMIDICommandsProcessed;
		}
	}

	return true;
}

bool ParseMIDIPacket(const u8* pBuffer, size_t nSize, TRTPMIDI* pOutPacket, CAppleMIDIHandler* pHandler)
{
	assert(pHandler != nullptr);

	const TRTPMIDI* const pInPacket = reinterpret_cast<const TRTPMIDI*>(pBuffer);
	const u16 nRTPFlags = ntohs(pInPacket->nFlags);

	// Check size (RTP-MIDI header plus MIDI command section header)
	if (nSize < sizeof(TRTPMIDI) + 1)
		return false;

	// Check version
	if (((nRTPFlags >> 14) & 0x03) != RTPMIDIVersion)
		return false;

	// Ensure no CSRC identifiers
	if (((nRTPFlags >> 8) & 0x0F) != 0)
		return false;

	// Check payload type
	if ((nRTPFlags & 0xFF) != RTPMIDIPayloadType)
		return false;

	pOutPacket->nFlags = nRTPFlags;
	pOutPacket->nSequence = ntohs(pInPacket->nSequence);
	pOutPacket->nTimestamp = ntohl(pInPacket->nTimestamp);
	pOutPacket->nSSRC = ntohl(pInPacket->nSSRC);

	// RTP-MIDI variable-length header
	const u8* const pMIDICommandSection = pBuffer + sizeof(TRTPMIDI);
	size_t nRemaining = nSize - sizeof(TRTPMIDI);
	return ParseMIDICommandSection(pMIDICommandSection, nRemaining, pHandler);
}

CAppleMIDIParticipant::CAppleMIDIParticipant(CBcmRandomNumberGenerator* pRandom, CAppleMIDIHandler* pHandler)
	: CTask(TASK_STACK_SIZE, true),

	  m_pRandom(pRandom),

	  m_pControlSocket(nullptr),
	  m_pMIDISocket(nullptr),

	  m_nForeignControlPort(0),
	  m_nForeignMIDIPort(0),
	  m_nInitiatorControlPort(0),
	  m_nInitiatorMIDIPort(0),
	  m_ControlBuffer{0},
	  m_MIDIBuffer{0},

	  m_nControlResult(0),
	  m_nMIDIResult(0),

	  m_pHandler(pHandler),

	  m_State(TState::ControlInvitation),

	  m_nInitiatorToken(0),
	  m_nInitiatorSSRC(0),
	  m_nSSRC(0),
	  m_nLastMIDISequenceNumber(0),

	  m_nOffsetEstimate(0),
	  m_nLastSyncTime(0),

	  m_nSequence(0),
	  m_nLastFeedbackSequence(0),
	  m_nLastFeedbackTime(0)
{
}

CAppleMIDIParticipant::~CAppleMIDIParticipant()
{
	if (m_pControlSocket)
		delete m_pControlSocket;

	if (m_pMIDISocket)
		delete m_pMIDISocket;
}

bool CAppleMIDIParticipant::Initialize()
{
	assert(m_pControlSocket == nullptr);
	assert(m_pMIDISocket == nullptr);

	CLogger* const pLogger    = CLogger::Get();
	CNetSubSystem* const pNet = CNetSubSystem::Get();

	if ((m_pControlSocket = new CSocket(pNet, IPPROTO_UDP)) == nullptr)
		return false;

	if ((m_pMIDISocket = new CSocket(pNet, IPPROTO_UDP)) == nullptr)
		return false;

	if (m_pControlSocket->Bind(ControlPort) != 0)
	{
		pLogger->Write(AppleMIDIName, LogError, "Couldn't bind to port %d", ControlPort);
		return false;
	}

	if (m_pMIDISocket->Bind(MIDIPort) != 0)
	{
		pLogger->Write(AppleMIDIName, LogError, "Couldn't bind to port %d", MIDIPort);
		return false;
	}

	// We started as a suspended task; run now that initialization is successful
	Start();

	return true;
}

void CAppleMIDIParticipant::Run()
{
	assert(m_pControlSocket != nullptr);
	assert(m_pMIDISocket != nullptr);

	CLogger* const pLogger       = CLogger::Get();
	CScheduler* const pScheduler = CScheduler::Get();

	while (true)
	{
		if ((m_nControlResult = m_pControlSocket->ReceiveFrom(m_ControlBuffer, sizeof(m_ControlBuffer), MSG_DONTWAIT, &m_ForeignControlIPAddress, &m_nForeignControlPort)) < 0)
			pLogger->Write(AppleMIDIName, LogError, "Control socket receive error: %d", m_nControlResult);

		if ((m_nMIDIResult = m_pMIDISocket->ReceiveFrom(m_MIDIBuffer, sizeof(m_MIDIBuffer), MSG_DONTWAIT, &m_ForeignMIDIIPAddress, &m_nForeignMIDIPort)) < 0)
			pLogger->Write(AppleMIDIName, LogError, "MIDI socket receive error: %d", m_nMIDIResult);

		switch (m_State)
		{
		case TState::ControlInvitation:
			ControlInvitationState();
			break;

		case TState::MIDIInvitation:
			MIDIInvitationState();
			break;

		case TState::Connected:
			ConnectedState();
			break;
		}

		// Allow other tasks to run
		pScheduler->Yield();
	}
}

void CAppleMIDIParticipant::ControlInvitationState()
{
	CLogger* const pLogger = CLogger::Get();
	TAppleMIDISession SessionPacket;

	if (m_nControlResult == 0)
		return;

	if (!ParseInvitationPacket(m_ControlBuffer, m_nControlResult, &SessionPacket))
	{
		pLogger->Write(AppleMIDIName, LogError, "Unexpected packet");
		return;
	}

#ifdef APPLEMIDI_DEBUG
	pLogger->Write(AppleMIDIName, LogNotice, "<-- Control invitation");
#endif

	// Store initiator details
	m_InitiatorIPAddress.Set(m_ForeignControlIPAddress);
	m_nInitiatorControlPort = m_nForeignControlPort;
	m_nInitiatorToken = SessionPacket.nInitiatorToken;
	m_nInitiatorSSRC = SessionPacket.nSSRC;

	// Generate random SSRC and accept
	m_nSSRC = m_pRandom->GetNumber();
	if (!SendAcceptInvitationPacket(m_pControlSocket, &m_InitiatorIPAddress, m_nInitiatorControlPort))
	{
		pLogger->Write(AppleMIDIName, LogError, "Couldn't accept control invitation");
		return;
	}

	m_nLastSyncTime = GetSyncClock();
	m_State = TState::MIDIInvitation;
}

void CAppleMIDIParticipant::MIDIInvitationState()
{
	CLogger* const pLogger = CLogger::Get();
	TAppleMIDISession SessionPacket;

	if (m_nControlResult > 0)
	{
		if (ParseInvitationPacket(m_ControlBuffer, m_nControlResult, &SessionPacket))
		{
			// Unexpected peer; reject invitation
			if (m_ForeignControlIPAddress != m_InitiatorIPAddress || m_nForeignControlPort != m_nInitiatorControlPort)
				SendRejectInvitationPacket(m_pControlSocket, &m_ForeignControlIPAddress, m_nForeignControlPort, SessionPacket.nInitiatorToken);
			else
				pLogger->Write(AppleMIDIName, LogError, "Unexpected packet");
		}
	}

	if (m_nMIDIResult > 0)
	{
		if (!ParseInvitationPacket(m_MIDIBuffer, m_nMIDIResult, &SessionPacket))
		{
			pLogger->Write(AppleMIDIName, LogError, "Unexpected packet");
			return;
		}

		// Unexpected peer; reject invitation
		if (m_ForeignMIDIIPAddress != m_InitiatorIPAddress)
		{
			SendRejectInvitationPacket(m_pMIDISocket, &m_ForeignMIDIIPAddress, m_nForeignMIDIPort, SessionPacket.nInitiatorToken);
			return;
		}

#ifdef APPLEMIDI_DEBUG
		pLogger->Write(AppleMIDIName, LogNotice, "<-- MIDI invitation");
#endif

		m_nInitiatorMIDIPort = m_nForeignMIDIPort;

		if (SendAcceptInvitationPacket(m_pMIDISocket, &m_InitiatorIPAddress, m_nInitiatorMIDIPort))
		{
			CString IPAddressString;
			m_InitiatorIPAddress.Format(&IPAddressString);
			pLogger->Write(AppleMIDIName, LogNotice, "Connection to %s (%s) established", SessionPacket.Name, static_cast<const char*>(IPAddressString));
			m_nLastSyncTime = GetSyncClock();
			m_State = TState::Connected;
			m_pHandler->OnAppleMIDIConnect(&m_InitiatorIPAddress, SessionPacket.Name);
		}
		else
		{
			pLogger->Write(AppleMIDIName, LogError, "Couldn't accept MIDI invitation");
			Reset();
		}
	}

	// Timeout
	else if ((GetSyncClock() - m_nLastSyncTime) > InvitationTimeout)
	{
		pLogger->Write(AppleMIDIName, LogError, "MIDI port invitation timed out");
		Reset();
	}
}

void CAppleMIDIParticipant::ConnectedState()
{
	CLogger* const pLogger = CLogger::Get();

	TAppleMIDISession SessionPacket;
	TRTPMIDI MIDIPacket;
	TAppleMIDISync SyncPacket;

	if (m_nControlResult > 0)
	{
		if (ParseEndSessionPacket(m_ControlBuffer, m_nControlResult, &SessionPacket))
		{
#ifdef APPLEMIDI_DEBUG
			pLogger->Write(AppleMIDIName, LogNotice, "<-- End session");
#endif

			if (m_ForeignControlIPAddress == m_InitiatorIPAddress &&
				m_nForeignControlPort == m_nInitiatorControlPort &&
				SessionPacket.nSSRC == m_nInitiatorSSRC)
			{
				pLogger->Write(AppleMIDIName, LogNotice, "Initiator ended session");
				m_pHandler->OnAppleMIDIDisconnect(&m_InitiatorIPAddress, SessionPacket.Name);
				Reset();
				return;
			}
		}
		else if (ParseInvitationPacket(m_ControlBuffer, m_nControlResult, &SessionPacket))
		{
			// Unexpected peer; reject invitation
			if (m_ForeignControlIPAddress != m_InitiatorIPAddress || m_nForeignControlPort != m_nInitiatorControlPort)
				SendRejectInvitationPacket(m_pControlSocket, &m_ForeignControlIPAddress, m_nForeignControlPort, SessionPacket.nInitiatorToken);
			else
				pLogger->Write(AppleMIDIName, LogError, "Unexpected packet");
		}
	}

	if (m_nMIDIResult > 0)
	{
		if (m_ForeignMIDIIPAddress != m_InitiatorIPAddress || m_nForeignMIDIPort != m_nInitiatorMIDIPort)
			pLogger->Write(AppleMIDIName, LogError, "Unexpected packet");
		else if (ParseMIDIPacket(m_MIDIBuffer, m_nMIDIResult, &MIDIPacket, m_pHandler))
			m_nSequence = MIDIPacket.nSequence;
		else if (ParseSyncPacket(m_MIDIBuffer, m_nMIDIResult, &SyncPacket))
		{
#ifdef APPLEMIDI_DEBUG
			pLogger->Write(AppleMIDIName, LogNotice, "<-- Sync %d", SyncPacket.nCount);
#endif

			if (SyncPacket.nSSRC == m_nInitiatorSSRC && (SyncPacket.nCount == 0 || SyncPacket.nCount == 2))
			{
				if (SyncPacket.nCount == 0)
					SendSyncPacket(SyncPacket.Timestamps[0], GetSyncClock());
				else if (SyncPacket.nCount == 2)
				{
					m_nOffsetEstimate = ((SyncPacket.Timestamps[2] + SyncPacket.Timestamps[0]) / 2) - SyncPacket.Timestamps[1];
#ifdef APPLEMIDI_DEBUG
					pLogger->Write(AppleMIDIName, LogNotice, "Offset estimate: %llu", m_nOffsetEstimate);
#endif
				}

				m_nLastSyncTime = GetSyncClock();
			}
			else
			{
				pLogger->Write(AppleMIDIName, LogError, "Unexpected sync packet");
			}
		}
	}

	const u64 nTicks = GetSyncClock();

	if ((nTicks - m_nLastFeedbackTime) > ReceiverFeedbackPeriod)
	{
		if (m_nSequence != m_nLastFeedbackSequence)
		{
			SendFeedbackPacket();
			m_nLastFeedbackSequence = m_nSequence;
		}
		m_nLastFeedbackTime = nTicks;
	}

	if ((nTicks - m_nLastSyncTime) > SyncTimeout)
	{
		pLogger->Write(AppleMIDIName, LogError, "Initiator timed out");
		Reset();
	}
}

void CAppleMIDIParticipant::Reset()
{
	m_State = TState::ControlInvitation;

	m_nInitiatorToken = 0;
	m_nInitiatorSSRC = 0;
	m_nSSRC = 0;
	m_nLastMIDISequenceNumber = 0;

	m_nOffsetEstimate = 0;
	m_nLastSyncTime = 0;

	m_nSequence = 0;
	m_nLastFeedbackSequence = 0;
	m_nLastFeedbackTime = 0;
}

bool CAppleMIDIParticipant::SendPacket(CSocket* pSocket, CIPAddress* pIPAddress, u16 nPort, const void* pData, size_t nSize)
{
	CLogger* const pLogger = CLogger::Get();

	const int nResult = pSocket->SendTo(pData, nSize, MSG_DONTWAIT, *pIPAddress, nPort);

	if (nResult < 0)
	{
		pLogger->Write(AppleMIDIName, LogError, "Send failure, error code: %d", nResult);
		return false;
	}

	if (static_cast<size_t>(nResult) != nSize)
	{
		pLogger->Write(AppleMIDIName, LogError, "Send failure, only %d/%d bytes sent", nResult, nSize);
		return false;
	}

#ifdef APPLEMIDI_DEBUG
	pLogger->Write(AppleMIDIName, LogNotice, "Sent %d bytes to port %d", nResult, nPort);
#endif

	return true;
}

bool CAppleMIDIParticipant::SendAcceptInvitationPacket(CSocket* pSocket, CIPAddress* pIPAddress, u16 nPort)
{
	TAppleMIDISession AcceptPacket =
	{
		htons(AppleMIDISignature),
		htons(InvitationAccepted),
		htonl(AppleMIDIVersion),
		htonl(m_nInitiatorToken),
		htonl(m_nSSRC),
		{'\0'}
	};

	// TODO: configurable name
	strncpy(AcceptPacket.Name, "mt32-pi", sizeof(AcceptPacket.Name));

#ifdef APPLEMIDI_DEBUG
	CLogger::Get()->Write(AppleMIDIName, LogNotice, "--> Accept invitation");
#endif

	const size_t nSendSize = NamelessSessionPacketSize + strlen(AcceptPacket.Name) + 1;
	return SendPacket(pSocket, pIPAddress, nPort, &AcceptPacket, nSendSize);
}

bool CAppleMIDIParticipant::SendRejectInvitationPacket(CSocket* pSocket, CIPAddress* pIPAddress, u16 nPort, u32 nInitiatorToken)
{
	TAppleMIDISession RejectPacket =
	{
		htons(AppleMIDISignature),
		htons(InvitationRejected),
		htonl(AppleMIDIVersion),
		htonl(nInitiatorToken),
		htonl(m_nSSRC),
		{'\0'}
	};

#ifdef APPLEMIDI_DEBUG
	CLogger::Get()->Write(AppleMIDIName, LogNotice, "--> Reject invitation");
#endif

	// Send without name
	return SendPacket(pSocket, pIPAddress, nPort, &RejectPacket, NamelessSessionPacketSize);
}

bool CAppleMIDIParticipant::SendSyncPacket(u64 nTimestamp1, u64 nTimestamp2)
{
	const TAppleMIDISync SyncPacket =
	{
		htons(AppleMIDISignature),
		htons(Sync),
		htonl(m_nSSRC),
		1,
		{0},
		{
			htonll(nTimestamp1),
			htonll(nTimestamp2),
			0
		}
	};

#ifdef APPLEMIDI_DEBUG
	CLogger::Get()->Write(AppleMIDIName, LogNotice, "--> Sync 1");
#endif

	return SendPacket(m_pMIDISocket, &m_InitiatorIPAddress, m_nInitiatorMIDIPort, &SyncPacket, sizeof(SyncPacket));
}

bool CAppleMIDIParticipant::SendFeedbackPacket()
{
	const TAppleMIDIReceiverFeedback FeedbackPacket =
	{
		htons(AppleMIDISignature),
		htons(ReceiverFeedback),
		htonl(m_nSSRC),
		htonl(m_nSequence << 16)
	};

#ifdef APPLEMIDI_DEBUG
	CLogger::Get()->Write(AppleMIDIName, LogNotice, "--> Feedback");
#endif

	return SendPacket(m_pControlSocket, &m_InitiatorIPAddress, m_nInitiatorControlPort, &FeedbackPacket, sizeof(FeedbackPacket));
}
