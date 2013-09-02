/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#include "COM_MixSaturationOperation.h"

extern "C" {
	#include "BLI_math.h"
}

MixSaturationOperation::MixSaturationOperation() : MixBaseOperation()
{
	/* pass */
}

void MixSaturationOperation::executePixel(float output[4], float x, float y, PixelSampler sampler)
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];

	this->m_inputValueOperation->read(inputValue, x, y, sampler);
	this->m_inputColor1Operation->read(inputColor1, x, y, sampler);
	this->m_inputColor2Operation->read(inputColor2, x, y, sampler);

	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}
	float valuem = 1.0f - value;
	
	float rH, rS, rV;
	rgb_to_hsv(inputColor1[0], inputColor1[1], inputColor1[2], &rH, &rS, &rV);
	if (rS != 0.0f) {
		float colH, colS, colV;
		rgb_to_hsv(inputColor2[0], inputColor2[1], inputColor2[2], &colH, &colS, &colV);
		hsv_to_rgb(rH, (valuem * rS + value * colS), rV, &output[0], &output[1], &output[2]);
	}
	else {
		copy_v3_v3(output, inputColor1);
	}

	output[3] = inputColor1[3];

	clampIfNeeded(output);
}
