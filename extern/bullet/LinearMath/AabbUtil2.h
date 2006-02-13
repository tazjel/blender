/*

Copyright (c) 2005 Gino van den Bergen / Erwin Coumans http://continuousphysics.com

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/


#ifndef AABB_UTIL2
#define AABB_UTIL2

#include "SimdVector3.h"

#define SimdMin(a,b) ((a < b ? a : b))
#define SimdMax(a,b) ((a > b ? a : b))


/// conservative test for overlap between two aabbs
SIMD_FORCE_INLINE bool TestAabbAgainstAabb2(const SimdVector3 &aabbMin1, const SimdVector3 &aabbMax1,
								const SimdVector3 &aabbMin2, const SimdVector3 &aabbMax2)
{
	bool overlap = true;
	overlap = (aabbMin1[0] > aabbMax2[0] || aabbMax1[0] < aabbMin2[0]) ? false : overlap;
	overlap = (aabbMin1[2] > aabbMax2[2] || aabbMax1[2] < aabbMin2[2]) ? false : overlap;
	overlap = (aabbMin1[1] > aabbMax2[1] || aabbMax1[1] < aabbMin2[1]) ? false : overlap;
	return overlap;
}

/// conservative test for overlap between triangle and aabb
SIMD_FORCE_INLINE bool TestTriangleAgainstAabb2(const SimdVector3 *vertices,
									const SimdVector3 &aabbMin, const SimdVector3 &aabbMax)
{
	const SimdVector3 &p1 = vertices[0];
	const SimdVector3 &p2 = vertices[1];
	const SimdVector3 &p3 = vertices[2];

	if (SimdMin(SimdMin(p1[0], p2[0]), p3[0]) > aabbMax[0]) return false;
	if (SimdMax(SimdMax(p1[0], p2[0]), p3[0]) < aabbMin[0]) return false;

	if (SimdMin(SimdMin(p1[2], p2[2]), p3[2]) > aabbMax[2]) return false;
	if (SimdMax(SimdMax(p1[2], p2[2]), p3[2]) < aabbMin[2]) return false;
  
	if (SimdMin(SimdMin(p1[1], p2[1]), p3[1]) > aabbMax[1]) return false;
	if (SimdMax(SimdMax(p1[1], p2[1]), p3[1]) < aabbMin[1]) return false;
	return true;
}

#endif

