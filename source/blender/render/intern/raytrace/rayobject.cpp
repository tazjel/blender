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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

#include "BKE_utildefines.h"
#include "BLI_math.h"
#include "DNA_material_types.h"

#include "RE_raytrace.h"
#include "object_mesh.h"
#include "rayobject.h"
#include "raycounter.h"

/*
 * Determines the distance that the ray must travel to hit the bounding volume of the given node
 * Based on Tactical Optimization of Ray/Box Intersection, by Graham Fyffe
 *  [http://tog.acm.org/resources/RTNews/html/rtnv21n1.html#art9]
 */
int RE_rayobject_bb_intersect_test(const Isect *isec, const float *_bb)
{
	const float *bb = _bb;
	
	float t1x = (bb[isec->bv_index[0]] - isec->start[0]) * isec->idot_axis[0];
	float t2x = (bb[isec->bv_index[1]] - isec->start[0]) * isec->idot_axis[0];
	float t1y = (bb[isec->bv_index[2]] - isec->start[1]) * isec->idot_axis[1];
	float t2y = (bb[isec->bv_index[3]] - isec->start[1]) * isec->idot_axis[1];
	float t1z = (bb[isec->bv_index[4]] - isec->start[2]) * isec->idot_axis[2];
	float t2z = (bb[isec->bv_index[5]] - isec->start[2]) * isec->idot_axis[2];

	RE_RC_COUNT(isec->raycounter->bb.test);
	
	if(t1x > t2y || t2x < t1y || t1x > t2z || t2x < t1z || t1y > t2z || t2y < t1z) return 0;
	if(t2x < 0.0 || t2y < 0.0 || t2z < 0.0) return 0;
	if(t1x > isec->labda || t1y > isec->labda || t1z > isec->labda) return 0;
	RE_RC_COUNT(isec->raycounter->bb.hit);	

	return 1;
}

static inline int vlr_check_intersect(Isect *is, ObjectInstanceRen *obi, VlakRen *vlr)
{
	/* for baking selected to active non-traceable materials might still
	 * be in the raytree */
	if(!(vlr->flag & R_TRACEBLE))
		return 0;

	/* I know... cpu cycle waste, might do smarter once */
	if(is->mode==RE_RAY_MIRROR)
		return !(vlr->mat->mode & MA_ONLYCAST);
	else
		return (is->lay & obi->lay);
}

static inline int vlr_check_intersect_solid(Isect *is, ObjectInstanceRen* obi, VlakRen *vlr)
{
	/* solid material types only */
	if (vlr->mat->material_type == MA_TYPE_SURFACE)
		return 1;
	else
		return 0;
}

static inline int rayface_check_cullface(RayFace *face, Isect *is)
{
	float nor[3];
	
	/* don't intersect if the ray faces along the face normal */
	if(face->quad) normal_quad_v3( nor,face->v1, face->v2, face->v3, face->v4);
	else normal_tri_v3( nor,face->v1, face->v2, face->v3);

	return (INPR(nor, is->vec) < 0);
}

static int isec_tri_quad(float start[3], float vec[3], RayFace *face, float uv[2], float *lambda)
{
	float co1[3], co2[3], co3[3], co4[4];
	float t0[3], t1[3], x[3], r[3], m[3], u, v, divdet, det1, l;
	int quad;

	quad= RE_rayface_isQuad(face);

	copy_v3_v3(co1, face->v1);
	copy_v3_v3(co2, face->v2);
	copy_v3_v3(co3, face->v3);

	copy_v3_v3(r, vec);

	/* intersect triangle */
	sub_v3_v3v3(t0, co3, co2);
	sub_v3_v3v3(t1, co3, co1);

	cross_v3_v3v3(x, r, t1);
	divdet= dot_v3v3(t0, x);

	sub_v3_v3v3(m, start, co3);
	det1= dot_v3v3(m, x);
	
	if(divdet != 0.0f) {
		divdet= 1.0f/divdet;
		v= det1*divdet;

		if(v < ISECT_EPSILON && v > -(1.0f+ISECT_EPSILON)) {
			float cros[3];

			cross_v3_v3v3(cros, m, t0);
			u= divdet*dot_v3v3(cros, r);

			if(u < ISECT_EPSILON && (v + u) > -(1.0f+ISECT_EPSILON)) {
				l= divdet*dot_v3v3(cros, t1);

				/* check if intersection is within ray length */
				if(l > -ISECT_EPSILON && l < *lambda) {
					uv[0]= u;
					uv[1]= v;
					*lambda= l;
					return 1;
				}
			}
		}
	}

	/* intersect second triangle in quad */
	if(quad) {
		copy_v3_v3(co4, face->v4);
		sub_v3_v3v3(t0, co3, co4);
		divdet= dot_v3v3(t0, x);

		if(divdet != 0.0f) {
			divdet= 1.0f/divdet;
			v = det1*divdet;
			
			if(v < ISECT_EPSILON && v > -(1.0f+ISECT_EPSILON)) {
				float cros[3];

				cross_v3_v3v3(cros, m, t0);
				u= divdet*dot_v3v3(cros, r);
	
				if(u < ISECT_EPSILON && (v + u) > -(1.0f+ISECT_EPSILON)) {
					l= divdet*dot_v3v3(cros, t1);
					
					if(l >- ISECT_EPSILON && l < *lambda) {
						uv[0]= u;
						uv[1]= -(1.0f + v + u);
						*lambda= l;
						return 2;
					}
				}
			}
		}
	}

	return 0;
}

static int isec_tri_quad_2(float start[3], float vec[3], RayFace *face)
{
	float co1[3], co2[3], co3[3], co4[4];
	float t0[3], t1[3], x[3], r[3], m[3], u, v, divdet, det1;
	int quad;

	quad= RE_rayface_isQuad(face);

	copy_v3_v3(co1, face->v1);
	copy_v3_v3(co2, face->v2);
	copy_v3_v3(co3, face->v3);

	negate_v3_v3(r, vec); /* note, different than above function */

	/* intersect triangle */
	sub_v3_v3v3(t0, co3, co2);
	sub_v3_v3v3(t1, co3, co1);

	cross_v3_v3v3(x, r, t1);
	divdet= dot_v3v3(t0, x);

	sub_v3_v3v3(m, start, co3);
	det1= dot_v3v3(m, x);
	
	if(divdet != 0.0f) {
		divdet= 1.0f/divdet;
		v= det1*divdet;

		if(v < ISECT_EPSILON && v > -(1.0f+ISECT_EPSILON)) {
			float cros[3];

			cross_v3_v3v3(cros, m, t0);
			u= divdet*dot_v3v3(cros, r);

			if(u < ISECT_EPSILON && (v + u) > -(1.0f+ISECT_EPSILON))
				return 1;
		}
	}

	/* intersect second triangle in quad */
	if(quad) {
		sub_v3_v3v3(t0, co3, co4);
		divdet= dot_v3v3(t0, x);

		if(divdet != 0.0f) {
			divdet= 1.0f/divdet;
			v = det1*divdet;
			
			if(v < ISECT_EPSILON && v > -(1.0f+ISECT_EPSILON)) {
				float cros[3];

				cross_v3_v3v3(cros, m, t0);
				u= divdet*dot_v3v3(cros, r);
	
				if(u < ISECT_EPSILON && (v + u) > -(1.0f+ISECT_EPSILON))
					return 2;
			}
		}
	}

	return 0;
}

/* ray - triangle or quad intersection */
/* this function shall only modify Isect if it detects an hit */
static int intersect_rayface(RayObject *hit_obj, RayFace *face, Isect *is)
{
	float labda, uv[2];
	int ok= 0;
	
	/* avoid self-intersection */
	if(is->orig.ob == face->ob && is->orig.face == face->face)
		return 0;
		
	/* check if we should intersect this face */
	if(is->skip & RE_SKIP_VLR_RENDER_CHECK)
	{
		if(vlr_check_intersect(is, (ObjectInstanceRen*)face->ob, (VlakRen*)face->face ) == 0)
			return 0;
	}
	if(is->skip & RE_SKIP_VLR_NON_SOLID_MATERIAL)
	{
		if(vlr_check_intersect_solid(is, (ObjectInstanceRen*)face->ob, (VlakRen*)face->face) == 0)
			return 0;
	}
	if(is->skip & RE_SKIP_CULLFACE)
	{
		if(rayface_check_cullface(face, is) == 0)
			return 0;
	}

	/* ray counter */
	RE_RC_COUNT(is->raycounter->faces.test);

	labda= is->labda;
	ok= isec_tri_quad(is->start, is->vec, face, uv, &labda);

	if(ok) {
	
		/* when a shadow ray leaves a face, it can be little outside the edges of it, causing
		intersection to be detected in its neighbour face */
		if(is->skip & RE_SKIP_VLR_NEIGHBOUR)
		{
			if(labda < 0.1f && is->orig.ob == face->ob)
			{
				VlakRen * a = (VlakRen*)is->orig.face;
				VlakRen * b = (VlakRen*)face->face;

				/* so there's a shared edge or vertex, let's intersect ray with face
				itself, if that's true we can safely return 1, otherwise we assume
				the intersection is invalid, 0 */
				if(a->v1==b->v1 || a->v2==b->v1 || a->v3==b->v1 || a->v4==b->v1
				|| a->v1==b->v2 || a->v2==b->v2 || a->v3==b->v2 || a->v4==b->v2
				|| a->v1==b->v3 || a->v2==b->v3 || a->v3==b->v3 || a->v4==b->v3
				|| (b->v4 && (a->v1==b->v4 || a->v2==b->v4 || a->v3==b->v4 || a->v4==b->v4)))
				if(!isec_tri_quad_2(is->start, is->vec, (RayFace*)a))
				{
					return 0;
				}
			}
		}

		RE_RC_COUNT(is->raycounter->faces.hit);

		is->isect= ok;	// wich half of the quad
		is->labda= labda;
		is->u= uv[0]; is->v= uv[1];

		is->hit.ob   = face->ob;
		is->hit.face = face->face;
#ifdef RT_USE_LAST_HIT
		is->last_hit = hit_obj;
#endif
		return 1;
	}

	return 0;
}

RayObject* RE_rayface_from_vlak(RayFace *rayface, ObjectInstanceRen *obi, VlakRen *vlr)
{
	return RE_rayface_from_coords(rayface, obi, vlr, vlr->v1->co, vlr->v2->co, vlr->v3->co, vlr->v4 ? vlr->v4->co : 0 );
}

RayObject* RE_rayface_from_coords(RayFace *rayface, void *ob, void *face, float *v1, float *v2, float *v3, float *v4)
{
	rayface->ob = ob;
	rayface->face = face;

	VECCOPY(rayface->v1, v1);
	VECCOPY(rayface->v2, v2);
	VECCOPY(rayface->v3, v3);
	if(v4)
	{
		VECCOPY(rayface->v4, v4);
		rayface->quad = 1;
	}
	else
	{
		rayface->quad = 0;
	}

	return RE_rayobject_unalignRayFace(rayface);
}

RayObject* RE_vlakprimitive_from_vlak(VlakPrimitive *face, struct ObjectInstanceRen *obi, struct VlakRen *vlr)
{
	face->ob = obi;
	face->face = vlr;
	return RE_rayobject_unalignVlakPrimitive(face);
}


int RE_rayobject_raycast(RayObject *r, Isect *isec)
{
	int i;
	RE_RC_COUNT(isec->raycounter->raycast.test);

	/* Setup vars used on raycast */
	isec->dist = len_v3(isec->vec);
	
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
		RE_RC_COUNT(isec->raycounter->raycast.hit);

#ifdef RT_USE_HINT
		isec->hint = isec->hit_hint;
#endif
		return 1;
	}
	return 0;
}

int RE_rayobject_intersect(RayObject *r, Isect *i)
{
	if(RE_rayobject_isRayFace(r))
	{
		return intersect_rayface(r, (RayFace*) RE_rayobject_align(r), i);
	}
	else if(RE_rayobject_isVlakPrimitive(r))
	{
		//TODO optimize (useless copy to RayFace to avoid duplicate code)
		VlakPrimitive *face = (VlakPrimitive*) RE_rayobject_align(r);
		RayFace nface;
		RE_rayface_from_vlak(&nface, face->ob, face->face);

		if(face->ob->transform_primitives)
		{
			mul_m4_v3(face->ob->mat, nface.v1);
			mul_m4_v3(face->ob->mat, nface.v2);
			mul_m4_v3(face->ob->mat, nface.v3);
			if(RE_rayface_isQuad(&nface))
				mul_m4_v3(face->ob->mat, nface.v4);
		}

		return intersect_rayface(r, &nface, i);
	}
	else if(RE_rayobject_isRayAPI(r))
	{
		r = RE_rayobject_align( r );
		return r->api->raycast( r, i );
	}
	else assert(0);
}

void RE_rayobject_add(RayObject *r, RayObject *o)
{
	r = RE_rayobject_align( r );
	return r->api->add( r, o );
}

void RE_rayobject_done(RayObject *r)
{
	r = RE_rayobject_align( r );
	r->api->done( r );
}

void RE_rayobject_free(RayObject *r)
{
	r = RE_rayobject_align( r );
	r->api->free( r );
}

void RE_rayobject_merge_bb(RayObject *r, float *min, float *max)
{
	if(RE_rayobject_isRayFace(r))
	{
		RayFace *face = (RayFace*) RE_rayobject_align(r);
		
		DO_MINMAX( face->v1, min, max );
		DO_MINMAX( face->v2, min, max );
		DO_MINMAX( face->v3, min, max );
		if(RE_rayface_isQuad(face)) DO_MINMAX( face->v4, min, max );
	}
	else if(RE_rayobject_isVlakPrimitive(r))
	{
		VlakPrimitive *face = (VlakPrimitive*) RE_rayobject_align(r);
		VlakRen *vlr = face->face;

		DO_MINMAX( vlr->v1->co, min, max );
		DO_MINMAX( vlr->v2->co, min, max );
		DO_MINMAX( vlr->v3->co, min, max );
		if(vlr->v4) DO_MINMAX( vlr->v4->co, min, max );
	}
	else if(RE_rayobject_isRayAPI(r))
	{
		r = RE_rayobject_align( r );
		r->api->bb( r, min, max );
	}
	else assert(0);
}

float RE_rayobject_cost(RayObject *r)
{
	if(RE_rayobject_isRayFace(r) || RE_rayobject_isVlakPrimitive(r))
	{
		return 1.0;
	}
	else if(RE_rayobject_isRayAPI(r))
	{
		r = RE_rayobject_align( r );
		return r->api->cost( r );
	}
	else assert(0);
}

void RE_rayobject_hint_bb(RayObject *r, RayHint *hint, float *min, float *max)
{
	if(RE_rayobject_isRayFace(r) || RE_rayobject_isVlakPrimitive(r))
	{
		return;
	}
	else if(RE_rayobject_isRayAPI(r))
	{
		r = RE_rayobject_align( r );
		return r->api->hint_bb( r, hint, min, max );
	}
	else assert(0);
}

int RE_rayobjectcontrol_test_break(RayObjectControl *control)
{
	if(control->test_break)
		return control->test_break( control->data );

	return 0;
}


/*
 * Empty raytree
 */
static int RE_rayobject_empty_intersect(RayObject *o, Isect *is)
{
	return 0;
}

static void RE_rayobject_empty_free(RayObject *o)
{
}

static void RE_rayobject_empty_bb(RayObject *o, float *min, float *max)
{
	return;
}

static float RE_rayobject_empty_cost(RayObject *o)
{
	return 0.0;
}

static void RE_rayobject_empty_hint_bb(RayObject *o, RayHint *hint, float *min, float *max)
{}

static RayObjectAPI empty_api =
{
	RE_rayobject_empty_intersect,
	NULL, //static void RE_rayobject_instance_add(RayObject *o, RayObject *ob);
	NULL, //static void RE_rayobject_instance_done(RayObject *o);
	RE_rayobject_empty_free,
	RE_rayobject_empty_bb,
	RE_rayobject_empty_cost,
	RE_rayobject_empty_hint_bb
};

static RayObject empty_raytree = { &empty_api, {0, 0} };

RayObject *RE_rayobject_empty_create()
{
	return RE_rayobject_unalignRayAPI( &empty_raytree );
}
