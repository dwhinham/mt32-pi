//
// ssd1306.cpp
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

#include <type_traits>

#include "lcd/font6x8.h"
#include "lcd/images.h"
#include "lcd/ssd1306.h"
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

constexpr auto MisterLogo = CSSD1306Image<128, 32>(MisterLogo128x32);

// Drawing constants
constexpr u8 BarSpacing = 2;

CSSD1306::CSSD1306(CI2CMaster *pI2CMaster, u8 nAddress, u8 nWidth, u8 nHeight, TLCDRotation Rotation)
	: CSynthLCD(),
	  m_pI2CMaster(pI2CMaster),
	  m_nAddress(nAddress),
	  m_nWidth(nWidth),
	  m_nHeight(nHeight),
	  m_Rotation(Rotation),

	  m_FrameBuffers{{0x40, {0}}, {0x40, {0}}},
	  m_nCurrentFrameBuffer(0)
{
}

bool CSSD1306::Initialize()
{
	assert(m_pI2CMaster != nullptr);

	// Validate dimensions - only 128x32 and 128x64 supported for now
	if (!(m_nHeight == 32 || m_nHeight == 64) || m_nWidth != 128)
		return false;

	const u8 nMultiplexRatio  = m_nHeight - 1;
	const u8 nCOMPins         = m_nHeight == 32 ? 0x02 : 0x12;
	const u8 nColumnAddrRange = m_nWidth - 1;
	const u8 nPageAddrRange   = m_nHeight / 8 - 1;
	const u8 nSegRemap        = m_Rotation == TLCDRotation::Inverted ? 0xA0 : 0xA1;
	const u8 nCOMScanDir      = m_Rotation == TLCDRotation::Inverted ? 0xC0 : 0xC8;

	const u8 InitSequence[] =
	{
		SetDisplayOff,
		SetDisplayClockDivideRatio,	0x80,					// Default value
		SetMultiplexRatio,			nMultiplexRatio,		// Screen height - 1
		SetDisplayOffset,			0x00,					// None
		SetStartLine | 0x00,								// Set start line
		SetChargePump,				0x14,					// Enable charge pump
		SetMemoryAddressingMode,	0x00,					// 00 = horizontal
		nSegRemap,
		nCOMScanDir,										// COM output scan direction
		SetCOMPins,					nCOMPins,				// Alternate COM config and disable COM left/right
		SetContrast,				0x7F,					// 00-FF, default to half
		SetPrechargePeriod,			0x22,					// Default value
		SetVCOMHDeselectLevel,		0x20,					// Default value
		EntireDisplayOnResume,								// Resume to RAM content display
		SetNormalDisplay,
		SetDisplayOn,
		SetColumnAddress,			0x00,	nColumnAddrRange,
		SetPageAddress,				0x00,	nPageAddrRange,
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

void CSSD1306::DrawChar(char chChar, u8 nCursorX, u8 nCursorY, bool bInverted, bool bDoubleWidth)
{
	const size_t nRowOffset    = nCursorY * m_nWidth * 2;
	const size_t nColumnOffset = nCursorX * (bDoubleWidth ? 12 : 6) + 4;
	u8* pFrameBuffer           = m_FrameBuffers[m_nCurrentFrameBuffer].FrameBuffer;

	// FIXME: Won't be needed when the full font is implemented in font6x8.h
	if (chChar == '\xFF')
		chChar = '\x80';

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

void CSSD1306::DrawImage()
{
	u8* pFrameBuffer     = m_FrameBuffers[m_nCurrentFrameBuffer].FrameBuffer;
	const u8* pPixelData = nullptr;
	u8 nImageWidth       = 0;
	u8 nImageHeight      = 0;

	switch (m_CurrentImage)
	{
		case TImage::MisterLogo:
			pPixelData   = MisterLogo.GetPixelData();
			nImageWidth  = MisterLogo.Width();
			nImageHeight = MisterLogo.Height();
			break;

		default:
			return;
	}

	const size_t nBytes = nImageWidth * nImageHeight / 8;

	// Exact framebuffer size match
	if (nImageWidth == m_nWidth && nImageHeight == m_nHeight)
	{
		memcpy(pFrameBuffer, pPixelData, nBytes);
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
}

void CSSD1306::DrawSystemState()
{
	if (m_SystemState == TSystemState::DisplayingImage)
		DrawImage();
	else
	{
		const u8 nMessageRow = m_nHeight == 32 ? 0 : 1;
		Print(m_SystemMessageTextBuffer, 0, nMessageRow, true);
	}
}

void CSSD1306::DrawChannelLevels(u8 nFirstRow, u8 nRows, u8 nBarXOffset, u8 nBarWidth, u8 nBarSpacing, u8 nChannels, bool bDrawPeaks, bool bDrawBarBases)
{
	const size_t nFirstPageOffset = nFirstRow * m_nWidth;
	const u8 nTotalPages          = nRows;
	const u8 nBarHeight           = nRows * 8;
	u8* pFrameBuffer              = m_FrameBuffers[m_nCurrentFrameBuffer].FrameBuffer;

	// For each channel
	for (u8 i = 0; i < nChannels; ++i)
	{
		u8 PageValues[nTotalPages];
		u8 nPartLevelPixels = m_ChannelLevels[i] * nBarHeight;
		if (bDrawBarBases && nPartLevelPixels == 0)
			nPartLevelPixels = 1;

		const u8 nPeakLevelPixels = m_ChannelPeakLevels[i] * nBarHeight;
		const u8 nFullPages       = nPartLevelPixels / 8;
		const u8 nRemainder       = nPartLevelPixels % 8;

		for (u8 j = 0; j < nFullPages; ++j)
			PageValues[j] = 0xFF;

		for (u8 j = nFullPages; j < nTotalPages; ++j)
			PageValues[j] = 0;

		if (nRemainder)
			PageValues[nFullPages] = 0xFF << (8 - nRemainder);

		// Peak meters
		if (bDrawPeaks && nPeakLevelPixels)
		{
			const u8 nPeakPage      = nPeakLevelPixels / 8;
			const u8 nPeakRemainder = nPeakLevelPixels % 8;

			if (nPeakRemainder)
				PageValues[nPeakPage] |= 1 << (8 - nPeakRemainder);
			else
				PageValues[nPeakPage - 1] |= 1;
		}

		// For each bar column
		for (u8 j = 0; j < nBarWidth; ++j)
		{
			// For each bar row
			for (u8 k = 0; k < nTotalPages; ++k)
			{
				size_t nOffset = nFirstPageOffset;

				// Start BarXOffset pixels from the left
				nOffset += nBarXOffset;

				// Start from bottom-most page
				nOffset += (nTotalPages - 1) * m_nWidth - k * m_nWidth;

				// i'th bar + j'th bar column
				nOffset += i * (nBarWidth + nBarSpacing) + j;

				pFrameBuffer[nOffset] = PageValues[k];
			}
		}
	}
}

void CSSD1306::DrawSC55Dots(u8 nFirstRow, u8 nRows)
{
	// Pixel data is 16x16, scale to 128x64 or 64x32 and center
	const u8 nScaleX      = m_nHeight == 64 ? 8 : 4;
	const u8 nScaleY      = m_nHeight == 64 ? 4 : 2;
	const size_t nOffsetX = (m_nWidth - 16 * nScaleX) / 2;
	const size_t nOffsetY = (m_nHeight - 16 * nScaleY) / 2;

	for (u8 nByte = 0; nByte < sizeof(m_SC55PixelBuffer); ++nByte)
	{
		// First 48 bytes have 5 columns of pixels, last 16 bytes have only 1
		const u8 nPixels = nByte < 48 ? 5 : 1;

		for (u8 nPixel = 0; nPixel < nPixels; ++nPixel)
		{
			const bool bPixelValue = (m_SC55PixelBuffer[nByte] >> (5 - 1 - nPixel)) & 1;

			if (!bPixelValue)
				continue;

			const u8 nPosX = nByte / 16 * 5 + nPixel;
			const u8 nPosY = nByte % 16;

			const u8 nScaledX = nOffsetX + nPosX * nScaleX;
			const u8 nScaledY = nOffsetY + nPosY * nScaleY;

			for (u8 nX = 0; nX < nScaleX; ++nX)
			{
				for (u8 nY = 0; nY < nScaleY; ++nY)
				{
					SetPixel(nScaledX + nX, nScaledY + nY);
				}
			}
		}
	}
}

void CSSD1306::Print(const char* pText, u8 nCursorX, u8 nCursorY, bool bClearLine, bool bImmediate)
{
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

void CSSD1306::SetBacklightEnabled(bool bEnabled)
{
	CSynthLCD::SetBacklightEnabled(bEnabled);

	// Power on/off display
	WriteCommand(bEnabled ? SetDisplayOn : SetDisplayOff);
}

void CSSD1306::Update(CMT32Synth& Synth)
{
	CSynthLCD::Update(Synth);

	// Bail out if display is off
	if (!m_bBacklightEnabled)
		return;

	Clear(false);
	UpdateChannelLevels(Synth);
	UpdateChannelPeakLevels();

	if (m_SystemState != TSystemState::None)
		DrawSystemState();
	else
	{
		const u8 nRows = m_nHeight / 8 - 2;
		const u8 nBarWidth = (m_nWidth - (MT32ChannelCount * BarSpacing)) / MT32ChannelCount;
		DrawChannelLevels(0, nRows, 2, nBarWidth, BarSpacing, MT32ChannelCount, true, false);
	}

	// MT-32 status row
	if (m_SystemState != TSystemState::EnteringPowerSavingMode && m_SystemState != TSystemState::DisplayingImage)
	{
		const u8 nStatusRow = m_nHeight == 32 ? 1 : 3;
		Print(m_MT32TextBuffer, 0, nStatusRow, true);
	}

	WriteFrameBuffer();
	SwapFrameBuffers();
}

void CSSD1306::Update(CSoundFontSynth& Synth)
{
	CSynthLCD::Update(Synth);

	// Bail out if display is off
	if (!m_bBacklightEnabled)
		return;

	Clear(false);
	UpdateChannelLevels(Synth);
	UpdateChannelPeakLevels();

	if (m_SystemState != TSystemState::None)
		DrawSystemState();
	else
	{
		const u8 nRows = m_nHeight / 8;
		if (m_bSC55DisplayingDots)
			DrawSC55Dots(0, nRows);
		else
		{
			const u8 nBarWidth = (m_nWidth - (MIDIChannelCount * BarSpacing)) / MIDIChannelCount;
			DrawChannelLevels(0, nRows, 1, nBarWidth, BarSpacing, MIDIChannelCount);
		}
	}

	WriteFrameBuffer();
	SwapFrameBuffers();
}
