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
 *
 * Contributor(s): Blender Foundation, 2002-2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"
#include "BKE_tessmesh.h"

#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_array.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_image.h"
#include "ED_mesh.h"

#include "UI_resources.h"

#include "uvedit_intern.h"

static void drawcursor_sima(SpaceImage *sima, ARegion *ar)
{
	View2D *v2d= &ar->v2d;
	float zoomx, zoomy, w, h;
	int width, height;

	ED_space_image_size(sima, &width, &height);
	ED_space_image_zoom(sima, ar, &zoomx, &zoomy);

	w= zoomx*width/256.0f;
	h= zoomy*height/256.0f;
	
	cpack(0xFFFFFF);
	glTranslatef(v2d->cursor[0], v2d->cursor[1], 0.0f);  
	fdrawline(-0.05/w, 0, 0, 0.05/h);
	fdrawline(0, 0.05/h, 0.05/w, 0);
	fdrawline(0.05/w, 0, 0, -0.05/h);
	fdrawline(0, -0.05/h, -0.05/w, 0);
	
	setlinestyle(4);
	cpack(0xFF);
	fdrawline(-0.05/w, 0, 0, 0.05/h);
	fdrawline(0, 0.05/h, 0.05/w, 0);
	fdrawline(0.05/w, 0, 0, -0.05/h);
	fdrawline(0, -0.05/h, -0.05/w, 0);
	
	
	setlinestyle(0);
	cpack(0x0);
	fdrawline(-0.020/w, 0, -0.1/w, 0);
	fdrawline(0.1/w, 0, .020/w, 0);
	fdrawline(0, -0.020/h, 0, -0.1/h);
	fdrawline(0, 0.1/h, 0, 0.020/h);
	
	setlinestyle(1);
	cpack(0xFFFFFF);
	fdrawline(-0.020/w, 0, -0.1/w, 0);
	fdrawline(0.1/w, 0, .020/w, 0);
	fdrawline(0, -0.020/h, 0, -0.1/h);
	fdrawline(0, 0.1/h, 0, 0.020/h);
	
	glTranslatef(-v2d->cursor[0], -v2d->cursor[1], 0.0f);
	setlinestyle(0);
}

static int draw_uvs_face_check(Scene *scene)
{
	ToolSettings *ts= scene->toolsettings;

	/* checks if we are selecting only faces */
	if(ts->uv_flag & UV_SYNC_SELECTION) {
		if(ts->selectmode == SCE_SELECT_FACE)
			return 2;
		else if(ts->selectmode & SCE_SELECT_FACE)
			return 1;
		else
			return 0;
	}
	else
		return (ts->uv_selectmode == UV_SELECT_FACE);
}

static void draw_uvs_shadow(SpaceImage *sima, Object *obedit)
{
	BMEditMesh *em;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MLoopUV *luv;
	
	em= ((Mesh*)obedit->data)->edit_btmesh;

	/* draws the grey mesh when painting */
	glColor3ub(112, 112, 112);

	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		glBegin(GL_LINE_LOOP);
		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			luv= CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

			glVertex2fv(luv->uv);
		}
		glEnd();
	}
}

static int draw_uvs_dm_shadow(DerivedMesh *dm)
{
	/* draw shadow mesh - this is the mesh with the modifier applied */

	if(dm && dm->drawUVEdges && CustomData_has_layer(&dm->loopData, CD_MLOOPUV)) {
		glColor3ub(112, 112, 112);
		dm->drawUVEdges(dm);
		return 1;
	}

	return 0;
}

static void draw_uvs_stretch(SpaceImage *sima, Scene *scene, BMEditMesh *em, MTexPoly *activetf)
{
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	Image *ima= sima->image;
	BLI_array_declare(tf_uv);
	BLI_array_declare(tf_uvorig);
	float aspx, aspy, col[4], (*tf_uv)[2] = NULL, (*tf_uvorig)[2] = NULL;
	int i;

	ED_space_image_uv_aspect(sima, &aspx, &aspy);
	
	switch(sima->dt_uvstretch) {
		case SI_UVDT_STRETCH_AREA:
		{
			float totarea=0.0f, totuvarea=0.0f, areadiff, uvarea, area;
			
			BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
				tf= CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
				
				BLI_array_empty(tf_uv);
				BLI_array_empty(tf_uvorig);
				
				i = 0;
				BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
					luv= CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
					BLI_array_growone(tf_uv);
					BLI_array_growone(tf_uvorig);

					tf_uvorig[i][0] = luv->uv[0];
					tf_uvorig[i][1] = luv->uv[1];

					i++;
				}

				poly_copy_aspect(tf_uvorig, tf_uv, aspx, aspy, efa->len);

				totarea += BM_face_area(efa);
				//totuvarea += tf_area(tf, efa->v4!=0);
				totuvarea += poly_uv_area(tf_uv, efa->len);
				
				if(uvedit_face_visible(scene, ima, efa, tf)) {
					BMINDEX_SET(efa, 1);
				}
				else {
					if(tf == activetf)
						activetf= NULL;
					BMINDEX_SET(efa, 0);
				}
			}
			
			if(totarea < FLT_EPSILON || totuvarea < FLT_EPSILON) {
				col[0] = 1.0;
				col[1] = col[2] = 0.0;
				glColor3fv(col);
				BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
					if(BMINDEX_GET(efa)) {
						glBegin(GL_POLYGON);
						BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
							luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
							glVertex2fv(luv->uv);
						}
						glEnd();
					}
				}
			}
			else {
				BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
					if(BMINDEX_GET(efa)) {
						area = BM_face_area(efa) / totarea;

						BLI_array_empty(tf_uv);
						BLI_array_empty(tf_uvorig);

						i = 0;
						BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
							luv= CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
							BLI_array_growone(tf_uv);
							BLI_array_growone(tf_uvorig);

							tf_uvorig[i][0] = luv->uv[0];
							tf_uvorig[i][1] = luv->uv[1];

							i++;
						}

						poly_copy_aspect(tf_uvorig, tf_uv, aspx, aspy, efa->len);

						//uvarea = tf_area(tf, efa->v4!=0) / totuvarea;
						uvarea = poly_uv_area(tf_uv, efa->len) / totuvarea;
						
						if(area < FLT_EPSILON || uvarea < FLT_EPSILON)
							areadiff = 1.0;
						else if(area>uvarea)
							areadiff = 1.0-(uvarea/area);
						else
							areadiff = 1.0-(area/uvarea);
						
						weight_to_rgb(areadiff, col, col+1, col+2);
						glColor3fv(col);
						
						glBegin(GL_POLYGON);
						BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
							luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
							glVertex2fv(luv->uv);
						}
						glEnd();
					}
				}
			}
			break;
		}
		case SI_UVDT_STRETCH_ANGLE:
		{
#if 0 //BMESH_TODO
			float uvang1,uvang2,uvang3,uvang4;
			float ang1,ang2,ang3,ang4;
			float av1[3], av2[3], av3[3], av4[3]; /* use for 2d and 3d  angle vectors */
			float a;
			
			col[3] = 0.5; /* hard coded alpha, not that nice */
			
			glShadeModel(GL_SMOOTH);
			
			for(efa= em->faces.first; efa; efa= efa->next) {
				tf= CustomData_em_get(&em->fdata, efa->head.data, CD_MTFACE);
				
				if(uvedit_face_visible(scene, ima, efa, tf)) {
					efa->tmp.p = tf;
					uv_copy_aspect(tf->uv, tf_uv, aspx, aspy);
					if(efa->v4) {
						
#if 0						/* Simple but slow, better reuse normalized vectors */
						uvang1 = RAD2DEG(Vec2Angle3(tf_uv[3], tf_uv[0], tf_uv[1]));
						ang1 = RAD2DEG(VecAngle3(efa->v4->co, efa->v1->co, efa->v2->co));
						
						uvang2 = RAD2DEG(Vec2Angle3(tf_uv[0], tf_uv[1], tf_uv[2]));
						ang2 = RAD2DEG(VecAngle3(efa->v1->co, efa->v2->co, efa->v3->co));
						
						uvang3 = RAD2DEG(Vec2Angle3(tf_uv[1], tf_uv[2], tf_uv[3]));
						ang3 = RAD2DEG(VecAngle3(efa->v2->co, efa->v3->co, efa->v4->co));
						
						uvang4 = RAD2DEG(Vec2Angle3(tf_uv[2], tf_uv[3], tf_uv[0]));
						ang4 = RAD2DEG(VecAngle3(efa->v3->co, efa->v4->co, efa->v1->co));
#endif
						
						/* uv angles */
						VECSUB2D(av1, tf_uv[3], tf_uv[0]); Normalize2(av1);
						VECSUB2D(av2, tf_uv[0], tf_uv[1]); Normalize2(av2);
						VECSUB2D(av3, tf_uv[1], tf_uv[2]); Normalize2(av3);
						VECSUB2D(av4, tf_uv[2], tf_uv[3]); Normalize2(av4);
						
						/* This is the correct angle however we are only comparing angles
						 * uvang1 = 90-((NormalizedVecAngle2_2D(av1, av2) * 180.0/M_PI)-90);*/
						uvang1 = NormalizedVecAngle2_2D(av1, av2)*180.0/M_PI;
						uvang2 = NormalizedVecAngle2_2D(av2, av3)*180.0/M_PI;
						uvang3 = NormalizedVecAngle2_2D(av3, av4)*180.0/M_PI;
						uvang4 = NormalizedVecAngle2_2D(av4, av1)*180.0/M_PI;
						
						/* 3d angles */
						VECSUB(av1, efa->v4->co, efa->v1->co); Normalize(av1);
						VECSUB(av2, efa->v1->co, efa->v2->co); Normalize(av2);
						VECSUB(av3, efa->v2->co, efa->v3->co); Normalize(av3);
						VECSUB(av4, efa->v3->co, efa->v4->co); Normalize(av4);
						
						/* This is the correct angle however we are only comparing angles
						 * ang1 = 90-((NormalizedVecAngle2(av1, av2) * 180.0/M_PI)-90);*/
						ang1 = NormalizedVecAngle2(av1, av2)*180.0/M_PI;
						ang2 = NormalizedVecAngle2(av2, av3)*180.0/M_PI;
						ang3 = NormalizedVecAngle2(av3, av4)*180.0/M_PI;
						ang4 = NormalizedVecAngle2(av4, av1)*180.0/M_PI;
						
						glBegin(GL_QUADS);
						
						/* This simple makes the angles display worse then they really are ;)
						 * 1.0-pow((1.0-a), 2) */
						
						a = fabs(uvang1-ang1)/180.0;
						weight_to_rgb(1.0-pow((1.0-a), 2), col, col+1, col+2);
						glColor3fv(col);
						glVertex2fv(tf->uv[0]);
						a = fabs(uvang2-ang2)/180.0;
						weight_to_rgb(1.0-pow((1.0-a), 2), col, col+1, col+2);
						glColor3fv(col);
						glVertex2fv(tf->uv[1]);
						a = fabs(uvang3-ang3)/180.0;
						weight_to_rgb(1.0-pow((1.0-a), 2), col, col+1, col+2);
						glColor3fv(col);
						glVertex2fv(tf->uv[2]);
						a = fabs(uvang4-ang4)/180.0;
						weight_to_rgb(1.0-pow((1.0-a), 2), col, col+1, col+2);
						glColor3fv(col);
						glVertex2fv(tf->uv[3]);
						
					}
					else {
#if 0						/* Simple but slow, better reuse normalized vectors */
						uvang1 = RAD2DEG(Vec2Angle3(tf_uv[2], tf_uv[0], tf_uv[1]));
						ang1 = RAD2DEG(VecAngle3(efa->v3->co, efa->v1->co, efa->v2->co));
						
						uvang2 = RAD2DEG(Vec2Angle3(tf_uv[0], tf_uv[1], tf_uv[2]));
						ang2 = RAD2DEG(VecAngle3(efa->v1->co, efa->v2->co, efa->v3->co));
						
						uvang3 = M_PI-(uvang1+uvang2);
						ang3 = M_PI-(ang1+ang2);
#endif						
						
						/* uv angles */
						VECSUB2D(av1, tf_uv[2], tf_uv[0]); Normalize2(av1);
						VECSUB2D(av2, tf_uv[0], tf_uv[1]); Normalize2(av2);
						VECSUB2D(av3, tf_uv[1], tf_uv[2]); Normalize2(av3);
						
						/* This is the correct angle however we are only comparing angles
						 * uvang1 = 90-((NormalizedVecAngle2_2D(av1, av2) * 180.0/M_PI)-90); */
						uvang1 = NormalizedVecAngle2_2D(av1, av2)*180.0/M_PI;
						uvang2 = NormalizedVecAngle2_2D(av2, av3)*180.0/M_PI;
						uvang3 = NormalizedVecAngle2_2D(av3, av1)*180.0/M_PI;
						
						/* 3d angles */
						VECSUB(av1, efa->v3->co, efa->v1->co); Normalize(av1);
						VECSUB(av2, efa->v1->co, efa->v2->co); Normalize(av2);
						VECSUB(av3, efa->v2->co, efa->v3->co); Normalize(av3);
						/* This is the correct angle however we are only comparing angles
						 * ang1 = 90-((NormalizedVecAngle2(av1, av2) * 180.0/M_PI)-90); */
						ang1 = NormalizedVecAngle2(av1, av2)*180.0/M_PI;
						ang2 = NormalizedVecAngle2(av2, av3)*180.0/M_PI;
						ang3 = NormalizedVecAngle2(av3, av1)*180.0/M_PI;
						
						/* This simple makes the angles display worse then they really are ;)
						 * 1.0-pow((1.0-a), 2) */
						
						glBegin(GL_TRIANGLES);
						a = fabs(uvang1-ang1)/180.0;
						weight_to_rgb(1.0-pow((1.0-a), 2), col, col+1, col+2);
						glColor3fv(col);
						glVertex2fv(tf->uv[0]);
						a = fabs(uvang2-ang2)/180.0;
						weight_to_rgb(1.0-pow((1.0-a), 2), col, col+1, col+2);
						glColor3fv(col);
						glVertex2fv(tf->uv[1]);
						a = fabs(uvang3-ang3)/180.0;
						weight_to_rgb(1.0-pow((1.0-a), 2), col, col+1, col+2);
						glColor3fv(col);
						glVertex2fv(tf->uv[2]);
					}
					glEnd();
				}
				else {
					if(tf == activetf)
						activetf= NULL;
					efa->tmp.p = NULL;
				}
			}

			glShadeModel(GL_FLAT);
			break;

#endif
		}
	}
}

static void draw_uvs_other(SpaceImage *sima, Scene *scene, Object *obedit, MTexPoly *activetf)
{
	Base *base;
	Image *curimage;

	curimage= (activetf)? activetf->tpage: NULL;

	glColor3ub(96, 96, 96);

	for(base=scene->base.first; base; base=base->next) {
		Object *ob= base->object;

		if(!(base->flag & SELECT)) continue;
		if(!(base->lay & scene->lay)) continue;
		if(ob->restrictflag & OB_RESTRICT_VIEW) continue;

		if((ob->type==OB_MESH) && (ob!=obedit)) {
			Mesh *me= ob->data;

			if(me->mtface) {
				MPoly *mface= me->mpoly;
				MTexPoly *tface= me->mtpoly;
				MLoopUV *mloopuv;
				int a, b;

				for(a=me->totpoly; a>0; a--, tface++, mface++) {
					if(tface->tpage == curimage) {
						glBegin(GL_LINE_LOOP);

						mloopuv = me->mloopuv + mface->loopstart;
						for (b=0; b<mface->totloop; b++, mloopuv++) {
							glVertex2fv(mloopuv->uv);
						}
						glEnd();
					}
				}
			}
		}
	}
}

/* draws uv's in the image space */
static void draw_uvs(SpaceImage *sima, Scene *scene, Object *obedit)
{
	ToolSettings *ts;
	Mesh *me= obedit->data;
	BMEditMesh *em;
	BMFace *efa, *efa_act, *activef;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf, *activetf = NULL;
	MLoopUV *luv;
	DerivedMesh *finaldm, *cagedm;
	char col1[4], col2[4];
	float pointsize;
	int drawfaces, interpedges, lastsel, sel;
	int i;
	Image *ima= sima->image;
 	
	em= me->edit_btmesh;
	activetf= EDBM_get_active_mtexpoly(em, &efa_act, 0); /* will be set to NULL if hidden */
	activef = EDBM_get_actFace(em, 0);
	ts= scene->toolsettings;

	drawfaces= draw_uvs_face_check(scene);
	if(ts->uv_flag & UV_SYNC_SELECTION)
		interpedges= (ts->selectmode & SCE_SELECT_VERTEX);
	else
		interpedges= (ts->uv_selectmode == UV_SELECT_VERTEX);
	
	/* draw other uvs */
	if(sima->flag & SI_DRAW_OTHER)
		draw_uvs_other(sima, scene, obedit, activetf);

	/* 1. draw shadow mesh */
	
	if(sima->flag & SI_DRAWSHADOW) {
		/* first try existing derivedmesh */
		if(!draw_uvs_dm_shadow(em->derivedFinal)) {
			/* create one if it does not exist */
			cagedm = editbmesh_get_derived_cage_and_final(scene, obedit, me->edit_btmesh, &finaldm, CD_MASK_BAREMESH|CD_MASK_MTFACE);

			/* when sync selection is enabled, all faces are drawn (except for hidden)
			 * so if cage is the same as the final, theres no point in drawing this */
			if(!((ts->uv_flag & UV_SYNC_SELECTION) && (cagedm == finaldm)))
				draw_uvs_dm_shadow(finaldm);
			
			/* release derivedmesh again */
			if(cagedm != finaldm) cagedm->release(cagedm);
			finaldm->release(finaldm);
		}
	}
	
	/* 2. draw colored faces */
	
	if(sima->flag & SI_DRAW_STRETCH) {
		draw_uvs_stretch(sima, scene, em, activetf);
	}
	else if(me->drawflag & ME_DRAWFACES) {
		/* draw transparent faces */
		UI_GetThemeColor4ubv(TH_FACE, col1);
		UI_GetThemeColor4ubv(TH_FACE_SELECT, col2);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			tf= CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
			
			if(uvedit_face_visible(scene, ima, efa, tf)) {
				BMINDEX_SET(efa, 1);
				if(tf==activetf) continue; /* important the temp boolean is set above */

				if(uvedit_face_selected(scene, em, efa))
					glColor4ubv((GLubyte *)col2);
				else
					glColor4ubv((GLubyte *)col1);
				
				glBegin(GL_POLYGON);
				BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
					luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
					glVertex2fv(luv->uv);
				}
				glEnd();
			}
			else {
				if(tf == activetf)
					activetf= NULL;
				BMINDEX_SET(efa, 0);
			}
		}
		glDisable(GL_BLEND);
	}
	else {
		/* would be nice to do this within a draw loop but most below are optional, so it would involve too many checks */
		
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			tf= CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);

			if(uvedit_face_visible(scene, ima, efa, tf)) {		
				BMINDEX_SET(efa, 1);
			}
			else {
				if(tf == activetf)
					activetf= NULL;
				BMINDEX_SET(efa, 0);
			}
		}
		
	}
	
	/* 3. draw active face stippled */

	if(activef) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		UI_ThemeColor4(TH_EDITMESH_ACTIVE);

		glEnable(GL_POLYGON_STIPPLE);
		glPolygonStipple(stipple_quarttone);

		glBegin(GL_POLYGON);
		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, activef) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			glVertex2fv(luv->uv);
		}
		glEnd();

		glDisable(GL_POLYGON_STIPPLE);
		glDisable(GL_BLEND);
	}
	
	/* 4. draw edges */

	if(sima->flag & SI_SMOOTH_UV) {
		glEnable(GL_LINE_SMOOTH);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	
	switch(sima->dt_uv) {
		case SI_UVDT_DASH:
			BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
				if (!BMINDEX_GET(efa))
					continue;
				tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);

				if(tf) {
					cpack(0x111111);

					glBegin(GL_LINE_LOOP);
					BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
						luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
						glVertex2fv(luv->uv);
					}
					glEnd();

					setlinestyle(2);
					cpack(0x909090);

					glBegin(GL_LINE_LOOP);
					BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
						luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
						glVertex2fv(luv->uv);
					}
					glEnd();

					/*glBegin(GL_LINE_STRIP);
						luv = CustomData_bmesh_get(&em->bm->ldata, efa->loopbase->head.data, CD_MLOOPUV);
						glVertex2fv(luv->uv);
						luv = CustomData_bmesh_get(&em->bm->ldata, efa->loopbase->head.next->data, CD_MLOOPUV);
						glVertex2fv(luv->uv);
					glEnd();*/

					setlinestyle(0);
				}
			}
			break;
		case SI_UVDT_BLACK: /* black/white */
		case SI_UVDT_WHITE: 
			if(sima->dt_uv==SI_UVDT_WHITE) glColor3f(1.0f, 1.0f, 1.0f);
			else glColor3f(0.0f, 0.0f, 0.0f);

			BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
				if (!BMINDEX_GET(efa))
					continue;

				glBegin(GL_LINE_LOOP);
				BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
					luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
					glVertex2fv(luv->uv);
				}
				glEnd();
			}
			break;
		case SI_UVDT_OUTLINE:
			glLineWidth(3);
			cpack(0x0);
			
			BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
				if (!BMINDEX_GET(efa))
					continue;

				glBegin(GL_LINE_LOOP);
				BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
					luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
					glVertex2fv(luv->uv);
				}
				glEnd();
			}
			
			glLineWidth(1);
			col2[0] = col2[1] = col2[2] = 192; col2[3] = 255;
			glColor4ubv((unsigned char *)col2); 
			
			if(me->drawflag & ME_DRAWEDGES) {
				UI_GetThemeColor4ubv(TH_VERTEX_SELECT, col1);
				lastsel = sel = 0;

				if(interpedges) {
					glShadeModel(GL_SMOOTH);

					BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
						if (!BMINDEX_GET(efa))
							continue;

						glBegin(GL_LINE_LOOP);
						i = 0;
						BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
							sel = (uvedit_uv_selected(em, scene, l)? 1 : 0);
							if(sel != lastsel) { glColor4ubv(sel ? (GLubyte *)col1 : (GLubyte *)col2); lastsel = sel; }

							luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
							glVertex2fv(luv->uv);
							luv = CustomData_bmesh_get(&em->bm->ldata, l->head.next->data, CD_MLOOPUV);
							glVertex2fv(luv->uv);
							i += 1;
						}
						glEnd();
					}

					glShadeModel(GL_FLAT);
				}
				else {
					BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
						if (!BMINDEX_GET(efa))
							continue;

						glBegin(GL_LINE_LOOP);
						i = 0;
						BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
							sel = (uvedit_edge_selected(em, scene, l)? 1 : 0);
							if(sel != lastsel) { glColor4ubv(sel ? (GLubyte *)col1 : (GLubyte *)col2); lastsel = sel; }

							luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
							glVertex2fv(luv->uv);
							luv = CustomData_bmesh_get(&em->bm->ldata, l->head.next->data, CD_MLOOPUV);
							glVertex2fv(luv->uv);
							i += 1;
						}
						glEnd();
					}
				}
			}
			else {
				/* no nice edges */
				BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
					if (!BMINDEX_GET(efa))
						continue;
				
					glBegin(GL_LINE_LOOP);
					BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
						luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
						glVertex2fv(luv->uv);
					}
					glEnd();
				}
			}
			
			break;
	}

	if(sima->flag & SI_SMOOTH_UV) {
		glDisable(GL_LINE_SMOOTH);
		glDisable(GL_BLEND);
	}

	/* 5. draw face centers */

	if(drawfaces) {
		float cent[2];
		
		pointsize = UI_GetThemeValuef(TH_FACEDOT_SIZE);
		glPointSize(pointsize); // TODO - drawobject.c changes this value after - Investigate!
		
	    /* unselected faces */
		UI_ThemeColor(TH_WIRE);

		bglBegin(GL_POINTS);
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			if (!BMINDEX_GET(efa))
				continue;

			if(!uvedit_face_selected(scene, em, efa)) {
				poly_uv_center(em, efa, cent);
				bglVertex2fv(cent);
			}
		}
		bglEnd();

		/* selected faces */
		UI_ThemeColor(TH_FACE_DOT);

		bglBegin(GL_POINTS);
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			if (!BMINDEX_GET(efa))
				continue;

			if(uvedit_face_selected(scene, em, efa)) {
				poly_uv_center(em, efa, cent);
				bglVertex2fv(cent);
			}
		}
		bglEnd();
	}

	/* 6. draw uv vertices */
	
	if(drawfaces != 2) { /* 2 means Mesh Face Mode */
	    /* unselected uvs */
		UI_ThemeColor(TH_VERTEX);
		pointsize = UI_GetThemeValuef(TH_VERTEX_SIZE);
		glPointSize(pointsize);
	
		bglBegin(GL_POINTS);
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			if (!BMINDEX_GET(efa))
				continue;

			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
				if(!uvedit_uv_selected(em, scene, l))
					bglVertex2fv(luv->uv);
			}
		}
		bglEnd();
	
		/* pinned uvs */
		/* give odd pointsizes odd pin pointsizes */
	    glPointSize(pointsize*2 + (((int)pointsize % 2)? (-1): 0));
		cpack(0xFF);
	
		bglBegin(GL_POINTS);
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			if (!BMINDEX_GET(efa))
				continue;

			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

				if(luv->flag & MLOOPUV_PINNED)
					bglVertex2fv(luv->uv);
			}
		}
		bglEnd();
	
		/* selected uvs */
		UI_ThemeColor(TH_VERTEX_SELECT);
		glPointSize(pointsize);
	
		bglBegin(GL_POINTS);
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			if (!BMINDEX_GET(efa))
				continue;

			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

				if(uvedit_uv_selected(em, scene, l))
					bglVertex2fv(luv->uv);
			}
		}
		bglEnd();	
	}

	glPointSize(1.0);
}

void draw_uvedit_main(SpaceImage *sima, ARegion *ar, Scene *scene, Object *obedit)
{
	int show_uvedit, show_uvshadow;

	show_uvedit= ED_space_image_show_uvedit(sima, obedit);
	show_uvshadow= ED_space_image_show_uvshadow(sima, obedit);

	if(show_uvedit || show_uvshadow) {
		/* this is basically the same object_handle_update as in the 3d view,
		 * here we have to do it as well for the object we are editing if we
		 * are displaying the final result */
		if(obedit && (sima->flag & SI_DRAWSHADOW))
			object_handle_update(scene, obedit);

		if(show_uvshadow)
			draw_uvs_shadow(sima, obedit);
		else
			draw_uvs(sima, scene, obedit);

		if(show_uvedit)
			drawcursor_sima(sima, ar);
	}
}

