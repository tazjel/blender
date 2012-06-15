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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Daniel Genrich
 *                 Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/smoke.c
 *  \ingroup bke
 */


/* Part of the code copied from elbeem fluid library, copyright by Nils Thuerey */

#include <GL/glew.h>

#include "MEM_guardedalloc.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h> /* memset */

#include "BLI_linklist.h"
#include "BLI_rand.h"
#include "BLI_jitter.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_edgehash.h"
#include "BLI_kdtree.h"
#include "BLI_kdopbvh.h"
#include "BLI_utildefines.h"

#include "BKE_bvhutils.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_collision.h"
#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_effect.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_smoke.h"


#include "DNA_customdata_types.h"
#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_smoke_types.h"

#include "BKE_smoke.h"

/* UNUSED so far, may be enabled later */
/* #define USE_SMOKE_COLLISION_DM */

#ifdef WITH_SMOKE

#include "smoke_API.h"

#ifdef _WIN32
#include <time.h>
#include <stdio.h>
#include <conio.h>
#include <windows.h>

static LARGE_INTEGER liFrequency;
static LARGE_INTEGER liStartTime;
static LARGE_INTEGER liCurrentTime;

static void tstart ( void )
{
	QueryPerformanceFrequency ( &liFrequency );
	QueryPerformanceCounter ( &liStartTime );
}
static void tend ( void )
{
	QueryPerformanceCounter ( &liCurrentTime );
}
static double tval( void )
{
	return ((double)( (liCurrentTime.QuadPart - liStartTime.QuadPart)* (double)1000.0/(double)liFrequency.QuadPart ));
}
#else
#include <sys/time.h>
static struct timeval _tstart, _tend;
static struct timezone tz;
static void tstart ( void )
{
	gettimeofday ( &_tstart, &tz );
}
static void tend ( void )
{
	gettimeofday ( &_tend,&tz );
}

static double UNUSED_FUNCTION(tval)( void )
{
	double t1, t2;
	t1 = ( double ) _tstart.tv_sec*1000 + ( double ) _tstart.tv_usec/ ( 1000 );
	t2 = ( double ) _tend.tv_sec*1000 + ( double ) _tend.tv_usec/ ( 1000 );
	return t2-t1;
}
#endif

struct Object;
struct Scene;
struct DerivedMesh;
struct SmokeModifierData;

#define TRI_UVOFFSET (1./4.)

// timestep default value for nice appearance 0.1f
#define DT_DEFAULT 0.1f

/* forward declerations */
static void calcTriangleDivs(Object *ob, MVert *verts, int numverts, MFace *tris, int numfaces, int numtris, int **tridivs, float cell_len);
static void get_cell(float *p0, int res[3], float dx, float *pos, int *cell, int correct);
static void fill_scs_points(Object *ob, DerivedMesh *dm, SmokeCollSettings *scs);

#else /* WITH_SMOKE */

/* Stubs to use when smoke is disabled */
struct WTURBULENCE *smoke_turbulence_init(int *UNUSED(res), int UNUSED(amplify), int UNUSED(noisetype)) { return NULL; }
struct FLUID_3D *smoke_init(int *UNUSED(res), float *UNUSED(p0)) { return NULL; }
void smoke_free(struct FLUID_3D *UNUSED(fluid)) {}
float *smoke_get_density(struct FLUID_3D *UNUSED(fluid)) { return NULL; }
void smoke_turbulence_free(struct WTURBULENCE *UNUSED(wt)) {}
void smoke_initWaveletBlenderRNA(struct WTURBULENCE *UNUSED(wt), float *UNUSED(strength)) {}
void smoke_initBlenderRNA(struct FLUID_3D *UNUSED(fluid), float *UNUSED(alpha), float *UNUSED(beta), float *UNUSED(dt_factor), float *UNUSED(vorticity), int *UNUSED(border_colli)) {}
long long smoke_get_mem_req(int UNUSED(xres), int UNUSED(yres), int UNUSED(zres), int UNUSED(amplify)) { return 0; }
void smokeModifier_do(SmokeModifierData *UNUSED(smd), Scene *UNUSED(scene), Object *UNUSED(ob), DerivedMesh *UNUSED(dm)) {}

#endif /* WITH_SMOKE */

#ifdef WITH_SMOKE

static int smokeModifier_init (SmokeModifierData *smd, Object *ob, Scene *scene, DerivedMesh *dm)
{
	if((smd->type & MOD_SMOKE_TYPE_DOMAIN) && smd->domain && !smd->domain->fluid)
	{
		size_t i;
		float min[3] = {FLT_MAX, FLT_MAX, FLT_MAX}, max[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
		float size[3];
		MVert *verts = dm->getVertArray(dm);
		float scale = 0.0;
		int res;		

		res = smd->domain->maxres;

		// get BB of domain
		for(i = 0; i < dm->getNumVerts(dm); i++)
		{
			float tmp[3];

			copy_v3_v3(tmp, verts[i].co);
			mul_m4_v3(ob->obmat, tmp);

			// min BB
			min[0] = MIN2(min[0], tmp[0]);
			min[1] = MIN2(min[1], tmp[1]);
			min[2] = MIN2(min[2], tmp[2]);

			// max BB
			max[0] = MAX2(max[0], tmp[0]);
			max[1] = MAX2(max[1], tmp[1]);
			max[2] = MAX2(max[2], tmp[2]);
		}

		copy_v3_v3(smd->domain->p0, min);
		copy_v3_v3(smd->domain->p1, max);

		// calc other res with max_res provided
		sub_v3_v3v3(size, max, min);

		// prevent crash when initializing a plane as domain
		if((size[0] < FLT_EPSILON) || (size[1] < FLT_EPSILON) || (size[2] < FLT_EPSILON))
			return 0;

		if(size[0] > size[1])
		{
			if(size[0] > size[2])
			{
				scale = res / size[0];
				smd->domain->scale = size[0];
				smd->domain->dx = 1.0f / res; 
				smd->domain->res[0] = res;
				smd->domain->res[1] = (int)(size[1] * scale + 0.5);
				smd->domain->res[2] = (int)(size[2] * scale + 0.5);
			}
			else {
				scale = res / size[2];
				smd->domain->scale = size[2];
				smd->domain->dx = 1.0f / res;
				smd->domain->res[2] = res;
				smd->domain->res[0] = (int)(size[0] * scale + 0.5);
				smd->domain->res[1] = (int)(size[1] * scale + 0.5);
			}
		}
		else {
			if(size[1] > size[2])
			{
				scale = res / size[1];
				smd->domain->scale = size[1];
				smd->domain->dx = 1.0f / res; 
				smd->domain->res[1] = res;
				smd->domain->res[0] = (int)(size[0] * scale + 0.5);
				smd->domain->res[2] = (int)(size[2] * scale + 0.5);
			}
			else {
				scale = res / size[2];
				smd->domain->scale = size[2];
				smd->domain->dx = 1.0f / res;
				smd->domain->res[2] = res;
				smd->domain->res[0] = (int)(size[0] * scale + 0.5);
				smd->domain->res[1] = (int)(size[1] * scale + 0.5);
			}
		}

		// TODO: put in failsafe if res<=0 - dg

		// dt max is 0.1
		smd->domain->fluid = smoke_init(smd->domain->res, smd->domain->p0, DT_DEFAULT);
		smd->time = scene->r.cfra;

		if(smd->domain->flags & MOD_SMOKE_HIGHRES)
		{
			smd->domain->wt = smoke_turbulence_init(smd->domain->res, smd->domain->amplify + 1, smd->domain->noise);
			smd->domain->res_wt[0] = smd->domain->res[0] * (smd->domain->amplify + 1);
			smd->domain->res_wt[1] = smd->domain->res[1] * (smd->domain->amplify + 1);			
			smd->domain->res_wt[2] = smd->domain->res[2] * (smd->domain->amplify + 1);			
			smd->domain->dx_wt = smd->domain->dx / (smd->domain->amplify + 1);		
		}

		if(!smd->domain->shadow)
			smd->domain->shadow = MEM_callocN(sizeof(float) * smd->domain->res[0] * smd->domain->res[1] * smd->domain->res[2], "SmokeDomainShadow");

		smoke_initBlenderRNA(smd->domain->fluid, &(smd->domain->alpha), &(smd->domain->beta), &(smd->domain->time_scale), &(smd->domain->vorticity), &(smd->domain->border_collisions));

		if(smd->domain->wt)	
		{
			smoke_initWaveletBlenderRNA(smd->domain->wt, &(smd->domain->strength));
		}
		return 1;
	}
	else if((smd->type & MOD_SMOKE_TYPE_FLOW) && smd->flow)
	{
		// handle flow object here
		// XXX TODO

		smd->time = scene->r.cfra;

		return 1;
	}
	else if((smd->type & MOD_SMOKE_TYPE_COLL))
	{
		if(!smd->coll)
		{
			smokeModifier_createType(smd);
		}

		smd->time = scene->r.cfra;

		return 1;
	}

	return 2;
}

#endif /* WITH_SMOKE */

static void smokeModifier_freeDomain(SmokeModifierData *smd)
{
	if(smd->domain)
	{
		if(smd->domain->shadow)
				MEM_freeN(smd->domain->shadow);
			smd->domain->shadow = NULL;

		if(smd->domain->fluid)
			smoke_free(smd->domain->fluid);

		if(smd->domain->wt)
			smoke_turbulence_free(smd->domain->wt);

		if(smd->domain->effector_weights)
				MEM_freeN(smd->domain->effector_weights);
		smd->domain->effector_weights = NULL;

		BKE_ptcache_free_list(&(smd->domain->ptcaches[0]));
		smd->domain->point_cache[0] = NULL;

		MEM_freeN(smd->domain);
		smd->domain = NULL;
	}
}

static void smokeModifier_freeFlow(SmokeModifierData *smd)
{
	if(smd->flow)
	{
/*
		if(smd->flow->bvh)
		{
			free_bvhtree_from_mesh(smd->flow->bvh);
			MEM_freeN(smd->flow->bvh);
		}
		smd->flow->bvh = NULL;
*/
		MEM_freeN(smd->flow);
		smd->flow = NULL;
	}
}

static void smokeModifier_freeCollision(SmokeModifierData *smd)
{
	if(smd->coll)
	{
		SmokeCollSettings *scs = smd->coll;

		if(scs->numverts)
		{
			if(scs->verts_old)
			{
				MEM_freeN(scs->verts_old);
				scs->verts_old = NULL;
			}
		}

		if(smd->coll->dm)
			smd->coll->dm->release(smd->coll->dm);
		smd->coll->dm = NULL;

		MEM_freeN(smd->coll);
		smd->coll = NULL;
	}
}

void smokeModifier_reset_turbulence(struct SmokeModifierData *smd)
{
	if(smd && smd->domain && smd->domain->wt)
	{
		smoke_turbulence_free(smd->domain->wt);
		smd->domain->wt = NULL;
	}
}

void smokeModifier_reset(struct SmokeModifierData *smd)
{
	if(smd)
	{
		if(smd->domain)
		{
			if(smd->domain->shadow)
				MEM_freeN(smd->domain->shadow);
			smd->domain->shadow = NULL;

			if(smd->domain->fluid)
			{
				smoke_free(smd->domain->fluid);
				smd->domain->fluid = NULL;
			}

			smokeModifier_reset_turbulence(smd);

			smd->time = -1;

			// printf("reset domain end\n");
		}
		else if(smd->flow)
		{
			/*
			if(smd->flow->bvh)
			{
				free_bvhtree_from_mesh(smd->flow->bvh);
				MEM_freeN(smd->flow->bvh);
			}
			smd->flow->bvh = NULL;
			*/
		}
		else if(smd->coll)
		{
			SmokeCollSettings *scs = smd->coll;

			if(scs->numverts && scs->verts_old)
			{
				MEM_freeN(scs->verts_old);
				scs->verts_old = NULL;
			}
		}
	}
}

void smokeModifier_free(SmokeModifierData *smd)
{
	if(smd)
	{
		smokeModifier_freeDomain(smd);
		smokeModifier_freeFlow(smd);
		smokeModifier_freeCollision(smd);
	}
}

void smokeModifier_createType(struct SmokeModifierData *smd)
{
	if(smd)
	{
		if(smd->type & MOD_SMOKE_TYPE_DOMAIN)
		{
			if(smd->domain)
				smokeModifier_freeDomain(smd);

			smd->domain = MEM_callocN(sizeof(SmokeDomainSettings), "SmokeDomain");

			smd->domain->smd = smd;

			smd->domain->point_cache[0] = BKE_ptcache_add(&(smd->domain->ptcaches[0]));
			smd->domain->point_cache[0]->flag |= PTCACHE_DISK_CACHE;
			smd->domain->point_cache[0]->step = 1;

			/* Deprecated */
			smd->domain->point_cache[1] = NULL;
			smd->domain->ptcaches[1].first = smd->domain->ptcaches[1].last = NULL;
			/* set some standard values */
			smd->domain->fluid = NULL;
			smd->domain->wt = NULL;			
			smd->domain->eff_group = NULL;
			smd->domain->fluid_group = NULL;
			smd->domain->coll_group = NULL;
			smd->domain->maxres = 32;
			smd->domain->amplify = 1;			
			smd->domain->omega = 1.0;			
			smd->domain->alpha = -0.001;
			smd->domain->beta = 0.1;
			smd->domain->time_scale = 1.0;
			smd->domain->vorticity = 2.0;
			smd->domain->border_collisions = SM_BORDER_OPEN; // open domain
			smd->domain->flags = MOD_SMOKE_DISSOLVE_LOG | MOD_SMOKE_HIGH_SMOOTH;
			smd->domain->strength = 2.0;
			smd->domain->noise = MOD_SMOKE_NOISEWAVE;
			smd->domain->diss_speed = 5;
			// init 3dview buffer

			smd->domain->viewsettings = MOD_SMOKE_VIEW_SHOWBIG;
			smd->domain->effector_weights = BKE_add_effector_weights(NULL);
		}
		else if(smd->type & MOD_SMOKE_TYPE_FLOW)
		{
			if(smd->flow)
				smokeModifier_freeFlow(smd);

			smd->flow = MEM_callocN(sizeof(SmokeFlowSettings), "SmokeFlow");

			smd->flow->smd = smd;

			/* set some standard values */
			smd->flow->density = 1.0;
			smd->flow->temp = 1.0;
			smd->flow->flags = MOD_SMOKE_FLOW_ABSOLUTE;
			smd->flow->vel_multi = 1.0;

			smd->flow->psys = NULL;

		}
		else if(smd->type & MOD_SMOKE_TYPE_COLL)
		{
			if(smd->coll)
				smokeModifier_freeCollision(smd);

			smd->coll = MEM_callocN(sizeof(SmokeCollSettings), "SmokeColl");

			smd->coll->smd = smd;
			smd->coll->verts_old = NULL;
			smd->coll->numverts = 0;
			smd->coll->type = 0; // static obstacle
			smd->coll->dm = NULL;

#ifdef USE_SMOKE_COLLISION_DM
			smd->coll->dm = NULL;
#endif
		}
	}
}

void smokeModifier_copy(struct SmokeModifierData *smd, struct SmokeModifierData *tsmd)
{
	tsmd->type = smd->type;
	tsmd->time = smd->time;
	
	smokeModifier_createType(tsmd);

	if (tsmd->domain) {
		tsmd->domain->maxres = smd->domain->maxres;
		tsmd->domain->amplify = smd->domain->amplify;
		tsmd->domain->omega = smd->domain->omega;
		tsmd->domain->alpha = smd->domain->alpha;
		tsmd->domain->beta = smd->domain->beta;
		tsmd->domain->flags = smd->domain->flags;
		tsmd->domain->strength = smd->domain->strength;
		tsmd->domain->noise = smd->domain->noise;
		tsmd->domain->diss_speed = smd->domain->diss_speed;
		tsmd->domain->viewsettings = smd->domain->viewsettings;
		tsmd->domain->fluid_group = smd->domain->fluid_group;
		tsmd->domain->coll_group = smd->domain->coll_group;
		tsmd->domain->vorticity = smd->domain->vorticity;
		tsmd->domain->time_scale = smd->domain->time_scale;
		tsmd->domain->border_collisions = smd->domain->border_collisions;
		
		MEM_freeN(tsmd->domain->effector_weights);
		tsmd->domain->effector_weights = MEM_dupallocN(smd->domain->effector_weights);
	} 
	else if (tsmd->flow) {
		tsmd->flow->density = smd->flow->density;
		tsmd->flow->temp = smd->flow->temp;
		tsmd->flow->psys = smd->flow->psys;
		tsmd->flow->type = smd->flow->type;
		tsmd->flow->flags = smd->flow->flags;
		tsmd->flow->vel_multi = smd->flow->vel_multi;
	}
	else if (tsmd->coll) {
		;
		/* leave it as initialized, collision settings is mostly caches */
	}
}

#ifdef WITH_SMOKE

// forward decleration
static void smoke_calc_transparency(float *result, float *input, float *p0, float *p1, int res[3], float dx, float *light, bresenham_callback cb, float correct);
static float calc_voxel_transp(float *result, float *input, int res[3], int *pixel, float *tRay, float correct);

static int get_lamp(Scene *scene, float *light)
{	
	Base *base_tmp = NULL;	
	int found_lamp = 0;

	// try to find a lamp, preferably local
	for(base_tmp = scene->base.first; base_tmp; base_tmp= base_tmp->next) {
		if(base_tmp->object->type == OB_LAMP) {
			Lamp *la = base_tmp->object->data;

			if(la->type == LA_LOCAL) {
				copy_v3_v3(light, base_tmp->object->obmat[3]);
				return 1;
			}
			else if(!found_lamp) {
				copy_v3_v3(light, base_tmp->object->obmat[3]);
				found_lamp = 1;
			}
		}
	}

	return found_lamp;
}

static void smoke_calc_domain(Scene *UNUSED(scene), Object *UNUSED(ob), SmokeModifierData *UNUSED(smd))
{
#if 0
	SmokeDomainSettings *sds = smd->domain;
	GroupObject *go = NULL;			
	Base *base = NULL;

	/* do collisions, needs to be done before emission, so that smoke isn't emitted inside collision cells */
	if(1)
	{
		unsigned int i;
		Object **collobjs = NULL;
		unsigned int numcollobj = 0;
		collobjs = get_collisionobjects(scene, ob, sds->coll_group, &numcollobj);

		for(i = 0; i < numcollobj; i++)
		{
			Object *collob= collobjs[i];
			SmokeModifierData *smd2 = (SmokeModifierData*)modifiers_findByType(collob, eModifierType_Smoke);		
			
			// check for active smoke modifier
			// if(md && md->mode & (eModifierMode_Realtime | eModifierMode_Render))
			// SmokeModifierData *smd2 = (SmokeModifierData *)md;

			if((smd2->type & MOD_SMOKE_TYPE_COLL) && smd2->coll && smd2->coll->points && smd2->coll->points_old)
			{
				// ??? anything to do here?

				// TODO: only something to do for ANIMATED obstacles: need to update positions

			}
		}

		if(collobjs)
			MEM_freeN(collobjs);
	}

#endif
}

static void obstacles_from_derivedmesh(Object *coll_ob, SmokeDomainSettings *sds, SmokeCollSettings *scs, unsigned char *obstacle_map, float *velocityX, float *velocityY, float *velocityZ, float dt)
{
	if (!scs->dm) return;
	{
		DerivedMesh *dm = NULL;
		MDeformVert *dvert = NULL;
		MVert *mvert = NULL;
		MFace *mface = NULL;
		BVHTreeFromMesh treeData = {0};
		int numverts, i, z;
		int *res = sds->res;

		float surface_distance = 0.6;

		float *vert_vel = NULL;
		int has_velocity = 0;

		tstart();

		dm = CDDM_copy(scs->dm);
		CDDM_calc_normals(dm);
		mvert = dm->getVertArray(dm);
		mface = dm->getTessFaceArray(dm);
		numverts = dm->getNumVerts(dm);
		dvert = dm->getVertDataArray(dm, CD_MDEFORMVERT);

		// DG TODO
		// if(scs->type > SM_COLL_STATIC)
		// if line above is used, the code is in trouble if the object moves but is declared as "does not move"

		// if (sfs->flags & MOD_SMOKE_FLOW_INITVELOCITY) 
		{
			vert_vel = MEM_callocN(sizeof(float) * numverts * 3, "smoke_obs_velocity");

			if (scs->numverts != numverts || !scs->verts_old) {
				if (scs->verts_old) MEM_freeN(scs->verts_old);

				scs->verts_old = MEM_callocN(sizeof(float) * numverts * 3, "smoke_obs_verts_old");
				scs->numverts = numverts;
			}
			else {
				has_velocity = 1;
			}
		}

		/*	Transform collider vertices to
		*   domain grid space for fast lookups */
		for (i = 0; i < numverts; i++) {
			float n[3];

			/* vert pos */
			mul_m4_v3(coll_ob->obmat, mvert[i].co);
			sub_v3_v3(mvert[i].co, sds->p0);
			mul_v3_fl(mvert[i].co, (1.0f / sds->dx) / sds->scale);

			/* vert normal */
			normal_short_to_float_v3(n, mvert[i].no);
			mul_mat3_m4_v3(coll_ob->obmat, n);
			normalize_v3(n);
			normal_float_to_short_v3(mvert[i].no, n);

			/* vert velocity */
			// DG TODO 
			// if (sfs->flags & MOD_SMOKE_FLOW_INITVELOCITY) 
			{
				if (has_velocity) 
				{
					sub_v3_v3v3(&vert_vel[i*3], mvert[i].co, &scs->verts_old[i*3]);
					mul_v3_fl(&vert_vel[i*3], sds->dx/dt);
				}
				copy_v3_v3(&scs->verts_old[i*3], mvert[i].co);
			}
		}

		if (bvhtree_from_mesh_faces(&treeData, dm, 0.0f, 4, 6)) {
			#pragma omp parallel for schedule(static)
			for (z = 0; z < res[2]; z++) {
				int x,y;
				for (x = 0; x < res[0]; x++)
				for (y = 0; y < res[1]; y++) {
					int index = x + y*res[0] + z*res[0]*res[1];

					float ray_start[3] = {(float)x + 0.5f, (float)y + 0.5f, (float)z + 0.5f};
					float ray_dir[3] = {1.0f, 0.0f, 0.0f};

					BVHTreeRayHit hit = {0};
					BVHTreeNearest nearest = {0};

					hit.index = -1;
					hit.dist = 9999;
					nearest.index = -1;
					nearest.dist = surface_distance * surface_distance; /* find_nearest uses squared distance */

					/* find the nearest point on the mesh */
					if (BLI_bvhtree_find_nearest(treeData.tree, ray_start, &nearest, treeData.nearest_callback, &treeData) != -1) {
						float weights[4];
						int v1, v2, v3, f_index = nearest.index;
						float n1[3], n2[3], n3[3], hit_normal[3];

						/* calculate barycentric weights for nearest point */
						v1 = mface[f_index].v1;
						v2 = (nearest.flags & BVH_ONQUAD) ? mface[f_index].v3 : mface[f_index].v2;
						v3 = (nearest.flags & BVH_ONQUAD) ? mface[f_index].v4 : mface[f_index].v3;
						interp_weights_face_v3(weights, mvert[v1].co, mvert[v2].co, mvert[v3].co, NULL, nearest.co);

						// DG TODO
						// if(sfs->flags & MOD_SMOKE_FLOW_INITVELOCITY) 
						if(has_velocity)
						{
							/* interpolate vertex normal vectors to get nearest point normal */
							normal_short_to_float_v3(n1, mvert[v1].no);
							normal_short_to_float_v3(n2, mvert[v2].no);
							normal_short_to_float_v3(n3, mvert[v3].no);
							interp_v3_v3v3v3(hit_normal, n1, n2, n3, weights);
							normalize_v3(hit_normal);
							
							/* apply object velocity */
							{
								float hit_vel[3];
								interp_v3_v3v3v3(hit_vel, &vert_vel[v1*3], &vert_vel[v2*3], &vert_vel[v3*3], weights);
								velocityX[index] += hit_vel[0];
								velocityY[index] += hit_vel[1];
								velocityZ[index] += hit_vel[2];
							}
						}

						/* tag obstacle cells */
						obstacle_map[index] = 1;

						if(has_velocity)
							obstacle_map[index] |= 8;
					}
				}
			}
		}
		/* free bvh tree */
		free_bvhtree_from_mesh(&treeData);
		dm->release(dm);

		if (vert_vel) MEM_freeN(vert_vel);

		tend();
		printf ( "Time: %f\n\n", ( float ) tval() );

	}
}

/* Animated obstacles: dx_step = ((x_new - x_old) / totalsteps) * substep */
static void update_obstacles(Scene *scene, Object *ob, SmokeDomainSettings *sds, float dt, int substep, int totalsteps)
{
	Object **collobjs = NULL;
	unsigned int numcollobj = 0;

	unsigned int collIndex;
	unsigned char *obstacles = smoke_get_obstacle(sds->fluid);
	float *velx = NULL;
	float *vely = NULL;
	float *velz = NULL;
	float *velxOrig = smoke_get_velocity_x(sds->fluid);
	float *velyOrig = smoke_get_velocity_y(sds->fluid);
	float *velzOrig = smoke_get_velocity_z(sds->fluid);
	float *density = smoke_get_density(sds->fluid);
	unsigned int z;

	smoke_get_ob_velocity(sds->fluid, &velx, &vely, &velz);

	// TODO: delete old obstacle flags
	for(z = 0; z < sds->res[0] * sds->res[1] * sds->res[2]; z++)
	{
		if(obstacles[z] & 8) // Do not delete static obstacles
		{
			obstacles[z] = 0;
		}

		velx[z] = 0;
		vely[z] = 0;
		velz[z] = 0;
	}

	collobjs = get_collisionobjects(scene, ob, sds->coll_group, &numcollobj, eModifierType_Smoke);

	// update obstacle tags in cells
	for(collIndex = 0; collIndex < numcollobj; collIndex++)
	{
		Object *collob= collobjs[collIndex];
		SmokeModifierData *smd2 = (SmokeModifierData*)modifiers_findByType(collob, eModifierType_Smoke);

		// DG TODO: check if modifier is active?
		
		if((smd2->type & MOD_SMOKE_TYPE_COLL) && smd2->coll)
		{
			SmokeCollSettings *scs = smd2->coll;
			
			obstacles_from_derivedmesh(collob, sds, scs, obstacles, velx, vely, velz, dt);
		}
	}

	if(collobjs)
		MEM_freeN(collobjs);

	/* obstacle cells should not contain any velocity from the smoke simulation */
	for(z = 0; z < sds->res[0] * sds->res[1] * sds->res[2]; z++)
	{
		if(obstacles[z])
		{
			velxOrig[z] = 0;
			velyOrig[z] = 0;
			velzOrig[z] = 0;

			density[z] = 0;
		}
	}
}

static void update_flowsfluids(Scene *scene, Object *ob, SmokeDomainSettings *sds, float time)
{
	Object **flowobjs = NULL;
	unsigned int numflowobj = 0;
	unsigned int flowIndex;

	flowobjs = get_collisionobjects(scene, ob, sds->fluid_group, &numflowobj, eModifierType_Smoke);

	// update obstacle tags in cells
	for(flowIndex = 0; flowIndex < numflowobj; flowIndex++)
	{
		Object *collob= flowobjs[flowIndex];
		SmokeModifierData *smd2 = (SmokeModifierData*)modifiers_findByType(collob, eModifierType_Smoke);

		// check for initialized smoke object
		if((smd2->type & MOD_SMOKE_TYPE_FLOW) && smd2->flow)						
		{
			// we got nice flow object
			SmokeFlowSettings *sfs = smd2->flow;

			if(sfs && sfs->psys && sfs->psys->part && sfs->psys->part->type==PART_EMITTER) // is particle system selected
			{
				ParticleSimulationData sim;
				ParticleSystem *psys = sfs->psys;
				int totpart=psys->totpart, totchild;
				int p = 0;								
				float *density = smoke_get_density(sds->fluid);								
				float *bigdensity = smoke_turbulence_get_density(sds->wt);								
				float *heat = smoke_get_heat(sds->fluid);								
				float *velocity_x = smoke_get_velocity_x(sds->fluid);								
				float *velocity_y = smoke_get_velocity_y(sds->fluid);								
				float *velocity_z = smoke_get_velocity_z(sds->fluid);								
				unsigned char *obstacle = smoke_get_obstacle(sds->fluid);							
				// DG TODO UNUSED unsigned char *obstacleAnim = smoke_get_obstacle_anim(sds->fluid);
				int bigres[3];
				short absolute_flow = (sfs->flags & MOD_SMOKE_FLOW_ABSOLUTE);
				short high_emission_smoothing = bigdensity ? (sds->flags & MOD_SMOKE_HIGH_SMOOTH) : 0;

				/*
				* A temporary volume map used to store whole emissive
				* area to be added to smoke density and interpolated
				* for high resolution smoke.
				*/
				float *temp_emission_map = NULL;

				sim.scene = scene;
				sim.ob = collob;
				sim.psys = psys;

				// initialize temp emission map
				if(!(sfs->type & MOD_SMOKE_FLOW_TYPE_OUTFLOW))
				{
					int i;
					temp_emission_map = MEM_callocN(sizeof(float) * sds->res[0]*sds->res[1]*sds->res[2], "SmokeTempEmission");
					// set whole volume to 0.0f
					for (i=0; i<sds->res[0]*sds->res[1]*sds->res[2]; i++) {
						temp_emission_map[i] = 0.0f;
					}
				}

				// mostly copied from particle code								
				if(psys->part->type==PART_HAIR)
				{
					/*
					if(psys->childcache)
					{
					totchild = psys->totchildcache;
					}
					else
					*/

					// TODO: PART_HAIR not supported whatsoever
					totchild=0;
				}
				else
					totchild=psys->totchild*psys->part->disp/100;

				for(p=0; p<totpart+totchild; p++)								
				{
					int cell[3];
					size_t i = 0;
					size_t index = 0;
					int badcell = 0;
					ParticleKey state;

					if(p < totpart)
					{
						if(psys->particles[p].flag & (PARS_NO_DISP|PARS_UNEXIST))
							continue;
					}
					else {
						/* handle child particle */
						ChildParticle *cpa = &psys->child[p - totpart];

						if(psys->particles[cpa->parent].flag & (PARS_NO_DISP|PARS_UNEXIST))
							continue;
					}

					state.time = time;
					if(psys_get_particle_state(&sim, p, &state, 0) == 0)
						continue;

					// copy_v3_v3(pos, pa->state.co);
					// mul_m4_v3(ob->imat, pos);
					// 1. get corresponding cell
					get_cell(sds->p0, sds->res, sds->dx*sds->scale, state.co, cell, 0);																	
					// check if cell is valid (in the domain boundary)									
					for(i = 0; i < 3; i++)									
					{										
						if((cell[i] > sds->res[i] - 1) || (cell[i] < 0))										
						{											
							badcell = 1;											
							break;										
						}									
					}																			
					if(badcell)										
						continue;																		
					// 2. set cell values (heat, density and velocity)									
					index = smoke_get_index(cell[0], sds->res[0], cell[1], sds->res[1], cell[2]);																		
					if(!(sfs->type & MOD_SMOKE_FLOW_TYPE_OUTFLOW) && !(obstacle[index])) // this is inflow
					{										
						// heat[index] += sfs->temp * 0.1;										
						// density[index] += sfs->density * 0.1;
						heat[index] = sfs->temp;

						// Add emitter density to temp emission map
						temp_emission_map[index] = sfs->density;

						// Uses particle velocity as initial velocity for smoke
						if(sfs->flags & MOD_SMOKE_FLOW_INITVELOCITY && (psys->part->phystype != PART_PHYS_NO))
						{
							velocity_x[index] = state.vel[0]*sfs->vel_multi;
							velocity_y[index] = state.vel[1]*sfs->vel_multi;
							velocity_z[index] = state.vel[2]*sfs->vel_multi;
						}										
					}									
					else if(sfs->type & MOD_SMOKE_FLOW_TYPE_OUTFLOW) // outflow									
					{										
						heat[index] = 0.f;										
						density[index] = 0.f;										
						velocity_x[index] = 0.f;										
						velocity_y[index] = 0.f;										
						velocity_z[index] = 0.f;
						// we need different handling for the high-res feature
						if(bigdensity)
						{
							// init all surrounding cells according to amplification, too											
							int i, j, k;
							smoke_turbulence_get_res(sds->wt, bigres);

							for(i = 0; i < sds->amplify + 1; i++)
								for(j = 0; j < sds->amplify + 1; j++)
									for(k = 0; k < sds->amplify + 1; k++)
									{														
										index = smoke_get_index((sds->amplify + 1)* cell[0] + i, bigres[0], (sds->amplify + 1)* cell[1] + j, bigres[1], (sds->amplify + 1)* cell[2] + k);														
										bigdensity[index] = 0.f;													
									}										
						}
					}
				}	// particles loop

				// apply emission values
				if(!(sfs->type & MOD_SMOKE_FLOW_TYPE_OUTFLOW)) 
				{
					// initialize variables
					int ii, jj, kk, x, y, z, block_size;
					size_t index, index_big;

					smoke_turbulence_get_res(sds->wt, bigres);
					block_size = sds->amplify + 1;	// high res block size

					// loop through every low res cell
					for(x = 0; x < sds->res[0]; x++)
						for(y = 0; y < sds->res[1]; y++)
							for(z = 0; z < sds->res[2]; z++)													
							{
								// neighbor cell emission densities (for high resolution smoke smooth interpolation)
								float c000, c001, c010, c011,  c100, c101, c110, c111;

								c000 = (x>0 && y>0 && z>0) ? temp_emission_map[smoke_get_index(x-1, sds->res[0], y-1, sds->res[1], z-1)] : 0;
								c001 = (x>0 && y>0) ? temp_emission_map[smoke_get_index(x-1, sds->res[0], y-1, sds->res[1], z)] : 0;
								c010 = (x>0 && z>0) ? temp_emission_map[smoke_get_index(x-1, sds->res[0], y, sds->res[1], z-1)] : 0;
								c011 = (x>0) ? temp_emission_map[smoke_get_index(x-1, sds->res[0], y, sds->res[1], z)] : 0;

								c100 = (y>0 && z>0) ? temp_emission_map[smoke_get_index(x, sds->res[0], y-1, sds->res[1], z-1)] : 0;
								c101 = (y>0) ? temp_emission_map[smoke_get_index(x, sds->res[0], y-1, sds->res[1], z)] : 0;
								c110 = (z>0) ? temp_emission_map[smoke_get_index(x, sds->res[0], y, sds->res[1], z-1)] : 0;
								c111 = temp_emission_map[smoke_get_index(x, sds->res[0], y, sds->res[1], z)]; // this cell

								// get cell index
								index = smoke_get_index(x, sds->res[0], y, sds->res[1], z);

								// add emission to low resolution density
								if (absolute_flow) 
								{
									if (temp_emission_map[index]>0) 
										density[index] = temp_emission_map[index];
								}
								else 
								{
									density[index] += temp_emission_map[index];

									if (density[index]>1) 
										density[index]=1.0f;
								}

								smoke_turbulence_get_res(sds->wt, bigres);

								/* loop through high res blocks if high res enabled */
								if (bigdensity)
									for(ii = 0; ii < block_size; ii++)
										for(jj = 0; jj < block_size; jj++)
											for(kk = 0; kk < block_size; kk++)													
											{

												float fx,fy,fz, interpolated_value;
												int shift_x, shift_y, shift_z;


												/*
												* Do volume interpolation if emitter smoothing
												* is enabled
												*/
												if (high_emission_smoothing) 
												{
													// convert block position to relative
													// for interpolation smoothing
													fx = (float)ii/block_size + 0.5f/block_size;
													fy = (float)jj/block_size + 0.5f/block_size;
													fz = (float)kk/block_size + 0.5f/block_size;

													// calculate trilinear interpolation
													interpolated_value = c000 * (1-fx) * (1-fy) * (1-fz) +
														c100 * fx * (1-fy) * (1-fz) +
														c010 * (1-fx) * fy * (1-fz) +
														c001 * (1-fx) * (1-fy) * fz +
														c101 * fx * (1-fy) * fz +
														c011 * (1-fx) * fy * fz +
														c110 * fx * fy * (1-fz) +
														c111 * fx * fy * fz;


													// add some contrast / sharpness
													// depending on hi-res block size

													interpolated_value = (interpolated_value-0.4f*sfs->density)*(block_size/2) + 0.4f*sfs->density;
													if (interpolated_value<0.0f) interpolated_value = 0.0f;
													if (interpolated_value>1.0f) interpolated_value = 1.0f;

													// shift smoke block index
													// (because pixel center is actually
													// in halfway of the low res block)
													shift_x = (x < 1) ? 0 : block_size/2;
													shift_y = (y < 1) ? 0 : block_size/2;
													shift_z = (z < 1) ? 0 : block_size/2;
												}
												else 
												{
													// without interpolation use same low resolution
													// block value for all hi-res blocks
													interpolated_value = c111;
													shift_x = 0;
													shift_y = 0;
													shift_z = 0;
												}

												// get shifted index for current high resolution block
												index_big = smoke_get_index(block_size * x + ii - shift_x, bigres[0], block_size * y + jj - shift_y, bigres[1], block_size * z + kk - shift_z);														

												// add emission data to high resolution density
												if (absolute_flow) 
												{
													if (interpolated_value > 0) 
														bigdensity[index_big] = interpolated_value;
												}
												else 
												{
													bigdensity[index_big] += interpolated_value;

													if (bigdensity[index_big]>1) 
														bigdensity[index_big]=1.0f;
												}
											} // end of hires loop

							} // end of low res loop

							// free temporary emission map
							if (temp_emission_map) 
								MEM_freeN(temp_emission_map);

				} // end emission
			}
		}
	}

	if(flowobjs)
		MEM_freeN(flowobjs);
}

static void update_effectors(Scene *scene, Object *ob, SmokeDomainSettings *sds, float UNUSED(dt))
{
	ListBase *effectors = pdInitEffectors(scene, ob, NULL, sds->effector_weights);

	if(effectors)
	{
		float *density = smoke_get_density(sds->fluid);
		float *force_x = smoke_get_force_x(sds->fluid);
		float *force_y = smoke_get_force_y(sds->fluid);
		float *force_z = smoke_get_force_z(sds->fluid);
		float *velocity_x = smoke_get_velocity_x(sds->fluid);
		float *velocity_y = smoke_get_velocity_y(sds->fluid);
		float *velocity_z = smoke_get_velocity_z(sds->fluid);
		unsigned char *obstacle = smoke_get_obstacle(sds->fluid);
		int x, y, z;

		// precalculate wind forces
		for(x = 0; x < sds->res[0]; x++)
			for(y = 0; y < sds->res[1]; y++)
				for(z = 0; z < sds->res[2]; z++)
		{	
			EffectedPoint epoint;
			float voxelCenter[3] = {0,0,0}, vel[3] = {0,0,0}, retvel[3] = {0,0,0};
			unsigned int index = smoke_get_index(x, sds->res[0], y, sds->res[1], z);

			if((density[index] < FLT_EPSILON) || obstacle[index])					
				continue;	

			vel[0] = velocity_x[index];
			vel[1] = velocity_y[index];
			vel[2] = velocity_z[index];

			voxelCenter[0] = sds->p0[0] + sds->dx * sds->scale * x + sds->dx * sds->scale * 0.5;
			voxelCenter[1] = sds->p0[1] + sds->dx * sds->scale * y + sds->dx * sds->scale * 0.5;
			voxelCenter[2] = sds->p0[2] + sds->dx * sds->scale * z + sds->dx * sds->scale * 0.5;

			pd_point_from_loc(scene, voxelCenter, vel, index, &epoint);
			pdDoEffectors(effectors, NULL, sds->effector_weights, &epoint, retvel, NULL);

			// TODO dg - do in force!
			force_x[index] = MIN2(MAX2(-1.0, retvel[0] * 0.2), 1.0); 
			force_y[index] = MIN2(MAX2(-1.0, retvel[1] * 0.2), 1.0); 
			force_z[index] = MIN2(MAX2(-1.0, retvel[2] * 0.2), 1.0);
		}
	}

	pdEndEffectors(&effectors);
}

static void step(Scene *scene, Object *ob, SmokeModifierData *smd, float fps)
{
	/* stability values copied from wturbulence.cpp */
	const int maxSubSteps = 25;
	float maxVel;
	// maxVel should be 1.5 (1.5 cell max movement) * dx (cell size)

	float dt = DT_DEFAULT;
	float maxVelMag = 0.0f;
	int totalSubsteps;
	int substep = 0;
	float dtSubdiv;

	SmokeDomainSettings *sds = smd->domain;

	/* get max velocity and lower the dt value if it is too high */
	size_t size= sds->res[0] * sds->res[1] * sds->res[2];

	float *velX = smoke_get_velocity_x(sds->fluid);
	float *velY = smoke_get_velocity_y(sds->fluid);
	float *velZ = smoke_get_velocity_z(sds->fluid);
	size_t i;

	/* adapt timestep for different framerates, dt = 0.1 is at 25fps */
	dt *= (25.0f / fps);

	// maximum timestep/"CFL" constraint: dt < 5.0 *dx / maxVel
	maxVel = (sds->dx * 5.0);

	for(i = 0; i < size; i++)
	{
		float vtemp = (velX[i]*velX[i]+velY[i]*velY[i]+velZ[i]*velZ[i]);
		if(vtemp > maxVelMag)
			maxVelMag = vtemp;
	}

	maxVelMag = sqrt(maxVelMag) * dt * sds->time_scale;
	totalSubsteps = (int)((maxVelMag / maxVel) + 1.0f); /* always round up */
	totalSubsteps = (totalSubsteps < 1) ? 1 : totalSubsteps;
	totalSubsteps = (totalSubsteps > maxSubSteps) ? maxSubSteps : totalSubsteps;

	/* Disable substeps for now, since it results in numerical instability */
	totalSubsteps = 1.0f; 

	dtSubdiv = (float)dt / (float)totalSubsteps;

	// printf("totalSubsteps: %d, maxVelMag: %f, dt: %f\n", totalSubsteps, maxVelMag, dt);

	for(substep = 0; substep < totalSubsteps; substep++)
	{
		// calc animated obstacle velocities
		update_obstacles(scene, ob, sds, dtSubdiv, substep, totalSubsteps);
		update_flowsfluids(scene, ob, sds, smd->time);
		update_effectors(scene, ob, sds, dtSubdiv); // DG TODO? problem --> uses forces instead of velocity, need to check how they need to be changed with variable dt

		smoke_step(sds->fluid, dtSubdiv);

		// move animated obstacle: Done in update_obstacles() */

		// where to delete old obstacles from array? Done in update_obstacles() */
	}
}

void smokeModifier_do(SmokeModifierData *smd, Scene *scene, Object *ob, DerivedMesh *dm)
{	
	if((smd->type & MOD_SMOKE_TYPE_FLOW))
	{
		if(scene->r.cfra >= smd->time)
			smokeModifier_init(smd, ob, scene, dm);

		if(scene->r.cfra > smd->time)
		{
			// XXX TODO
			smd->time = scene->r.cfra;

			// rigid movement support
			/*
			copy_m4_m4(smd->flow->mat_old, smd->flow->mat);
			copy_m4_m4(smd->flow->mat, ob->obmat);
			*/
		}
		else if(scene->r.cfra < smd->time)
		{
			smd->time = scene->r.cfra;
			smokeModifier_reset(smd);
		}
	}
	else if(smd->type & MOD_SMOKE_TYPE_COLL)
	{
		if(scene->r.cfra >= smd->time)
			smokeModifier_init(smd, ob, scene, dm);

		if(smd->coll)
		{
			if (smd->coll->dm) 
				smd->coll->dm->release(smd->coll->dm);

			smd->coll->dm = CDDM_copy(dm);
			DM_ensure_tessface(smd->coll->dm);
		}

		if(scene->r.cfra > smd->time)
		{
			SmokeCollSettings *scs = smd->coll;
			smd->time = scene->r.cfra;
		}
		else if(scene->r.cfra < smd->time)
		{
			smd->time = scene->r.cfra;
			smokeModifier_reset(smd);
		}
	}
	else if(smd->type & MOD_SMOKE_TYPE_DOMAIN)
	{
		SmokeDomainSettings *sds = smd->domain;
		float light[3];	
		PointCache *cache = NULL;
		PTCacheID pid;
		int startframe, endframe, framenr;
		float timescale;

		framenr = scene->r.cfra;

		//printf("time: %d\n", scene->r.cfra);

		cache = sds->point_cache[0];
		BKE_ptcache_id_from_smoke(&pid, ob, smd);
		BKE_ptcache_id_time(&pid, scene, framenr, &startframe, &endframe, &timescale);

		if(!smd->domain->fluid || framenr == startframe)
		{
			BKE_ptcache_id_reset(scene, &pid, PTCACHE_RESET_OUTDATED);
			BKE_ptcache_validate(cache, framenr);
			cache->flag &= ~PTCACHE_REDO_NEEDED;
		}

		if(!smd->domain->fluid && (framenr != startframe) && (smd->domain->flags & MOD_SMOKE_FILE_LOAD)==0 && (cache->flag & PTCACHE_BAKED)==0)
			return;

		smd->domain->flags &= ~MOD_SMOKE_FILE_LOAD;

		CLAMP(framenr, startframe, endframe);

		/* If already viewing a pre/after frame, no need to reload */
		if ((smd->time == framenr) && (framenr != scene->r.cfra))
			return;

		// printf("startframe: %d, framenr: %d\n", startframe, framenr);

		if(smokeModifier_init(smd, ob, scene, dm)==0)
		{
			printf("bad smokeModifier_init\n");
			return;
		}

		/* try to read from cache */
		if(BKE_ptcache_read(&pid, (float)framenr) == PTCACHE_READ_EXACT) {
			BKE_ptcache_validate(cache, framenr);
			smd->time = framenr;
			return;
		}
		
		/* only calculate something when we advanced a single frame */
		if(framenr != (int)smd->time+1)
			return;

		/* don't simulate if viewing start frame, but scene frame is not real start frame */
		if (framenr != scene->r.cfra)
			return;

		// tstart();

		smoke_calc_domain(scene, ob, smd);

		/* if on second frame, write cache for first frame */
		if((int)smd->time == startframe && (cache->flag & PTCACHE_OUTDATED || cache->last_exact==0)) {
			// create shadows straight after domain initialization so we get nice shadows for startframe, too
			if(get_lamp(scene, light))
				smoke_calc_transparency(sds->shadow, smoke_get_density(sds->fluid), sds->p0, sds->p1, sds->res, sds->dx, light, calc_voxel_transp, -7.0*sds->dx);

			if(sds->wt)
			{
				if(sds->flags & MOD_SMOKE_DISSOLVE)
					smoke_dissolve_wavelet(sds->wt, sds->diss_speed, sds->flags & MOD_SMOKE_DISSOLVE_LOG);
				smoke_turbulence_step(sds->wt, sds->fluid);
			}

			BKE_ptcache_write(&pid, startframe);
		}
		
		// set new time
		smd->time = scene->r.cfra;

		/* do simulation */

		// low res

		// simulate the actual smoke (c++ code in intern/smoke)
		// DG: interesting commenting this line + deactivating loading of noise files
		if(framenr!=startframe)
		{
			if(sds->flags & MOD_SMOKE_DISSOLVE)
				smoke_dissolve(sds->fluid, sds->diss_speed, sds->flags & MOD_SMOKE_DISSOLVE_LOG);
			
			step(scene, ob, smd, scene->r.frs_sec / scene->r.frs_sec_base);
		}

		// create shadows before writing cache so they get stored
		if(get_lamp(scene, light))
			smoke_calc_transparency(sds->shadow, smoke_get_density(sds->fluid), sds->p0, sds->p1, sds->res, sds->dx, light, calc_voxel_transp, -7.0*sds->dx);

		if(sds->wt)
		{
			if(sds->flags & MOD_SMOKE_DISSOLVE)
				smoke_dissolve_wavelet(sds->wt, sds->diss_speed, sds->flags & MOD_SMOKE_DISSOLVE_LOG);
			smoke_turbulence_step(sds->wt, sds->fluid);
		}
	
		BKE_ptcache_validate(cache, framenr);
		if(framenr != startframe)
			BKE_ptcache_write(&pid, framenr);

		//tend();
		// printf ( "Frame: %d, Time: %f\n\n", (int)smd->time, ( float ) tval() );
	}
}

static float calc_voxel_transp(float *result, float *input, int res[3], int *pixel, float *tRay, float correct)
{
	const size_t index = smoke_get_index(pixel[0], res[0], pixel[1], res[1], pixel[2]);

	// T_ray *= T_vox
	*tRay *= exp(input[index]*correct);
	
	if(result[index] < 0.0f)	
	{
// #pragma omp critical		
		result[index] = *tRay;	
	}	

	return *tRay;
}

long long smoke_get_mem_req(int xres, int yres, int zres, int amplify)
{
	int totalCells = xres * yres * zres;
	int amplifiedCells = totalCells * amplify * amplify * amplify;

	// print out memory requirements
	long long int coarseSize = sizeof(float) * totalCells * 22 +
	sizeof(unsigned char) * totalCells;

	long long int fineSize = sizeof(float) * amplifiedCells * 7 + // big grids
	sizeof(float) * totalCells * 8 +     // small grids
	sizeof(float) * 128 * 128 * 128;     // noise tile

	long long int totalMB = (coarseSize + fineSize) / (1024 * 1024);

	return totalMB;
}

static void bresenham_linie_3D(int x1, int y1, int z1, int x2, int y2, int z2, float *tRay, bresenham_callback cb, float *result, float *input, int res[3], float correct)
{
	int dx, dy, dz, i, l, m, n, x_inc, y_inc, z_inc, err_1, err_2, dx2, dy2, dz2;
	int pixel[3];

	pixel[0] = x1;
	pixel[1] = y1;
	pixel[2] = z1;

	dx = x2 - x1;
	dy = y2 - y1;
	dz = z2 - z1;

	x_inc = (dx < 0) ? -1 : 1;
	l = abs(dx);
	y_inc = (dy < 0) ? -1 : 1;
	m = abs(dy);
	z_inc = (dz < 0) ? -1 : 1;
	n = abs(dz);
	dx2 = l << 1;
	dy2 = m << 1;
	dz2 = n << 1;

	if ((l >= m) && (l >= n)) {
		err_1 = dy2 - l;
		err_2 = dz2 - l;
		for (i = 0; i < l; i++) {
			if(cb(result, input, res, pixel, tRay, correct) <= FLT_EPSILON)
				break;
			if (err_1 > 0) {
				pixel[1] += y_inc;
				err_1 -= dx2;
			}
			if (err_2 > 0) {
				pixel[2] += z_inc;
				err_2 -= dx2;
			}
			err_1 += dy2;
			err_2 += dz2;
			pixel[0] += x_inc;
		}
	} 
	else if ((m >= l) && (m >= n)) {
		err_1 = dx2 - m;
		err_2 = dz2 - m;
		for (i = 0; i < m; i++) {
			if(cb(result, input, res, pixel, tRay, correct) <= FLT_EPSILON)
				break;
			if (err_1 > 0) {
				pixel[0] += x_inc;
				err_1 -= dy2;
			}
			if (err_2 > 0) {
				pixel[2] += z_inc;
				err_2 -= dy2;
			}
			err_1 += dx2;
			err_2 += dz2;
			pixel[1] += y_inc;
		}
	} 
	else {
		err_1 = dy2 - n;
		err_2 = dx2 - n;
		for (i = 0; i < n; i++) {
			if(cb(result, input, res, pixel, tRay, correct) <= FLT_EPSILON)
				break;
			if (err_1 > 0) {
				pixel[1] += y_inc;
				err_1 -= dz2;
			}
			if (err_2 > 0) {
				pixel[0] += x_inc;
				err_2 -= dz2;
			}
			err_1 += dy2;
			err_2 += dx2;
			pixel[2] += z_inc;
		}
	}
	cb(result, input, res, pixel, tRay, correct);
}

static void get_cell(float *p0, int res[3], float dx, float *pos, int *cell, int correct)
{
	float tmp[3];

	sub_v3_v3v3(tmp, pos, p0);
	mul_v3_fl(tmp, 1.0 / dx);

	if (correct) {
		cell[0] = MIN2(res[0] - 1, MAX2(0, (int)floor(tmp[0])));
		cell[1] = MIN2(res[1] - 1, MAX2(0, (int)floor(tmp[1])));
		cell[2] = MIN2(res[2] - 1, MAX2(0, (int)floor(tmp[2])));
	}
	else {
		cell[0] = (int)floor(tmp[0]);
		cell[1] = (int)floor(tmp[1]);
		cell[2] = (int)floor(tmp[2]);
	}
}

static void smoke_calc_transparency(float *result, float *input, float *p0, float *p1, int res[3], float dx, float *light, bresenham_callback cb, float correct)
{
	float bv[6];
	int a, z, slabsize=res[0]*res[1], size= res[0]*res[1]*res[2];

	for(a=0; a<size; a++)
		result[a]= -1.0f;

	bv[0] = p0[0];
	bv[1] = p1[0];
	// y
	bv[2] = p0[1];
	bv[3] = p1[1];
	// z
	bv[4] = p0[2];
	bv[5] = p1[2];

// #pragma omp parallel for schedule(static,1)
	for(z = 0; z < res[2]; z++)
	{
		size_t index = z*slabsize;
		int x,y;

		for(y = 0; y < res[1]; y++)
			for(x = 0; x < res[0]; x++, index++)
			{
				float voxelCenter[3];
				float pos[3];
				int cell[3];
				float tRay = 1.0;

				if(result[index] >= 0.0f)					
					continue;								
				voxelCenter[0] = p0[0] + dx *  x + dx * 0.5;
				voxelCenter[1] = p0[1] + dx *  y + dx * 0.5;
				voxelCenter[2] = p0[2] + dx *  z + dx * 0.5;

				// get starting position (in voxel coords)
				if(BLI_bvhtree_bb_raycast(bv, light, voxelCenter, pos) > FLT_EPSILON)
				{
					// we're ouside
					get_cell(p0, res, dx, pos, cell, 1);
				}
				else {
					// we're inside
					get_cell(p0, res, dx, light, cell, 1);
				}

				bresenham_linie_3D(cell[0], cell[1], cell[2], x, y, z, &tRay, cb, result, input, res, correct);

				// convention -> from a RGBA float array, use G value for tRay
// #pragma omp critical
				result[index] = tRay;			
			}
	}
}

#endif /* WITH_SMOKE */
