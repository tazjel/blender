/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 
 * The Original Code is: some of this file.
 *
 * ***** END GPL LICENSE BLOCK *****
 * */

#include "BLI_math.h"

#ifndef BLI_MATH_VECTOR_INLINE
#define BLI_MATH_VECTOR_INLINE

/********************************** Init *************************************/

MINLINE void zero_v2(float r[2])
{
	r[0]= 0.0f;
	r[1]= 0.0f;
}

MINLINE void zero_v3(float r[3])
{
	r[0]= 0.0f;
	r[1]= 0.0f;
	r[2]= 0.0f;
}

MINLINE void copy_v2_v2(float r[2], const float a[2])
{
	r[0]= a[0];
	r[1]= a[1];
}

MINLINE void copy_v3_v3(float r[3], const float a[3])
{
	r[0]= a[0];
	r[1]= a[1];
	r[2]= a[2];
}

MINLINE void swap_v2_v2(float a[2], float b[2])
{
	SWAP(float, a[0], b[0]);
	SWAP(float, a[1], b[1]);
}

MINLINE void swap_v3_v3(float a[3], float b[3])
{
	SWAP(float, a[0], b[0]);
	SWAP(float, a[1], b[1]);
	SWAP(float, a[2], b[2]);
}

/********************************* Arithmetic ********************************/

MINLINE void add_v2_v2(float *r, float *a)
{
	r[0] += a[0];
	r[1] += a[1];
}

MINLINE void add_v2_v2v2(float *r, float *a, float *b)
{
	r[0]= a[0] + b[0];
	r[1]= a[1] + b[1];
}

MINLINE void add_v3_v3(float *r, float *a)
{
	r[0] += a[0];
	r[1] += a[1];
	r[2] += a[2];
}

MINLINE void add_v3_v3v3(float *r, float *a, float *b)
{
	r[0]= a[0] + b[0];
	r[1]= a[1] + b[1];
	r[2]= a[2] + b[2];
}

MINLINE void sub_v2_v2(float *r, float *a)
{
	r[0] -= a[0];
	r[1] -= a[1];
}

MINLINE void sub_v2_v2v2(float *r, float *a, float *b)
{
	r[0]= a[0] - b[0];
	r[1]= a[1] - b[1];
}

MINLINE void sub_v3_v3(float *r, float *a)
{
	r[0] -= a[0];
	r[1] -= a[1];
	r[2] -= a[2];
}

MINLINE void sub_v3_v3v3(float r[3], const float a[3], const float b[3])
{
	r[0]= a[0] - b[0];
	r[1]= a[1] - b[1];
	r[2]= a[2] - b[2];
}

MINLINE void mul_v2_fl(float *v1, float f)
{
	v1[0]*= f;
	v1[1]*= f;
}

MINLINE void mul_v3_fl(float r[3], float f)
{
	r[0] *= f;
	r[1] *= f;
	r[2] *= f;
}

MINLINE void mul_v3_v3fl(float r[3], float a[3], float f)
{
	r[0]= a[0]*f;
	r[1]= a[1]*f;
	r[2]= a[2]*f;
}

MINLINE void mul_v3_v3(float r[3], float a[3])
{
	r[0] *= a[0];
	r[1] *= a[1];
	r[2] *= a[2];
}

MINLINE void madd_v3_v3fl(float r[3], float a[3], float f)
{
	r[0] += a[0]*f;
	r[1] += a[1]*f;
	r[2] += a[2]*f;
}

MINLINE void madd_v3_v3v3(float r[3], float a[3], float b[3])
{
	r[0] += a[0]*b[0];
	r[1] += a[1]*b[1];
	r[2] += a[2]*b[2];
}

MINLINE void madd_v3_v3v3fl(float r[3], float a[3], float b[3], float f)
{
	r[0] = a[0] + b[0]*f;
	r[1] = a[1] + b[1]*f;
	r[2] = a[2] + b[2]*f;
}

MINLINE void madd_v3_v3v3v3(float r[3], float a[3], float b[3], float c[3])
{
	r[0] = a[0] + b[0]*c[0];
	r[1] = a[1] + b[1]*c[1];
	r[2] = a[2] + b[2]*c[2];
}

MINLINE void mul_v3_v3v3(float *v, float *v1, float *v2)
{
	v[0] = v1[0] * v2[0];
	v[1] = v1[1] * v2[1];
	v[2] = v1[2] * v2[2];
}

MINLINE void negate_v3(float r[3])
{
	r[0]= -r[0];
	r[1]= -r[1];
	r[2]= -r[2];
}

MINLINE void negate_v3_v3(float r[3], const float a[3])
{
	r[0]= -a[0];
	r[1]= -a[1];
	r[2]= -a[2];
}

MINLINE float dot_v2v2(const float a[2], const float b[2])
{
	return a[0]*b[0] + a[1]*b[1];
}

MINLINE float dot_v3v3(const float a[3], const float b[3])
{
	return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

MINLINE float cross_v2v2(const float a[2], const float b[2])
{
	 return a[0]*b[1] - a[1]*b[0];
}

MINLINE void cross_v3_v3v3(float r[3], const float a[3], const float b[3])
{
	r[0]= a[1]*b[2] - a[2]*b[1];
	r[1]= a[2]*b[0] - a[0]*b[2];
	r[2]= a[0]*b[1] - a[1]*b[0];
}

MINLINE void star_m3_v3(float mat[][3], float *vec)
{
	mat[0][0]= mat[1][1]= mat[2][2]= 0.0;
	mat[0][1]= -vec[2];	
	mat[0][2]= vec[1];
	mat[1][0]= vec[2];	
	mat[1][2]= -vec[0];
	mat[2][0]= -vec[1];	
	mat[2][1]= vec[0];
}

/*********************************** Length **********************************/

MINLINE float len_v2(const float v[2])
{
	return (float)sqrt(v[0]*v[0] + v[1]*v[1]);
}

MINLINE float len_v2v2(const float v1[2], const float v2[2])
{
	float x, y;

	x = v1[0]-v2[0];
	y = v1[1]-v2[1];
	return (float)sqrt(x*x+y*y);
}

MINLINE float len_v3(const float a[3])
{
	return sqrtf(dot_v3v3(a, a));
}

MINLINE float len_v3v3(const float a[3], const float b[3])
{
	float d[3];

	sub_v3_v3v3(d, b, a);
	return len_v3(d);
}

MINLINE float normalize_v2(float n[2])
{
	float d= dot_v2v2(n, n);

	if(d > 1.0e-35f) {
		d= sqrtf(d);
		mul_v2_fl(n, 1.0f/d);
	} else {
		zero_v2(n);
		d= 0.0f;
	}
	return d;
}

MINLINE float normalize_v3(float n[3])
{
	float d= dot_v3v3(n, n);

	/* a larger value causes normalize errors in a
	   scaled down models with camera xtreme close */
	if(d > 1.0e-35f) {
		d= sqrtf(d);
		mul_v3_fl(n, 1.0f/d);
	}
	else {
		zero_v3(n);
		d= 0.0f;
	}

	return d;
}

MINLINE void normal_short_to_float_v3(float *out, short *in)
{
	out[0] = in[0]*(1.0f/32767.0f);
	out[1] = in[1]*(1.0f/32767.0f);
	out[2] = in[2]*(1.0f/32767.0f);
}

MINLINE void normal_float_to_short_v3(short *out, float *in)
{
	out[0] = (short)(in[0]*32767.0f);
	out[1] = (short)(in[1]*32767.0f);
	out[2] = (short)(in[2]*32767.0f);
}

#endif /* BLI_MATH_VECTOR_INLINE */

