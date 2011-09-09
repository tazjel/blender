//
// Copyright (c) 2009 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include "Recast.h"
#include "RecastTimer.h"
#include "RecastLog.h"

inline bool overlapBounds(const float* amin, const float* amax, const float* bmin, const float* bmax)
{
	bool overlap = true;
	overlap = (amin[0] > bmax[0] || amax[0] < bmin[0]) ? false : overlap;
	overlap = (amin[1] > bmax[1] || amax[1] < bmin[1]) ? false : overlap;
	overlap = (amin[2] > bmax[2] || amax[2] < bmin[2]) ? false : overlap;
	return overlap;
}

inline bool overlapInterval(unsigned short amin, unsigned short amax,
							unsigned short bmin, unsigned short bmax)
{
	if (amax < bmin) return false;
	if (amin > bmax) return false;
	return true;
}


static rcSpan* allocSpan(rcHeightfield& hf)
{
	// If running out of memory, allocate new page and update the freelist.
	if (!hf.freelist || !hf.freelist->next)
	{
		// Create new page.
		// Allocate memory for the new pool.
		const int size = (sizeof(rcSpanPool)-sizeof(rcSpan)) + sizeof(rcSpan)*RC_SPANS_PER_POOL;
		rcSpanPool* pool = reinterpret_cast<rcSpanPool*>(new unsigned char[size]);
		if (!pool) return 0;
		pool->next = 0;
		// Add the pool into the list of pools.
		pool->next = hf.pools;
		hf.pools = pool;
		// Add new items to the free list.
		rcSpan* freelist = hf.freelist;
		rcSpan* head = &pool->items[0];
		rcSpan* it = &pool->items[RC_SPANS_PER_POOL];
		do
		{
			--it;
			it->next = freelist;
			freelist = it;
		}
		while (it != head);
		hf.freelist = it;
	}
	
	// Pop item from in front of the free list.
	rcSpan* it = hf.freelist;
	hf.freelist = hf.freelist->next;
	return it;
}

static void freeSpan(rcHeightfield& hf, rcSpan* ptr)
{
	if (!ptr) return;
	// Add the node in front of the free list.
	ptr->next = hf.freelist;
	hf.freelist = ptr;
}

static void addSpan(rcHeightfield& hf, int x, int y,
					unsigned short smin, unsigned short smax,
					unsigned short flags)
{
	int idx = x + y*hf.width;
	
	rcSpan* s = allocSpan(hf);
	s->smin = smin;
	s->smax = smax;
	s->flags = flags;
	s->next = 0;
	
	// Empty cell, add he first span.
	if (!hf.spans[idx])
	{
		hf.spans[idx] = s;
		return;
	}
	rcSpan* prev = 0;
	rcSpan* cur = hf.spans[idx];
	
	// Insert and merge spans.
	while (cur)
	{
		if (cur->smin > s->smax)
		{
			// Current span is further than the new span, break.
			break;
		}
		else if (cur->smax < s->smin)
		{
			// Current span is before the new span advance.
			prev = cur;
			cur = cur->next;
		}
		else
		{
			// Merge spans.
			if (cur->smin < s->smin)
				s->smin = cur->smin;
			if (cur->smax > s->smax)
				s->smax = cur->smax;
			
			// Merge flags.
//			if (s->smax == cur->smax)
			if (rcAbs((int)s->smax - (int)cur->smax) <= 1)
				s->flags |= cur->flags;
			
			// Remove current span.
			rcSpan* next = cur->next;
			freeSpan(hf, cur);
			if (prev)
				prev->next = next;
			else
				hf.spans[idx] = next;
			cur = next;
		}
	}
	
	// Insert new span.
	if (prev)
	{
		s->next = prev->next;
		prev->next = s;
	}
	else
	{
		s->next = hf.spans[idx];
		hf.spans[idx] = s;
	}
}

static int clipPoly(const float* in, int n, float* out, float pnx, float pnz, float pd)
{
	float d[12];
	for (int i = 0; i < n; ++i)
		d[i] = pnx*in[i*3+0] + pnz*in[i*3+2] + pd;
	
	int m = 0;
	for (int i = 0, j = n-1; i < n; j=i, ++i)
	{
		bool ina = d[j] >= 0;
		bool inb = d[i] >= 0;
		if (ina != inb)
		{
			float s = d[j] / (d[j] - d[i]);
			out[m*3+0] = in[j*3+0] + (in[i*3+0] - in[j*3+0])*s;
			out[m*3+1] = in[j*3+1] + (in[i*3+1] - in[j*3+1])*s;
			out[m*3+2] = in[j*3+2] + (in[i*3+2] - in[j*3+2])*s;
			m++;
		}
		if (inb)
		{
			out[m*3+0] = in[i*3+0];
			out[m*3+1] = in[i*3+1];
			out[m*3+2] = in[i*3+2];
			m++;
		}
	}
	return m;
}

static void rasterizeTri(const float* v0, const float* v1, const float* v2,
						 unsigned char flags, rcHeightfield& hf,
						 const float* bmin, const float* bmax,
						 const float cs, const float ics, const float ich)
{
	const int w = hf.width;
	const int h = hf.height;
	float tmin[3], tmax[3];
	const float by = bmax[1] - bmin[1];
	
	// Calculate the bounding box of the triangle.
	vcopy(tmin, v0);
	vcopy(tmax, v0);
	vmin(tmin, v1);
	vmin(tmin, v2);
	vmax(tmax, v1);
	vmax(tmax, v2);
	
	// If the triangle does not touch the bbox of the heightfield, skip the triagle.
	if (!overlapBounds(bmin, bmax, tmin, tmax))
		return;
	
	// Calculate the footpring of the triangle on the grid.
	int x0 = (int)((tmin[0] - bmin[0])*ics);
	int y0 = (int)((tmin[2] - bmin[2])*ics);
	int x1 = (int)((tmax[0] - bmin[0])*ics);
	int y1 = (int)((tmax[2] - bmin[2])*ics);
	x0 = rcClamp(x0, 0, w-1);
	y0 = rcClamp(y0, 0, h-1);
	x1 = rcClamp(x1, 0, w-1);
	y1 = rcClamp(y1, 0, h-1);
	
	// Clip the triangle into all grid cells it touches.
	float in[7*3], out[7*3], inrow[7*3];
	
	for (int y = y0; y <= y1; ++y)
	{
		// Clip polygon to row.
		vcopy(&in[0], v0);
		vcopy(&in[1*3], v1);
		vcopy(&in[2*3], v2);
		int nvrow = 3;
		const float cz = bmin[2] + y*cs;
		nvrow = clipPoly(in, nvrow, out, 0, 1, -cz);
		if (nvrow < 3) continue;
		nvrow = clipPoly(out, nvrow, inrow, 0, -1, cz+cs);
		if (nvrow < 3) continue;
		
		for (int x = x0; x <= x1; ++x)
		{
			// Clip polygon to column.
			int nv = nvrow;
			const float cx = bmin[0] + x*cs;
			nv = clipPoly(inrow, nv, out, 1, 0, -cx);
			if (nv < 3) continue;
			nv = clipPoly(out, nv, in, -1, 0, cx+cs);
			if (nv < 3) continue;
			
			// Calculate min and max of the span.
			float smin = in[1], smax = in[1];
			for (int i = 1; i < nv; ++i)
			{
				smin = rcMin(smin, in[i*3+1]);
				smax = rcMax(smax, in[i*3+1]);
			}
			smin -= bmin[1];
			smax -= bmin[1];
			// Skip the span if it is outside the heightfield bbox
			if (smax < 0.0f) continue;
			if (smin > by) continue;
			// Clamp the span to the heightfield bbox.
			if (smin < 0.0f) smin = bmin[1];
			if (smax > by) smax = bmax[1];
			
			// Snap the span to the heightfield height grid.
			unsigned short ismin = (unsigned short)rcClamp((int)floorf(smin * ich), 0, 0x7fff);
			unsigned short ismax = (unsigned short)rcClamp((int)ceilf(smax * ich), 0, 0x7fff);
			
			addSpan(hf, x, y, ismin, ismax, flags);
		}
	}
}

void rcRasterizeTriangle(const float* v0, const float* v1, const float* v2,
						 unsigned char flags, rcHeightfield& solid)
{
	rcTimeVal startTime = rcGetPerformanceTimer();

	const float ics = 1.0f/solid.cs;
	const float ich = 1.0f/solid.ch;
	rasterizeTri(v0, v1, v2, flags, solid, solid.bmin, solid.bmax, solid.cs, ics, ich);

	rcTimeVal endTime = rcGetPerformanceTimer();
	
	if (rcGetBuildTimes())
		rcGetBuildTimes()->rasterizeTriangles += rcGetDeltaTimeUsec(startTime, endTime);
}
						 
void rcRasterizeTriangles(const float* verts, int nv,
						  const int* tris, const unsigned char* flags, int nt,
						  rcHeightfield& solid)
{
	rcTimeVal startTime = rcGetPerformanceTimer();
	
	const float ics = 1.0f/solid.cs;
	const float ich = 1.0f/solid.ch;
	// Rasterize triangles.
	for (int i = 0; i < nt; ++i)
	{
		const float* v0 = &verts[tris[i*3+0]*3];
		const float* v1 = &verts[tris[i*3+1]*3];
		const float* v2 = &verts[tris[i*3+2]*3];
		// Rasterize.
		rasterizeTri(v0, v1, v2, flags[i], solid, solid.bmin, solid.bmax, solid.cs, ics, ich);
	}
	
	rcTimeVal endTime = rcGetPerformanceTimer();

	if (rcGetBuildTimes())
		rcGetBuildTimes()->rasterizeTriangles += rcGetDeltaTimeUsec(startTime, endTime);
}
