//
// mdnsresponder.h
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

#ifndef _mdnsresponder_h
#define _mdnsresponder_h

#include <circle/net/socket.h>
#include <circle/sched/task.h>

class CMDNSResponder : protected CTask
{
public:
	CMDNSResponder();
	virtual ~CMDNSResponder() override;

	bool Initialize();

	virtual void Run() override;

private:
	void Probe();
	void Announce();
	void Goodbye();

	static constexpr size_t MaxMDNSNameLength = 256;

	// UDP socket
	CSocket* m_pSocket;

	// Socket receive buffer
	u8 m_Buffer[FRAME_BUFFER_SIZE];
	char m_DNSName[MaxMDNSNameLength];

	// static int MDNSServiceCallback(mdns_socket_t sock, const struct sockaddr* from, size_t addrlen, mdns_entry_type_t entry,
	// 		uint16_t query_id, uint16_t rtype, uint16_t rclass, uint32_t ttl, const void* data,
	// 		size_t size, size_t name_offset, size_t name_length, size_t record_offset,
	// 		size_t record_length, void* user_data);
};

#endif
