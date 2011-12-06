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

#include "COM_ConvertVectorToColorOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

ConvertVectorToColorOperation::ConvertVectorToColorOperation(): NodeOperation() {
    this->addInputSocket(COM_DT_VECTOR);
    this->addOutputSocket(COM_DT_COLOR);
    this->inputOperation = NULL;
}

void ConvertVectorToColorOperation::initExecution() {
	this->inputOperation = this->getInputSocketReader(0);
}

void ConvertVectorToColorOperation::executePixel(float* outputValue, float x, float y, MemoryBuffer *inputBuffers[]) {
	inputOperation->read(outputValue, x, y, inputBuffers);
    outputValue[3] = 1.0f;
}

void ConvertVectorToColorOperation::deinitExecution() {
    this->inputOperation = NULL;
}
