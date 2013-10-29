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
 * Contributor(s): Joseph Eagar, Geoffrey Bantle, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/intern/bmesh_polygon.c
 *  \ingroup bmesh
 *
 * This file contains code for dealing
 * with polygons (normal/area calculation,
 * tessellation, etc)
 */

#include "DNA_listBase.h"
#include "DNA_modifier_types.h"

#include "BLI_alloca.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_scanfill.h"
#include "BLI_listbase.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "intern/bmesh_private.h"

/**
 * \brief TEST EDGE SIDE and POINT IN TRIANGLE
 *
 * Point in triangle tests stolen from scanfill.c.
 * Used for tessellator
 */

static bool testedgesidef(const float v1[2], const float v2[2], const float v3[2])
{
	/* is v3 to the right of v1 - v2 ? With exception: v3 == v1 || v3 == v2 */
	double inp;

	//inp = (v2[cox] - v1[cox]) * (v1[coy] - v3[coy]) + (v1[coy] - v2[coy]) * (v1[cox] - v3[cox]);
	inp = (v2[0] - v1[0]) * (v1[1] - v3[1]) + (v1[1] - v2[1]) * (v1[0] - v3[0]);

	if (inp < 0.0) {
		return false;
	}
	else if (inp == 0) {
		if (v1[0] == v3[0] && v1[1] == v3[1]) return false;
		if (v2[0] == v3[0] && v2[1] == v3[1]) return false;
	}
	return true;
}

/**
 * \brief COMPUTE POLY NORMAL
 *
 * Computes the normal of a planar
 * polygon See Graphics Gems for
 * computing newell normal.
 */
static void calc_poly_normal(float normal[3], float verts[][3], int nverts)
{
	float const *v_prev = verts[nverts - 1];
	float const *v_curr = verts[0];
	float n[3] = {0.0f};
	int i;

	/* Newell's Method */
	for (i = 0; i < nverts; v_prev = v_curr, v_curr = verts[++i]) {
		add_newell_cross_v3_v3v3(n, v_prev, v_curr);
	}

	if (UNLIKELY(normalize_v3_v3(normal, n) == 0.0f)) {
		normal[2] = 1.0f; /* other axis set to 0.0 */
	}
}

/**
 * \brief COMPUTE POLY NORMAL (BMFace)
 *
 * Same as #calc_poly_normal but operates directly on a bmesh face.
 */
static void bm_face_calc_poly_normal(const BMFace *f, float n[3])
{
	BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
	BMLoop *l_iter  = l_first;
	float const *v_prev = l_first->prev->v->co;
	float const *v_curr = l_first->v->co;

	zero_v3(n);

	/* Newell's Method */
	do {
		add_newell_cross_v3_v3v3(n, v_prev, v_curr);

		l_iter = l_iter->next;
		v_prev = v_curr;
		v_curr = l_iter->v->co;

	} while (l_iter != l_first);

	if (UNLIKELY(normalize_v3(n) == 0.0f)) {
		n[2] = 1.0f;
	}
}

/**
 * \brief COMPUTE POLY NORMAL (BMFace)
 *
 * Same as #calc_poly_normal and #bm_face_calc_poly_normal
 * but takes an array of vertex locations.
 */
static void bm_face_calc_poly_normal_vertex_cos(BMFace *f, float r_no[3],
                                                float const (*vertexCos)[3])
{
	BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
	BMLoop *l_iter  = l_first;
	float const *v_prev = vertexCos[BM_elem_index_get(l_first->prev->v)];
	float const *v_curr = vertexCos[BM_elem_index_get(l_first->v)];

	zero_v3(r_no);

	/* Newell's Method */
	do {
		add_newell_cross_v3_v3v3(r_no, v_prev, v_curr);

		l_iter = l_iter->next;
		v_prev = v_curr;
		v_curr = vertexCos[BM_elem_index_get(l_iter->v)];
	} while (l_iter != l_first);

	if (UNLIKELY(normalize_v3(r_no) == 0.0f)) {
		r_no[2] = 1.0f; /* other axis set to 0.0 */
	}
}

/**
 * \brief COMPUTE POLY CENTER (BMFace)
 */
static void bm_face_calc_poly_center_mean_vertex_cos(BMFace *f, float r_cent[3],
                                                     float const (*vertexCos)[3])
{
	BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
	BMLoop *l_iter  = l_first;

	zero_v3(r_cent);

	/* Newell's Method */
	do {
		add_v3_v3(r_cent, vertexCos[BM_elem_index_get(l_iter->v)]);
	} while ((l_iter = l_iter->next) != l_first);
	mul_v3_fl(r_cent, 1.0f / f->len);
}

/**
 * For tools that insist on using triangles, ideally we would cache this data.
 *
 * \param r_loops  Store face loop pointers, (f->len)
 * \param r_index  Store triangle triples, indicies into \a r_loops,  ((f->len - 2) * 3)
 */
int BM_face_calc_tessellation(const BMFace *f, BMLoop **r_loops, int (*_r_index)[3])
{
	int *r_index = (int *)_r_index;
	BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
	BMLoop *l_iter;
	int totfilltri;

	if (f->len == 3) {
		*r_loops++ = (l_iter = l_first);
		*r_loops++ = (l_iter = l_iter->next);
		*r_loops++ = (         l_iter->next);

		r_index[0] = 0;
		r_index[1] = 1;
		r_index[2] = 2;
		totfilltri = 1;
	}
	else if (f->len == 4) {
		*r_loops++ = (l_iter = l_first);
		*r_loops++ = (l_iter = l_iter->next);
		*r_loops++ = (l_iter = l_iter->next);
		*r_loops++ = (         l_iter->next);

		r_index[0] = 0;
		r_index[1] = 1;
		r_index[2] = 2;

		r_index[3] = 0;
		r_index[4] = 2;
		r_index[5] = 3;
		totfilltri = 2;
	}
	else {
		int j;

		ScanFillContext sf_ctx;
		ScanFillVert *sf_vert, *sf_vert_last = NULL, *sf_vert_first = NULL;
		/* ScanFillEdge *e; */ /* UNUSED */
		ScanFillFace *sf_tri;

		BLI_scanfill_begin(&sf_ctx);

		j = 0;
		l_iter = l_first;
		do {
			sf_vert = BLI_scanfill_vert_add(&sf_ctx, l_iter->v->co);
			sf_vert->tmp.p = l_iter;

			if (sf_vert_last) {
				/* e = */ BLI_scanfill_edge_add(&sf_ctx, sf_vert_last, sf_vert);
			}

			sf_vert_last = sf_vert;
			if (sf_vert_first == NULL) {
				sf_vert_first = sf_vert;
			}

			r_loops[j] = l_iter;

			/* mark order */
			BM_elem_index_set(l_iter, j++); /* set_loop */

		} while ((l_iter = l_iter->next) != l_first);

		/* complete the loop */
		BLI_scanfill_edge_add(&sf_ctx, sf_vert_first, sf_vert);

		totfilltri = BLI_scanfill_calc_ex(&sf_ctx, 0, f->no);
		BLI_assert(totfilltri <= f->len - 2);
		BLI_assert(totfilltri == BLI_countlist(&sf_ctx.fillfacebase));

		for (sf_tri = sf_ctx.fillfacebase.first; sf_tri; sf_tri = sf_tri->next) {
			int i1 = BM_elem_index_get((BMLoop *)sf_tri->v1->tmp.p);
			int i2 = BM_elem_index_get((BMLoop *)sf_tri->v2->tmp.p);
			int i3 = BM_elem_index_get((BMLoop *)sf_tri->v3->tmp.p);

			if (i1 > i2) { SWAP(int, i1, i2); }
			if (i2 > i3) { SWAP(int, i2, i3); }
			if (i1 > i2) { SWAP(int, i1, i2); }

			*r_index++ = i1;
			*r_index++ = i2;
			*r_index++ = i3;
		}

		BLI_scanfill_end(&sf_ctx);
	}

	return totfilltri;
}

/**
 * get the area of the face
 */
float BM_face_calc_area(BMFace *f)
{
	BMLoop *l;
	BMIter iter;
	float (*verts)[3] = BLI_array_alloca(verts, f->len);
	float area;
	int i;

	BM_ITER_ELEM_INDEX (l, &iter, f, BM_LOOPS_OF_FACE, i) {
		copy_v3_v3(verts[i], l->v->co);
	}

	if (f->len == 3) {
		area = area_tri_v3(verts[0], verts[1], verts[2]);
	}
	else if (f->len == 4) {
		area = area_quad_v3(verts[0], verts[1], verts[2], verts[3]);
	}
	else {
		float normal[3];
		calc_poly_normal(normal, verts, f->len);
		area = area_poly_v3(f->len, verts, normal);
	}

	return area;
}

/**
 * compute the perimeter of an ngon
 */
float BM_face_calc_perimeter(BMFace *f)
{
	BMLoop *l_iter, *l_first;
	float perimeter = 0.0f;

	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		perimeter += len_v3v3(l_iter->v->co, l_iter->next->v->co);
	} while ((l_iter = l_iter->next) != l_first);

	return perimeter;
}

/**
 * Compute a meaningful direction along the face (use for manipulator axis).
 * \note result isnt normalized.
 */
void BM_face_calc_plane(BMFace *f, float r_plane[3])
{
	if (f->len == 3) {
		BMVert *verts[3];
		float lens[3];
		float difs[3];
		int  order[3] = {0, 1, 2};

		BM_face_as_array_vert_tri(f, verts);

		lens[0] = len_v3v3(verts[0]->co, verts[1]->co);
		lens[1] = len_v3v3(verts[1]->co, verts[2]->co);
		lens[2] = len_v3v3(verts[2]->co, verts[0]->co);

		/* find the shortest or the longest loop */
		difs[0] = fabsf(lens[1] - lens[2]);
		difs[1] = fabsf(lens[2] - lens[0]);
		difs[2] = fabsf(lens[0] - lens[1]);

		axis_sort_v3(difs, order);
		sub_v3_v3v3(r_plane, verts[order[0]]->co, verts[(order[0] + 1) % 3]->co);
	}
	else if (f->len == 4) {
		BMVert *verts[4];
		float vec[3], vec_a[3], vec_b[3];

		// BM_iter_as_array(NULL, BM_VERTS_OF_FACE, efa, (void **)verts, 4);
		BM_face_as_array_vert_quad(f, verts);

		sub_v3_v3v3(vec_a, verts[3]->co, verts[2]->co);
		sub_v3_v3v3(vec_b, verts[0]->co, verts[1]->co);
		add_v3_v3v3(r_plane, vec_a, vec_b);

		sub_v3_v3v3(vec_a, verts[0]->co, verts[3]->co);
		sub_v3_v3v3(vec_b, verts[1]->co, verts[2]->co);
		add_v3_v3v3(vec, vec_a, vec_b);
		/* use the biggest edge length */
		if (dot_v3v3(r_plane, r_plane) < dot_v3v3(vec, vec)) {
			copy_v3_v3(r_plane, vec);
		}
	}
	else {
		BMLoop *l_long  = BM_face_find_longest_loop(f);

		sub_v3_v3v3(r_plane, l_long->v->co, l_long->next->v->co);
	}

	normalize_v3(r_plane);
}

/**
 * computes center of face in 3d.  uses center of bounding box.
 */
void BM_face_calc_center_bounds(BMFace *f, float r_cent[3])
{
	BMLoop *l_iter;
	BMLoop *l_first;
	float min[3], max[3];

	INIT_MINMAX(min, max);

	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		minmax_v3v3_v3(min, max, l_iter->v->co);
	} while ((l_iter = l_iter->next) != l_first);

	mid_v3_v3v3(r_cent, min, max);
}

/**
 * computes the center of a face, using the mean average
 */
void BM_face_calc_center_mean(BMFace *f, float r_cent[3])
{
	BMLoop *l_iter, *l_first;

	zero_v3(r_cent);

	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		add_v3_v3(r_cent, l_iter->v->co);
	} while ((l_iter = l_iter->next) != l_first);
	mul_v3_fl(r_cent, 1.0f / (float) f->len);
}

/**
 * computes the center of a face, using the mean average
 * weighted by edge length
 */
void BM_face_calc_center_mean_weighted(BMFace *f, float r_cent[3])
{
	BMLoop *l_iter;
	BMLoop *l_first;
	float totw = 0.0f;
	float w_prev;

	zero_v3(r_cent);


	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	w_prev = BM_edge_calc_length(l_iter->prev->e);
	do {
		const float w_curr = BM_edge_calc_length(l_iter->e);
		const float w = (w_curr + w_prev);
		madd_v3_v3fl(r_cent, l_iter->v->co, w);
		totw += w;
		w_prev = w_curr;
	} while ((l_iter = l_iter->next) != l_first);

	if (totw != 0.0f)
		mul_v3_fl(r_cent, 1.0f / (float) totw);
}

/**
 * COMPUTE POLY PLANE
 *
 * Projects a set polygon's vertices to
 * a plane defined by the average
 * of its edges cross products
 */
void calc_poly_plane(float (*verts)[3], const int nverts)
{
	
	float avgc[3], norm[3], mag, avgn[3];
	float *v1, *v2, *v3;
	int i;
	
	if (nverts < 3)
		return;

	zero_v3(avgn);
	zero_v3(avgc);

	for (i = 0; i < nverts; i++) {
		v1 = verts[i];
		v2 = verts[(i + 1) % nverts];
		v3 = verts[(i + 2) % nverts];
		normal_tri_v3(norm, v1, v2, v3);

		add_v3_v3(avgn, norm);
	}

	if (UNLIKELY(normalize_v3(avgn) == 0.0f)) {
		avgn[2] = 1.0f;
	}
	
	for (i = 0; i < nverts; i++) {
		v1 = verts[i];
		mag = dot_v3v3(v1, avgn);
		madd_v3_v3fl(v1, avgn, -mag);
	}
}

/**
 * \brief BM LEGAL EDGES
 *
 * takes in a face and a list of edges, and sets to NULL any edge in
 * the list that bridges a concave region of the face or intersects
 * any of the faces's edges.
 */
static void scale_edge_v2f(float v1[2], float v2[2], const float fac)
{
	float mid[2];

	mid_v2_v2v2(mid, v1, v2);

	sub_v2_v2v2(v1, v1, mid);
	sub_v2_v2v2(v2, v2, mid);

	mul_v2_fl(v1, fac);
	mul_v2_fl(v2, fac);

	add_v2_v2v2(v1, v1, mid);
	add_v2_v2v2(v2, v2, mid);
}

/**
 * \brief POLY ROTATE PLANE
 *
 * Rotates a polygon so that it's
 * normal is pointing towards the mesh Z axis
 */
void poly_rotate_plane(const float normal[3], float (*verts)[3], const int nverts)
{
	float mat[3][3];

	if (axis_dominant_v3_to_m3(mat, normal)) {
		int i;
		for (i = 0; i < nverts; i++) {
			mul_m3_v3(mat, verts[i]);
		}
	}
}

/**
 * updates face and vertex normals incident on an edge
 */
void BM_edge_normals_update(BMEdge *e)
{
	BMIter iter;
	BMFace *f;
	
	BM_ITER_ELEM (f, &iter, e, BM_FACES_OF_EDGE) {
		BM_face_normal_update(f);
	}

	BM_vert_normal_update(e->v1);
	BM_vert_normal_update(e->v2);
}

/**
 * update a vert normal (but not the faces incident on it)
 */
void BM_vert_normal_update(BMVert *v)
{
	/* TODO, we can normalize each edge only once, then compare with previous edge */

	BMIter liter;
	BMLoop *l;
	float vec1[3], vec2[3], fac;
	int len = 0;

	zero_v3(v->no);

	BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
		/* Same calculation used in BM_mesh_normals_update */
		sub_v3_v3v3(vec1, l->v->co, l->prev->v->co);
		sub_v3_v3v3(vec2, l->next->v->co, l->v->co);
		normalize_v3(vec1);
		normalize_v3(vec2);

		fac = saacos(-dot_v3v3(vec1, vec2));

		madd_v3_v3fl(v->no, l->f->no, fac);

		len++;
	}

	if (len) {
		normalize_v3(v->no);
	}
}

void BM_vert_normal_update_all(BMVert *v)
{
	BMIter iter;
	BMFace *f;

	BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
		BM_face_normal_update(f);
	}

	BM_vert_normal_update(v);
}

/**
 * \brief BMESH UPDATE FACE NORMAL
 *
 * Updates the stored normal for the
 * given face. Requires that a buffer
 * of sufficient length to store projected
 * coordinates for all of the face's vertices
 * is passed in as well.
 */

void BM_face_calc_normal(const BMFace *f, float r_no[3])
{
	BMLoop *l;

	/* common cases first */
	switch (f->len) {
		case 4:
		{
			const float *co1 = (l = BM_FACE_FIRST_LOOP(f))->v->co;
			const float *co2 = (l = l->next)->v->co;
			const float *co3 = (l = l->next)->v->co;
			const float *co4 = (l->next)->v->co;

			normal_quad_v3(r_no, co1, co2, co3, co4);
			break;
		}
		case 3:
		{
			const float *co1 = (l = BM_FACE_FIRST_LOOP(f))->v->co;
			const float *co2 = (l = l->next)->v->co;
			const float *co3 = (l->next)->v->co;

			normal_tri_v3(r_no, co1, co2, co3);
			break;
		}
		default:
		{
			bm_face_calc_poly_normal(f, r_no);
			break;
		}
	}
}
void BM_face_normal_update(BMFace *f)
{
	BM_face_calc_normal(f, f->no);
}

/* exact same as 'BM_face_calc_normal' but accepts vertex coords */
void BM_face_calc_normal_vcos(BMesh *bm, BMFace *f, float r_no[3],
                              float const (*vertexCos)[3])
{
	BMLoop *l;

	/* must have valid index data */
	BLI_assert((bm->elem_index_dirty & BM_VERT) == 0);
	(void)bm;

	/* common cases first */
	switch (f->len) {
		case 4:
		{
			const float *co1 = vertexCos[BM_elem_index_get((l = BM_FACE_FIRST_LOOP(f))->v)];
			const float *co2 = vertexCos[BM_elem_index_get((l = l->next)->v)];
			const float *co3 = vertexCos[BM_elem_index_get((l = l->next)->v)];
			const float *co4 = vertexCos[BM_elem_index_get((l->next)->v)];

			normal_quad_v3(r_no, co1, co2, co3, co4);
			break;
		}
		case 3:
		{
			const float *co1 = vertexCos[BM_elem_index_get((l = BM_FACE_FIRST_LOOP(f))->v)];
			const float *co2 = vertexCos[BM_elem_index_get((l = l->next)->v)];
			const float *co3 = vertexCos[BM_elem_index_get((l->next)->v)];

			normal_tri_v3(r_no, co1, co2, co3);
			break;
		}
		case 0:
		{
			zero_v3(r_no);
			break;
		}
		default:
		{
			bm_face_calc_poly_normal_vertex_cos(f, r_no, vertexCos);
			break;
		}
	}
}

/* exact same as 'BM_face_calc_normal' but accepts vertex coords */
void BM_face_calc_center_mean_vcos(BMesh *bm, BMFace *f, float r_cent[3],
                                   float const (*vertexCos)[3])
{
	/* must have valid index data */
	BLI_assert((bm->elem_index_dirty & BM_VERT) == 0);
	(void)bm;

	bm_face_calc_poly_center_mean_vertex_cos(f, r_cent, vertexCos);
}

/**
 * \brief Face Flip Normal
 *
 * Reverses the winding of a face.
 * \note This updates the calculated normal.
 */
void BM_face_normal_flip(BMesh *bm, BMFace *f)
{
	bmesh_loop_reverse(bm, f);
	negate_v3(f->no);
}

/* detects if two line segments cross each other (intersects).
 * note, there could be more winding cases then there needs to be. */
static bool line_crosses_v2f(const float v1[2], const float v2[2], const float v3[2], const float v4[2])
{

#define GETMIN2_AXIS(a, b, ma, mb, axis)   \
	{                                      \
		ma[axis] = min_ff(a[axis], b[axis]); \
		mb[axis] = max_ff(a[axis], b[axis]); \
	} (void)0

#define GETMIN2(a, b, ma, mb)          \
	{                                  \
		GETMIN2_AXIS(a, b, ma, mb, 0); \
		GETMIN2_AXIS(a, b, ma, mb, 1); \
	} (void)0

#define EPS (FLT_EPSILON * 15)

	int w1, w2, w3, w4, w5 /*, re */;
	float mv1[2], mv2[2], mv3[2], mv4[2];
	
	/* now test winding */
	w1 = testedgesidef(v1, v3, v2);
	w2 = testedgesidef(v2, v4, v1);
	w3 = !testedgesidef(v1, v2, v3);
	w4 = testedgesidef(v3, v2, v4);
	w5 = !testedgesidef(v3, v1, v4);
	
	if (w1 == w2 && w2 == w3 && w3 == w4 && w4 == w5) {
		return true;
	}
	
	GETMIN2(v1, v2, mv1, mv2);
	GETMIN2(v3, v4, mv3, mv4);
	
	/* do an interval test on the x and y axes */
	/* first do x axis */
	if (fabsf(v1[1] - v2[1]) < EPS &&
	    fabsf(v3[1] - v4[1]) < EPS &&
	    fabsf(v1[1] - v3[1]) < EPS)
	{
		return (mv4[0] >= mv1[0] && mv3[0] <= mv2[0]);
	}

	/* now do y axis */
	if (fabsf(v1[0] - v2[0]) < EPS &&
	    fabsf(v3[0] - v4[0]) < EPS &&
	    fabsf(v1[0] - v3[0]) < EPS)
	{
		return (mv4[1] >= mv1[1] && mv3[1] <= mv2[1]);
	}

	return false;

#undef GETMIN2_AXIS
#undef GETMIN2
#undef EPS

}

/**
 *  BM POINT IN FACE
 *
 * Projects co onto face f, and returns true if it is inside
 * the face bounds.
 *
 * \note this uses a best-axis projection test,
 * instead of projecting co directly into f's orientation space,
 * so there might be accuracy issues.
 */
bool BM_face_point_inside_test(BMFace *f, const float co[3])
{
	int ax, ay;
	float co2[2], cent[2] = {0.0f, 0.0f}, out[2] = {FLT_MAX * 0.5f, FLT_MAX * 0.5f};
	BMLoop *l_iter;
	BMLoop *l_first;
	int crosses = 0;
	float onepluseps = 1.0f + (float)FLT_EPSILON * 150.0f;
	
	if (dot_v3v3(f->no, f->no) <= FLT_EPSILON * 10)
		BM_face_normal_update(f);
	
	/* find best projection of face XY, XZ or YZ: barycentric weights of
	 * the 2d projected coords are the same and faster to compute
	 *
	 * this probably isn't all that accurate, but it has the advantage of
	 * being fast (especially compared to projecting into the face orientation)
	 */
	axis_dominant_v3(&ax, &ay, f->no);

	co2[0] = co[ax];
	co2[1] = co[ay];
	
	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		cent[0] += l_iter->v->co[ax];
		cent[1] += l_iter->v->co[ay];
	} while ((l_iter = l_iter->next) != l_first);
	
	mul_v2_fl(cent, 1.0f / (float)f->len);
	
	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		float v1[2], v2[2];
		
		v1[0] = (l_iter->prev->v->co[ax] - cent[0]) * onepluseps + cent[0];
		v1[1] = (l_iter->prev->v->co[ay] - cent[1]) * onepluseps + cent[1];
		
		v2[0] = (l_iter->v->co[ax] - cent[0]) * onepluseps + cent[0];
		v2[1] = (l_iter->v->co[ay] - cent[1]) * onepluseps + cent[1];
		
		crosses += line_crosses_v2f(v1, v2, co2, out) != 0;
	} while ((l_iter = l_iter->next) != l_first);
	
	return crosses % 2 != 0;
}

/**
 * \brief BMESH TRIANGULATE FACE
 *
 * Breaks all quads and ngons down to triangles.
 * It uses scanfill for the ngons splitting, and
 * the beautify operator when use_beauty is true.
 *
 * \param r_faces_new if non-null, must be an array of BMFace pointers,
 * with a length equal to (f->len - 3). It will be filled with the new
 * triangles (not including the original triangle).
 *
 * \note use_tag tags new flags and edges.
 */
void BM_face_triangulate(BMesh *bm, BMFace *f,
                         BMFace **r_faces_new,
                         MemArena *sf_arena,
                         const int quad_method,
                         const int ngon_method,
                         const bool use_tag)
{
	BMLoop *l_iter, *l_first, *l_new;
	BMFace *f_new;
	int orig_f_len = f->len;
	int nf_i = 0;
	BMEdge **edge_array;
	int edge_array_len;
	bool use_beauty = (ngon_method == MOD_TRIANGULATE_NGON_BEAUTY);

#define SF_EDGE_IS_BOUNDARY 0xff

	BLI_assert(BM_face_is_normal_valid(f));


	if (f->len == 4) {
		BMVert *v1, *v2;
		l_first = BM_FACE_FIRST_LOOP(f);

		switch (quad_method) {
			case MOD_TRIANGULATE_QUAD_FIXED:
			{
				v1 = l_first->v;
				v2 = l_first->next->next->v;
				break;
			}
			case MOD_TRIANGULATE_QUAD_ALTERNATE:
			{
				v1 = l_first->next->v;
				v2 = l_first->prev->v;
				break;
			}
			case MOD_TRIANGULATE_QUAD_SHORTEDGE:
			{
				BMVert *v3, *v4;
				float d1, d2;

				v1 = l_first->v;
				v2 = l_first->next->next->v;
				v3 = l_first->next->v;
				v4 = l_first->prev->v;

				d1 = len_squared_v3v3(v1->co, v2->co);
				d2 = len_squared_v3v3(v3->co, v4->co);

				if (d2 < d1) {
					v1 = v3;
					v2 = v4;
				}
				break;
			}
			case MOD_TRIANGULATE_QUAD_BEAUTY:
			default:
			{
				BMVert *v3, *v4;
				float cost;

				v1 = l_first->next->v;
				v2 = l_first->next->next->v;
				v3 = l_first->prev->v;
				v4 = l_first->v;

				cost = BM_verts_calc_rotate_beauty(v1, v2, v3, v4, 0, 0);

				if (cost < 0.0f) {
					v1 = v4;
					//v2 = v2;
				}
				else {
					//v1 = v1;
					v2 = v3;
				}
				break;
			}
		}

		f_new = BM_face_split(bm, f, v1, v2, &l_new, NULL, false);
		copy_v3_v3(f_new->no, f->no);

		if (use_tag) {
			BM_elem_flag_enable(l_new->e, BM_ELEM_TAG);
			BM_elem_flag_enable(f_new, BM_ELEM_TAG);
		}

		if (r_faces_new) {
			r_faces_new[nf_i++] = f_new;
		}
	}
	else if (f->len > 4) {
		/* scanfill */
		ScanFillContext sf_ctx;
		ScanFillVert *sf_vert, *sf_vert_prev = NULL;
		ScanFillEdge *sf_edge;
		ScanFillFace *sf_tri;
		int totfilltri;

		/* populate scanfill */
		BLI_scanfill_begin_arena(&sf_ctx, sf_arena);
		l_iter = l_first = BM_FACE_FIRST_LOOP(f);

		/* step once before entering the loop */
		sf_vert = BLI_scanfill_vert_add(&sf_ctx, l_iter->v->co);
		sf_vert->tmp.p = l_iter;
		sf_vert_prev = sf_vert;
		l_iter = l_iter->next;

		do {
			sf_vert = BLI_scanfill_vert_add(&sf_ctx, l_iter->v->co);
			sf_edge = BLI_scanfill_edge_add(&sf_ctx, sf_vert_prev, sf_vert);
			sf_edge->tmp.c = SF_EDGE_IS_BOUNDARY;

			sf_vert->tmp.p = l_iter;
			sf_vert_prev = sf_vert;
		} while ((l_iter = l_iter->next) != l_first);

		sf_edge = BLI_scanfill_edge_add(&sf_ctx, sf_vert_prev, sf_ctx.fillvertbase.first);
		sf_edge->tmp.c = SF_EDGE_IS_BOUNDARY;

		/* calculate filled triangles */
		totfilltri = BLI_scanfill_calc_ex(&sf_ctx, 0, f->no);
		BLI_assert(totfilltri <= f->len - 2);


		/* loop over calculated triangles and create new geometry */
		for (sf_tri = sf_ctx.fillfacebase.first; sf_tri; sf_tri = sf_tri->next) {
			/* the order is reverse, otherwise the normal is flipped */
			BMLoop *l_tri[3] = {
			    sf_tri->v3->tmp.p,
			    sf_tri->v2->tmp.p,
			    sf_tri->v1->tmp.p};

			BMVert *v_tri[3] = {
			    l_tri[0]->v,
			    l_tri[1]->v,
			    l_tri[2]->v};

			f_new = BM_face_create_verts(bm, v_tri, 3, f, false, true);
			l_new = BM_FACE_FIRST_LOOP(f_new);

			BLI_assert(v_tri[0] == l_new->v);

			/* copy CD data */
			BM_elem_attrs_copy(bm, bm, l_tri[0], l_new);
			BM_elem_attrs_copy(bm, bm, l_tri[1], l_new->next);
			BM_elem_attrs_copy(bm, bm, l_tri[2], l_new->prev);

			/* add all but the last face which is swapped and removed (below) */
			if (sf_tri->next) {
				if (use_tag) {
					BM_elem_flag_enable(f_new, BM_ELEM_TAG);
				}
				if (r_faces_new) {
					r_faces_new[nf_i++] = f_new;
				}
			}
		}

		if (use_beauty || use_tag) {
			ScanFillEdge *sf_edge;
			edge_array = BLI_array_alloca(edge_array, orig_f_len - 3);
			edge_array_len = 0;

			for (sf_edge = sf_ctx.filledgebase.first; sf_edge; sf_edge = sf_edge->next) {
				BMLoop *l1 = sf_edge->v1->tmp.p;
				BMLoop *l2 = sf_edge->v2->tmp.p;

				BMEdge *e = BM_edge_exists(l1->v, l2->v);
				if (sf_edge->tmp.c != SF_EDGE_IS_BOUNDARY) {

					if (use_beauty) {
						BM_elem_index_set(e, edge_array_len);  /* set_dirty */
						edge_array[edge_array_len] = e;
						edge_array_len++;
					}

					if (use_tag) {
						BM_elem_flag_enable(e, BM_ELEM_TAG);
					}
				}
				else if (use_tag) {
					BM_elem_flag_disable(e, BM_ELEM_TAG);
				}
			}

		}

		if ((!use_beauty) || (!r_faces_new)) {
			/* we can't delete the real face, because some of the callers expect it to remain valid.
			 * so swap data and delete the last created tri */
			bmesh_face_swap_data(bm, f, f_new);
			BM_face_kill(bm, f_new);
		}

		if (use_beauty) {
			bm->elem_index_dirty |= BM_EDGE;
			BM_mesh_beautify_fill(bm, edge_array, edge_array_len, 0, 0, 0, 0);

			if (r_faces_new) {
				/* beautify deletes and creates new faces
				 * we need to re-populate the r_faces_new array
				 * with the new faces
				 */
				int i;


#define FACE_USED_TEST(f) (BM_elem_index_get(f) == -2)
#define FACE_USED_SET(f)   BM_elem_index_set(f,    -2)

				nf_i = 0;
				for (i = 0; i < edge_array_len; i++) {
					BMFace *f_a, *f_b;
					BMEdge *e = edge_array[i];
					const bool ok = BM_edge_face_pair(e, &f_a, &f_b);

					BLI_assert(ok);

					if (FACE_USED_TEST(f_a) == false) {
						FACE_USED_SET(f_a);

						if (nf_i < edge_array_len) {
							r_faces_new[nf_i++] = f_a;
						}
						else {
							f_new = f_a;
							break;
						}
					}

					if (FACE_USED_TEST(f_b) == false) {
						FACE_USED_SET(f_b);

						if (nf_i < edge_array_len) {
							r_faces_new[nf_i++] = f_b;
						}
						else {
							f_new = f_b;
							break;
						}
					}
				}

#undef FACE_USED_TEST
#undef FACE_USED_SET

				/* nf_i doesn't include the last face */
				BLI_assert(nf_i == orig_f_len - 3);

				/* we can't delete the real face, because some of the callers expect it to remain valid.
				 * so swap data and delete the last created tri */
				bmesh_face_swap_data(bm, f, f_new);
				BM_face_kill(bm, f_new);
			}
		}

		/* garbage collection */
		BLI_scanfill_end_arena(&sf_ctx, sf_arena);
	}

#undef SF_EDGE_IS_BOUNDARY

}

/**
 * each pair of loops defines a new edge, a split.  this function goes
 * through and sets pairs that are geometrically invalid to null.  a
 * split is invalid, if it forms a concave angle or it intersects other
 * edges in the face, or it intersects another split.  in the case of
 * intersecting splits, only the first of the set of intersecting
 * splits survives
 */
void BM_face_legal_splits(BMFace *f, BMLoop *(*loops)[2], int len)
{
	const int len2 = len * 2;
	BMLoop *l;
	float v1[2], v2[2], v3[2], mid[2], *p1, *p2, *p3, *p4;
	float out[2] = {-FLT_MAX, -FLT_MAX};
	float axis_mat[3][3];
	float (*projverts)[2] = BLI_array_alloca(projverts, f->len);
	float (*edgeverts)[2] = BLI_array_alloca(edgeverts, len2);
	float fac1 = 1.0000001f, fac2 = 0.9f; //9999f; //0.999f;
	int i, j, a = 0, clen;

	BLI_assert(BM_face_is_normal_valid(f));

	axis_dominant_v3_to_m3(axis_mat, f->no);

	for (i = 0, l = BM_FACE_FIRST_LOOP(f); i < f->len; i++, l = l->next) {
		BM_elem_index_set(l, i); /* set_loop */
		mul_v2_m3v3(projverts[i], axis_mat, l->v->co);

		out[0] = max_ff(out[0], projverts[i][0]);
		out[1] = max_ff(out[1], projverts[i][1]);
	}
	
	/* ensure we are well outside the face bounds (value is arbitrary) */
	add_v2_fl(out, 1.0f);

	for (i = 0; i < len; i++) {
		copy_v2_v2(edgeverts[a + 0], projverts[BM_elem_index_get(loops[i][0])]);
		copy_v2_v2(edgeverts[a + 1], projverts[BM_elem_index_get(loops[i][1])]);
		scale_edge_v2f(edgeverts[a + 0], edgeverts[a + 1], fac2);
		a += 2;
	}

	/* do convexity test */
	for (i = 0; i < len; i++) {
		copy_v2_v2(v2, edgeverts[i * 2 + 0]);
		copy_v2_v2(v3, edgeverts[i * 2 + 1]);

		mid_v2_v2v2(mid, v2, v3);
		
		clen = 0;
		for (j = 0; j < f->len; j++) {
			p1 = projverts[j];
			p2 = projverts[(j + 1) % f->len];
			
#if 0
			copy_v2_v2(v1, p1);
			copy_v2_v2(v2, p2);

			scale_edge_v2f(v1, v2, fac1);
			if (line_crosses_v2f(v1, v2, mid, out)) {
				clen++;
			}
#else
			if (line_crosses_v2f(p1, p2, mid, out)) {
				clen++;
			}
#endif
		}

		if (clen % 2 == 0) {
			loops[i][0] = NULL;
		}
	}

	/* do line crossing tests */
	for (i = 0; i < f->len; i++) {
		p1 = projverts[i];
		p2 = projverts[(i + 1) % f->len];
		
		copy_v2_v2(v1, p1);
		copy_v2_v2(v2, p2);

		scale_edge_v2f(v1, v2, fac1);

		for (j = 0; j < len; j++) {
			if (!loops[j][0]) {
				continue;
			}

			p3 = edgeverts[j * 2];
			p4 = edgeverts[j * 2 + 1];

			if (line_crosses_v2f(v1, v2, p3, p4)) {
				loops[j][0] = NULL;
			}
		}
	}

	for (i = 0; i < len; i++) {
		for (j = 0; j < len; j++) {
			if (j != i && loops[i][0] && loops[j][0]) {
				p1 = edgeverts[i * 2];
				p2 = edgeverts[i * 2 + 1];
				p3 = edgeverts[j * 2];
				p4 = edgeverts[j * 2 + 1];

				copy_v2_v2(v1, p1);
				copy_v2_v2(v2, p2);

				scale_edge_v2f(v1, v2, fac1);

				if (line_crosses_v2f(v1, v2, p3, p4)) {
					loops[i][0] = NULL;
				}
			}
		}
	}
}


/**
 * Small utility functions for fast access
 *
 * faster alternative to:
 *  BM_iter_as_array(bm, BM_VERTS_OF_FACE, f, (void **)v, 3);
 */
void BM_face_as_array_vert_tri(BMFace *f, BMVert *r_verts[3])
{
	BMLoop *l = BM_FACE_FIRST_LOOP(f);

	BLI_assert(f->len == 3);

	r_verts[0] = l->v; l = l->next;
	r_verts[1] = l->v; l = l->next;
	r_verts[2] = l->v;
}

/**
 * faster alternative to:
 *  BM_iter_as_array(bm, BM_VERTS_OF_FACE, f, (void **)v, 4);
 */
void BM_face_as_array_vert_quad(BMFace *f, BMVert *r_verts[4])
{
	BMLoop *l = BM_FACE_FIRST_LOOP(f);

	BLI_assert(f->len == 4);

	r_verts[0] = l->v; l = l->next;
	r_verts[1] = l->v; l = l->next;
	r_verts[2] = l->v; l = l->next;
	r_verts[3] = l->v;
}


/**
 * Small utility functions for fast access
 *
 * faster alternative to:
 *  BM_iter_as_array(bm, BM_LOOPS_OF_FACE, f, (void **)l, 3);
 */
void BM_face_as_array_loop_tri(BMFace *f, BMLoop *r_loops[3])
{
	BMLoop *l = BM_FACE_FIRST_LOOP(f);

	BLI_assert(f->len == 3);

	r_loops[0] = l; l = l->next;
	r_loops[1] = l; l = l->next;
	r_loops[2] = l;
}

/**
 * faster alternative to:
 *  BM_iter_as_array(bm, BM_LOOPS_OF_FACE, f, (void **)l, 4);
 */
void BM_face_as_array_loop_quad(BMFace *f, BMLoop *r_loops[4])
{
	BMLoop *l = BM_FACE_FIRST_LOOP(f);

	BLI_assert(f->len == 4);

	r_loops[0] = l; l = l->next;
	r_loops[1] = l; l = l->next;
	r_loops[2] = l; l = l->next;
	r_loops[3] = l;
}
