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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/colortools.c
 *  \ingroup bke
 */


#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "DNA_color_types.h"
#include "DNA_curve_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_colortools.h"
#include "BKE_curve.h"
#include "BKE_fcurve.h"


#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

/* ********************************* color curve ********************* */

/* ***************** operations on full struct ************* */

CurveMapping *curvemapping_add(int tot, float minx, float miny, float maxx, float maxy)
{
	CurveMapping *cumap;
	int a;
	float clipminx, clipminy, clipmaxx, clipmaxy;
	
	cumap = MEM_callocN(sizeof(CurveMapping), "new curvemap");
	cumap->flag = CUMA_DO_CLIP;
	if (tot == 4) cumap->cur = 3;   /* rhms, hack for 'col' curve? */
	
	clipminx = MIN2(minx, maxx);
	clipminy = MIN2(miny, maxy);
	clipmaxx = MAX2(minx, maxx);
	clipmaxy = MAX2(miny, maxy);
	
	BLI_init_rctf(&cumap->curr, clipminx, clipmaxx, clipminy, clipmaxy);
	cumap->clipr = cumap->curr;
	
	cumap->white[0] = cumap->white[1] = cumap->white[2] = 1.0f;
	cumap->bwmul[0] = cumap->bwmul[1] = cumap->bwmul[2] = 1.0f;
	
	for (a = 0; a < tot; a++) {
		cumap->cm[a].flag = CUMA_EXTEND_EXTRAPOLATE;
		cumap->cm[a].totpoint = 2;
		cumap->cm[a].curve = MEM_callocN(2 * sizeof(CurveMapPoint), "curve points");

		cumap->cm[a].curve[0].x = minx;
		cumap->cm[a].curve[0].y = miny;
		cumap->cm[a].curve[1].x = maxx;
		cumap->cm[a].curve[1].y = maxy;
	}	

	cumap->changed_timestamp = 0;

	return cumap;
}

void curvemapping_free(CurveMapping *cumap)
{
	int a;
	
	if (cumap) {
		for (a = 0; a < CM_TOT; a++) {
			if (cumap->cm[a].curve) MEM_freeN(cumap->cm[a].curve);
			if (cumap->cm[a].table) MEM_freeN(cumap->cm[a].table);
			if (cumap->cm[a].premultable) MEM_freeN(cumap->cm[a].premultable);
		}
		MEM_freeN(cumap);
	}
}

CurveMapping *curvemapping_copy(CurveMapping *cumap)
{
	int a;
	
	if (cumap) {
		CurveMapping *cumapn = MEM_dupallocN(cumap);
		for (a = 0; a < CM_TOT; a++) {
			if (cumap->cm[a].curve) 
				cumapn->cm[a].curve = MEM_dupallocN(cumap->cm[a].curve);
			if (cumap->cm[a].table) 
				cumapn->cm[a].table = MEM_dupallocN(cumap->cm[a].table);
			if (cumap->cm[a].premultable) 
				cumapn->cm[a].premultable = MEM_dupallocN(cumap->cm[a].premultable);
		}
		return cumapn;
	}
	return NULL;
}

void curvemapping_set_black_white(CurveMapping *cumap, const float black[3], const float white[3])
{
	int a;
	
	if (white)
		copy_v3_v3(cumap->white, white);
	if (black)
		copy_v3_v3(cumap->black, black);
	
	for (a = 0; a < 3; a++) {
		if (cumap->white[a] == cumap->black[a])
			cumap->bwmul[a] = 0.0f;
		else
			cumap->bwmul[a] = 1.0f / (cumap->white[a] - cumap->black[a]);
	}	
}

/* ***************** operations on single curve ************* */
/* ********** NOTE: requires curvemapping_changed() call after ******** */

/* remove specified point */
void curvemap_remove_point(CurveMap *cuma, CurveMapPoint *point)
{
	CurveMapPoint *cmp;
	int a, b, removed = 0;
	
	/* must have 2 points minimum */
	if (cuma->totpoint <= 2)
		return;

	cmp = MEM_mallocN((cuma->totpoint) * sizeof(CurveMapPoint), "curve points");

	/* well, lets keep the two outer points! */
	for (a = 0, b = 0; a < cuma->totpoint; a++) {
		if (&cuma->curve[a] != point) {
			cmp[b] = cuma->curve[a];
			b++;
		}
		else removed++;
	}
	
	MEM_freeN(cuma->curve);
	cuma->curve = cmp;
	cuma->totpoint -= removed;
}

/* removes with flag set */
void curvemap_remove(CurveMap *cuma, int flag)
{
	CurveMapPoint *cmp = MEM_mallocN((cuma->totpoint) * sizeof(CurveMapPoint), "curve points");
	int a, b, removed = 0;
	
	/* well, lets keep the two outer points! */
	cmp[0] = cuma->curve[0];
	for (a = 1, b = 1; a < cuma->totpoint - 1; a++) {
		if (!(cuma->curve[a].flag & flag)) {
			cmp[b] = cuma->curve[a];
			b++;
		}
		else removed++;
	}
	cmp[b] = cuma->curve[a];
	
	MEM_freeN(cuma->curve);
	cuma->curve = cmp;
	cuma->totpoint -= removed;
}

CurveMapPoint *curvemap_insert(CurveMap *cuma, float x, float y)
{
	CurveMapPoint *cmp = MEM_callocN((cuma->totpoint + 1) * sizeof(CurveMapPoint), "curve points");
	CurveMapPoint *newcmp = NULL;
	int a, b, foundloc = 0;
		
	/* insert fragments of the old one and the new point to the new curve */
	cuma->totpoint++;
	for (a = 0, b = 0; a < cuma->totpoint; a++) {
		if ((x < cuma->curve[a].x) && !foundloc) {
			cmp[a].x = x;
			cmp[a].y = y;
			cmp[a].flag = CUMA_SELECT;
			foundloc = 1;
			newcmp = &cmp[a];
		}
		else {
			cmp[a].x = cuma->curve[b].x;
			cmp[a].y = cuma->curve[b].y;
			cmp[a].flag = cuma->curve[b].flag;
			cmp[a].flag &= ~CUMA_SELECT; /* make sure old points don't remain selected */
			cmp[a].shorty = cuma->curve[b].shorty;
			b++;
		}
	}

	/* free old curve and replace it with new one */
	MEM_freeN(cuma->curve);
	cuma->curve = cmp;

	return newcmp;
}

void curvemap_reset(CurveMap *cuma, rctf *clipr, int preset, int slope)
{
	if (cuma->curve)
		MEM_freeN(cuma->curve);

	switch (preset) {
		case CURVE_PRESET_LINE: cuma->totpoint = 2; break;
		case CURVE_PRESET_SHARP: cuma->totpoint = 4; break;
		case CURVE_PRESET_SMOOTH: cuma->totpoint = 4; break;
		case CURVE_PRESET_MAX: cuma->totpoint = 2; break;
		case CURVE_PRESET_MID9: cuma->totpoint = 9; break;
		case CURVE_PRESET_ROUND: cuma->totpoint = 4; break;
		case CURVE_PRESET_ROOT: cuma->totpoint = 4; break;
	}

	cuma->curve = MEM_callocN(cuma->totpoint * sizeof(CurveMapPoint), "curve points");

	switch (preset) {
		case CURVE_PRESET_LINE:
			cuma->curve[0].x = clipr->xmin;
			cuma->curve[0].y = clipr->ymax;
			cuma->curve[0].flag = 0;
			cuma->curve[1].x = clipr->xmax;
			cuma->curve[1].y = clipr->ymin;
			cuma->curve[1].flag = 0;
			break;
		case CURVE_PRESET_SHARP:
			cuma->curve[0].x = 0;
			cuma->curve[0].y = 1;
			cuma->curve[1].x = 0.25;
			cuma->curve[1].y = 0.50;
			cuma->curve[2].x = 0.75;
			cuma->curve[2].y = 0.04;
			cuma->curve[3].x = 1;
			cuma->curve[3].y = 0;
			break;
		case CURVE_PRESET_SMOOTH:
			cuma->curve[0].x = 0;
			cuma->curve[0].y = 1;
			cuma->curve[1].x = 0.25;
			cuma->curve[1].y = 0.94;
			cuma->curve[2].x = 0.75;
			cuma->curve[2].y = 0.06;
			cuma->curve[3].x = 1;
			cuma->curve[3].y = 0;
			break;
		case CURVE_PRESET_MAX:
			cuma->curve[0].x = 0;
			cuma->curve[0].y = 1;
			cuma->curve[1].x = 1;
			cuma->curve[1].y = 1;
			break;
		case CURVE_PRESET_MID9:
		{
			int i;
			for (i = 0; i < cuma->totpoint; i++) {
				cuma->curve[i].x = i / ((float)cuma->totpoint - 1);
				cuma->curve[i].y = 0.5;
			}
		}
		break;
		case CURVE_PRESET_ROUND:
			cuma->curve[0].x = 0;
			cuma->curve[0].y = 1;
			cuma->curve[1].x = 0.5;
			cuma->curve[1].y = 0.90;
			cuma->curve[2].x = 0.86;
			cuma->curve[2].y = 0.5;
			cuma->curve[3].x = 1;
			cuma->curve[3].y = 0;
			break;
		case CURVE_PRESET_ROOT:
			cuma->curve[0].x = 0;
			cuma->curve[0].y = 1;
			cuma->curve[1].x = 0.25;
			cuma->curve[1].y = 0.95;
			cuma->curve[2].x = 0.75;
			cuma->curve[2].y = 0.44;
			cuma->curve[3].x = 1;
			cuma->curve[3].y = 0;
			break;
	}

	/* mirror curve in x direction to have positive slope
	 * rather than default negative slope */
	if (slope == CURVEMAP_SLOPE_POSITIVE) {
		int i, last = cuma->totpoint - 1;
		CurveMapPoint *newpoints = MEM_dupallocN(cuma->curve);
		
		for (i = 0; i < cuma->totpoint; i++) {
			newpoints[i].y = cuma->curve[last - i].y;
		}
		
		MEM_freeN(cuma->curve);
		cuma->curve = newpoints;
	}
	
	if (cuma->table) {
		MEM_freeN(cuma->table);
		cuma->table = NULL;
	}
}

/* if type==1: vector, else auto */
void curvemap_sethandle(CurveMap *cuma, int type)
{
	int a;
	
	for (a = 0; a < cuma->totpoint; a++) {
		if (cuma->curve[a].flag & CUMA_SELECT) {
			if (type) cuma->curve[a].flag |= CUMA_VECTOR;
			else cuma->curve[a].flag &= ~CUMA_VECTOR;
		}
	}
}

/* *********************** Making the tables and display ************** */

/* reduced copy of garbled calchandleNurb() code in curve.c */
static void calchandle_curvemap(BezTriple *bezt, BezTriple *prev, BezTriple *next, int UNUSED(mode))
{
	float *p1, *p2, *p3, pt[3];
	float len, len_a, len_b;
	float dvec_a[2], dvec_b[2];

	if (bezt->h1 == 0 && bezt->h2 == 0) {
		return;
	}
	
	p2 = bezt->vec[1];
	
	if (prev == NULL) {
		p3 = next->vec[1];
		pt[0] = 2.0f * p2[0] - p3[0];
		pt[1] = 2.0f * p2[1] - p3[1];
		p1 = pt;
	}
	else {
		p1 = prev->vec[1];
	}
	
	if (next == NULL) {
		p1 = prev->vec[1];
		pt[0] = 2.0f * p2[0] - p1[0];
		pt[1] = 2.0f * p2[1] - p1[1];
		p3 = pt;
	}
	else {
		p3 = next->vec[1];
	}

	sub_v2_v2v2(dvec_a, p2, p1);
	sub_v2_v2v2(dvec_b, p3, p2);

	len_a = len_v2(dvec_a);
	len_b = len_v2(dvec_b);

	if (len_a == 0.0f) len_a = 1.0f;
	if (len_b == 0.0f) len_b = 1.0f;

	if (bezt->h1 == HD_AUTO || bezt->h2 == HD_AUTO) { /* auto */
		float tvec[2];
		tvec[0] = dvec_b[0] / len_b + dvec_a[0] / len_a;
		tvec[1] = dvec_b[1] / len_b + dvec_a[1] / len_a;

		len = len_v2(tvec) * 2.5614f;
		if (len != 0.0f) {
			
			if (bezt->h1 == HD_AUTO) {
				len_a /= len;
				madd_v2_v2v2fl(p2 - 3, p2, tvec, -len_a);
			}
			if (bezt->h2 == HD_AUTO) {
				len_b /= len;
				madd_v2_v2v2fl(p2 + 3, p2, tvec,  len_b);
			}
		}
	}

	if (bezt->h1 == HD_VECT) {    /* vector */
		madd_v2_v2v2fl(p2 - 3, p2, dvec_a, -1.0f / 3.0f);
	}
	if (bezt->h2 == HD_VECT) {
		madd_v2_v2v2fl(p2 + 3, p2, dvec_b,  1.0f / 3.0f);
	}
}

/* in X, out Y. 
 * X is presumed to be outside first or last */
static float curvemap_calc_extend(CurveMap *cuma, float x, const float first[2], const float last[2])
{
	if (x <= first[0]) {
		if ((cuma->flag & CUMA_EXTEND_EXTRAPOLATE) == 0) {
			/* no extrapolate */
			return first[1];
		}
		else {
			if (cuma->ext_in[0] == 0.0f)
				return first[1] + cuma->ext_in[1] * 10000.0f;
			else
				return first[1] + cuma->ext_in[1] * (x - first[0]) / cuma->ext_in[0];
		}
	}
	else if (x >= last[0]) {
		if ((cuma->flag & CUMA_EXTEND_EXTRAPOLATE) == 0) {
			/* no extrapolate */
			return last[1];
		}
		else {
			if (cuma->ext_out[0] == 0.0f)
				return last[1] - cuma->ext_out[1] * 10000.0f;
			else
				return last[1] + cuma->ext_out[1] * (x - last[0]) / cuma->ext_out[0];
		}
	}
	return 0.0f;
}

/* only creates a table for a single channel in CurveMapping */
static void curvemap_make_table(CurveMap *cuma, rctf *clipr)
{
	CurveMapPoint *cmp = cuma->curve;
	BezTriple *bezt;
	float *fp, *allpoints, *lastpoint, curf, range;
	int a, totpoint;
	
	if (cuma->curve == NULL) return;
	
	/* default rect also is table range */
	cuma->mintable = clipr->xmin;
	cuma->maxtable = clipr->xmax;
	
	/* hrmf... we now rely on blender ipo beziers, these are more advanced */
	bezt = MEM_callocN(cuma->totpoint * sizeof(BezTriple), "beztarr");
	
	for (a = 0; a < cuma->totpoint; a++) {
		cuma->mintable = MIN2(cuma->mintable, cmp[a].x);
		cuma->maxtable = MAX2(cuma->maxtable, cmp[a].x);
		bezt[a].vec[1][0] = cmp[a].x;
		bezt[a].vec[1][1] = cmp[a].y;
		if (cmp[a].flag & CUMA_VECTOR)
			bezt[a].h1 = bezt[a].h2 = HD_VECT;
		else
			bezt[a].h1 = bezt[a].h2 = HD_AUTO;
	}
	
	for (a = 0; a < cuma->totpoint; a++) {
		if (a == 0)
			calchandle_curvemap(bezt, NULL, bezt + 1, 0);
		else if (a == cuma->totpoint - 1)
			calchandle_curvemap(bezt + a, bezt + a - 1, NULL, 0);
		else
			calchandle_curvemap(bezt + a, bezt + a - 1, bezt + a + 1, 0);
	}
	
	/* first and last handle need correction, instead of pointing to center of next/prev, 
	 * we let it point to the closest handle */
	if (cuma->totpoint > 2) {
		float hlen, nlen, vec[3];
		
		if (bezt[0].h2 == HD_AUTO) {
			
			hlen = len_v3v3(bezt[0].vec[1], bezt[0].vec[2]); /* original handle length */
			/* clip handle point */
			copy_v3_v3(vec, bezt[1].vec[0]);
			if (vec[0] < bezt[0].vec[1][0])
				vec[0] = bezt[0].vec[1][0];
			
			sub_v3_v3(vec, bezt[0].vec[1]);
			nlen = len_v3(vec);
			if (nlen > FLT_EPSILON) {
				mul_v3_fl(vec, hlen / nlen);
				add_v3_v3v3(bezt[0].vec[2], vec, bezt[0].vec[1]);
				sub_v3_v3v3(bezt[0].vec[0], bezt[0].vec[1], vec);
			}
		}
		a = cuma->totpoint - 1;
		if (bezt[a].h2 == HD_AUTO) {
			
			hlen = len_v3v3(bezt[a].vec[1], bezt[a].vec[0]); /* original handle length */
			/* clip handle point */
			copy_v3_v3(vec, bezt[a - 1].vec[2]);
			if (vec[0] > bezt[a].vec[1][0])
				vec[0] = bezt[a].vec[1][0];
			
			sub_v3_v3(vec, bezt[a].vec[1]);
			nlen = len_v3(vec);
			if (nlen > FLT_EPSILON) {
				mul_v3_fl(vec, hlen / nlen);
				add_v3_v3v3(bezt[a].vec[0], vec, bezt[a].vec[1]);
				sub_v3_v3v3(bezt[a].vec[2], bezt[a].vec[1], vec);
			}
		}
	}	
	/* make the bezier curve */
	if (cuma->table)
		MEM_freeN(cuma->table);
	totpoint = (cuma->totpoint - 1) * CM_RESOL;
	fp = allpoints = MEM_callocN(totpoint * 2 * sizeof(float), "table");
	
	for (a = 0; a < cuma->totpoint - 1; a++, fp += 2 * CM_RESOL) {
		correct_bezpart(bezt[a].vec[1], bezt[a].vec[2], bezt[a + 1].vec[0], bezt[a + 1].vec[1]);
		BKE_curve_forward_diff_bezier(bezt[a].vec[1][0], bezt[a].vec[2][0], bezt[a + 1].vec[0][0], bezt[a + 1].vec[1][0], fp, CM_RESOL - 1, 2 * sizeof(float));
		BKE_curve_forward_diff_bezier(bezt[a].vec[1][1], bezt[a].vec[2][1], bezt[a + 1].vec[0][1], bezt[a + 1].vec[1][1], fp + 1, CM_RESOL - 1, 2 * sizeof(float));
	}
	
	/* store first and last handle for extrapolation, unit length */
	cuma->ext_in[0] = bezt[0].vec[0][0] - bezt[0].vec[1][0];
	cuma->ext_in[1] = bezt[0].vec[0][1] - bezt[0].vec[1][1];
	range = sqrt(cuma->ext_in[0] * cuma->ext_in[0] + cuma->ext_in[1] * cuma->ext_in[1]);
	cuma->ext_in[0] /= range;
	cuma->ext_in[1] /= range;

	a = cuma->totpoint - 1;
	cuma->ext_out[0] = bezt[a].vec[1][0] - bezt[a].vec[2][0];
	cuma->ext_out[1] = bezt[a].vec[1][1] - bezt[a].vec[2][1];
	range = sqrt(cuma->ext_out[0] * cuma->ext_out[0] + cuma->ext_out[1] * cuma->ext_out[1]);
	cuma->ext_out[0] /= range;
	cuma->ext_out[1] /= range;
	
	/* cleanup */
	MEM_freeN(bezt);

	range = CM_TABLEDIV * (cuma->maxtable - cuma->mintable);
	cuma->range = 1.0f / range;
	
	/* now make a table with CM_TABLE equal x distances */
	fp = allpoints;
	lastpoint = allpoints + 2 * (totpoint - 1);
	cmp = MEM_callocN((CM_TABLE + 1) * sizeof(CurveMapPoint), "dist table");

	for (a = 0; a <= CM_TABLE; a++) {
		curf = cuma->mintable + range * (float)a;
		cmp[a].x = curf;
		
		/* get the first x coordinate larger than curf */
		while (curf >= fp[0] && fp != lastpoint) {
			fp += 2;
		}
		if (fp == allpoints || (curf >= fp[0] && fp == lastpoint))
			cmp[a].y = curvemap_calc_extend(cuma, curf, allpoints, lastpoint);
		else {
			float fac1 = fp[0] - fp[-2];
			float fac2 = fp[0] - curf;
			if (fac1 > FLT_EPSILON)
				fac1 = fac2 / fac1;
			else
				fac1 = 0.0f;
			cmp[a].y = fac1 * fp[-1] + (1.0f - fac1) * fp[1];
		}
	}
	
	MEM_freeN(allpoints);
	cuma->table = cmp;
}

/* call when you do images etc, needs restore too. also verifies tables */
/* it uses a flag to prevent premul or free to happen twice */
void curvemapping_premultiply(CurveMapping *cumap, int restore)
{
	int a;
	
	if (restore) {
		if (cumap->flag & CUMA_PREMULLED) {
			for (a = 0; a < 3; a++) {
				MEM_freeN(cumap->cm[a].table);
				cumap->cm[a].table = cumap->cm[a].premultable;
				cumap->cm[a].premultable = NULL;
			}
			
			cumap->flag &= ~CUMA_PREMULLED;
		}
	}
	else {
		if ((cumap->flag & CUMA_PREMULLED) == 0) {
			/* verify and copy */
			for (a = 0; a < 3; a++) {
				if (cumap->cm[a].table == NULL)
					curvemap_make_table(cumap->cm + a, &cumap->clipr);
				cumap->cm[a].premultable = cumap->cm[a].table;
				cumap->cm[a].table = MEM_mallocN((CM_TABLE + 1) * sizeof(CurveMapPoint), "premul table");
				memcpy(cumap->cm[a].table, cumap->cm[a].premultable, (CM_TABLE + 1) * sizeof(CurveMapPoint));
			}
			
			if (cumap->cm[3].table == NULL)
				curvemap_make_table(cumap->cm + 3, &cumap->clipr);
		
			/* premul */
			for (a = 0; a < 3; a++) {
				int b;
				for (b = 0; b <= CM_TABLE; b++) {
					cumap->cm[a].table[b].y = curvemap_evaluateF(cumap->cm + 3, cumap->cm[a].table[b].y);
				}
			}
			
			cumap->flag |= CUMA_PREMULLED;
		}
	}
}

static int sort_curvepoints(const void *a1, const void *a2)
{
	const struct CurveMapPoint *x1 = a1, *x2 = a2;
	
	if (x1->x > x2->x) return 1;
	else if (x1->x < x2->x) return -1;
	return 0;
}

/* ************************ more CurveMapping calls *************** */

/* note; only does current curvemap! */
void curvemapping_changed(CurveMapping *cumap, int rem_doubles)
{
	CurveMap *cuma = cumap->cm + cumap->cur;
	CurveMapPoint *cmp = cuma->curve;
	rctf *clipr = &cumap->clipr;
	float thresh = 0.01f * (clipr->xmax - clipr->xmin);
	float dx = 0.0f, dy = 0.0f;
	int a;

	cumap->changed_timestamp++;

	/* clamp with clip */
	if (cumap->flag & CUMA_DO_CLIP) {
		for (a = 0; a < cuma->totpoint; a++) {
			if (cmp[a].flag & CUMA_SELECT) {
				if (cmp[a].x < clipr->xmin)
					dx = MIN2(dx, cmp[a].x - clipr->xmin);
				else if (cmp[a].x > clipr->xmax)
					dx = MAX2(dx, cmp[a].x - clipr->xmax);
				if (cmp[a].y < clipr->ymin)
					dy = MIN2(dy, cmp[a].y - clipr->ymin);
				else if (cmp[a].y > clipr->ymax)
					dy = MAX2(dy, cmp[a].y - clipr->ymax);
			}
		}
		for (a = 0; a < cuma->totpoint; a++) {
			if (cmp[a].flag & CUMA_SELECT) {
				cmp[a].x -= dx;
				cmp[a].y -= dy;
			}
		}
	}
	
	
	qsort(cmp, cuma->totpoint, sizeof(CurveMapPoint), sort_curvepoints);
	
	/* remove doubles, threshold set on 1% of default range */
	if (rem_doubles && cuma->totpoint > 2) {
		for (a = 0; a < cuma->totpoint - 1; a++) {
			dx = cmp[a].x - cmp[a + 1].x;
			dy = cmp[a].y - cmp[a + 1].y;
			if (sqrtf(dx * dx + dy * dy) < thresh) {
				if (a == 0) {
					cmp[a + 1].flag |= 2;
					if (cmp[a + 1].flag & CUMA_SELECT)
						cmp[a].flag |= CUMA_SELECT;
				}
				else {
					cmp[a].flag |= 2;
					if (cmp[a].flag & CUMA_SELECT)
						cmp[a + 1].flag |= CUMA_SELECT;
				}
				break;  /* we assume 1 deletion per edit is ok */
			}
		}
		if (a != cuma->totpoint - 1)
			curvemap_remove(cuma, 2);
	}	
	curvemap_make_table(cuma, clipr);
}

void curvemapping_changed_all(CurveMapping *cumap)
{
	int a, cur = cumap->cur;

	for (a = 0; a < CM_TOT; a++) {
		if (cumap->cm[a].curve) {
			cumap->cur = a;
			curvemapping_changed(cumap, 0);
		}
	}

	cumap->cur = cur;
}

/* table should be verified */
float curvemap_evaluateF(CurveMap *cuma, float value)
{
	float fi;
	int i;

	/* index in table */
	fi = (value - cuma->mintable) * cuma->range;
	i = (int)fi;
	
	/* fi is table float index and should check against table range i.e. [0.0 CM_TABLE] */
	if (fi < 0.0f || fi > CM_TABLE)
		return curvemap_calc_extend(cuma, value, &cuma->table[0].x, &cuma->table[CM_TABLE].x);
	else {
		if (i < 0) return cuma->table[0].y;
		if (i >= CM_TABLE) return cuma->table[CM_TABLE].y;
		
		fi = fi - (float)i;
		return (1.0f - fi) * cuma->table[i].y + (fi) * cuma->table[i + 1].y;
	}
}

/* works with curve 'cur' */
float curvemapping_evaluateF(CurveMapping *cumap, int cur, float value)
{
	CurveMap *cuma = cumap->cm + cur;
	
	/* allocate or bail out */
	if (cuma->table == NULL) {
		curvemap_make_table(cuma, &cumap->clipr);
		if (cuma->table == NULL)
			return 1.0f - value;
	}
	return curvemap_evaluateF(cuma, value);
}

/* vector case */
void curvemapping_evaluate3F(CurveMapping *cumap, float vecout[3], const float vecin[3])
{
	vecout[0] = curvemapping_evaluateF(cumap, 0, vecin[0]);
	vecout[1] = curvemapping_evaluateF(cumap, 1, vecin[1]);
	vecout[2] = curvemapping_evaluateF(cumap, 2, vecin[2]);
}

/* RGB case, no black/white points, no premult */
void curvemapping_evaluateRGBF(CurveMapping *cumap, float vecout[3], const float vecin[3])
{
	vecout[0] = curvemapping_evaluateF(cumap, 0, curvemapping_evaluateF(cumap, 3, vecin[0]));
	vecout[1] = curvemapping_evaluateF(cumap, 1, curvemapping_evaluateF(cumap, 3, vecin[1]));
	vecout[2] = curvemapping_evaluateF(cumap, 2, curvemapping_evaluateF(cumap, 3, vecin[2]));
}


/* RGB with black/white points and premult. tables are checked */
void curvemapping_evaluate_premulRGBF(CurveMapping *cumap, float vecout[3], const float vecin[3])
{
	float fac;
	
	fac = (vecin[0] - cumap->black[0]) * cumap->bwmul[0];
	vecout[0] = curvemap_evaluateF(cumap->cm, fac);
	
	fac = (vecin[1] - cumap->black[1]) * cumap->bwmul[1];
	vecout[1] = curvemap_evaluateF(cumap->cm + 1, fac);
	
	fac = (vecin[2] - cumap->black[2]) * cumap->bwmul[2];
	vecout[2] = curvemap_evaluateF(cumap->cm + 2, fac);
}


/* only used for image editor curves */
void curvemapping_do_ibuf(CurveMapping *cumap, ImBuf *ibuf)
{
	ImBuf *tmpbuf;
	int pixel;
	float *pix_in;
	float col[3];
	int stride = 4;
	float *pix_out;
	
	if (ibuf == NULL)
		return;
	if (ibuf->rect_float == NULL)
		IMB_float_from_rect(ibuf);
	else if (ibuf->rect == NULL)
		imb_addrectImBuf(ibuf);
	
	if (!ibuf->rect || !ibuf->rect_float)
		return;
	
	/* work on a temp buffer, so can color manage afterwards.
	 * No worse off memory wise than comp nodes */
	tmpbuf = IMB_dupImBuf(ibuf);
	
	curvemapping_premultiply(cumap, 0);
	
	pix_in = ibuf->rect_float;
	pix_out = tmpbuf->rect_float;

	if (ibuf->channels)
		stride = ibuf->channels;
	
	for (pixel = ibuf->x * ibuf->y; pixel > 0; pixel--, pix_in += stride, pix_out += stride) {
		if (stride < 3) {
			col[0] = curvemap_evaluateF(cumap->cm, *pix_in);
			
			pix_out[1] = pix_out[2] = pix_out[3] = pix_out[0] = col[0];
		}
		else {
			curvemapping_evaluate_premulRGBF(cumap, col, pix_in);
			pix_out[0] = col[0];
			pix_out[1] = col[1];
			pix_out[2] = col[2];
			if (stride > 3)
				pix_out[3] = pix_in[3];
			else
				pix_out[3] = 1.f;
		}
	}
	
	IMB_rect_from_float(tmpbuf);
	SWAP(unsigned int *, tmpbuf->rect, ibuf->rect);
	IMB_freeImBuf(tmpbuf);
	
	curvemapping_premultiply(cumap, 1);
}

int curvemapping_RGBA_does_something(CurveMapping *cumap)
{
	int a;
	
	if (cumap->black[0] != 0.0f) return 1;
	if (cumap->black[1] != 0.0f) return 1;
	if (cumap->black[2] != 0.0f) return 1;
	if (cumap->white[0] != 1.0f) return 1;
	if (cumap->white[1] != 1.0f) return 1;
	if (cumap->white[2] != 1.0f) return 1;
	
	for (a = 0; a < CM_TOT; a++) {
		if (cumap->cm[a].curve) {
			if (cumap->cm[a].totpoint != 2) return 1;
			
			if (cumap->cm[a].curve[0].x != 0.0f) return 1;
			if (cumap->cm[a].curve[0].y != 0.0f) return 1;
			if (cumap->cm[a].curve[1].x != 1.0f) return 1;
			if (cumap->cm[a].curve[1].y != 1.0f) return 1;
		}
	}
	return 0;
}

void curvemapping_initialize(CurveMapping *cumap)
{
	int a;
	
	if (cumap == NULL) return;
	
	for (a = 0; a < CM_TOT; a++) {
		if (cumap->cm[a].table == NULL)
			curvemap_make_table(cumap->cm + a, &cumap->clipr);
	}
}

void curvemapping_table_RGBA(CurveMapping *cumap, float **array, int *size)
{
	int a;
	
	*size = CM_TABLE + 1;
	*array = MEM_callocN(sizeof(float) * (*size) * 4, "CurveMapping");
	curvemapping_initialize(cumap);

	for (a = 0; a < *size; a++) {
		if (cumap->cm[0].table)
			(*array)[a * 4 + 0] = cumap->cm[0].table[a].y;
		if (cumap->cm[1].table)
			(*array)[a * 4 + 1] = cumap->cm[1].table[a].y;
		if (cumap->cm[2].table)
			(*array)[a * 4 + 2] = cumap->cm[2].table[a].y;
		if (cumap->cm[3].table)
			(*array)[a * 4 + 3] = cumap->cm[3].table[a].y;
	}
}

/* ***************** Histogram **************** */

#define INV_255     (1.f / 255.f)

DO_INLINE int get_bin_float(float f)
{
	int bin = (int)((f * 255.0f) + 0.5f);  /* 0.5 to prevent quantisation differences */

	/* note: clamp integer instead of float to avoid problems with NaN */
	CLAMP(bin, 0, 255);

	return bin;
}

static void save_sample_line(Scopes *scopes, const int idx, const float fx, const float rgb[3], const float ycc[3])
{
	float yuv[3];

	/* vectorscope*/
	rgb_to_yuv(rgb[0], rgb[1], rgb[2], &yuv[0], &yuv[1], &yuv[2]);
	scopes->vecscope[idx + 0] = yuv[1];
	scopes->vecscope[idx + 1] = yuv[2];

	/* waveform */
	switch (scopes->wavefrm_mode) {
		case SCOPES_WAVEFRM_RGB:
			scopes->waveform_1[idx + 0] = fx;
			scopes->waveform_1[idx + 1] = rgb[0];
			scopes->waveform_2[idx + 0] = fx;
			scopes->waveform_2[idx + 1] = rgb[1];
			scopes->waveform_3[idx + 0] = fx;
			scopes->waveform_3[idx + 1] = rgb[2];
			break;
		case SCOPES_WAVEFRM_LUMA:
			scopes->waveform_1[idx + 0] = fx;
			scopes->waveform_1[idx + 1] = ycc[0];
			break;
		case SCOPES_WAVEFRM_YCC_JPEG:
		case SCOPES_WAVEFRM_YCC_709:
		case SCOPES_WAVEFRM_YCC_601:
			scopes->waveform_1[idx + 0] = fx;
			scopes->waveform_1[idx + 1] = ycc[0];
			scopes->waveform_2[idx + 0] = fx;
			scopes->waveform_2[idx + 1] = ycc[1];
			scopes->waveform_3[idx + 0] = fx;
			scopes->waveform_3[idx + 1] = ycc[2];
			break;
	}
}

void BKE_histogram_update_sample_line(Histogram *hist, ImBuf *ibuf, const short use_color_management)
{
	int i, x, y;
	float *fp;
	float rgb[3];
	unsigned char *cp;

	int x1 = 0.5f + hist->co[0][0] * ibuf->x;
	int x2 = 0.5f + hist->co[1][0] * ibuf->x;
	int y1 = 0.5f + hist->co[0][1] * ibuf->y;
	int y2 = 0.5f + hist->co[1][1] * ibuf->y;

	hist->channels = 3;
	hist->x_resolution = 256;
	hist->xmax = 1.0f;
	hist->ymax = 1.0f;

	if (ibuf->rect == NULL && ibuf->rect_float == NULL) return;

	/* persistent draw */
	hist->flag |= HISTO_FLAG_SAMPLELINE; /* keep drawing the flag after */

	for (i = 0; i < 256; i++) {
		x = (int)(0.5f + x1 + (float)i * (x2 - x1) / 255.0f);
		y = (int)(0.5f + y1 + (float)i * (y2 - y1) / 255.0f);

		if (x < 0 || y < 0 || x >= ibuf->x || y >= ibuf->y) {
			hist->data_luma[i] = hist->data_r[i] = hist->data_g[i] = hist->data_b[i] = hist->data_a[i] = 0.0f;
		}
		else {
			if (ibuf->rect_float) {
				fp = (ibuf->rect_float + (ibuf->channels) * (y * ibuf->x + x));

				if (use_color_management)
					linearrgb_to_srgb_v3_v3(rgb, fp);
				else
					copy_v3_v3(rgb, fp);

				hist->data_luma[i]  = rgb_to_luma(rgb);
				hist->data_r[i]     = rgb[0];
				hist->data_g[i]     = rgb[1];
				hist->data_b[i]     = rgb[2];
				hist->data_a[i]     = fp[3];
			}
			else if (ibuf->rect) {
				cp = (unsigned char *)(ibuf->rect + y * ibuf->x + x);
				hist->data_luma[i]  = (float)rgb_to_luma_byte(cp) / 255.0f;
				hist->data_r[i]     = (float)cp[0] / 255.0f;
				hist->data_g[i]     = (float)cp[1] / 255.0f;
				hist->data_b[i]     = (float)cp[2] / 255.0f;
				hist->data_a[i]     = (float)cp[3] / 255.0f;
			}
		}
	}
}

void scopes_update(Scopes *scopes, ImBuf *ibuf, int use_color_management)
{
	int x, y, c;
	unsigned int n, nl;
	double div, divl;
	float *rf = NULL;
	unsigned char *rc = NULL;
	unsigned int *bin_lum, *bin_r, *bin_g, *bin_b, *bin_a;
	int savedlines, saveline;
	float rgba[4], ycc[3], luma;
	int ycc_mode = -1;
	const short is_float = (ibuf->rect_float != NULL);

	if (ibuf->rect == NULL && ibuf->rect_float == NULL) return;

	if (scopes->ok == 1) return;

	if (scopes->hist.ymax == 0.f) scopes->hist.ymax = 1.f;

	/* hmmmm */
	if (!(ELEM(ibuf->channels, 3, 4))) return;

	scopes->hist.channels = 3;
	scopes->hist.x_resolution = 256;

	switch (scopes->wavefrm_mode) {
		case SCOPES_WAVEFRM_RGB:
			ycc_mode = -1;
			break;
		case SCOPES_WAVEFRM_LUMA:
		case SCOPES_WAVEFRM_YCC_JPEG:
			ycc_mode = BLI_YCC_JFIF_0_255;
			break;
		case SCOPES_WAVEFRM_YCC_601:
			ycc_mode = BLI_YCC_ITU_BT601;
			break;
		case SCOPES_WAVEFRM_YCC_709:
			ycc_mode = BLI_YCC_ITU_BT709;
			break;
	}

	/* temp table to count pix value for histogram */
	bin_r     = MEM_callocN(256 * sizeof(unsigned int), "temp historgram bins");
	bin_g     = MEM_callocN(256 * sizeof(unsigned int), "temp historgram bins");
	bin_b     = MEM_callocN(256 * sizeof(unsigned int), "temp historgram bins");
	bin_a = MEM_callocN(256 * sizeof(unsigned int), "temp historgram bins");
	bin_lum   = MEM_callocN(256 * sizeof(unsigned int), "temp historgram bins");

	/* convert to number of lines with logarithmic scale */
	scopes->sample_lines = (scopes->accuracy * 0.01f) * (scopes->accuracy * 0.01f) * ibuf->y;
	
	if (scopes->sample_full)
		scopes->sample_lines = ibuf->y;

	/* scan the image */
	savedlines = 0;
	for (c = 0; c < 3; c++) {
		scopes->minmax[c][0] = 25500.0f;
		scopes->minmax[c][1] = -25500.0f;
	}
	
	scopes->waveform_tot = ibuf->x * scopes->sample_lines;
	
	if (scopes->waveform_1)
		MEM_freeN(scopes->waveform_1);
	if (scopes->waveform_2)
		MEM_freeN(scopes->waveform_2);
	if (scopes->waveform_3)
		MEM_freeN(scopes->waveform_3);
	if (scopes->vecscope)
		MEM_freeN(scopes->vecscope);
	
	scopes->waveform_1 = MEM_callocN(scopes->waveform_tot * 2 * sizeof(float), "waveform point channel 1");
	scopes->waveform_2 = MEM_callocN(scopes->waveform_tot * 2 * sizeof(float), "waveform point channel 2");
	scopes->waveform_3 = MEM_callocN(scopes->waveform_tot * 2 * sizeof(float), "waveform point channel 3");
	scopes->vecscope = MEM_callocN(scopes->waveform_tot * 2 * sizeof(float), "vectorscope point channel");
	
	if (is_float)
		rf = ibuf->rect_float;
	else
		rc = (unsigned char *)ibuf->rect;

	for (y = 0; y < ibuf->y; y++) {
		if (savedlines < scopes->sample_lines && y >= ((savedlines) * ibuf->y) / (scopes->sample_lines + 1)) {
			saveline = 1;
		}
		else {
			saveline = 0;
		}
		for (x = 0; x < ibuf->x; x++) {

			if (is_float) {
				if (use_color_management)
					linearrgb_to_srgb_v3_v3(rgba, rf);
				else
					copy_v3_v3(rgba, rf);
				rgba[3] = rf[3];
			}
			else {
				for (c = 0; c < 4; c++)
					rgba[c] = rc[c] * INV_255;
			}

			/* we still need luma for histogram */
			luma = rgb_to_luma(rgba);

			/* check for min max */
			if (ycc_mode == -1) {
				for (c = 0; c < 3; c++) {
					if (rgba[c] < scopes->minmax[c][0]) scopes->minmax[c][0] = rgba[c];
					if (rgba[c] > scopes->minmax[c][1]) scopes->minmax[c][1] = rgba[c];
				}
			}
			else {
				rgb_to_ycc(rgba[0], rgba[1], rgba[2], &ycc[0], &ycc[1], &ycc[2], ycc_mode);
				for (c = 0; c < 3; c++) {
					ycc[c] *= INV_255;
					if (ycc[c] < scopes->minmax[c][0]) scopes->minmax[c][0] = ycc[c];
					if (ycc[c] > scopes->minmax[c][1]) scopes->minmax[c][1] = ycc[c];
				}
			}
			/* increment count for histo*/
			bin_lum[get_bin_float(luma)] += 1;
			bin_r[get_bin_float(rgba[0])] += 1;
			bin_g[get_bin_float(rgba[1])] += 1;
			bin_b[get_bin_float(rgba[2])] += 1;
			bin_a[get_bin_float(rgba[3])] += 1;

			/* save sample if needed */
			if (saveline) {
				const float fx = (float)x / (float)ibuf->x;
				const int idx = 2 * (ibuf->x * savedlines + x);
				save_sample_line(scopes, idx, fx, rgba, ycc);
			}

			rf += ibuf->channels;
			rc += ibuf->channels;
		}
		if (saveline)
			savedlines += 1;
	}

	/* convert hist data to float (proportional to max count) */
	n = 0;
	nl = 0;
	for (x = 0; x < 256; x++) {
		if (bin_lum[x] > nl) nl = bin_lum[x];
		if (bin_r[x]   > n) n = bin_r[x];
		if (bin_g[x]   > n) n = bin_g[x];
		if (bin_b[x]   > n) n = bin_b[x];
		if (bin_a[x]   > n) n = bin_a[x];
	}
	div = 1.0 / (double)n;
	divl = 1.0 / (double)nl;
	for (x = 0; x < 256; x++) {
		scopes->hist.data_luma[x] = bin_lum[x] * divl;
		scopes->hist.data_r[x] = bin_r[x] * div;
		scopes->hist.data_g[x] = bin_g[x] * div;
		scopes->hist.data_b[x] = bin_b[x] * div;
		scopes->hist.data_a[x] = bin_a[x] * div;
	}
	MEM_freeN(bin_lum);
	MEM_freeN(bin_r);
	MEM_freeN(bin_g);
	MEM_freeN(bin_b);
	MEM_freeN(bin_a);

	scopes->ok = 1;
}

void scopes_free(Scopes *scopes)
{
	if (scopes->waveform_1) {
		MEM_freeN(scopes->waveform_1);
		scopes->waveform_1 = NULL;
	}
	if (scopes->waveform_2) {
		MEM_freeN(scopes->waveform_2);
		scopes->waveform_2 = NULL;
	}
	if (scopes->waveform_3) {
		MEM_freeN(scopes->waveform_3);
		scopes->waveform_3 = NULL;
	}
	if (scopes->vecscope) {
		MEM_freeN(scopes->vecscope);
		scopes->vecscope = NULL;
	}
}

void scopes_new(Scopes *scopes)
{
	scopes->accuracy = 30.0;
	scopes->hist.mode = HISTO_MODE_RGB;
	scopes->wavefrm_alpha = 0.3;
	scopes->vecscope_alpha = 0.3;
	scopes->wavefrm_height = 100;
	scopes->vecscope_height = 100;
	scopes->hist.height = 100;
	scopes->ok = 0;
	scopes->waveform_1 = NULL;
	scopes->waveform_2 = NULL;
	scopes->waveform_3 = NULL;
	scopes->vecscope = NULL;
}

void BKE_color_managed_display_settings_init(ColorManagedDisplaySettings *settings)
{
	const char *display_name = IMB_colormanagement_display_get_default_name();

	BLI_strncpy(settings->display_device, display_name, sizeof(settings->display_device));
}

void BKE_color_managed_display_settings_copy(ColorManagedDisplaySettings *new_settings,
                                             const ColorManagedDisplaySettings *settings)
{
	BLI_strncpy(new_settings->display_device, settings->display_device, sizeof(new_settings->display_device));
}

void BKE_color_managed_view_settings_init(ColorManagedViewSettings *settings)
{
	/* OCIO_TODO: use default view transform here when OCIO is completely integrated
	*             and proper versioning stuff is added.
	*             for now use NONE to be compatible with all current files
	*/
	BLI_strncpy(settings->view_transform, "NONE", sizeof(settings->view_transform));

	settings->gamma = 1.0f;
	settings->exposure = 0.0f;

	settings->flag |= COLORMANAGE_VIEW_USE_GLOBAL;
}

void BKE_color_managed_view_settings_copy(ColorManagedViewSettings *new_settings,
                                          const ColorManagedViewSettings *settings)
{
	BLI_strncpy(new_settings->view_transform, settings->view_transform, sizeof(new_settings->view_transform));

	new_settings->exposure = settings->exposure;
	new_settings->gamma = settings->gamma;
}
