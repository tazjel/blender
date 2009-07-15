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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): André Pinto.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#include <assert.h>
#include <stdio.h>

#include "BKE_utildefines.h"
#include "BLI_arithb.h"

#include "RE_raytrace.h"
#include "render_types.h"
#include "rayobject.h"


/*
 * Determines the distance that the ray must travel to hit the bounding volume of the given node
 * Based on Tactical Optimization of Ray/Box Intersection, by Graham Fyffe
 *  [http://tog.acm.org/resources/RTNews/html/rtnv21n1.html#art9]
 */
float RE_rayobject_bb_intersect(const Isect *isec, const float *_bb)
{
	const float *bb = _bb;
	float dist;
	
	float t1x = (bb[isec->bv_index[0]] - isec->start[0]) * isec->idot_axis[0];
	float t2x = (bb[isec->bv_index[1]] - isec->start[0]) * isec->idot_axis[0];
	float t1y = (bb[isec->bv_index[2]] - isec->start[1]) * isec->idot_axis[1];
	float t2y = (bb[isec->bv_index[3]] - isec->start[1]) * isec->idot_axis[1];
	float t1z = (bb[isec->bv_index[4]] - isec->start[2]) * isec->idot_axis[2];
	float t2z = (bb[isec->bv_index[5]] - isec->start[2]) * isec->idot_axis[2];

	RE_RC_COUNT(isec->raycounter->bb.test);

	if(t1x > t2y || t2x < t1y || t1x > t2z || t2x < t1z || t1y > t2z || t2y < t1z) return FLT_MAX;
	if(t2x < 0.0 || t2y < 0.0 || t2z < 0.0) return FLT_MAX;
	if(t1x > isec->labda || t1y > isec->labda || t1z > isec->labda) return FLT_MAX;

	RE_RC_COUNT(isec->raycounter->bb.hit);

	dist = t1x;
	if (t1y > dist) dist = t1y;
    if (t1z > dist) dist = t1z;
	return dist;
}


/* only for self-intersecting test with current render face (where ray left) */
static int intersection2(VlakRen *face, float r0, float r1, float r2, float rx1, float ry1, float rz1)
{
	float co1[3], co2[3], co3[3], co4[3];
	float x0,x1,x2,t00,t01,t02,t10,t11,t12,t20,t21,t22;
	float m0, m1, m2, divdet, det, det1;
	float u1, v, u2;

	VECCOPY(co1, face->v1->co);
	VECCOPY(co2, face->v2->co);
	if(face->v4)
	{
		VECCOPY(co3, face->v4->co);
		VECCOPY(co4, face->v3->co);
	}
	else
	{
		VECCOPY(co3, face->v3->co);
	}

	t00= co3[0]-co1[0];
	t01= co3[1]-co1[1];
	t02= co3[2]-co1[2];
	t10= co3[0]-co2[0];
	t11= co3[1]-co2[1];
	t12= co3[2]-co2[2];
	
	x0= t11*r2-t12*r1;
	x1= t12*r0-t10*r2;
	x2= t10*r1-t11*r0;

	divdet= t00*x0+t01*x1+t02*x2;

	m0= rx1-co3[0];
	m1= ry1-co3[1];
	m2= rz1-co3[2];
	det1= m0*x0+m1*x1+m2*x2;
	
	if(divdet!=0.0f) {
		u1= det1/divdet;

		if(u1<ISECT_EPSILON) {
			det= t00*(m1*r2-m2*r1);
			det+= t01*(m2*r0-m0*r2);
			det+= t02*(m0*r1-m1*r0);
			v= det/divdet;

			if(v<ISECT_EPSILON && (u1 + v) > -(1.0f+ISECT_EPSILON)) {
				return 1;
			}
		}
	}

	if(face->v4) {

		t20= co3[0]-co4[0];
		t21= co3[1]-co4[1];
		t22= co3[2]-co4[2];

		divdet= t20*x0+t21*x1+t22*x2;
		if(divdet!=0.0f) {
			u2= det1/divdet;
		
			if(u2<ISECT_EPSILON) {
				det= t20*(m1*r2-m2*r1);
				det+= t21*(m2*r0-m0*r2);
				det+= t22*(m0*r1-m1*r0);
				v= det/divdet;
	
				if(v<ISECT_EPSILON && (u2 + v) >= -(1.0f+ISECT_EPSILON)) {
					return 2;
				}
			}
		}
	}
	return 0;
}


/* ray - triangle or quad intersection */
/* this function shall only modify Isect if it detects an hit */
static int intersect_rayface(RayFace *face, Isect *is)
{
	float co1[3],co2[3],co3[3],co4[3];
	float x0,x1,x2,t00,t01,t02,t10,t11,t12,t20,t21,t22,r0,r1,r2;
	float m0, m1, m2, divdet, det1;
	float labda, u, v;
	short ok=0;
	
	if(is->orig.ob == face->ob && is->orig.face == face->face)
		return 0;

	RE_RC_COUNT(is->raycounter->faces.test);

	VECCOPY(co1, face->v1);
	VECCOPY(co2, face->v2);
	if(face->v4)
	{
		VECCOPY(co3, face->v4);
		VECCOPY(co4, face->v3);
	}
	else
	{
		VECCOPY(co3, face->v3);
	}

	t00= co3[0]-co1[0];
	t01= co3[1]-co1[1];
	t02= co3[2]-co1[2];
	t10= co3[0]-co2[0];
	t11= co3[1]-co2[1];
	t12= co3[2]-co2[2];
	
	r0= is->vec[0];
	r1= is->vec[1];
	r2= is->vec[2];
	
	x0= t12*r1-t11*r2;
	x1= t10*r2-t12*r0;
	x2= t11*r0-t10*r1;

	divdet= t00*x0+t01*x1+t02*x2;

	m0= is->start[0]-co3[0];
	m1= is->start[1]-co3[1];
	m2= is->start[2]-co3[2];
	det1= m0*x0+m1*x1+m2*x2;
	
	if(divdet!=0.0f) {

		divdet= 1.0f/divdet;
		u= det1*divdet;
		if(u<ISECT_EPSILON && u>-(1.0f+ISECT_EPSILON)) {
			float cros0, cros1, cros2;
			
			cros0= m1*t02-m2*t01;
			cros1= m2*t00-m0*t02;
			cros2= m0*t01-m1*t00;
			v= divdet*(cros0*r0 + cros1*r1 + cros2*r2);

			if(v<ISECT_EPSILON && (u + v) > -(1.0f+ISECT_EPSILON)) {
				labda= divdet*(cros0*t10 + cros1*t11 + cros2*t12);

				if(labda>-ISECT_EPSILON && labda<is->labda) {
					ok= 1;
				}
			}
		}
	}

	if(ok==0 && face->v4) {

		t20= co3[0]-co4[0];
		t21= co3[1]-co4[1];
		t22= co3[2]-co4[2];

		divdet= t20*x0+t21*x1+t22*x2;
		if(divdet!=0.0f) {
			divdet= 1.0f/divdet;
			u = det1*divdet;
			
			if(u<ISECT_EPSILON && u>-(1.0f+ISECT_EPSILON)) {
				float cros0, cros1, cros2;
				cros0= m1*t22-m2*t21;
				cros1= m2*t20-m0*t22;
				cros2= m0*t21-m1*t20;
				v= divdet*(cros0*r0 + cros1*r1 + cros2*r2);
	
				if(v<ISECT_EPSILON && (u + v) >-(1.0f+ISECT_EPSILON)) {
					labda= divdet*(cros0*t10 + cros1*t11 + cros2*t12);
					
					if(labda>-ISECT_EPSILON && labda<is->labda) {
						ok= 2;
					}
				}
			}
		}
	}

	if(ok) {
	
		/* when a shadow ray leaves a face, it can be little outside the edges of it, causing
		intersection to be detected in its neighbour face */
		if(is->skip & RE_SKIP_VLR_NEIGHBOUR)
		{
			if(labda < 0.1f && is->orig.ob == face->ob)
			{
				VlakRen * a = is->orig.face;
				VlakRen * b = face->face;

				/* so there's a shared edge or vertex, let's intersect ray with face
				itself, if that's true we can safely return 1, otherwise we assume
				the intersection is invalid, 0 */
				if(a->v1==b->v1 || a->v2==b->v1 || a->v3==b->v1 || a->v4==b->v1
				|| a->v1==b->v2 || a->v2==b->v2 || a->v3==b->v2 || a->v4==b->v2
				|| a->v1==b->v3 || a->v2==b->v3 || a->v3==b->v3 || a->v4==b->v3
				|| (b->v4 && (a->v1==b->v4 || a->v2==b->v4 || a->v3==b->v4 || a->v4==b->v4)))
				if(intersection2((VlakRen*)b, -r0, -r1, -r2, is->start[0], is->start[1], is->start[2]))
				{
					return 0;
				}
			}
		}
#if 0
		else if(labda < ISECT_EPSILON)
		{
			/* too close to origin */
			return 0;
		}
#endif

		RE_RC_COUNT(is->raycounter->faces.hit);

		is->isect= ok;	// wich half of the quad
		is->labda= labda;
		is->u= u; is->v= v;

		is->hit.ob   = face->ob;
		is->hit.face = face->face;
#ifdef RT_USE_LAST_HIT
		is->last_hit = (RayObject*) RayObject_unalignRayFace(face);
#endif
		return 1;
	}

	return 0;
}

int RE_rayobject_raycast(RayObject *r, Isect *isec)
{
	int i;
	RE_RC_COUNT(isec->raycounter->raycast.test);

	/* Setup vars used on raycast */
	isec->labda *= Normalize(isec->vec);
	isec->dist = VecLength(isec->vec);
	
	for(i=0; i<3; i++)
	{
		isec->idot_axis[i]		= 1.0f / isec->vec[i];
		
		isec->bv_index[2*i]		= isec->idot_axis[i] < 0.0 ? 1 : 0;
		isec->bv_index[2*i+1]	= 1 - isec->bv_index[2*i];
		
		isec->bv_index[2*i]		= i+3*isec->bv_index[2*i];
		isec->bv_index[2*i+1]	= i+3*isec->bv_index[2*i+1];
	}

#ifdef RT_USE_LAST_HIT	
	/* Last hit heuristic */
	if(isec->mode==RE_RAY_SHADOW && isec->last_hit)
	{
		RE_RC_COUNT(isec->raycounter->rayshadow_last_hit.test);
		
		if(RE_rayobject_intersect(isec->last_hit, isec))
		{
			RE_RC_COUNT(isec->raycounter->raycast.hit);
			RE_RC_COUNT(isec->raycounter->rayshadow_last_hit.hit);
			return 1;
		}
	}
#endif

#ifdef RT_USE_HINT
	isec->hit_hint = 0;
#endif

	if(RE_rayobject_intersect(r, isec))
	{
#ifdef RE_RAYCOUNTER
		RE_RC_COUNT(isec->raycounter->raycast.hit);
#endif

#ifdef RT_USE_HINT
		isec->hint = isec->hit_hint;
#endif
		return 1;
	}
	return 0;
}

int RE_rayobject_intersect(RayObject *r, Isect *i)
{
	if(RayObject_isRayFace(r))
	{
		return intersect_rayface( (RayFace*) RayObject_align(r), i);
	}
	else if(RayObject_isRayAPI(r))
	{
		r = RayObject_align( r );
		return r->api->raycast( r, i );
	}
	else assert(0);
}

void RE_rayobject_add(RayObject *r, RayObject *o)
{
	r = RayObject_align( r );
	return r->api->add( r, o );
}

void RE_rayobject_done(RayObject *r)
{
	r = RayObject_align( r );
	r->api->done( r );
}

void RE_rayobject_free(RayObject *r)
{
	r = RayObject_align( r );
	r->api->free( r );
}

void RE_rayobject_merge_bb(RayObject *r, float *min, float *max)
{
	if(RayObject_isRayFace(r))
	{
		RayFace *face = (RayFace*) RayObject_align(r);
		DO_MINMAX( face->v1, min, max );
		DO_MINMAX( face->v2, min, max );
		DO_MINMAX( face->v3, min, max );
		if(face->v4) DO_MINMAX( face->v4, min, max );
	}
	else if(RayObject_isRayAPI(r))
	{
		r = RayObject_align( r );
		r->api->bb( r, min, max );
	}
	else assert(0);
}

float RE_rayobject_cost(RayObject *r)
{
	if(RayObject_isRayFace(r))
	{
		return 1.0;
	}
	else if(RayObject_isRayAPI(r))
	{
		r = RayObject_align( r );
		return r->api->cost( r );
	}
	else assert(0);
}

void RE_rayobject_hint_bb(RayObject *r, RayHint *hint, float *min, float *max)
{
	if(RayObject_isRayFace(r))
	{
		return;
	}
	else if(RayObject_isRayAPI(r))
	{
		r = RayObject_align( r );
		return r->api->hint_bb( r, hint, min, max );
	}
	else assert(0);
}

#ifdef RE_RAYCOUNTER
void RE_RC_INFO(RayCounter *info)
{
	printf("----------- Raycast counter --------\n");
	printf("Rays total: %llu\n", info->raycast.test );
	printf("Rays hit: %llu\n",   info->raycast.hit  );
	printf("\n");
	printf("BB tests: %llu\n", info->bb.test );
	printf("BB hits: %llu\n", info->bb.hit );
	printf("\n");	
	printf("Primitives tests: %llu\n", info->faces.test );
	printf("Primitives hits: %llu\n", info->faces.hit );
	printf("------------------------------------\n");
	printf("Shadow last-hit tests per ray: %f\n", info->rayshadow_last_hit.test / ((float)info->raycast.test) );
	printf("Shadow last-hit hits per ray: %f\n",  info->rayshadow_last_hit.hit  / ((float)info->raycast.test) );
	printf("\n");
	printf("Hint tests per ray: %f\n", info->raytrace_hint.test / ((float)info->raycast.test) );
	printf("Hint hits per ray: %f\n",  info->raytrace_hint.hit  / ((float)info->raycast.test) );
	printf("\n");
	printf("BB tests per ray: %f\n", info->bb.test / ((float)info->raycast.test) );
	printf("BB hits per ray: %f\n", info->bb.hit / ((float)info->raycast.test) );
	printf("\n");
	printf("Primitives tests per ray: %f\n", info->faces.test / ((float)info->raycast.test) );
	printf("Primitives hits per ray: %f\n", info->faces.hit / ((float)info->raycast.test) );
	printf("------------------------------------\n");
}

void RE_RC_MERGE(RayCounter *dest, RayCounter *tmp)
{
	dest->faces.test += tmp->faces.test;
	dest->faces.hit  += tmp->faces.hit;

	dest->bb.test += tmp->bb.test;
	dest->bb.hit  += tmp->bb.hit;

	dest->raycast.test += tmp->raycast.test;
	dest->raycast.hit  += tmp->raycast.hit;
	
	dest->rayshadow_last_hit.test += tmp->rayshadow_last_hit.test;
	dest->rayshadow_last_hit.hit  += tmp->rayshadow_last_hit.hit;

	dest->raytrace_hint.test += tmp->raytrace_hint.test;
	dest->raytrace_hint.hit  += tmp->raytrace_hint.hit;
}

#endif