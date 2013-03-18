/*
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __FREESTYLE_PSEUDO_NOISE_H__
#define __FREESTYLE_PSEUDO_NOISE_H__

/** \file blender/freestyle/intern/system/PseudoNoise.h
 *  \ingroup freestyle
 *  \brief Class to define a pseudo Perlin noise
 *  \author Fredo Durand
 *  \date 16/06/2003
 */

#include "FreestyleConfig.h"
#include "Precision.h"

class LIB_SYSTEM_EXPORT PseudoNoise
{
public:
	PseudoNoise();

	virtual ~PseudoNoise() {}

	real smoothNoise(real x);
	real linearNoise(real x);

	real turbulenceSmooth(real x, unsigned nbOctave = 8);
	real turbulenceLinear(real x, unsigned nbOctave = 8);

	static void init(long seed);

protected:
	static real *_values;
};

#endif // __FREESTYLE_PSEUDO_NOISE_H__