//
// ssd1306.cpp
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

#include <type_traits>

#include "lcd/drivers/ssd1306.h"
#include "lcd/font6x8.h"
#include "lcd/images.h"
#include "utility.h"

enum TSSD1306Command : u8
{
	SetMemoryAddressingMode    = 0x20,
	SetColumnAddress           = 0x21,
	SetPageAddress             = 0x22,
	SetStartLine               = 0x40,
	SetContrast                = 0x81,
	SetChargePump              = 0x8D,
	EntireDisplayOnResume      = 0xA4,
	SetNormalDisplay           = 0xA6,
	SetMultiplexRatio          = 0xA8,
	SetDisplayOff              = 0xAE,
	SetDisplayOn               = 0xAF,
	SetDisplayOffset           = 0xD3,
	SetDisplayClockDivideRatio = 0xD5,
	SetPrechargePeriod         = 0xD9,
	SetCOMPins                 = 0xDA,
	SetVCOMHDeselectLevel      = 0xDB,
};

// Compile-time (constexpr) font/graphics conversion functions.
// The SSD1306 stores pixel data in columns, but our source data is stored as rows.
// These templated functions generate column-wise versions of our graphics at compile-time.
namespace
{
	using CharData = u8[8];

	// Iterate through each row of the character data and collect bits for the nth column
	static constexpr u8 SingleColumn(const CharData& CharData, u8 nColumn)
	{
		u8 bit = 5 - nColumn;
		u8 column = 0;

		for (u8 i = 0; i < 8; ++i)
			column |= (CharData[i] >> bit & 1) << i;

		return column;
	}

	// Double the height of the character by duplicating column bits into a 16-bit value
	static constexpr u16 DoubleColumn(const CharData& CharData, u8 nColumn)
	{
		u8 singleColumn = SingleColumn(CharData, nColumn);
		u16 column = 0;

		for (u8 i = 0; i < 8; ++i)
		{
			bool bit = singleColumn >> i & 1;
			column |= bit << i * 2 | bit << (i * 2 + 1);
		}

		return column;
	}

	// Templated array-like structure with precomputed font data
	template<size_t N, class F>
	class Font
	{
	public:
		// Result type of conversion function determines array type
		using Column = typename std::result_of<F& (const CharData&, u8)>::type;
		using ColumnData = Column[6];

		constexpr Font(const CharData(&CharData)[N], F Function) : mCharData{ 0 }
		{
			for (size_t i = 0; i < N; ++i)
				for (u8 j = 0; j < 6; ++j)
					mCharData[i][j] = Function(CharData[i], j);
		}

		const ColumnData& operator[](size_t nIndex) const { return mCharData[nIndex]; }

	private:
		ColumnData mCharData[N];
	};

	// Templated array-like structure with precomputed pixel data
	template<size_t W, size_t H>
	class CSSD1306Image
	{
	private:
		static constexpr size_t SizeInBytes = W * H / 8;
		static constexpr size_t BytesPerRow = W / 8;

	public:
		constexpr CSSD1306Image(const u8(&PixelData)[SizeInBytes])
			: m_PixelData{0}
		{
			for (size_t i = 0; i < SizeInBytes; ++i)
			{
				const size_t nByteX = i * 8 % W;
				const size_t nByteY = i / BytesPerRow;

				for (u8 nBit = 0; nBit < 8; ++nBit)
				{
					if ((PixelData[i] >> (7 - nBit) & 1))
						m_PixelData[((nByteY & 0xF8) << 4) + nByteX + nBit] |= 1 << (nByteY & 7);
				}
			}
		}

		constexpr u8 Width() const { return W; }
		constexpr u8 Height() const { return H; }
		constexpr const u8* GetPixelData() const { return m_PixelData; }

		u8 operator[](size_t nIndex) const { return m_PixelData[nIndex]; }

	private:
		u8 m_PixelData[SizeInBytes];
	};
}

// Single and double-height versions of the font
constexpr auto FontSingle = Font<Utility::ArraySize(Font6x8), decltype(SingleColumn)>(Font6x8, SingleColumn);
constexpr auto FontDouble = Font<Utility::ArraySize(Font6x8), decltype(DoubleColumn)>(Font6x8, DoubleColumn);

constexpr auto MT32PiLogo = CSSD1306Image<128, 32>(MT32PiLogo128x32);
constexpr auto MisterLogo = CSSD1306Image<128, 32>(MisterLogo128x32);

// Drawing constants
constexpr u8 BarSpacing = 2;

CSSD1306::CSSD1306(CI2CMaster* pI2CMaster, u8 nAddress, u8 nWidth, u8 nHeight, TLCDRotation Rotation, TLCDMirror Mirror)
	: CLCD(nWidth, nHeight),
	  m_pI2CMaster(pI2CMaster),
	  m_nAddress(nAddress),
	  m_Rotation(Rotation),
	  m_Mirror(Mirror),

	  m_FrameBuffers{{0x40, {0}}, {0x40, {0}}},
	  m_nCurrentFrameBuffer(0)
{
}

bool CSSD1306::Initialize()
{
	assert(m_pI2CMaster != nullptr);

	// Validate dimensions - only 128x32, 128x64, and 132x{32, 64} (SSD1305) supported for now
	if (!(m_nHeight == 32 || m_nHeight == 64) || !(m_nWidth == 128 || m_nWidth == 132))
		return false;

	// HACK: Assume SSD1305 if width is 132 (visible width is usually 128 on these modules)
	// TODO: Some kind of abstraction between visible size and pixel memory size?
	const bool bIsSSD1305 = m_nWidth == 132;

	const u8 nMultiplexRatio  = m_nHeight - 1;
	const u8 nCOMPins         = (m_nHeight == 32 && !bIsSSD1305) ? 0x02 : 0x12;
	const u8 nColumnAddrRange = m_nWidth - 1;
	const u8 nPageAddrRange   = m_nHeight / 8 - 1;

	// https://www.buydisplay.com/download/ic/SSD1312_Datasheet.pdf Pg. 51 Section 2.1.19
	//            normal    inverted
	// normal     A1 C8       A0 C0
	// mirrored   A0 C8       A1 C0
	const u8 nSegRemap        = (m_Rotation == TLCDRotation::Inverted && m_Mirror == TLCDMirror::Normal) ||
                                    (m_Rotation == TLCDRotation::Normal   && m_Mirror == TLCDMirror::Mirrored) ? 0xA0 : 0xA1;
	const u8 nCOMScanDir      = m_Rotation == TLCDRotation::Inverted ? 0xC0 : 0xC8;

	const u8 InitSequence[] =
	{
		SetDisplayOff,
		SetDisplayClockDivideRatio,	0x80,				// Default value
		SetMultiplexRatio,		nMultiplexRatio,		// Screen height - 1
		SetDisplayOffset,		0x00,				// None
		SetStartLine | 0x00,						// Set start line
		SetChargePump,			0x14,				// Enable charge pump
		SetMemoryAddressingMode,	0x00,				// 00 = horizontal
		nSegRemap,
		nCOMScanDir,							// COM output scan direction
		SetCOMPins,			nCOMPins,			// Alternate COM config and disable COM left/right
		SetContrast,			0x7F,				// 00-FF, default to half
		SetPrechargePeriod,		0x22,				// Default value
		SetVCOMHDeselectLevel,		0x20,				// Default value
		EntireDisplayOnResume,						// Resume to RAM content display
		SetNormalDisplay,
		SetDisplayOn,
		SetColumnAddress,		0x00,	nColumnAddrRange,
		SetPageAddress,			0x00,	nPageAddrRange,
	};

	for (u8 nCommand : InitSequence)
		WriteCommand(nCommand);

	return true;
}

void CSSD1306::WriteCommand(u8 nCommand) const
{
	const u8 Buffer[] = { 0x80, nCommand };
	m_pI2CMaster->Write(m_nAddress, Buffer, sizeof(Buffer));
}

void CSSD1306::WriteFrameBuffer(bool bForceFullUpdate) const
{
	// Reset start line
	WriteCommand(SetStartLine | 0x00);

	// Compare two framebuffers
	const size_t nFrameBufferSize = m_nWidth * m_nHeight / 8;
	const bool bNeedsUpdate = bForceFullUpdate || memcmp(m_FrameBuffers[0].FrameBuffer, m_FrameBuffers[1].FrameBuffer, nFrameBufferSize) != 0;

	// Copy entire framebuffer
	if (bNeedsUpdate)
		m_pI2CMaster->Write(m_nAddress, &m_FrameBuffers[m_nCurrentFrameBuffer], sizeof(TFrameBufferUpdatePacket::DataControlByte) + nFrameBufferSize);
}

void CSSD1306::SwapFrameBuffers()
{
	// Make other framebuffer current
	m_nCurrentFrameBuffer = (m_nCurrentFrameBuffer + 1) % 2;
}

void CSSD1306::SetPixel(u8 nX, u8 nY)
{
	// Ensure range is within 0-127 for x, 0-63 for y
	nX &= 0x7F;
	nY &= 0x3F;

	u8* pFrameBuffer = m_FrameBuffers[m_nCurrentFrameBuffer].FrameBuffer;
	pFrameBuffer[((nY & 0xF8) << 4) + nX] |= 1 << (nY & 7);
}

void CSSD1306::ClearPixel(u8 nX, u8 nY)
{
	// Ensure range is within 0-127 for x, 0-63 for y
	nX &= 0x7F;
	nY &= 0x3F;

	u8* pFrameBuffer = m_FrameBuffers[m_nCurrentFrameBuffer].FrameBuffer;
	pFrameBuffer[((nY & 0xF8) << 4) + nX] &= ~(1 << (nY & 7));
}

void CSSD1306::DrawFilledRect(u8 nX1, u8 nY1, u8 nX2, u8 nY2, bool bImmediate)
{
	if (nX1 >= m_nWidth || nX2 >= m_nWidth || nY1 >= m_nHeight || nY2 >= m_nHeight)
		return;

	if (nX1 > nX2)
		Utility::Swap(nX1, nX2);

	if (nY1 > nY2)
		Utility::Swap(nY1, nY2);

	const u8 nStartPage = nY1 / 8;
	const u8 nEndPage   = nY2 / 8;
	const u8 nMidPage   = nEndPage - nStartPage;

	u8* pFrameBuffer = m_FrameBuffers[m_nCurrentFrameBuffer].FrameBuffer;
	u8* pPixel       = &pFrameBuffer[nStartPage * m_nWidth + nX1];
	u8 nMask         = 0xFF << (nY1 & 7);

	// Rectangle starts and ends within the same page
	if (nMidPage == 0)
		nMask &= (0xFF >> (7 - (nY2 & 7)));

	// Draw top page
	for (u8 nX = nX1; nX <= nX2; ++nX)
		*pPixel++ |= nMask;

	// Draw middle pages
	if (nMidPage > 1)
	{
		nMask = 0xFF;
		for (u8 nY = 1; nY < nMidPage; ++nY)
		{
			pPixel = &pFrameBuffer[nStartPage * m_nWidth + nX1 + nY * m_nWidth];
			for (u8 nX = nX1; nX <= nX2; ++nX)
				*pPixel++ = nMask;
		}
	}

	// Draw bottom page
	if (nMidPage >= 1)
	{
		nMask  = 0xFF >> (7 - (nY2 & 7));
		pPixel = &pFrameBuffer[nEndPage * m_nWidth + nX1];
		for (u8 nX = nX1; nX <= nX2; ++nX)
			*pPixel++ |= nMask;
	}

	if (bImmediate)
		WriteFrameBuffer(true);
}

void CSSD1306::DrawChar(char chChar, u8 nCursorX, u8 nCursorY, bool bInverted, bool bDoubleWidth)
{
	const size_t nRowOffset    = nCursorY * m_nWidth * 2;
	const size_t nColumnOffset = nCursorX * (bDoubleWidth ? 12 : 6) + 4;
	u8* pFrameBuffer           = m_FrameBuffers[m_nCurrentFrameBuffer].FrameBuffer;

	// FIXME: Won't be needed when the full font is implemented in font6x8.h
	if (chChar == '\xFF')
		chChar = '\x80';

	else if (chChar < ' ')
		chChar = ' ';

	for (u8 i = 0; i < 6; ++i)
	{
		u16 nFontColumn = FontDouble[static_cast<u8>(chChar - ' ')][i];

		// Don't invert the leftmost column or last two rows
		if (i > 0 && bInverted)
			nFontColumn ^= 0x3FFF;

		// Shift down by 2 pixels
		nFontColumn <<= 2;

		// Upper half of font
		const size_t nOffset = nRowOffset + nColumnOffset + (bDoubleWidth ? i * 2 : i);

		pFrameBuffer[nOffset] = nFontColumn & 0xFF;
		pFrameBuffer[nOffset + m_nWidth] = (nFontColumn >> 8) & 0xFF;
		if (bDoubleWidth)
		{
			pFrameBuffer[nOffset + 1] = pFrameBuffer[nOffset];
			pFrameBuffer[nOffset + m_nWidth + 1] = pFrameBuffer[nOffset + m_nWidth];
		}
	}
}

void CSSD1306::Flip()
{
	WriteFrameBuffer();
	SwapFrameBuffers();
}

void CSSD1306::DrawImage(TImage Image, bool bImmediate)
{
	u8* pFrameBuffer = m_FrameBuffers[m_nCurrentFrameBuffer].FrameBuffer;
	const CSSD1306Image<128, 32>* pImage;

	switch (Image)
	{
		case TImage::MT32PiLogo:
			pImage = &MT32PiLogo;
			break;

		case TImage::MisterLogo:
			pImage = &MisterLogo;
			break;

		default:
			return;
	}

	const u8* pPixelData  = pImage->GetPixelData();
	const u8 nImageWidth  = pImage->Width();
	const u8 nImageHeight = pImage->Height();
	const size_t nBytes   = nImageWidth * nImageHeight / 8;

	// Exact framebuffer size match
	if (nImageWidth == m_nWidth && nImageHeight == m_nHeight)
	{
		memcpy(pFrameBuffer, pPixelData, nBytes);
		if (bImmediate)
			WriteFrameBuffer(true);
		return;
	}

	// Center the image
	const size_t nOffsetX = (m_nWidth - nImageWidth) / 2;
	const size_t nOffsetY = (m_nHeight - nImageHeight) / 2 / 8 * m_nWidth;

	for (size_t i = 0; i < nBytes; ++i)
	{
		const size_t nImageX = i % nImageWidth;
		const size_t nImageY = i / nImageWidth * m_nWidth;
		pFrameBuffer[nOffsetX + nOffsetY + nImageX + nImageY] = pPixelData[i];
	}

	if (bImmediate)
		WriteFrameBuffer(true);
}

void CSSD1306::Print(const char* pText, u8 nCursorX, u8 nCursorY, bool bClearLine, bool bImmediate)
{
	if (bClearLine)
	{
		for (u8 nChar = 0; nChar < nCursorX; ++nChar)
			DrawChar(' ', nChar, nCursorY);
	}

	while (*pText && nCursorX < 20)
	{
		DrawChar(*pText++, nCursorX, nCursorY);
		++nCursorX;
	}

	if (bClearLine)
	{
		while (nCursorX < 20)
			DrawChar(' ', nCursorX++, nCursorY);
	}

	if (bImmediate)
		WriteFrameBuffer(true);
}

void CSSD1306::Clear(bool bImmediate)
{
	u8* pFrameBuffer = m_FrameBuffers[m_nCurrentFrameBuffer].FrameBuffer;
	memset(pFrameBuffer, 0, m_nWidth * m_nHeight / 8);

	if (bImmediate)
		WriteFrameBuffer(true);
}

void CSSD1306::SetBacklightState(bool bEnabled)
{
	m_bBacklightEnabled = bEnabled;

	// Power on/off display
	WriteCommand(bEnabled ? SetDisplayOn : SetDisplayOff);
}
