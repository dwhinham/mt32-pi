//
// fxprofile.h
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

#ifndef _fxprofile_h
#define _fxprofile_h

#include "optional.h"

struct TFXProfile
{
	TOptional<float> nGain;

	TOptional<bool> bReverbActive;
	TOptional<float> nReverbDamping;
	TOptional<float> nReverbLevel;
	TOptional<float> nReverbRoomSize;
	TOptional<float> nReverbWidth;

	TOptional<bool> bChorusActive;
	TOptional<float> nChorusDepth;
	TOptional<float> nChorusLevel;
	TOptional<int> nChorusVoices;
	TOptional<float> nChorusSpeed;
};

#endif
