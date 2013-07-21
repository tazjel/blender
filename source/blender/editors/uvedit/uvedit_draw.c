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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2002-2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/uvedit/uvedit_draw.c
 *  \ingroup eduv
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

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_DerivedMesh.h"
#include "BKE_mesh.h"
#include "BKE_editmesh.h"

#include "BLI_array.h"
#include "BLI_buffer.h"

#include "GPU_colors.h"
#include "GPU_primitives.h"

#include "BIF_glutil.h"

#include "ED_util.h"
#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_uvedit.h"

#include "UI_resources.h"
#include "UI_interface.h"
#include "UI_view2d.h"

#include "uvedit_intern.h"

void draw_image_cursor(SpaceImage *sima, ARegion *ar)
{
	float zoom[2], x_fac, y_fac;

	UI_view2d_getscale_inverse(&ar->v2d, &zoom[0], &zoom[1]);

	mul_v2_fl(zoom, 256.0f * UI_DPI_FAC);
	x_fac = zoom[0];
	y_fac = zoom[1];
	
	gpuTranslate(sima->cursor[0], sima->cursor[1], 0.0);

	gpuImmediateFormat_C4_V2(); // DOODLE: uvedit cursor sima, 16 colored lines
	gpuBegin(GL_LINES);

	gpuColor3x(CPACK_WHITE);
	gpuAppendLinef(-0.05f * x_fac, 0, 0, 0.05f * y_fac);
	gpuAppendLinef(0, 0.05f * y_fac, 0.05f * x_fac, 0.0f);
	gpuAppendLinef(0.05f * x_fac, 0.0f, 0.0f, -0.05f * y_fac);
	gpuAppendLinef(0.0f, -0.05f * y_fac, -0.05f * x_fac, 0.0f);

	setlinestyle(4);
	gpuColor3x(CPACK_BLUE);
	gpuAppendLinef(-0.05f * x_fac, 0.0f, 0.0f, 0.05f * y_fac);
	gpuAppendLinef(0.0f, 0.05f * y_fac, 0.05f * x_fac, 0.0f);
	gpuAppendLinef(0.05f * x_fac, 0.0f, 0.0f, -0.05f * y_fac);
	gpuAppendLinef(0.0f, -0.05f * y_fac, -0.05f * x_fac, 0.0f);


	setlinestyle(0.0f);
	gpuColor3x(CPACK_BLACK);
	gpuAppendLinef(-0.020f * x_fac, 0.0f, -0.1f * x_fac, 0.0f);
	gpuAppendLinef(0.1f * x_fac, 0.0f, 0.020f * x_fac, 0.0f);
	gpuAppendLinef(0.0f, -0.020f * y_fac, 0.0f, -0.1f * y_fac);
	gpuAppendLinef(0.0f, 0.1f * y_fac, 0.0f, 0.020f * y_fac);

	setlinestyle(1);
	gpuColor3x(CPACK_WHITE);
	gpuAppendLinef(-0.020f * x_fac, 0.0f, -0.1f * x_fac, 0.0f);
	gpuAppendLinef(0.1f * x_fac, 0.0f, 0.020f * x_fac, 0.0f);
	gpuAppendLinef(0.0f, -0.020f * y_fac, 0.0f, -0.1f * y_fac);
	gpuAppendLinef(0.0f, 0.1f * y_fac, 0.0f, 0.020f * y_fac);

	gpuEnd();
	gpuImmediateUnformat();

	gpuTranslate(-sima->cursor[0], -sima->cursor[1], 0.0);
	setlinestyle(0);
}

static int draw_uvs_face_check(Scene *scene)
{
	ToolSettings *ts = scene->toolsettings;

	/* checks if we are selecting only faces */
	if (ts->uv_flag & UV_SYNC_SELECTION) {
		if (ts->selectmode == SCE_SELECT_FACE)
			return 2;
		else if (ts->selectmode & SCE_SELECT_FACE)
			return 1;
		else
			return 0;
	}
	else
		return (ts->uv_selectmode == UV_SELECT_FACE);
}

static void draw_uvs_shadow(Object *obedit)
{
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MLoopUV *luv;

	const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

	/* draws the gray mesh when painting */
	gpuCurrentGray3f(0.439f);

	BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
		gpuBegin(GL_LINE_LOOP);
		BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
			luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
			gpuVertex2fv(luv->uv);
		}
		gpuEnd();
	}
}

static int draw_uvs_dm_shadow(DerivedMesh *dm)
{
	/* draw shadow mesh - this is the mesh with the modifier applied */

	if (dm && dm->drawUVEdges && CustomData_has_layer(&dm->loopData, CD_MLOOPUV)) {
		gpuCurrentGray3f(0.439f);
		dm->drawUVEdges(dm);
		return 1;
	}

	return 0;
}

static void draw_uvs_stretch(SpaceImage *sima, Scene *scene, BMEditMesh *em, MTexPoly *activetf)
{
	BMesh *bm = em->bm;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	Image *ima = sima->image;
	float aspx, aspy, col[4];
	int i;

	const int cd_loop_uv_offset  = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&bm->pdata, CD_MTEXPOLY);

	BLI_buffer_declare_static(vec2f, tf_uv_buf, BLI_BUFFER_NOP, BM_DEFAULT_NGON_STACK_SIZE);
	BLI_buffer_declare_static(vec2f, tf_uvorig_buf, BLI_BUFFER_NOP, BM_DEFAULT_NGON_STACK_SIZE);

	ED_space_image_get_uv_aspect(sima, &aspx, &aspy);
	
	switch (sima->dt_uvstretch) {
		case SI_UVDT_STRETCH_AREA:
		{
			float totarea = 0.0f, totuvarea = 0.0f, areadiff, uvarea, area;
			
			BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
				const int efa_len = efa->len;
				float (*tf_uv)[2]     = (float (*)[2])BLI_buffer_resize_data(&tf_uv_buf,     vec2f, efa_len);
				float (*tf_uvorig)[2] = (float (*)[2])BLI_buffer_resize_data(&tf_uvorig_buf, vec2f, efa_len);
				tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);

				BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
					luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
					copy_v2_v2(tf_uvorig[i], luv->uv);
				}

				uv_poly_copy_aspect(tf_uvorig, tf_uv, aspx, aspy, efa->len);

				totarea += BM_face_calc_area(efa);
				totuvarea += area_poly_v2(efa->len, tf_uv);
				
				if (uvedit_face_visible_test(scene, ima, efa, tf)) {
					BM_elem_flag_enable(efa, BM_ELEM_TAG);
				}
				else {
					if (tf == activetf)
						activetf = NULL;
					BM_elem_flag_disable(efa, BM_ELEM_TAG);
				}
			}
			
			if (totarea < FLT_EPSILON || totuvarea < FLT_EPSILON) {
				col[0] = 1.0;
				col[1] = col[2] = 0.0;
				gpuCurrentColor3fv(col);
				BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
					if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
						gpuBegin(GL_TRIANGLE_FAN);
						BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
							luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
							gpuVertex2fv(luv->uv);
						}
						gpuEnd();
					}
				}
			}
			else {
				BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
					if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
						const int efa_len = efa->len;
						float (*tf_uv)[2]     = (float (*)[2])BLI_buffer_resize_data(&tf_uv_buf,     vec2f, efa_len);
						float (*tf_uvorig)[2] = (float (*)[2])BLI_buffer_resize_data(&tf_uvorig_buf, vec2f, efa_len);

						area = BM_face_calc_area(efa) / totarea;

						BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
							luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
							copy_v2_v2(tf_uvorig[i], luv->uv);
						}

						uv_poly_copy_aspect(tf_uvorig, tf_uv, aspx, aspy, efa->len);

						uvarea = area_poly_v2(efa->len, tf_uv) / totuvarea;
						
						if (area < FLT_EPSILON || uvarea < FLT_EPSILON)
							areadiff = 1.0f;
						else if (area > uvarea)
							areadiff = 1.0f - (uvarea / area);
						else
							areadiff = 1.0f - (area / uvarea);
						
						weight_to_rgb(col, areadiff);
						gpuCurrentColor3fv(col);
						
						gpuBegin(GL_TRIANGLE_FAN);
						BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
							luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
							gpuVertex2fv(luv->uv);
						}
						gpuEnd();
					}
				}
			}
			break;
		}
		case SI_UVDT_STRETCH_ANGLE:
		{
			float a;

			BLI_buffer_declare_static(float, uvang_buf, BLI_BUFFER_NOP, BM_DEFAULT_NGON_STACK_SIZE);
			BLI_buffer_declare_static(float, ang_buf,   BLI_BUFFER_NOP, BM_DEFAULT_NGON_STACK_SIZE);
			BLI_buffer_declare_static(vec3f, av_buf,  BLI_BUFFER_NOP, BM_DEFAULT_NGON_STACK_SIZE);
			BLI_buffer_declare_static(vec2f, auv_buf, BLI_BUFFER_NOP, BM_DEFAULT_NGON_STACK_SIZE);

			col[3] = 0.5f; /* hard coded alpha, not that nice */
			
			gpuShadeModel(GL_SMOOTH);
			
			BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
				tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
				
				if (uvedit_face_visible_test(scene, ima, efa, tf)) {
					const int efa_len = efa->len;
					float (*tf_uv)[2]     = (float (*)[2])BLI_buffer_resize_data(&tf_uv_buf,     vec2f, efa_len);
					float (*tf_uvorig)[2] = (float (*)[2])BLI_buffer_resize_data(&tf_uvorig_buf, vec2f, efa_len);
					float *uvang = BLI_buffer_resize_data(&uvang_buf, float, efa_len);
					float *ang   = BLI_buffer_resize_data(&ang_buf,   float, efa_len);
					float (*av)[3]  = (float (*)[3])BLI_buffer_resize_data(&av_buf, vec3f, efa_len);
					float (*auv)[2] = (float (*)[2])BLI_buffer_resize_data(&auv_buf, vec2f, efa_len);
					int j;

					BM_elem_flag_enable(efa, BM_ELEM_TAG);

					BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
						luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
						copy_v2_v2(tf_uvorig[i], luv->uv);
					}

					uv_poly_copy_aspect(tf_uvorig, tf_uv, aspx, aspy, efa_len);

					j = efa_len - 1;
					BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
						sub_v2_v2v2(auv[i], tf_uv[j], tf_uv[i]); normalize_v2(auv[i]);
						sub_v3_v3v3(av[i], l->prev->v->co, l->v->co); normalize_v3(av[i]);
						j = i;
					}

					for (i = 0; i < efa_len; i++) {
#if 0
						/* Simple but slow, better reuse normalized vectors
						 * (Not ported to bmesh, copied for reference) */
						uvang1 = RAD2DEG(angle_v2v2v2(tf_uv[3], tf_uv[0], tf_uv[1]));
						ang1 = RAD2DEG(angle_v3v3v3(efa->v4->co, efa->v1->co, efa->v2->co));
#endif
						uvang[i] = angle_normalized_v2v2(auv[i], auv[(i + 1) % efa_len]);
						ang[i] = angle_normalized_v3v3(av[i], av[(i + 1) % efa_len]);
					}

					gpuBegin(GL_TRIANGLE_FAN);
					BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
						luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
						a = fabsf(uvang[i] - ang[i]) / (float)M_PI;
						weight_to_rgb(col, 1.0f - powf((1.0f - a), 2.0f));
						gpuColor3fv(col);
						gpuVertex2fv(luv->uv);
					}
					gpuEnd();
				}
				else {
					if (tf == activetf)
						activetf = NULL;
					BM_elem_flag_disable(efa, BM_ELEM_TAG);
				}
			}

			BLI_buffer_free(&uvang_buf);
			BLI_buffer_free(&ang_buf);
			BLI_buffer_free(&av_buf);
			BLI_buffer_free(&auv_buf);

			gpuShadeModel(GL_FLAT);

			break;
		}
	}

	BLI_buffer_free(&tf_uv_buf);
	BLI_buffer_free(&tf_uvorig_buf);
}

static void draw_uvs_other(Scene *scene, Object *obedit, Image *curimage)
{
	Base *base;

	gpuCurrentGray3f(0.376f);

	for (base = scene->base.first; base; base = base->next) {
		Object *ob = base->object;

		if (!(base->flag & SELECT)) continue;
		if (!(base->lay & scene->lay)) continue;
		if (ob->restrictflag & OB_RESTRICT_VIEW) continue;

		if ((ob->type == OB_MESH) && (ob != obedit)) {
			Mesh *me = ob->data;

			if (me->mtpoly) {
				MPoly *mpoly = me->mpoly;
				MTexPoly *mtpoly = me->mtpoly;
				MLoopUV *mloopuv;
				int a, b;

				for (a = me->totpoly; a > 0; a--, mtpoly++, mpoly++) {
					if (mtpoly->tpage == curimage) {
						gpuBegin(GL_LINE_LOOP);

						mloopuv = me->mloopuv + mpoly->loopstart;
						for (b = 0; b < mpoly->totloop; b++, mloopuv++) {
							gpuVertex2fv(mloopuv->uv);
						}
						gpuEnd();
					}
				}
			}
		}
	}
}

static void draw_uvs_texpaint(SpaceImage *sima, Scene *scene, Object *ob)
{
	Mesh *me = ob->data;
	Image *curimage = ED_space_image(sima);

	if (sima->flag & SI_DRAW_OTHER)
		draw_uvs_other(scene, ob, curimage);

	gpuCurrentGray3f(0.439f);

	if (me->mtpoly) {
		MPoly *mpoly = me->mpoly;
		MTexPoly *tface = me->mtpoly;
		MLoopUV *mloopuv;
		int a, b;

		for (a = me->totpoly; a > 0; a--, tface++, mpoly++) {
			if (tface->tpage == curimage) {
				gpuBegin(GL_LINE_LOOP);

				mloopuv = me->mloopuv + mpoly->loopstart;
				for (b = 0; b < mpoly->totloop; b++, mloopuv++) {
					gpuVertex2fv(mloopuv->uv);
				}
				gpuEnd();
			}
		}
	}
}

/* draws uv's in the image space */
static void draw_uvs(SpaceImage *sima, Scene *scene, Object *obedit)
{
	ToolSettings *ts;
	Mesh *me = obedit->data;
	BMEditMesh *em = me->edit_btmesh;
	BMesh *bm = em->bm;
	BMFace *efa, *efa_act, *activef;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf, *activetf = NULL;
	MLoopUV *luv;
	DerivedMesh *finaldm, *cagedm;
	unsigned char col1[4], col2[4];
	float pointsize;
	int drawfaces, interpedges;
	Image *ima = sima->image;

	const int cd_loop_uv_offset  = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);
	const int cd_poly_tex_offset = CustomData_get_offset(&bm->pdata, CD_MTEXPOLY);

	gpuImmediateFormat_C4_V2();


	activetf = EDBM_mtexpoly_active_get(em, &efa_act, FALSE, FALSE); /* will be set to NULL if hidden */
	activef = BM_active_face_get(bm, FALSE, FALSE);
	ts = scene->toolsettings;

	drawfaces = draw_uvs_face_check(scene);
	if (ts->uv_flag & UV_SYNC_SELECTION)
		interpedges = (ts->selectmode & SCE_SELECT_VERTEX);
	else
		interpedges = (ts->uv_selectmode == UV_SELECT_VERTEX);
	
	/* draw other uvs */
	if (sima->flag & SI_DRAW_OTHER) {
		Image *curimage = (activetf) ? activetf->tpage : ima;

		draw_uvs_other(scene, obedit, curimage);
	}

	/* 1. draw shadow mesh */
	
	if (sima->flag & SI_DRAWSHADOW) {
		/* first try existing derivedmesh */
		if (!draw_uvs_dm_shadow(em->derivedFinal)) {
			/* create one if it does not exist */
			cagedm = editbmesh_get_derived_cage_and_final(scene, obedit, me->edit_btmesh, &finaldm, CD_MASK_BAREMESH | CD_MASK_MTFACE);

			/* when sync selection is enabled, all faces are drawn (except for hidden)
			 * so if cage is the same as the final, theres no point in drawing this */
			if (!((ts->uv_flag & UV_SYNC_SELECTION) && (cagedm == finaldm)))
				draw_uvs_dm_shadow(finaldm);
			
			/* release derivedmesh again */
			if (cagedm != finaldm) cagedm->release(cagedm);
			finaldm->release(finaldm);
		}
	}
	
	/* 2. draw colored faces */
	
	if (sima->flag & SI_DRAW_STRETCH) {
		draw_uvs_stretch(sima, scene, em, activetf);
	}
	else if (!(sima->flag & SI_NO_DRAWFACES)) {
		/* draw transparent faces */
		UI_GetThemeColor4ubv(TH_FACE, col1);
		UI_GetThemeColor4ubv(TH_FACE_SELECT, col2);
		glEnable(GL_BLEND);
		
		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);
			
			if (uvedit_face_visible_test(scene, ima, efa, tf)) {
				BM_elem_flag_enable(efa, BM_ELEM_TAG);
				if (tf == activetf) continue;  /* important the temp boolean is set above */

				if (uvedit_face_select_test(scene, efa, cd_loop_uv_offset))
					gpuCurrentColor4ubv((GLubyte *)col2);
				else
					gpuCurrentColor4ubv((GLubyte *)col1);
				
				gpuBegin(GL_TRIANGLE_FAN);
				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
					gpuVertex2fv(luv->uv);
				}
				gpuEnd();
			}
			else {
				if (tf == activetf)
					activetf = NULL;
				BM_elem_flag_disable(efa, BM_ELEM_TAG);
			}
		}
		glDisable(GL_BLEND);
	}
	else {
		/* would be nice to do this within a draw loop but most below are optional, so it would involve too many checks */
		
		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);

			if (uvedit_face_visible_test(scene, ima, efa, tf)) {
				BM_elem_flag_enable(efa, BM_ELEM_TAG);
			}
			else {
				if (tf == activetf)
					activetf = NULL;
				BM_elem_flag_disable(efa, BM_ELEM_TAG);
			}
		}
		
	}

	/* 3. draw active face stippled */

	if (activef) {
		tf = BM_ELEM_CD_GET_VOID_P(activef, cd_poly_tex_offset);
		if (uvedit_face_visible_test(scene, ima, activef, tf)) {
			glEnable(GL_BLEND);
			UI_ThemeColor4(TH_EDITMESH_ACTIVE);

			gpuEnablePolygonStipple();
			gpuPolygonStipple(stipple_quarttone);

			gpuBegin(GL_TRIANGLE_FAN);
			BM_ITER_ELEM (l, &liter, activef, BM_LOOPS_OF_FACE) {
				luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
				gpuVertex2fv(luv->uv);
			}
			gpuEnd();

			gpuDisablePolygonStipple();
			glDisable(GL_BLEND);
		}
	}
	
	/* 4. draw edges */

	if (sima->flag & SI_SMOOTH_UV) {
		glEnable(GL_LINE_SMOOTH);
		glEnable(GL_BLEND);
	}
	
	switch (sima->dt_uv) {
		case SI_UVDT_DASH:
			BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
				if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
					continue;
				tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset);

				if (tf) {
					gpuCurrentGray3f(0.067f);

					gpuBegin(GL_LINE_LOOP);
					BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
						luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
						gpuVertex2fv(luv->uv);
					}
					gpuEnd();

					setlinestyle(2);
					gpuCurrentGray3f(0.565f);

					gpuBegin(GL_LINE_LOOP);
					BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
						luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
						gpuVertex2fv(luv->uv);
					}
					gpuEnd();

					setlinestyle(0);
				}
			}
			break;
		case SI_UVDT_BLACK: /* black/white */
		case SI_UVDT_WHITE: 
			gpuCurrentColor3x(
				sima->dt_uv == SI_UVDT_WHITE ? CPACK_WHITE : CPACK_BLACK);

			BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
				if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
					continue;

				gpuBegin(GL_LINE_LOOP);
				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
					gpuVertex2fv(luv->uv);
				}
				gpuEnd();
			}
			break;
		case SI_UVDT_OUTLINE:
			gpuLineWidth(3);
			gpuCurrentColor3x(CPACK_BLACK);
			
			BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
				if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
					continue;

				gpuBegin(GL_LINE_LOOP);
				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
					gpuVertex2fv(luv->uv);
				}
				gpuEnd();
			}
			
			gpuLineWidth(1);
			col2[0] = col2[1] = col2[2] = 192; col2[3] = 255;
			gpuCurrentColor4ubv((unsigned char *)col2); 
			
			if (me->drawflag & ME_DRAWEDGES) {
				int sel, lastsel = -1;
				UI_GetThemeColor4ubv(TH_VERTEX_SELECT, col1);

				if (interpedges) {
					gpuShadeModel(GL_SMOOTH);

					BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
						if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
							continue;

						gpuBegin(GL_LINE_LOOP);
						BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
							sel = uvedit_uv_select_test(scene, l, cd_loop_uv_offset);
							gpuColor4ubv(sel ? (GLubyte *)col1 : (GLubyte *)col2);

							luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
							gpuVertex2fv(luv->uv);
						}
						gpuEnd();
					}

					gpuShadeModel(GL_FLAT);
				}
				else {
					BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
						if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
							continue;

						gpuBegin(GL_LINES);
						BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
							sel = uvedit_edge_select_test(scene, l, cd_loop_uv_offset);
							if (sel != lastsel) {
								gpuColor4ubv(sel ? (GLubyte *)col1 : (GLubyte *)col2);
								lastsel = sel;
							}
							luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
							gpuVertex2fv(luv->uv);
							luv = BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_uv_offset);
							gpuVertex2fv(luv->uv);
						}
						gpuEnd();
					}
				}
			}
			else {
				/* no nice edges */
				BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
					if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
						continue;
				
					gpuBegin(GL_LINE_LOOP);
					BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
						luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
						gpuVertex2fv(luv->uv);
					}
					gpuEnd();
				}
			}
			
			break;
	}

	if (sima->flag & SI_SMOOTH_UV) {
		glDisable(GL_LINE_SMOOTH);
		glDisable(GL_BLEND);
	}

	/* 5. draw face centers */

	if (drawfaces) {
		float cent[2];
		
		pointsize = UI_GetThemeValuef(TH_FACEDOT_SIZE);
		gpuSpriteSize(pointsize); // TODO - drawobject.c changes this value after - Investigate!
		
		/* unselected faces */
		UI_ThemeColor(TH_WIRE);

		gpuBeginSprites();
		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
				continue;

			if (!uvedit_face_select_test(scene, efa, cd_loop_uv_offset)) {
				uv_poly_center(efa, cent, cd_loop_uv_offset);
				gpuSprite2fv(cent);
			}
		}
		gpuEndSprites();

		/* selected faces */
		UI_ThemeColor(TH_FACE_DOT);

		gpuBeginSprites();
		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
				continue;

			if (uvedit_face_select_test(scene, efa, cd_loop_uv_offset)) {
				uv_poly_center(efa, cent, cd_loop_uv_offset);
				gpuSprite2fv(cent);
			}
		}
		gpuEndSprites();
	}

	/* 6. draw uv vertices */
	
	if (drawfaces != 2) { /* 2 means Mesh Face Mode */
		/* unselected uvs */
		UI_ThemeColor(TH_VERTEX);
		pointsize = UI_GetThemeValuef(TH_VERTEX_SIZE);
		gpuSpriteSize(pointsize);
	
		gpuBeginSprites();
		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
				continue;

			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
				if (!uvedit_uv_select_test(scene, l, cd_loop_uv_offset))
					gpuSprite2fv(luv->uv);
			}
		}
		gpuEndSprites();

		/* pinned uvs */
		/* give odd pointsizes odd pin pointsizes */
		gpuSpriteSize(pointsize * 2 + (((int)pointsize % 2) ? (-1) : 0));
		gpuCurrentColor3x(CPACK_BLUE);

		gpuBeginSprites();
		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
				continue;

			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

				if (luv->flag & MLOOPUV_PINNED)
					gpuSprite2fv(luv->uv);
			}
		}
		gpuEndSprites();
	
		/* selected uvs */
		UI_ThemeColor(TH_VERTEX_SELECT);
		gpuSpriteSize(pointsize);
	
		gpuBeginSprites();
		BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
			if (!BM_elem_flag_test(efa, BM_ELEM_TAG))
				continue;

			BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
				luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

				if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset))
					gpuSprite2fv(luv->uv);
			}
		}
		gpuEndSprites();	
	}

	gpuSpriteSize(1.0);

	gpuImmediateUnformat();
}

void draw_uvedit_main(SpaceImage *sima, ARegion *ar, Scene *scene, Object *obedit, Object *obact)
{
	ToolSettings *toolsettings = scene->toolsettings;
	int show_uvedit, show_uvshadow, show_texpaint_uvshadow;

	show_texpaint_uvshadow = (obact && obact->type == OB_MESH && obact->mode == OB_MODE_TEXTURE_PAINT);
	show_uvedit = ED_space_image_show_uvedit(sima, obedit);
	show_uvshadow = ED_space_image_show_uvshadow(sima, obedit);

	if (show_uvedit || show_uvshadow || show_texpaint_uvshadow) {
		if (show_uvshadow)
			draw_uvs_shadow(obedit);
		else if (show_uvedit)
			draw_uvs(sima, scene, obedit);
		else
			draw_uvs_texpaint(sima, scene, obact);

		if (show_uvedit && !(toolsettings->use_uv_sculpt))
			draw_image_cursor(sima, ar);
	}
}

