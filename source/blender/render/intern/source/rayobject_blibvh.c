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

#include "MEM_guardedalloc.h"
#include "BKE_utildefines.h"
#include "BLI_kdopbvh.h"
#include "BLI_arithb.h"
#include "RE_raytrace.h"
#include "render_types.h"
#include "rayobject.h"

static int  RE_rayobject_blibvh_intersect(RayObject *o, Isect *isec);
static void RE_rayobject_blibvh_add(RayObject *o, RayObject *ob);
static void RE_rayobject_blibvh_done(RayObject *o);
static void RE_rayobject_blibvh_free(RayObject *o);
static void RE_rayobject_blibvh_bb(RayObject *o, float *min, float *max);

static float RE_rayobject_blibvh_cost(RayObject *o)
{
	//TODO calculate the expected cost to raycast on this structure
	return 1.0;
}

static void RE_rayobject_blibvh_hint_bb(RayObject *o, RayHint *hint, float *min, float *max)
{
	return;
}

static RayObjectAPI bvh_api =
{
	RE_rayobject_blibvh_intersect,
	RE_rayobject_blibvh_add,
	RE_rayobject_blibvh_done,
	RE_rayobject_blibvh_free,
	RE_rayobject_blibvh_bb,
	RE_rayobject_blibvh_cost,
	RE_rayobject_blibvh_hint_bb
};

typedef struct BVHObject
{
	RayObject rayobj;
	BVHTree *bvh;
	float bb[2][3];

} BVHObject;


RayObject *RE_rayobject_blibvh_create(int size)
{
	BVHObject *obj= (BVHObject*)MEM_callocN(sizeof(BVHObject), "BVHObject");
	assert( RE_rayobject_isAligned(obj) ); /* RayObject API assumes real data to be 4-byte aligned */	
	
	obj->rayobj.api = &bvh_api;
	obj->bvh = BLI_bvhtree_new(size, 0.0, 4, 6);
	
	INIT_MINMAX(obj->bb[0], obj->bb[1]);
	return RE_rayobject_unalignRayAPI((RayObject*) obj);
}

static void bvh_callback(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
	Isect *isec = (Isect*)userdata;
	RayObject *face = (RayObject*)index;
	
	if(RE_rayobject_intersect(face,isec))
	{
		hit->index = index;

		if(isec->mode == RE_RAY_SHADOW)
			hit->dist = 0;
		else
			hit->dist = isec->labda*isec->dist;
	}
}

static int  RE_rayobject_blibvh_intersect(RayObject *o, Isect *isec)
{
	BVHObject *obj = (BVHObject*)o;
	BVHTreeRayHit hit;
	float dir[3];

	VECCOPY(dir, isec->vec);
	Normalize(dir);

	hit.index = 0;
	hit.dist = isec->labda*isec->dist;
	
	return BLI_bvhtree_ray_cast(obj->bvh, isec->start, dir, 0.0, &hit, bvh_callback, isec);
}

static void RE_rayobject_blibvh_add(RayObject *o, RayObject *ob)
{
	BVHObject *obj = (BVHObject*)o;
	float min_max[6];
	INIT_MINMAX(min_max, min_max+3);
	RE_rayobject_merge_bb(ob, min_max, min_max+3);

	DO_MIN(min_max  , obj->bb[0]);
	DO_MAX(min_max+3, obj->bb[1]);
	
	BLI_bvhtree_insert(obj->bvh, (int)ob, min_max, 2 );	
}

static void RE_rayobject_blibvh_done(RayObject *o)
{
	BVHObject *obj = (BVHObject*)o;
	BLI_bvhtree_balance(obj->bvh);
}

static void RE_rayobject_blibvh_free(RayObject *o)
{
	BVHObject *obj = (BVHObject*)o;

	if(obj->bvh)
		BLI_bvhtree_free(obj->bvh);

	MEM_freeN(obj);
}

static void RE_rayobject_blibvh_bb(RayObject *o, float *min, float *max)
{
	BVHObject *obj = (BVHObject*)o;
	DO_MIN( obj->bb[0], min );
	DO_MAX( obj->bb[1], max );
}
