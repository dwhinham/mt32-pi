//
// sh1106.cpp
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

#include "lcd/drivers/ssd1306.h"

enum TSH1106Command : u8
{
	SetColumnAddressLow  = 0x00,
	SetColumnAddressHigh = 0x10,
	SetStartLine         = 0x40,
	SetPageAddress       = 0xB0,
};

CSH1106::CSH1106(CI2CMaster *pI2CMaster, u8 nAddress, u8 nWidth, u8 nHeight, TLCDRotation Rotation)
	: CSSD1306(pI2CMaster, nAddress, nWidth, nHeight, Rotation)
{
}

void CSH1106::WriteFrameBuffer(bool bForceFullUpdate) const
{
	// Reset start line
	WriteCommand(SetStartLine | 0x00);

	const size_t nPages = m_nHeight / 8;
	constexpr size_t nPageSize = 128;

	// Copy framebuffer one page at a time
	for (u8 nPage = 0; nPage < nPages; ++nPage)
	{
		const size_t nOffset = nPage * nPageSize;
		const bool bNeedsUpdate = bForceFullUpdate || memcmp(&m_FrameBuffers[0].FrameBuffer[nOffset], &m_FrameBuffers[1].FrameBuffer[nOffset], nPageSize) != 0;

		if (bNeedsUpdate)
		{
			WriteCommand(SetPageAddress | nPage);

			// SH1106 displays have a 132x64 pixel memory, but most modules have a visible width of 128 centred on this buffer
			WriteCommand(SetColumnAddressLow | 0x02);
			WriteCommand(SetColumnAddressHigh | 0x00);

			// Prefix this page's pixel data with a data control byte
			u8 Buffer[nPageSize + 1] = { 0x40 };
			memcpy(Buffer + 1, &m_FrameBuffers[m_nCurrentFrameBuffer].FrameBuffer[nOffset], nPageSize);

			m_pI2CMaster->Write(m_nAddress, Buffer, sizeof(Buffer));

		}
	}
}
