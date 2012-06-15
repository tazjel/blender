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

#include "COM_MixAddOperation.h"

MixAddOperation::MixAddOperation() : MixBaseOperation()
{
	/* pass */
}

void MixAddOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	float inputColor1[4];
	float inputColor2[4];
	float inputValue[4];
	
	inputValueOperation->read(inputValue, x, y, sampler, inputBuffers);
	inputColor1Operation->read(inputColor1, x, y, sampler, inputBuffers);
	inputColor2Operation->read(inputColor2, x, y, sampler, inputBuffers);


	float value = inputValue[0];
	if (this->useValueAlphaMultiply()) {
		value *= inputColor2[3];
	}
	outputValue[0] = inputColor1[0] + value * inputColor2[0];
	outputValue[1] = inputColor1[1] + value * inputColor2[1];
	outputValue[2] = inputColor1[2] + value * inputColor2[2];
	outputValue[3] = inputColor1[3];
}

