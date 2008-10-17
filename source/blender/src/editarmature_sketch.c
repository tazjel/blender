/**
 * $Id: $
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
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_armature_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_DerivedMesh.h"
#include "BKE_object.h"
#include "BKE_anim.h"

#include "BSE_view.h"

#include "BIF_gl.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_mywindow.h"
#include "BIF_editarmature.h"
#include "BIF_sketch.h"

#include "blendef.h"
#include "mydevice.h"

typedef enum SK_PType
{
	PT_CONTINUOUS,
	PT_EXACT,
} SK_PType;

typedef enum SK_PMode
{
	PT_EMBED,
	PT_SNAP,
	PT_PROJECT,
} SK_PMode;

typedef struct SK_Point
{
	float p[3];
	SK_PType type;
	SK_PMode mode;
} SK_Point;

typedef struct SK_Stroke
{
	struct SK_Stroke *next, *prev;

	SK_Point *points;
	int nb_points;
	int buf_size;
	int selected;
} SK_Stroke;

#define SK_Stroke_BUFFER_INIT_SIZE 20

typedef struct SK_DrawData
{
	short mval[2];
	short previous_mval[2];
	SK_PType type;
} SK_DrawData;

typedef struct SK_Intersection
{
	struct SK_Intersection *next, *prev;
	SK_Stroke *stroke;
	int			start;
	int			end;
	float		p[3];
} SK_Intersection;

typedef struct SK_Sketch
{
	ListBase	strokes;
	SK_Stroke	*active_stroke;
	SK_Stroke	*gesture;
	SK_Point	next_point;
} SK_Sketch;

SK_Sketch *GLOBAL_sketch = NULL;
SK_Point boneSnap;

#define SNAP_MIN_DISTANCE 12

/******************** PROTOTYPES ******************************/

void sk_freeStroke(SK_Stroke *stk);
void sk_freeSketch(SK_Sketch *sketch);

/******************** PEELING *********************************/

typedef struct SK_DepthPeel
{
	struct SK_DepthPeel *next, *prev;
	
	float depth;
	float p[3];
	float no[3];
	Object *ob;
	int flag;
} SK_DepthPeel;

int cmpPeel(void *arg1, void *arg2)
{
	SK_DepthPeel *p1 = arg1;
	SK_DepthPeel *p2 = arg2;
	int val = 0;
	
	if (p1->depth < p2->depth)
	{
		val = -1;
	}
	else if (p1->depth > p2->depth)
	{
		val = 1;
	}
	
	return val;
}

void addDepthPeel(ListBase *depth_peels, float depth, float p[3], float no[3], Object *ob)
{
	SK_DepthPeel *peel = MEM_callocN(sizeof(SK_DepthPeel), "DepthPeel");
	
	peel->depth = depth;
	peel->ob = ob;
	VECCOPY(peel->p, p);
	VECCOPY(peel->no, no);
	
	BLI_addtail(depth_peels, peel);
	
	peel->flag = 0;
}

int peelDerivedMesh(Object *ob, DerivedMesh *dm, float obmat[][4], float ray_start[3], float ray_normal[3], short mval[2], ListBase *depth_peels)
{
	int retval = 0;
	int totvert = dm->getNumVerts(dm);
	int totface = dm->getNumFaces(dm);
	
	if (totvert > 0) {
		float imat[4][4];
		float timat[3][3]; /* transpose inverse matrix for normals */
		float ray_start_local[3], ray_normal_local[3];
		int test = 1;

		Mat4Invert(imat, obmat);

		Mat3CpyMat4(timat, imat);
		Mat3Transp(timat);
		
		VECCOPY(ray_start_local, ray_start);
		VECCOPY(ray_normal_local, ray_normal);
		
		Mat4MulVecfl(imat, ray_start_local);
		Mat4Mul3Vecfl(imat, ray_normal_local);
		
		
		/* If number of vert is more than an arbitrary limit, 
		 * test against boundbox first
		 * */
		if (totface > 16) {
			struct BoundBox *bb = object_get_boundbox(ob);
			test = ray_hit_boundbox(bb, ray_start_local, ray_normal_local);
		}
		
		if (test == 1) {
			MVert *verts = dm->getVertArray(dm);
			MFace *faces = dm->getFaceArray(dm);
			int i;
			
			for( i = 0; i < totface; i++) {
				MFace *f = faces + i;
				float lambda;
				int result;
				
				
				result = RayIntersectsTriangle(ray_start_local, ray_normal_local, verts[f->v1].co, verts[f->v2].co, verts[f->v3].co, &lambda, NULL);
				
				if (result) {
					float location[3], normal[3];
					float intersect[3];
					float new_depth;
					
					VECCOPY(intersect, ray_normal_local);
					VecMulf(intersect, lambda);
					VecAddf(intersect, intersect, ray_start_local);
					
					VECCOPY(location, intersect);
					
					if (f->v4)
						CalcNormFloat4(verts[f->v1].co, verts[f->v2].co, verts[f->v3].co, verts[f->v4].co, normal);
					else
						CalcNormFloat(verts[f->v1].co, verts[f->v2].co, verts[f->v3].co, normal);

					Mat4MulVecfl(obmat, location);
					
					new_depth = VecLenf(location, ray_start);					
					
					Mat3MulVecfl(timat, normal);
					Normalize(normal);

					addDepthPeel(depth_peels, new_depth, location, normal, ob);
				}
		
				if (f->v4 && result == 0)
				{
					result = RayIntersectsTriangle(ray_start_local, ray_normal_local, verts[f->v3].co, verts[f->v4].co, verts[f->v1].co, &lambda, NULL);
					
					if (result) {
						float location[3], normal[3];
						float intersect[3];
						float new_depth;
						
						VECCOPY(intersect, ray_normal_local);
						VecMulf(intersect, lambda);
						VecAddf(intersect, intersect, ray_start_local);
						
						VECCOPY(location, intersect);
						
						if (f->v4)
							CalcNormFloat4(verts[f->v1].co, verts[f->v2].co, verts[f->v3].co, verts[f->v4].co, normal);
						else
							CalcNormFloat(verts[f->v1].co, verts[f->v2].co, verts[f->v3].co, normal);

						Mat4MulVecfl(obmat, location);
						
						new_depth = VecLenf(location, ray_start);					
						
						Mat3MulVecfl(timat, normal);
						Normalize(normal);
	
						addDepthPeel(depth_peels, new_depth, location, normal, ob);
					} 
				}
			}
		}
	}

	return retval;
} 

int peelObjects(ListBase *depth_peels, short mval[2])
{
	Base *base;
	int retval = 0;
	float ray_start[3], ray_normal[3];
	
	viewray(mval, ray_start, ray_normal);

	base= FIRSTBASE;
	for ( base = FIRSTBASE; base != NULL; base = base->next ) {
		if ( BASE_SELECTABLE(base) ) {
			Object *ob = base->object;
			
			if (ob->transflag & OB_DUPLI)
			{
				DupliObject *dupli_ob;
				ListBase *lb = object_duplilist(G.scene, ob);
				
				for(dupli_ob = lb->first; dupli_ob; dupli_ob = dupli_ob->next)
				{
					Object *ob = dupli_ob->ob;
					
					if (ob->type == OB_MESH) {
						DerivedMesh *dm = mesh_get_derived_final(ob, CD_MASK_BAREMESH);
						int val;
						
						val = peelDerivedMesh(ob, dm, dupli_ob->mat, ray_start, ray_normal, mval, depth_peels);
	
						retval = retval || val;
	
						dm->release(dm);
					}
				}
				
				free_object_duplilist(lb);
			}
			
			if (ob->type == OB_MESH) {
				DerivedMesh *dm = NULL;
				int val;

				if (ob != G.obedit)
				{
					dm = mesh_get_derived_final(ob, CD_MASK_BAREMESH);
					
					val = peelDerivedMesh(ob, dm, ob->obmat, ray_start, ray_normal, mval, depth_peels);
				}
				else
				{
					dm = editmesh_get_derived_cage(CD_MASK_BAREMESH);
					
					val = peelDerivedMesh(ob, dm, ob->obmat, ray_start, ray_normal, mval, depth_peels);
				}
					
				retval = retval || val;
				
				dm->release(dm);
			}
		}
	}
	
	BLI_sortlist(depth_peels, cmpPeel);
	
	return retval;
}

/**************************************************************/

void sk_freeSketch(SK_Sketch *sketch)
{
	SK_Stroke *stk, *next;
	
	for (stk = sketch->strokes.first; stk; stk = next)
	{
		next = stk->next;
		
		sk_freeStroke(stk);
	}
	
	MEM_freeN(sketch);
}

SK_Sketch* sk_createSketch()
{
	SK_Sketch *sketch;
	
	sketch = MEM_callocN(sizeof(SK_Sketch), "SK_Sketch");
	
	sketch->active_stroke = NULL;
	sketch->gesture = NULL;

	sketch->strokes.first = NULL;
	sketch->strokes.last = NULL;
	
	return sketch;
}

void sk_allocStrokeBuffer(SK_Stroke *stk)
{
	stk->points = MEM_callocN(sizeof(SK_Point) * stk->buf_size, "SK_Point buffer");
}

void sk_freeStroke(SK_Stroke *stk)
{
	MEM_freeN(stk->points);
	MEM_freeN(stk);
}

SK_Stroke* sk_createStroke()
{
	SK_Stroke *stk;
	
	stk = MEM_callocN(sizeof(SK_Stroke), "SK_Stroke");
	
	stk->selected = 0;
	stk->nb_points = 0;
	stk->buf_size = SK_Stroke_BUFFER_INIT_SIZE;
	
	sk_allocStrokeBuffer(stk);
	
	return stk;
}

void sk_shrinkStrokeBuffer(SK_Stroke *stk)
{
	if (stk->nb_points < stk->buf_size)
	{
		SK_Point *old_points = stk->points;
		
		stk->buf_size = stk->nb_points;

		sk_allocStrokeBuffer(stk);		
		
		memcpy(stk->points, old_points, sizeof(SK_Point) * stk->nb_points);
		
		MEM_freeN(old_points);
	}
}

void sk_growStrokeBuffer(SK_Stroke *stk)
{
	if (stk->nb_points == stk->buf_size)
	{
		SK_Point *old_points = stk->points;
		
		stk->buf_size *= 2;
		
		sk_allocStrokeBuffer(stk);
		
		memcpy(stk->points, old_points, sizeof(SK_Point) * stk->nb_points);
		
		MEM_freeN(old_points);
	}
}

void sk_insertStrokePoint(SK_Stroke *stk, SK_Point *pt, int n)
{
	int size = stk->nb_points - n;
	
	sk_growStrokeBuffer(stk);
	
	memmove(stk->points + n + 1, stk->points + n, size * sizeof(SK_Point));
	
	memcpy(stk->points + n, pt, sizeof(SK_Point));
	
	stk->nb_points++;
}

void sk_appendStrokePoint(SK_Stroke *stk, SK_Point *pt)
{
	sk_growStrokeBuffer(stk);
	
	memcpy(stk->points + stk->nb_points, pt, sizeof(SK_Point));
	
	stk->nb_points++;
}

/* Apply reverse Chaikin filter to simplify the polyline
 * */
void sk_filterStroke(SK_Stroke *stk, int start, int end)
{
	SK_Point *old_points = stk->points;
	int nb_points = stk->nb_points;
	int i, j;
	
	if (start == -1)
	{
		start = 0;
		end = stk->nb_points - 1;
	}

	sk_allocStrokeBuffer(stk);
	stk->nb_points = 0;
	
	/* adding points before range */
	for (i = 0; i < start; i++)
	{
		sk_appendStrokePoint(stk, old_points + i);
	}
	
	for (i = start, j = start; i <= end; i++)
	{
		if (i - j == 3)
		{
			SK_Point pt;
			float vec[3];
			
			pt.type = PT_CONTINUOUS;
			pt.p[0] = 0;
			pt.p[1] = 0;
			pt.p[2] = 0;
			
			VECCOPY(vec, old_points[j].p);
			VecMulf(vec, -0.25);
			VecAddf(pt.p, pt.p, vec);
			
			VECCOPY(vec, old_points[j+1].p);
			VecMulf(vec,  0.75);
			VecAddf(pt.p, pt.p, vec);

			VECCOPY(vec, old_points[j+2].p);
			VecMulf(vec,  0.75);
			VecAddf(pt.p, pt.p, vec);

			VECCOPY(vec, old_points[j+3].p);
			VecMulf(vec, -0.25);
			VecAddf(pt.p, pt.p, vec);
			
			sk_appendStrokePoint(stk, &pt);

			j += 2;
		}
		
		/* this might be uneeded when filtering last continuous stroke */
		if (old_points[i].type == PT_EXACT)
		{
			sk_appendStrokePoint(stk, old_points + i);
			j = i;
		}
	} 
	
	/* adding points after range */
	for (i = end + 1; i < nb_points; i++)
	{
		sk_appendStrokePoint(stk, old_points + i);
	}

	MEM_freeN(old_points);

	sk_shrinkStrokeBuffer(stk);
}

void sk_filterLastContinuousStroke(SK_Stroke *stk)
{
	int start, end;
	
	end = stk->nb_points -1;
	
	for (start = end - 1; start > 0 && stk->points[start].type == PT_CONTINUOUS; start--)
	{
		/* nothing to do here*/
	}
	
	if (end - start > 1)
	{
		sk_filterStroke(stk, start, end);
	}
}

SK_Point *sk_lastStrokePoint(SK_Stroke *stk)
{
	SK_Point *pt = NULL;
	
	if (stk->nb_points > 0)
	{
		pt = stk->points + (stk->nb_points - 1);
	}
	
	return pt;
}

void sk_drawStroke(SK_Stroke *stk, int id, float color[3])
{
	float rgb[3];
	int i;
	
	if (id != -1)
	{
		glLoadName(id);
		
		glBegin(GL_LINE_STRIP);
		
		for (i = 0; i < stk->nb_points; i++)
		{
			glVertex3fv(stk->points[i].p);
		}
		
		glEnd();
		
	}
	else
	{
		float d_rgb[3] = {1, 1, 1};
		
		VECCOPY(rgb, color);
		VecSubf(d_rgb, d_rgb, rgb);
		VecMulf(d_rgb, 1.0f / (float)stk->nb_points);
		
		glBegin(GL_LINE_STRIP);

		for (i = 0; i < stk->nb_points; i++)
		{
			glColor3fv(rgb);
			glVertex3fv(stk->points[i].p);
			VecAddf(rgb, rgb, d_rgb);
		}
		
		glEnd();
	
		glColor3f(0, 0, 0);
		glBegin(GL_POINTS);
	
		for (i = 0; i < stk->nb_points; i++)
		{
			if (stk->points[i].type == PT_EXACT)
			{
				glVertex3fv(stk->points[i].p);
			}
		}
		
		glEnd();
	}

//	glColor3f(1, 1, 1);
//	glBegin(GL_POINTS);
//
//	for (i = 0; i < stk->nb_points; i++)
//	{
//		if (stk->points[i].type == PT_CONTINUOUS)
//		{
//			glVertex3fv(stk->points[i].p);
//		}
//	}
//
//	glEnd();
}

SK_Point *sk_snapPointStroke(SK_Stroke *stk, short mval[2], int *dist)
{
	SK_Point *pt = NULL;
	int i;
	
	for (i = 0; i < stk->nb_points; i++)
	{
		if (stk->points[i].type == PT_EXACT)
		{
			short pval[2];
			int pdist;
			
			project_short_noclip(stk->points[i].p, pval);
			
			pdist = ABS(pval[0] - mval[0]) + ABS(pval[1] - mval[1]);
			
			if (pdist < *dist)
			{
				*dist = pdist;
				pt = stk->points + i;
			}
		}
	}
	
	return pt;
}

SK_Point *sk_snapPointArmature(ListBase *ebones, short mval[2], int *dist)
{
	SK_Point *pt = NULL;
	EditBone *bone;
	
	for (bone = ebones->first; bone; bone = bone->next)
	{
		short pval[2];
		int pdist;
		
		if ((bone->flag & BONE_CONNECTED) == 0)
		{
			project_short_noclip(bone->head, pval);
			
			pdist = ABS(pval[0] - mval[0]) + ABS(pval[1] - mval[1]);
			
			if (pdist < *dist)
			{
				*dist = pdist;
				pt = &boneSnap;
				VECCOPY(pt->p, bone->head);
				pt->type = PT_EXACT;
			}
		}
		
		
		project_short_noclip(bone->tail, pval);
		
		pdist = ABS(pval[0] - mval[0]) + ABS(pval[1] - mval[1]);
		
		if (pdist < *dist)
		{
			*dist = pdist;
			pt = &boneSnap;
			VECCOPY(pt->p, bone->tail);
			pt->type = PT_EXACT;
		}
	}
	
	return pt;
}

void sk_startStroke(SK_Sketch *sketch)
{
	SK_Stroke *stk = sk_createStroke();
	
	BLI_addtail(&sketch->strokes, stk);
	sketch->active_stroke = stk;
}

void sk_endStroke(SK_Sketch *sketch)
{
	sk_shrinkStrokeBuffer(sketch->active_stroke);
	sketch->active_stroke = NULL;
}

void sk_updateDrawData(SK_DrawData *dd)
{
	dd->type = PT_CONTINUOUS;
	
	dd->previous_mval[0] = dd->mval[0];
	dd->previous_mval[1] = dd->mval[1];
}

float sk_distanceDepth(float p1[3], float p2[3])
{
	float vec[3];
	float distance;
	
	VecSubf(vec, p1, p2);
	
	Projf(vec, vec, G.vd->viewinv[2]);
	
	distance = VecLength(vec);
	
	if (Inpf(G.vd->viewinv[2], vec) > 0)
	{
		distance *= -1;
	}
	
	return distance; 
}

void sk_interpolateDepth(SK_Stroke *stk, int start, int end, float length, float distance)
{
	float progress = 0;
	int i;
	
	progress = VecLenf(stk->points[start].p, stk->points[start - 1].p);
	
	for (i = start; i <= end; i++)
	{
		float ray_start[3], ray_normal[3];
		float delta = VecLenf(stk->points[i].p, stk->points[i + 1].p);
		short pval[2];
		
		project_short_noclip(stk->points[i].p, pval);
		viewray(pval, ray_start, ray_normal);
		
		VecMulf(ray_normal, distance * progress / length);
		VecAddf(stk->points[i].p, stk->points[i].p, ray_normal);

		progress += delta ;
	}
}

void sk_projectDrawPoint(float vec[3], SK_Stroke *stk, SK_DrawData *dd)
{
	/* copied from grease pencil, need fixing */	
	SK_Point *last = sk_lastStrokePoint(stk);
	short cval[2];
	//float *fp = give_cursor();
	float fp[3] = {0, 0, 0};
	float dvec[3];
	
	if (last != NULL)
	{
		VECCOPY(fp, last->p);
	}
	
	initgrabz(fp[0], fp[1], fp[2]);
	
	/* method taken from editview.c - mouse_cursor() */
	project_short_noclip(fp, cval);
	window_to_3d(dvec, cval[0] - dd->mval[0], cval[1] - dd->mval[1]);
	VecSubf(vec, fp, dvec);
}

int sk_getStrokeDrawPoint(SK_Point *pt, SK_Sketch *sketch, SK_Stroke *stk, SK_DrawData *dd)
{
	pt->type = dd->type;
	pt->mode = PT_PROJECT;
	sk_projectDrawPoint(pt->p, stk, dd);
	
	return 1;
}

int sk_addStrokeDrawPoint(SK_Sketch *sketch, SK_Stroke *stk, SK_DrawData *dd)
{
	SK_Point pt;
	
	sk_getStrokeDrawPoint(&pt, sketch, stk, dd);

	sk_appendStrokePoint(stk, &pt);
	
	return 1;
}


int sk_getStrokeSnapPoint(SK_Point *pt, SK_Sketch *sketch, SK_Stroke *source_stk, SK_DrawData *dd)
{
	SK_Stroke *stk;
	int dist = SNAP_MIN_DISTANCE;
	int point_added = 0;
	
	for (stk = sketch->strokes.first; stk; stk = stk->next)
	{
		SK_Point *spt = sk_snapPointStroke(stk, dd->mval, &dist);
		
		if (spt != NULL)
		{
			VECCOPY(pt->p, spt->p);
			point_added = 1;
		}
	}
	
	/* check on bones */
	{
		SK_Point *spt = sk_snapPointArmature(&G.edbo, dd->mval, &dist);
		
		if (spt != NULL)
		{
			VECCOPY(pt->p, spt->p);
			point_added = 1;
		}
	}
	
	if (point_added)
	{
		pt->type = PT_EXACT;
		pt->mode = PT_SNAP;
	}
	
	return point_added;
}

int sk_addStrokeSnapPoint(SK_Sketch *sketch, SK_Stroke *stk, SK_DrawData *dd)
{
	SK_Point pt;
	int point_added = 0;
	
	point_added = sk_getStrokeSnapPoint(&pt, sketch, stk, dd);

	if (point_added)
	{
		float final_p[3];
		float distance;
		float length;
		int i, total;
		
		VECCOPY(final_p, pt.p);

		sk_projectDrawPoint(pt.p, stk, dd);
		sk_appendStrokePoint(stk, &pt);
		
		/* update all previous point to give smooth Z progresion */
		total = 0;
		length = 0;
		for (i = stk->nb_points - 2; i > 0; i--)
		{
			length += VecLenf(stk->points[i].p, stk->points[i + 1].p);
			total++;
			if (stk->points[i].type == PT_EXACT)
			{
				break;
			}
		}
		
		if (total > 1)
		{
			distance = sk_distanceDepth(final_p, stk->points[i].p);
			
			sk_interpolateDepth(stk, i + 1, stk->nb_points - 2, length, distance);
		}
	
		VECCOPY(stk->points[stk->nb_points - 1].p, final_p);
	}
	
	return point_added;
}

int sk_getStrokeEmbedPoint(SK_Point *pt, SK_Sketch *sketch, SK_Stroke *stk, SK_DrawData *dd)
{
	ListBase depth_peels;
	SK_DepthPeel *p1, *p2;
	SK_Point *last_pt = NULL;
	float dist = FLT_MAX;
	float p[3];
	int point_added = 0;
	
	depth_peels.first = depth_peels.last = NULL;
	
	peelObjects(&depth_peels, dd->mval);
	
	if (stk->nb_points > 0 && stk->points[stk->nb_points - 1].type == PT_CONTINUOUS)
	{
		last_pt = stk->points + (stk->nb_points - 1);
	}
	
	
	for (p1 = depth_peels.first; p1; p1 = p1->next)
	{
		if (p1->flag == 0)
		{
			float vec[3];
			float new_dist;
			
			p1->flag = 0;
			
			for (p2 = p1->next; p2 && p2->ob != p1->ob; p2 = p2->next)
			{
				/* nothing to do here */
			}
			
			
			if (p2)
			{
				p2->flag = 1;
				
				VecAddf(vec, p1->p, p2->p);
				VecMulf(vec, 0.5f);
			}
			else
			{
				VECCOPY(vec, p1->p);
			}
			
			if (last_pt == NULL)
			{
				VECCOPY(p, vec);
				dist = 0;
				break;
			}
			
			new_dist = VecLenf(last_pt->p, vec);
			
			if (new_dist < dist)
			{
				VECCOPY(p, vec);
				dist = new_dist;
			}
		}
	}
	
	if (dist != FLT_MAX)
	{
		pt->type = dd->type;
		pt->mode = PT_EMBED;
		VECCOPY(pt->p, p);
		
		point_added = 1;
	}
	
	BLI_freelistN(&depth_peels);
	
	return point_added;
}

int sk_addStrokeEmbedPoint(SK_Sketch *sketch, SK_Stroke *stk, SK_DrawData *dd)
{
	SK_Point pt;
	int point_added;
	
	point_added = sk_getStrokeEmbedPoint(&pt, sketch, stk, dd);
	
	if (point_added)
	{
		float final_p[3];
		float length, distance;
		int total;
		int i;
		
		VECCOPY(final_p, pt.p);
		
		sk_projectDrawPoint(pt.p, stk, dd);
		sk_appendStrokePoint(stk, &pt);
		
		/* update all previous point to give smooth Z progresion */
		total = 0;
		length = 0;
		for (i = stk->nb_points - 2; i > 0; i--)
		{
			length += VecLenf(stk->points[i].p, stk->points[i + 1].p);
			total++;
			if (stk->points[i].mode == PT_EMBED || stk->points[i].type == PT_EXACT)
			{
				break;
			}
		}
		
		if (total > 1)
		{
			distance = sk_distanceDepth(final_p, stk->points[i].p);
			
			sk_interpolateDepth(stk, i + 1, stk->nb_points - 2, length, distance);
		}
		
		VECCOPY(stk->points[stk->nb_points - 1].p, final_p);
		
		point_added = 1;
	}
	
	return point_added;
}

void sk_addStrokePoint(SK_Sketch *sketch, SK_Stroke *stk, SK_DrawData *dd, short qual)
{
	int point_added = 0;
	
	if (qual & LR_CTRLKEY)
	{
		point_added = sk_addStrokeSnapPoint(sketch, stk, dd);
	}
	
	if (point_added == 0 && qual & LR_SHIFTKEY)
	{
		point_added = sk_addStrokeEmbedPoint(sketch, stk, dd);
	}
	
	if (point_added == 0)
	{
		point_added = sk_addStrokeDrawPoint(sketch, stk, dd);
	}	
}

void sk_getStrokePoint(SK_Point *pt, SK_Sketch *sketch, SK_Stroke *stk, SK_DrawData *dd, short qual)
{
	int point_added = 0;
	
	if (qual & LR_CTRLKEY)
	{
		point_added = sk_getStrokeSnapPoint(pt, sketch, stk, dd);
	}
	
	if (point_added == 0 && qual & LR_SHIFTKEY)
	{
		point_added = sk_getStrokeEmbedPoint(pt, sketch, stk, dd);
	}
	
	if (point_added == 0)
	{
		point_added = sk_getStrokeDrawPoint(pt, sketch, stk, dd);
	}	
}

void sk_endContinuousStroke(SK_Stroke *stk)
{
	stk->points[stk->nb_points - 1].type = PT_EXACT;
}

void sk_updateNextPoint(SK_Sketch *sketch)
{
	if (sketch->active_stroke)
	{
		SK_Stroke *stk = sketch->active_stroke;
		memcpy(&sketch->next_point, stk->points[stk->nb_points - 1].p, sizeof(SK_Point));
	}
}

int sk_stroke_filtermval(SK_DrawData *dd)
{
	int retval = 0;
	if (dd->mval[0] != dd->previous_mval[0] || dd->mval[1] != dd->previous_mval[1])
	{
		retval = 1;
	}
	
	return retval;
}

void sk_initDrawData(SK_DrawData *dd)
{
	getmouseco_areawin(dd->mval);
	dd->previous_mval[0] = -1;
	dd->previous_mval[1] = -1;
	dd->type = PT_EXACT;
}
/********************************************/

float calcStrokeCorrelation(SK_Stroke *stk, int start, int end, float v0[3], float n[3])
{
	int len = 2 + abs(end - start);
	
	if (len > 2)
	{
		float avg_t = 0.0f;
		float s_t = 0.0f;
		float s_xyz = 0.0f;
		int i;
		
		/* First pass, calculate average */
		for (i = start; i <= end; i++)
		{
			float v[3];
			
			VecSubf(v, stk->points[i].p, v0);
			avg_t += Inpf(v, n);
		}
		
		avg_t /= Inpf(n, n);
		avg_t += 1.0f; /* adding start (0) and end (1) values */
		avg_t /= len;
		
		/* Second pass, calculate s_xyz and s_t */
		for (i = start; i <= end; i++)
		{
			float v[3], d[3];
			float dt;
			
			VecSubf(v, stk->points[i].p, v0);
			Projf(d, v, n);
			VecSubf(v, v, d);
			
			dt = VecLength(d) - avg_t;
			
			s_t += dt * dt;
			s_xyz += Inpf(v, v);
		}
		
		/* adding start(0) and end(1) values to s_t */
		s_t += (avg_t * avg_t) + (1 - avg_t) * (1 - avg_t);
		
		return 1.0f - s_xyz / s_t; 
	}
	else
	{
		return 1.0f;
	}
}

EditBone * subdivideStrokeByCorrelation(SK_Stroke *stk, int start, int end, float invmat[][4])
{
	bArmature *arm= G.obedit->data;
	EditBone *lastBone = NULL;
	float n[3];
	float CORRELATION_THRESHOLD = G.scene->toolsettings->skgen_correlation_limit;
	
	if (G.scene->toolsettings->skgen_options & SKGEN_CUT_CORRELATION)
	{
		EditBone *child = NULL;
		EditBone *parent = NULL;
		int boneStart = start;
		int i;

		parent = addEditBone("Bone", &G.edbo, arm);
		VECCOPY(parent->head, stk->points[start].p);
		Mat4MulVecfl(invmat, parent->head);
		
		for (i = start + 1; i < end; i++)
		{
			/* Calculate normal */
			VecSubf(n, stk->points[i].p, parent->head);

			if (calcStrokeCorrelation(stk, boneStart, i, parent->head, n) < CORRELATION_THRESHOLD)
			{
				VECCOPY(parent->tail, stk->points[i - 1].p);
				Mat4MulVecfl(invmat, parent->tail);

				child = addEditBone("Bone", &G.edbo, arm);
				VECCOPY(child->head, parent->tail);
				child->parent = parent;
				child->flag |= BONE_CONNECTED;
				
				parent = child; // new child is next parent
				boneStart = i; // start next bone from current index
			}
		}

		VECCOPY(parent->tail, stk->points[end].p);
		Mat4MulVecfl(invmat, parent->tail);
		lastBone = parent;
	}
	
	return lastBone;
}

EditBone * subdivideStrokeByLength(SK_Stroke *stk, int start, int end, float invmat[][4])
{
	bArmature *arm= G.obedit->data;
	EditBone *lastBone = NULL;
	
	if (G.scene->toolsettings->skgen_options & SKGEN_CUT_LENGTH)
	{
		EditBone *child = NULL;
		EditBone *parent = NULL;
		float lengthLimit = G.scene->toolsettings->skgen_length_limit;
		int i;
		int same = 0;
		
		parent = addEditBone("Bone", &G.edbo, arm);
		VECCOPY(parent->head, stk->points[start].p);

		i = start + 1;
		while (i < end)
		{
			float *vec0 = stk->points[i - 1].p;
			float *vec1 = stk->points[i].p;

			/* If lengthLimit hits the current segment */
			if (VecLenf(vec1, parent->head) > lengthLimit)
			{
				if (same == 0)
				{
					float dv[3], off[3];
					float a, b, c, f;
					
					/* Solve quadratic distance equation */
					VecSubf(dv, vec1, vec0);
					a = Inpf(dv, dv);
					
					VecSubf(off, vec0, parent->head);
					b = 2 * Inpf(dv, off);
					
					c = Inpf(off, off) - (lengthLimit * lengthLimit);
					
					f = (-b + (float)sqrt(b * b - 4 * a * c)) / (2 * a);
					
					//printf("a %f, b %f, c %f, f %f\n", a, b, c, f);
					
					if (isnan(f) == 0 && f < 1.0f)
					{
						VECCOPY(parent->tail, dv);
						VecMulf(parent->tail, f);
						VecAddf(parent->tail, parent->tail, vec0);
					}
					else
					{
						VECCOPY(parent->tail, vec1);
					}
				}
				else
				{
					float dv[3];
					
					VecSubf(dv, vec1, vec0);
					Normalize(dv);
					 
					VECCOPY(parent->tail, dv);
					VecMulf(parent->tail, lengthLimit);
					VecAddf(parent->tail, parent->tail, parent->head);
				}
				
				/* put head in correct space */
				Mat4MulVecfl(invmat, parent->head);
				
				child = addEditBone("Bone", &G.edbo, arm);
				VECCOPY(child->head, parent->tail);
				child->parent = parent;
				child->flag |= BONE_CONNECTED;
				
				parent = child; // new child is next parent
				
				same = 1; // mark as same
			}
			else
			{
				i++;
				same = 0; // Reset same
			}
		}
		VECCOPY(parent->tail, stk->points[end].p);
		Mat4MulVecfl(invmat, parent->tail);
		lastBone = parent;
	}
	
	return lastBone;
}

void sk_convertStroke(SK_Stroke *stk)
{
	bArmature *arm= G.obedit->data;
	SK_Point *head;
	EditBone *parent = NULL;
	float invmat[4][4]; /* move in caller function */
	int head_index = 0;
	int i;
	
	head = NULL;
	
	Mat4Invert(invmat, G.obedit->obmat);
	
	for (i = 0; i < stk->nb_points; i++)
	{
		SK_Point *pt = stk->points + i;
		
		if (pt->type == PT_EXACT)
		{
			if (head == NULL)
			{
				head_index = i;
				head = pt;
			}
			else
			{
				EditBone *bone = NULL;
				EditBone *new_parent;
				
				if (i - head_index > 1)
				{
					bone = subdivideStrokeByCorrelation(stk, head_index, i, invmat);
					
					if (bone == NULL)
					{
						bone = subdivideStrokeByLength(stk, head_index, i, invmat);
					}
				}
				
				if (bone == NULL)
				{
					bone = addEditBone("Bone", &G.edbo, arm);
					
					VECCOPY(bone->head, head->p);
					VECCOPY(bone->tail, pt->p);
					
					Mat4MulVecfl(invmat, bone->head);
					Mat4MulVecfl(invmat, bone->tail);
				}
				
				new_parent = bone;
				bone->flag |= BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL;
				
				/* move to end of chain */
				while (bone->parent != NULL)
				{
					bone = bone->parent;
					bone->flag |= BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL;
				}

				if (parent != NULL)
				{
					bone->parent = parent;
					bone->flag |= BONE_CONNECTED;					
				}
				
				parent = new_parent;
				head_index = i;
				head = pt;
			}
		}
	}
}

void sk_convert(SK_Sketch *sketch)
{
	SK_Stroke *stk;
	
	for (stk = sketch->strokes.first; stk; stk = stk->next)
	{
		if (stk->selected == 1)
		{
			sk_convertStroke(stk);
		}
	}
}
/******************* GESTURE *************************/

/* returns the maximum number of intersections per stroke */
int sk_getIntersections(ListBase *list, SK_Sketch *sketch, SK_Stroke *gesture)
{
	SK_Stroke *stk;
	int added = 0;

	for (stk = sketch->strokes.first; stk; stk = stk->next)
	{
		int s_added = 0;
		int s_i;
		
		for (s_i = 0; s_i < stk->nb_points - 1; s_i++)
		{
			float s_p1[3] = {0, 0, 0};
			float s_p2[3] = {0, 0, 0};
			int g_i;
			
			project_float(stk->points[s_i].p, s_p1);
			project_float(stk->points[s_i + 1].p, s_p2);

			for (g_i = 0; g_i < gesture->nb_points - 1; g_i++)
			{
				float g_p1[3] = {0, 0, 0};
				float g_p2[3] = {0, 0, 0};
				float vi[3];
				float lambda;
				
				project_float(gesture->points[g_i].p, g_p1);
				project_float(gesture->points[g_i + 1].p, g_p2);
				
				if (LineIntersectLineStrict(s_p1, s_p2, g_p1, g_p2, vi, &lambda))
				{
					SK_Intersection *isect = MEM_callocN(sizeof(SK_Intersection), "Intersection");
					
					isect->start = s_i;
					isect->end = s_i + 1;
					isect->stroke = stk;

					VecSubf(isect->p, stk->points[s_i + 1].p, stk->points[s_i].p);
					VecMulf(isect->p, lambda);
					VecAddf(isect->p, isect->p, stk->points[s_i].p);
					
					BLI_addtail(list, isect);

					s_added++;
				}
			}
		}
		
		added = MAX2(s_added, added);
	}
	
	
	return added;
}

void sk_applyCutGesture(SK_Sketch *sketch, SK_Stroke *gesture, ListBase *list)
{
	SK_Intersection *isect;
	
	for (isect = list->first; isect; isect = isect->next)
	{
		SK_Point pt;
		
		pt.type = PT_EXACT;
		pt.mode = PT_PROJECT; /* take mode from neighbouring points */
		VECCOPY(pt.p, isect->p);
		
		sk_insertStrokePoint(isect->stroke, &pt, isect->end);
	}
}

void sk_applyGesture(SK_Sketch *sketch)
{
	ListBase list;
	int added;
	
	list.first = list.last = NULL;
	
	added = sk_getIntersections(&list, sketch, sketch->gesture);
	
	/* detect and apply */
	if (added == 1)
	{
		sk_applyCutGesture(sketch, sketch->gesture, &list);
	}
	
	BLI_freelistN(&list);
}

/********************************************/

void sk_deleteStrokes(SK_Sketch *sketch)
{
	SK_Stroke *stk, *next;
	
	for (stk = sketch->strokes.first; stk; stk = next)
	{
		next = stk->next;
		
		if (stk->selected == 1)
		{
			BLI_remlink(&sketch->strokes, stk);
			sk_freeStroke(stk);
		}
	}
}

void sk_selectAllSketch(SK_Sketch *sketch, int mode)
{
	SK_Stroke *stk = NULL;
	
	if (mode == -1)
	{
		for (stk = sketch->strokes.first; stk; stk = stk->next)
		{sk_applyGesture(sketch);
			stk->selected = 0;
		}
	}
	else if (mode == 0)
	{
		for (stk = sketch->strokes.first; stk; stk = stk->next)
		{
			stk->selected = 1;
		}
	}
	else if (mode == 1)
	{
		int selected = 1;
		
		for (stk = sketch->strokes.first; stk; stk = stk->next)
		{
			selected &= stk->selected;
		}
		
		selected ^= 1;

		for (stk = sketch->strokes.first; stk; stk = stk->next)
		{
			stk->selected = selected;
		}
	}
}

void sk_selectStroke(SK_Sketch *sketch)
{
	unsigned int buffer[MAXPICKBUF];
	short hits, mval[2];

	persp(PERSP_VIEW);

	getmouseco_areawin(mval);
	hits = view3d_opengl_select(buffer, MAXPICKBUF, mval[0]-5, mval[1]-5, mval[0]+5, mval[1]+5);
	if(hits==0)
		hits = view3d_opengl_select(buffer, MAXPICKBUF, mval[0]-12, mval[1]-12, mval[0]+12, mval[1]+12);
		
	if (hits>0)
	{
		int besthitresult = -1;
			
		if(hits == 1) {
			besthitresult = buffer[3];
		}
		else {
			besthitresult = buffer[3];
			/* loop and get best hit */
		}
		
		if (besthitresult != -1)
		{
			SK_Stroke *selected_stk = BLI_findlink(&sketch->strokes, besthitresult);
			
			if ((G.qual & LR_SHIFTKEY) == 0)
			{
				sk_selectAllSketch(sketch, -1);
				
				selected_stk->selected = 1;
			}
			else
			{
				selected_stk->selected ^= 1;
			}
			
			
		}
	}
}

void sk_queueRedrawSketch(SK_Sketch *sketch)
{
	if (sketch->active_stroke != NULL)
	{
		SK_Point *last = sk_lastStrokePoint(sketch->active_stroke);
		
		if (last != NULL)
		{
			allqueue(REDRAWVIEW3D, 0);
		}
	}
}

void sk_drawSketch(SK_Sketch *sketch, int with_names)
{
	SK_Stroke *stk;
	
	glDisable(GL_DEPTH_TEST);

	glLineWidth(BIF_GetThemeValuef(TH_VERTEX_SIZE));
	glPointSize(BIF_GetThemeValuef(TH_VERTEX_SIZE));
	
	if (with_names)
	{
		int id;
		for (id = 0, stk = sketch->strokes.first; stk; id++, stk = stk->next)
		{
			sk_drawStroke(stk, id, NULL);
		}
	}
	else
	{
		float selected_rgb[3] = {1, 0, 0};
		float unselected_rgb[3] = {1, 0.5, 0};
		
		for (stk = sketch->strokes.first; stk; stk = stk->next)
		{
			sk_drawStroke(stk, -1, (stk->selected==1?selected_rgb:unselected_rgb));
		}
	}
	
	/* only draw gesture in active area */
	if (sketch->gesture != NULL && area_is_active_area(G.vd->area))
	{
		float gesture_rgb[3] = {0, 0.5, 1};
		sk_drawStroke(sketch->gesture, -1, gesture_rgb);
	}
	
	if (sketch->active_stroke != NULL)
	{
		SK_Point *last = sk_lastStrokePoint(sketch->active_stroke);
		
		if (last != NULL)
		{
			/* update point if in active area */
			if (area_is_active_area(G.vd->area))
			{
				SK_DrawData dd;
				
				sk_initDrawData(&dd);
				sk_getStrokePoint(&sketch->next_point, sketch, sketch->active_stroke, &dd, G.qual);
			}
			
			glEnable(GL_LINE_STIPPLE);
			glColor3f(1, 0.5, 0);
			glBegin(GL_LINE_STRIP);
			
				glVertex3fv(last->p);
				glVertex3fv(sketch->next_point.p);
			
			glEnd();
			
			glDisable(GL_LINE_STIPPLE);

			switch (sketch->next_point.mode)
			{
				case PT_SNAP:
					glColor3f(0, 0.5, 1);
					break;
				case PT_EMBED:
					glColor3f(0, 1, 0);
					break;
				case PT_PROJECT:
					glColor3f(0, 0, 0);
					break;
			}
			
			glBegin(GL_POINTS);
			
				glVertex3fv(sketch->next_point.p);
			
			glEnd();
		}
	}
	
	glLineWidth(1.0);
	glPointSize(1.0);

	glEnable(GL_DEPTH_TEST);
}

int sk_paint(SK_Sketch *sketch, short mbut)
{
	int retval = 1;
	
	if (mbut == LEFTMOUSE)
	{
		SK_DrawData dd;
		if (sketch->active_stroke == NULL)
		{
			sk_startStroke(sketch);
		}

		sk_initDrawData(&dd);
		
		/* paint loop */
		do {
			/* get current user input */
			getmouseco_areawin(dd.mval);
			
			/* only add current point to buffer if mouse moved (otherwise wait until it does) */
			if (sk_stroke_filtermval(&dd)) {

				sk_addStrokePoint(sketch, sketch->active_stroke, &dd, G.qual);
				sk_updateDrawData(&dd);
				force_draw(0);
			}
			else
			{
				BIF_wait_for_statechange();
			}
			
			while( qtest() ) {
				short event, val;
				event = extern_qread(&val);
			}
			
			/* do mouse checking at the end, so don't check twice, and potentially
			 * miss a short tap 
			 */
		} while (get_mbut() & L_MOUSE);
		
		sk_endContinuousStroke(sketch->active_stroke);
		sk_filterLastContinuousStroke(sketch->active_stroke);
		sk_updateNextPoint(sketch);
	}
	else if (mbut == RIGHTMOUSE)
	{
		if (sketch->active_stroke != NULL)
		{
			sk_endStroke(sketch);
			allqueue(REDRAWVIEW3D, 0);
		}
		else
		{
			SK_DrawData dd;
			sketch->gesture = sk_createStroke();
	
			sk_initDrawData(&dd);
			
			/* paint loop */
			do {
				/* get current user input */
				getmouseco_areawin(dd.mval);
				
				/* only add current point to buffer if mouse moved (otherwise wait until it does) */
				if (sk_stroke_filtermval(&dd)) {

					sk_addStrokeDrawPoint(sketch, sketch->gesture, &dd);
					sk_updateDrawData(&dd);
					
					/* draw only if mouse has moved */
					if (sketch->gesture->nb_points > 1)
					{
						force_draw(0);
					}
				}
				else
				{
					BIF_wait_for_statechange();
				}
				
				while( qtest() ) {
					short event, val;
					event = extern_qread(&val);
				}
				
				/* do mouse checking at the end, so don't check twice, and potentially
				 * miss a short tap 
				 */
			} while (get_mbut() & R_MOUSE);
			
			sk_endContinuousStroke(sketch->gesture);
			sk_filterLastContinuousStroke(sketch->gesture);
			sk_filterLastContinuousStroke(sketch->gesture);
			sk_filterLastContinuousStroke(sketch->gesture);
	
			if (sketch->gesture->nb_points == 1)		
			{
				sk_selectStroke(sketch);
			}
			else
			{
				/* apply gesture here */
				sk_applyGesture(sketch);
				printf("FOO!\n");
			}
	
			sk_freeStroke(sketch->gesture);
			sketch->gesture = NULL;
			
			allqueue(REDRAWVIEW3D, 0);
		}
	}
	
	return retval;
}

void BDR_drawSketchNames()
{
	if (BIF_validSketchMode())
	{
		if (GLOBAL_sketch != NULL)
		{
			sk_drawSketch(GLOBAL_sketch, 1);
		}
	}
}

void BDR_drawSketch()
{
	if (BIF_validSketchMode())
	{
		if (GLOBAL_sketch != NULL)
		{
			sk_drawSketch(GLOBAL_sketch, 0);
		}
	}
}

void BIF_endStrokeSketch()
{
	if (BIF_validSketchMode())
	{
		if (GLOBAL_sketch != NULL)
		{
			sk_endStroke(GLOBAL_sketch);
			allqueue(REDRAWVIEW3D, 0);
		}
	}
}

void BIF_deleteSketch()
{
	if (BIF_validSketchMode())
	{
		if (GLOBAL_sketch != NULL)
		{
			sk_deleteStrokes(GLOBAL_sketch);
		}
	}
}

void BIF_convertSketch()
{
	if (BIF_validSketchMode())
	{
		if (GLOBAL_sketch != NULL)
		{
			sk_convert(GLOBAL_sketch);
		}
	}
}

int BIF_paintSketch(short mbut)
{
	if (BIF_validSketchMode())
	{
		if (GLOBAL_sketch == NULL)
		{
			GLOBAL_sketch = sk_createSketch();
		}
		
		return sk_paint(GLOBAL_sketch, mbut);
	}
	else
	{
		return 0;
	}
}
	

void BDR_queueDrawSketch()
{
	if (BIF_validSketchMode())
	{
		if (GLOBAL_sketch != NULL)
		{
			sk_queueRedrawSketch(GLOBAL_sketch);
		}
	}
}

void BIF_selectAllSketch(int mode)
{
	if (BIF_validSketchMode())
	{
		if (GLOBAL_sketch != NULL)
		{
			sk_selectAllSketch(GLOBAL_sketch, mode);
			allqueue(REDRAWVIEW3D, 0);
		}
	}
}

int BIF_validSketchMode()
{
	if (G.obedit && 
		G.obedit->type == OB_ARMATURE && 
		G.scene->toolsettings->bone_sketching & BONE_SKETCHING)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}
