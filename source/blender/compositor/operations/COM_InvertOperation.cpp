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

#include "COM_InvertOperation.h"

InvertOperation::InvertOperation(): NodeOperation() {
    this->addInputSocket(COM_DT_VALUE);
    this->addInputSocket(COM_DT_COLOR);
    this->addOutputSocket(COM_DT_VALUE);
    this->inputValueProgram = NULL;
    this->inputColorProgram = NULL;
    this->color = true;
    this->alpha = false;
    setResolutionInputSocketIndex(1);
}
void InvertOperation::initExecution() {
	this->inputValueProgram = this->getInputSocketReader(0);
	this->inputColorProgram = this->getInputSocketReader(1);
}

void InvertOperation::executePixel(float* out, float x, float y, MemoryBuffer *inputBuffers[]) {
    float inputValue[4];
    float inputColor[4];
	this->inputValueProgram->read(inputValue, x, y, inputBuffers);
	this->inputColorProgram->read(inputColor, x, y, inputBuffers);

    const float value = inputValue[0];
    const float invertedValue = 1.0f - value;

    if(color) {
        out[0] = (1.0f - inputColor[0])*value + inputColor[0]*invertedValue;
        out[1] = (1.0f - inputColor[1])*value + inputColor[1]*invertedValue;
        out[2] = (1.0f - inputColor[2])*value + inputColor[2]*invertedValue;
    } else {
        out[0] = inputColor[0];
        out[1] = inputColor[1];
        out[2] = inputColor[2];
    }

    if(alpha)
            out[3] = (1.0f - inputColor[3])*value + inputColor[3]*invertedValue;
    else
            out[3] = inputColor[3];

}

void InvertOperation::deinitExecution() {
    this->inputValueProgram = NULL;
    this->inputColorProgram = NULL;
}

