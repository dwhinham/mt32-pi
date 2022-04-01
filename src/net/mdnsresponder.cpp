//
// mdsnresponder.cpp
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

#include <cstdio>

#include <circle/logger.h>
#include <circle/net/in.h>
#include <circle/net/netsubsystem.h>
#include <circle/sched/scheduler.h>

#include <mdns.h>

#include "net/mdnsresponder.h"
#include "utility.h"

constexpr u32 MDNSAddress = Utility::IPAddressToInteger(224, 0, 0, 251);
constexpr u16 MDNSPort = 5353;

const char MDNSResponderName[] = "mdns";

struct TDNSHeader
{
	u16 nID;
	u16 nFlags;
#define DNS_FLAGS_QR		0x8000
#define DNS_FLAGS_OPCODE	0x7800
	#define DNS_FLAGS_OPCODE_QUERY		0x0000
	#define DNS_FLAGS_OPCODE_IQUERY		0x0800
	#define DNS_FLAGS_OPCODE_STATUS		0x1000
#define DNS_FLAGS_AA		0x0400
#define DNS_FLAGS_TC		0x0200
#define DNS_FLAGS_RD		0x0100
#define DNS_FLAGS_RA		0x0080
#define DNS_FLAGS_RCODE		0x000F
	#define DNS_RCODE_SUCCESS		0x0000
	#define DNS_RCODE_FORMAT_ERROR		0x0001
	#define DNS_RCODE_SERVER_FAILURE	0x0002
	#define DNS_RCODE_NAME_ERROR		0x0003
	#define DNS_RCODE_NOT_IMPLEMENTED	0x0004
	#define DNS_RCODE_REFUSED		0x0005
	u16 nQDCount;
	u16 nANCount;
	u16 nNSCount;
	u16 nARCount;
}
PACKED;

struct TDNSResourceRecordTrailerAIN
{
	u16 nType;
	u16 nClass;
	u32 nTTL;
	u16 nRDLength;
#define DNS_RDLENGTH_AIN	4
	unsigned char  RData[DNS_RDLENGTH_AIN];
}
PACKED;

CMDNSResponder::CMDNSResponder()
	: CTask(TASK_STACK_SIZE, true),
	  m_pSocket(nullptr),
	  m_Buffer{0}
{
	snprintf(m_DNSName, sizeof(m_DNSName), "_%s", "mt32-pi");
}

CMDNSResponder::~CMDNSResponder()
{
	if (m_pSocket)
		delete m_pSocket;
}

bool CMDNSResponder::Initialize()
{
	assert(m_pSocket == nullptr);

	CLogger* const pLogger    = CLogger::Get();
	CNetSubSystem* const pNet = CNetSubSystem::Get();

	if ((m_pSocket = new CSocket(pNet, IPPROTO_UDP)) == nullptr)
		return false;

	if (m_pSocket->Bind(MDNSPort) != 0)
	{
		pLogger->Write(MDNSResponderName, LogError, "Couldn't bind to port %d", MDNSPort);
		return false;
	}

	if (m_pSocket->SetOptionBroadcast(true) != 0)
	{
		pLogger->Write(MDNSResponderName, LogError, "Couldn't enable broadcast packet for socket");
		return false;
	}

	// We started as a suspended task; run now that initialization is successful
	Start();

	return true;
}

void CMDNSResponder::Run()
{
	assert(m_pSocket != nullptr);

	//CLogger* const pLogger       = CLogger::Get();
	CScheduler* const pScheduler = CScheduler::Get();

	// CIPAddress MDNSIPAddress(MDNSAddress);//Utility::IPAddressToInteger(192, 168, 175, 30));

	// char buf[] = "hello world";

	// while (true)
	// {
	// 	m_pSocket->SendTo(buf, sizeof(buf), 0, MDNSIPAddress, MDNSPort);
	// 	pScheduler->Sleep(1);
	// }

	// return;


	// mdns_record_t Record =
	// {
	// 	{ "_applemidi", 10 },
	// 	MDNS_RECORDTYPE_PTR,
	// 	{
	// 		.ptr.name = ""
	// 	}
	// };
	// mdns_record_t Record;
	// Record.name = { MDNS_STRING_CONST("_applemidi._udp.local.") };
	// Record.type = MDNS_RECORDTYPE_PTR;
	// Record.data.ptr.name = { MDNS_STRING_CONST("mt32-pi._applemidi._udp.local.") };
	// Record.rclass = 0;
	// Record.ttl = 0;


return;

	while (true)
	{
		// char buf[] = "hello world";
		// CIPAddress MDNSIPAddress(MDNSAddress);
		// m_pSocket->SendTo(buf, sizeof(buf), 0, MDNSIPAddress, MDNSPort);
		// pScheduler->Sleep(1);

		mdns_record_t Record =
		{
			.name = { MDNS_STRING_CONST("_apple-midi._udp.local.") },
			.type = MDNS_RECORDTYPE_PTR,
			.data = { .ptr = { .name = { MDNS_STRING_CONST("mt32-pi._apple-midi._udp.local.") } } },
			.rclass = 0,
			.ttl = 0
		};

		assert(mdns_announce_multicast(m_pSocket, m_Buffer, sizeof(m_Buffer), Record, nullptr, 0, nullptr, 0) == 0);
		pScheduler->Sleep(2);

		assert(mdns_announce_multicast(m_pSocket, m_Buffer, sizeof(m_Buffer), Record, nullptr, 0, nullptr, 0) == 0);
		pScheduler->Sleep(10);

		assert(mdns_announce_multicast(m_pSocket, m_Buffer, sizeof(m_Buffer), Record, nullptr, 0, nullptr, 0) == 0);
		pScheduler->Sleep(20);

		assert(mdns_goodbye_multicast(m_pSocket, m_Buffer, sizeof(m_Buffer), Record, nullptr, 0, nullptr, 0) == 0);
		pScheduler->Sleep(2);

		// // Non-blocking call
		// const int nResult = m_pSocket->Receive(m_ReceiveBuffer, sizeof(m_ReceiveBuffer), MSG_DONTWAIT);
		// m_pSocket->ReceiveFrom()

		// if (nResult < 0)
		// 	pLogger->Write(MDNSResponderName, LogError, "Socket receive error: %d", nResult);
		// else if (nResult > 0)
		// {
		// 	pLogger->Write(MDNSResponderName, LogDebug, "Received something of size %d", nResult);
		// }

		// TDNSHeader* Header = reinterpret_cast<TDNSHeader*>(m_ReceiveBuffer);
		// Header->nID = htons(0);
		// Header->nFlags = htons(DNS_FLAGS_QR | DNS_FLAGS_AA);
		// Header->nQDCount = htons(0);
		// Header->nANCount = htons(1);
		// Header->nNSCount = htons(0);
		// Header->nARCount = htons(0);

		// u8* pPtr = m_ReceiveBuffer + sizeof(TDNSHeader);
		// *pPtr++ = 0x0B;
		// *pPtr++ = '_';
		// *pPtr++ = 'a';
		// *pPtr++ = 'p';
		// *pPtr++ = 'p';
		// *pPtr++ = 'l';
		// *pPtr++ = 'e';
		// *pPtr++ = '-';
		// *pPtr++ = 'm';
		// *pPtr++ = 'i';
		// *pPtr++ = 'd';
		// *pPtr++ = 'i';
		// *pPtr++ = 0x04;
		// *pPtr++ = '_';
		// *pPtr++ = 'u';
		// *pPtr++ = 'd';
		// *pPtr++ = 'p';
		// *pPtr++ = 0x05;
		// *pPtr++ = 'l';
		// *pPtr++ = 'o';
		// *pPtr++ = 'c';
		// *pPtr++ = 'a';
		// *pPtr++ = 'l';
		// *pPtr++ = '\0';

		// TDNSResourceRecordTrailerAIN* Record = reinterpret_cast<TDNSResourceRecordTrailerAIN*>(pPtr);
		// Record->nType = 0x0C;
		// Record->nClass = 0x01;
		// Record->nTTL = 120;
		// //Record->nRDLength

		// const u8 nNameLength = strlen(m_DNSName);
		// *pPtr++ = nNameLength;
		// memcpy(reinterpret_cast<char*>(pPtr), m_DNSName, nNameLength);//, sizeof(m_ReceiveBuffer) - (pPtr - m_ReceiveBuffer));
		// pPtr += nNameLength;

		// // Allow other tasks to run
		// //pScheduler->Yield();
		// pScheduler->Sleep(10);
	}
}
