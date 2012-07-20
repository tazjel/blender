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
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_bevel.c
 *  \ingroup bmesh
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_array.h"
#include "BLI_math.h"
#include "BLI_smallhash.h"

#include "BKE_customdata.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */
#include "intern/bmesh_private.h"


#define BEVEL_FLAG	1
#define BEVEL_DEL	2
#define FACE_NEW	4
#define EDGE_OLD	8
#define FACE_OLD	16
#define VERT_OLD	32
#define FACE_SPAN	64
#define FACE_HOLE	128

#define EDGE_SELECTED 256

typedef struct LoopTag {
	BMVert *newv;
} LoopTag;

typedef struct EdgeTag {
	BMVert *newv1, *newv2;
} EdgeTag;

// Item in the list of vertices
typedef struct VertexItem{
    struct VertexItem *next, *prev;
    BMVert *v;
    int onEdge; //  true if new vertex located on edge; edge1 = edge, edge2 = NULL
                // false, new vert located betwen edge1 and edge2
    BMEdge *edge1;
    BMEdge *edge2;
    BMFace *f;
}VertexItem;

/* list of new vertices formed around v */
typedef struct AdditionalVert{
    struct AdditionalVert *next, *prev;
    BMVert *v;
    ListBase vertices;
    int count;
} AdditionalVert;

/*
* struct with bevel parametrs
*/
typedef struct BevelParams{
    ListBase vertList;  /* list additional vertex */
    float offset;
    int byPolygon; /* 1 - make bevel on each polygon, 0 - ignore internal polygon */
} BevelParams;

static void calc_corner_co(BMLoop *l, const float fac, float r_co[3],
                           const short do_dist, const short do_even)
{
	float  no[3], l_vec_prev[3], l_vec_next[3], l_co_prev[3], l_co[3], l_co_next[3], co_ofs[3];
	int is_concave;

	/* first get the prev/next verts */
	if (l->f->len > 2) {
		copy_v3_v3(l_co_prev, l->prev->v->co);
		copy_v3_v3(l_co, l->v->co);
		copy_v3_v3(l_co_next, l->next->v->co);

		/* calculate normal */
		sub_v3_v3v3(l_vec_prev, l_co_prev, l_co);
		sub_v3_v3v3(l_vec_next, l_co_next, l_co);

		cross_v3_v3v3(no, l_vec_prev, l_vec_next);
		is_concave = dot_v3v3(no, l->f->no) > 0.0f;
	}
	else {
		BMIter iter;
		BMLoop *l2;
		float up[3] = {0.0f, 0.0f, 1.0f};

		copy_v3_v3(l_co_prev, l->prev->v->co);
		copy_v3_v3(l_co, l->v->co);
		
		BM_ITER_ELEM (l2, &iter, l->v, BM_LOOPS_OF_VERT) {
			if (l2->f != l->f) {
				copy_v3_v3(l_co_next, BM_edge_other_vert(l2->e, l2->next->v)->co);
				break;
			}
		}
		
		sub_v3_v3v3(l_vec_prev, l_co_prev, l_co);
		sub_v3_v3v3(l_vec_next, l_co_next, l_co);

		cross_v3_v3v3(no, l_vec_prev, l_vec_next);
		if (dot_v3v3(no, no) == 0.0f) {
			no[0] = no[1] = 0.0f; no[2] = -1.0f;
		}
		
		is_concave = dot_v3v3(no, up) < 0.0f;
	}


	/* now calculate the new location */
	if (do_dist) { /* treat 'fac' as distance */

		normalize_v3(l_vec_prev);
		normalize_v3(l_vec_next);

		add_v3_v3v3(co_ofs, l_vec_prev, l_vec_next);
		if (UNLIKELY(normalize_v3(co_ofs) == 0.0f)) {  /* edges form a straight line */
			cross_v3_v3v3(co_ofs, l_vec_prev, l->f->no);
		}

		if (do_even) {
			negate_v3(l_vec_next);
			mul_v3_fl(co_ofs, fac * shell_angle_to_dist(0.5f * angle_normalized_v3v3(l_vec_prev, l_vec_next)));
			/* negate_v3(l_vec_next); */ /* no need unless we use again */
		}
		else {
			mul_v3_fl(co_ofs, fac);
		}
	}
	else { /* treat as 'fac' as a factor (0 - 1) */

		/* not strictly necessary, balance vectors
		 * so the longer edge doesn't skew the result,
		 * gives nicer, move even output.
		 *
		 * Use the minimum rather then the middle value so skinny faces don't flip along the short axis */
		float min_fac = minf(normalize_v3(l_vec_prev), normalize_v3(l_vec_next));
		float angle;

		if (do_even) {
			negate_v3(l_vec_next);
			angle = angle_normalized_v3v3(l_vec_prev, l_vec_next);
			negate_v3(l_vec_next); /* no need unless we use again */
		}
		else {
			angle = 0.0f;
		}

		mul_v3_fl(l_vec_prev, min_fac);
		mul_v3_fl(l_vec_next, min_fac);

		add_v3_v3v3(co_ofs, l_vec_prev, l_vec_next);

		if (UNLIKELY(is_zero_v3(co_ofs))) {
			cross_v3_v3v3(co_ofs, l_vec_prev, l->f->no);
			normalize_v3(co_ofs);
			mul_v3_fl(co_ofs, min_fac);
		}

		/* done */
		if (do_even) {
			mul_v3_fl(co_ofs, (fac * 0.5f) * shell_angle_to_dist(0.5f * angle));
		}
		else {
			mul_v3_fl(co_ofs, fac * 0.5f);
		}
	}

	/* apply delta vec */
	if (is_concave)
		negate_v3(co_ofs);

	add_v3_v3v3(r_co, co_ofs, l->v->co);
}


#define ETAG_SET(e, v, nv)  (                                                 \
	(v) == (e)->v1 ?                                                          \
		(etags[BM_elem_index_get((e))].newv1 = (nv)) :                        \
		(etags[BM_elem_index_get((e))].newv2 = (nv))                          \
	)

#define ETAG_GET(e, v)  (                                                     \
	(v) == (e)->v1 ?                                                          \
		(etags[BM_elem_index_get((e))].newv1) :                               \
		(etags[BM_elem_index_get((e))].newv2)                                 \
	)

// build point on edge
// todo rename function
BMVert* bevel_calc_aditional_vert(BMesh *bm, BevelParams *bp, BMEdge* edge, BMVert* vert)
{
    // TODO добавить проверку на то что эдж содержит вершину
    float vect[3], normV[3];

    BMVert *new_Vert = NULL;

    sub_v3_v3v3(vect, BM_edge_other_vert(edge, vert)->co, vert->co);
    normalize_v3_v3(normV, vect);
    mul_v3_fl(normV, bp->offset);

    add_v3_v3(normV, vert->co);

    new_Vert = BM_vert_create(bm, normV, NULL);
    return new_Vert;
}

// build point between edges
// rename
BMVert* bevel_middle_vert(BMesh *bm, BevelParams *bp, BMEdge *edge_a, BMEdge *edge_b, BMVert *vert)
{
    float offset, v_a[3], v_b[3], v_c[3], norm_a[3], norm_b[3],norm_c[3],  angel;
    BMVert* new_vert = NULL;
    offset = bp->offset;
    // calc vectors
    // TODO: add chec parallel case
    sub_v3_v3v3(v_a, BM_edge_other_vert(edge_a, vert)->co, vert->co);
    sub_v3_v3v3(v_b, BM_edge_other_vert(edge_b, vert)->co, vert->co);
    normalize_v3_v3(norm_a, v_a);
    normalize_v3_v3(norm_b, v_b);
    add_v3_v3v3(v_c, norm_a, norm_b);
    mul_v3_fl(v_c, 0.5);
    normalize_v3_v3(norm_c, v_c);

    // v_c ^ edge_a
    //cos = (v_c[0]*v_a[0] + v_c[1]*v_a[1] + v_c[2]*v_a[0]) / (len_v3(v_c)*len_v3(v_a));
    angel = angle_normalized_v3v3(norm_c, norm_a);
    //offset = offset / (sqrt(1-cos*cos));
    offset = offset / sin(angel);

    mul_v3_fl(norm_c, offset);
    add_v3_v3(norm_c, vert->co);
    new_vert = BM_vert_create(bm, norm_c, NULL);
    return new_vert;
}

// rename
/*
  e - selected edge
  return other selected edge
*/
BMEdge* find_selected_edge_in_face(BMesh *bm, BMFace *f, BMEdge *e, BMVert *v)
{
    BMEdge *oe = NULL;
    BMLoop *l = f->l_first;
    do {
        if (BMO_elem_flag_test(bm, l->e, EDGE_SELECTED) &&
            (l->e != e) &&
                ((l->e->v1 == v ) ||
                 BM_edge_other_vert(l->e, l->e->v1) == v  )) {
            oe = l->e;
        }
        l = l->next;
    } while (l != f->l_first);
    return oe;
}

int check_dublicated_vertex_item(AdditionalVert *item, BMFace *f)
{
    VertexItem *vItem;
    int result = 0;
    for (vItem = item->vertices.first; vItem; vItem = vItem->next) {
        if (vItem->f == f)
            result = 1;
    }
    return result;

}

/*
* additional construction arround the vertex
*/
void bevel_aditional_construction_by_vert(BMesh *bm, BevelParams *bp, BMOperator *op, BMVert *v)
{
    // TODO develop planar case
    BMOIter siter;
    //BMIter iter;
    BMEdge *e, **edges = NULL;
    //int i;
    BLI_array_declare(edges);

    // calc count input selected edges
    BMO_ITER (e, &siter, bm, op, "geom", BM_EDGE) {
        if ((e->v1 == v)|| (BM_edge_other_vert(e, e->v1) == v))
        {
            BMO_elem_flag_enable (bm, e, EDGE_SELECTED);
            BLI_array_append(edges, e);
        }
    }
    if (BLI_array_count(edges) > 0) {
        //BMEdge *prev_e;
        AdditionalVert *av;
        av = (AdditionalVert*)MEM_callocN(sizeof(AdditionalVert), "AdditionalVert");
        av->v = v;
        av->count = 0;
        av->vertices.first = av->vertices.last = NULL;
        BLI_addtail(&bp->vertList, av);
        e = bmesh_disk_faceedge_find_first(edges[0], v);

        // for (e = bmesh_disk_edge_next(edges[0], v); e != edges[0]; e = bmesh_disk_edge_next(e, v)) {
        do {
            //prev_e = e;
            e = bmesh_disk_edge_next(e, v);
            // point located beteween selecion edges
            if (BMO_elem_flag_test(bm, e, EDGE_SELECTED)) {
                BMFace *f;
                BMIter iter;
                BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
                    BMLoop *l = f->l_first;
                    //BMEdge **ee = NULL;
                    //BLI_array_declare (ee);
                    do {
                        if ((l->e == e) && (find_selected_edge_in_face(bm, f, e, v) !=NULL )){
                            if (!check_dublicated_vertex_item(av, f)) {
                                VertexItem *item;
                                item = (VertexItem*)MEM_callocN(sizeof(VertexItem), "VertexItem");
                                item->onEdge = 0; // false
                                item->edge1 = e;
                                item->edge2 = find_selected_edge_in_face(bm, f, e, v);
                                item->f = f;
                                item->v = bevel_middle_vert(bm, bp,e, item->edge2, v);
                                BLI_addtail(&av->vertices, item);
                                av->count ++;
                            }
                        }
                       l = l->next;
                    } while (l != f->l_first);
                }
            }
            if (!BMO_elem_flag_test(bm, e, EDGE_SELECTED)){
                VertexItem *item;
                item = (VertexItem*)MEM_callocN(sizeof(VertexItem), "VertexItem");
                item->onEdge = 1; /* true */
                item->edge1 = e;
                item->edge2 = NULL;
                item->f = NULL;
                item->v = bevel_calc_aditional_vert(bm, bp, e, v);
                BLI_addtail(&av->vertices, item);
                av->count ++;
            }
        } while (e != edges[0]);
    }
    BLI_array_free(edges);
}

//TODO rename it!
AdditionalVert* get_additionalVert_by_vert(BevelParams *bp, BMVert *v)
{
    VertexItem *item;
    AdditionalVert *av = NULL;
    for (item = bp->vertList.first; item ; item = item->next){
        if (item->v == v)
            //av = item;
			av->v = item->v;
    }
    return av;
}

/*
* return 1 if face content e
*/
int is_edge_of_face (BMFace *f, BMEdge* e)
{
    int result  = 0;
    BMLoop *l = f->l_first;
    do {
        if (l->e == e)
            result = 1;
        l = l->next;
    } while  (l != f->l_first);
    return result;
}



void rebuild_polygon(BMesh *bm, BevelParams *bp, BMFace *f)
{
    BMVert **vv = NULL; /* list vertex for new ngons */
    BMLoop *l;
    VertexItem *vItem;
    AdditionalVert *av;
    BLI_array_declare(vv);
    int count;
    l = f->l_first;
    do {
        av = get_additionalVert_by_vert(bp, l->v);
        if (av != NULL){
            for (vItem = av->vertices.first; vItem; vItem = vItem->next) {
                // case 1, all point located in edges
                if ((vItem->onEdge)&&(is_edge_of_face(f, vItem->edge1)))
                    BLI_array_append(vv, vItem->v);
                // case 2, point located between
                if ((!vItem->onEdge) &&
                        (is_edge_of_face(f, vItem->edge1)) &&
                        (is_edge_of_face(f, vItem->edge2)))
                    BLI_array_append(vv, vItem->v);
            }
        } else
            BLI_array_append(vv, l->v);
        l = l->next;
    } while (l != f->l_first);

    count = BLI_array_count(vv);
    // TODO check normal
    if (count > 0)
        BM_face_create_ngon_vcloud(bm, vv, count, 0);
    BLI_array_free(vv);
}

void bevel_build_polygon_arround_vertex(BMesh *bm, BevelParams *bp, BMVert *v)
{
    AdditionalVert *av = get_additionalVert_by_vert(bp, v);
    VertexItem *vi;
    BMVert **vv = NULL;
    BLI_array_declare(vv);
    if (av->count > 2) {
        for (vi = av->vertices.first; vi; vi = vi->next)
            BLI_array_append(vv, vi->v);
        /* TODO check normal */
        if (BLI_array_count(vv) > 0)
            BM_face_create_ngon_vcloud(bm, vv, BLI_array_count(vv), 0);
    }
    BLI_array_free(vv);
}

void bevel_rebuild_exist_polygons(BMesh *bm, BevelParams *bp, BMVert *v)
{
    BMFace *f;
    BMIter iter;
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
        BMLoop *l = f->l_first;
        do {
            if (l->v == v) {
                rebuild_polygon (bm, bp, f);
                BM_face_kill(bm,f);
            }
            l = l->next;
        } while (l != f->l_first);
    }
}

// TDOD commented it !!!!
BMEdge* find_edge_in_face(BMFace *f, BMEdge *e, BMVert* v)
{
    BMEdge *oe = NULL;
    BMLoop *l = f->l_first;
    do {
        if ((l->e != e) &&
                ((l->e->v1 == v ) ||
                 BM_edge_other_vert(l->e, l->e->v1) == v)) {
            oe = l->e;
        }
        l = l->next;
    } while (l != f->l_first);
    return oe;
}

/*
*   aE = adjacentE
*          v ... v
*         /     /
*        aE f aE
*       /     /
*    --v--e--v--
*      |     |
*      aE f aE
*      |     |
*      v ... v
*/
BMVert* get_additional_vert(AdditionalVert *av, BMFace *f,  BMEdge *adjacentE)
{
    VertexItem *vi;
    BMVert *v = NULL;

    for (vi = av->vertices.first; vi; vi = vi->next){
            if ((vi->onEdge) && (vi->edge1 == adjacentE))
                v = vi->v;
            if (vi->f == f)
                v = vi->v;
    }
    return v;
}

/*
* Build the polygons along the selected Edge
*/
void bevel_build_polygon(BMesh *bm, BevelParams *bp, BMEdge *e)
{
    //BMVert *vi[4]; // only for linear case with seg = 1
    AdditionalVert *item, *av1, *av2;
    //VertexItem *vItem;
    BMVert **vv=NULL , *v;
    BMFace *f;
    BMIter iter;
    BMEdge *e1, *e2;
    int i;


    BLI_array_declare(vv);

    for (item = bp->vertList.first; item ; item = item->next){
        if (item->v == e->v1)
            av1 = item;
        if (item->v == BM_edge_other_vert(e, e->v1))
            av2 = item;
    }

    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
         BMLoop *l = f->l_first;
         do {
             if (l->e ==  e){
                 e1 = find_edge_in_face(f, e, e->v1);
                 v = get_additional_vert(av1, f, e1);
                 if (v != NULL)
                     BLI_array_append(vv,v);
                 e2 = find_edge_in_face(f, e, BM_edge_other_vert(e, e->v1));
                 v = get_additional_vert(av2, f, e2);
                 if (v != NULL)
                     BLI_array_append(vv,v);
             }
            l = l->next;
         } while (l != f->l_first);
    }
    i = BLI_array_count(vv);
    if (BLI_array_count(vv) > 0)
        BM_face_create_ngon_vcloud(bm, vv, BLI_array_count(vv), 0);
    BLI_array_free(vv);

  /*  for (vItem = av1->vertices.first; vItem; vItem = vItem->next){
        if (vItem->onEdge) {
            if (vItem->edge1 == bmesh_disk_edge_next(e, av1->v)) {
                vi[i] = vItem->v;
                i++;
            }
            if (vItem->edge1 == bmesh_disk_edge_prev(e, av1->v)) {
                vi[i] = vItem->v;
                i++;
            }
        } else {
            if ((vItem->edge1 == bmesh_disk_edge_next(e, av1->v)) ||
            (vItem->edge2 == bmesh_disk_edge_next(e, av1->v)))
            {
                vi[i] = vItem->v;
                i++;
            }
            if ((vItem->edge1 == bmesh_disk_edge_prev(e, av1->v)) ||
            (vItem->edge2 == bmesh_disk_edge_prev(e, av1->v)))
            {
                vi[i] = vItem->v;
                i++;
            }
        }
    }

    for (vItem = av2->vertices.first; vItem; vItem = vItem->next){
        if (vItem->onEdge)
        {
            if (vItem->edge1 == bmesh_disk_edge_next(e, av2->v))
            {
                vi[i] = vItem->v;
                i++;
            }
            if (vItem->edge1 == bmesh_disk_edge_prev(e, av2->v))
            {
                vi[i] = vItem->v;
                i++;
            }
        }
        else
        {
            if ((vItem->edge1 == bmesh_disk_edge_next(e, av2->v)) ||
            (vItem->edge2 == bmesh_disk_edge_next(e, av2->v)))
            {
                vi[i] = vItem->v;
                i++;
            }
            if ((vItem->edge1 == bmesh_disk_edge_prev(e, av2->v)) ||
            (vItem->edge2 == bmesh_disk_edge_prev(e, av2->v)))
            {
                vi[i] = vItem->v;
                i++;
            }
        }
    }
    BM_face_create_quad_tri(bm, vi[0], vi[1], vi[3], vi[2], NULL, TRUE);
    */
}


void free_bevel_params(BevelParams *bp)
{
    AdditionalVert *item;
    for (item = bp->vertList.first; item ; item = item->next)
            BLI_freelistN(&item->vertices);
    BLI_freelistN(&bp->vertList);
}


void bmo_bevel_exec(BMesh *bm, BMOperator *op)
{
    BMOIter siter;
    BMVert *v;
    BMEdge *e;
    BevelParams bp;
    float fac = BMO_slot_float_get(op, "percent");
    //bp.offset = 0.1;
    bp.offset = fac;
    bp.byPolygon = 1;
    bp.vertList.first = bp.vertList.last = NULL;
    /* The analysis of the input vertices and execution additional constructions */
    BMO_ITER (v, &siter, bm, op, "geom", BM_VERT) {
        bevel_aditional_construction_by_vert(bm, &bp, op, v);

    }

    /* Build polgiony found at verteces */
    BMO_ITER(e, &siter, bm, op, "geom", BM_EDGE){
        bevel_build_polygon(bm, &bp, e);
    }

    BMO_ITER (v, &siter, bm, op, "geom", BM_VERT) {
        bevel_build_polygon_arround_vertex(bm, &bp, v);
        bevel_rebuild_exist_polygons(bm, &bp,v);
    }

    BMO_ITER (v, &siter, bm, op, "geom", BM_VERT) {
        BM_vert_kill(bm, v);
    }

    free_bevel_params(&bp);
}


void bmo_bevel_exec_old(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMIter iter;
	BMEdge *e;
	BMVert *v;
	BMFace **faces = NULL, *f;
	LoopTag *tags = NULL, *tag;
	EdgeTag *etags = NULL;
	BMVert **verts = NULL;
	BMEdge **edges = NULL;
	BLI_array_declare(faces);
	BLI_array_declare(tags);
	BLI_array_declare(etags);
	BLI_array_declare(verts);
	BLI_array_declare(edges);
	SmallHash hash;
	float fac = BMO_slot_float_get(op, "percent");
	const short do_even = BMO_slot_bool_get(op, "use_even");
	const short do_dist = BMO_slot_bool_get(op, "use_dist");
	int i, li, has_elens, HasMDisps = CustomData_has_layer(&bm->ldata, CD_MDISPS);
	
	has_elens = CustomData_has_layer(&bm->edata, CD_PROP_FLT) && BMO_slot_bool_get(op, "use_lengths");
	if (has_elens) {
		li = BMO_slot_int_get(op, "lengthlayer");
	}
	
	BLI_smallhash_init(&hash);
	
	BMO_ITER (e, &siter, bm, op, "geom", BM_EDGE) {
		BMO_elem_flag_enable(bm, e, BEVEL_FLAG | BEVEL_DEL);
		BMO_elem_flag_enable(bm, e->v1, BEVEL_FLAG | BEVEL_DEL);
		BMO_elem_flag_enable(bm, e->v2, BEVEL_FLAG | BEVEL_DEL);
        // разбраться с этим случаем
		if (BM_edge_face_count(e) < 2) {
			BMO_elem_flag_disable(bm, e, BEVEL_DEL);
			BMO_elem_flag_disable(bm, e->v1, BEVEL_DEL);
			BMO_elem_flag_disable(bm, e->v2, BEVEL_DEL);
		}
#if 0
		if (BM_edge_face_count(e) == 0) {
			BMVert *verts[2] = {e->v1, e->v2};
			BMEdge *edges[2] = {e, BM_edge_create(bm, e->v1, e->v2, e, 0)};
			
			BMO_elem_flag_enable(bm, edges[1], BEVEL_FLAG);
			BM_face_create(bm, verts, edges, 2, FALSE);
		}
#endif
	}
	
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		BMO_elem_flag_enable(bm, v, VERT_OLD);
	}

#if 0
	//a bit of cleaner code that, alas, doens't work.
	/* build edge tag */
	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		if (BMO_elem_flag_test(bm, e->v1, BEVEL_FLAG) || BMO_elem_flag_test(bm, e->v2, BEVEL_FLAG)) {
			BMIter liter;
			BMLoop *l;
			
			if (!BMO_elem_flag_test(bm, e, EDGE_OLD)) {
				BM_elem_index_set(e, BLI_array_count(etags)); /* set_dirty! */
				BLI_array_grow_one(etags);
				
				BMO_elem_flag_enable(bm, e, EDGE_OLD);
			}
			
			BM_ITER_ELEM (l, &liter, e, BM_LOOPS_OF_EDGE) {
				BMLoop *l2;
				BMIter liter2;
				
				if (BMO_elem_flag_test(bm, l->f, BEVEL_FLAG))
					continue;

				BM_ITER_ELEM (l2, &liter2, l->f, BM_LOOPS_OF_FACE) {
					BM_elem_index_set(l2, BLI_array_count(tags)); /* set_loop */
					BLI_array_grow_one(tags);

					if (!BMO_elem_flag_test(bm, l2->e, EDGE_OLD)) {
						BM_elem_index_set(l2->e, BLI_array_count(etags)); /* set_dirty! */
						BLI_array_grow_one(etags);
						
						BMO_elem_flag_enable(bm, l2->e, EDGE_OLD);
					}
				}

				BMO_elem_flag_enable(bm, l->f, BEVEL_FLAG);
				BLI_array_append(faces, l->f);
			}
		}
		else {
			BM_elem_index_set(e, -1); /* set_dirty! */
		}
	}
#endif
	
	/* create and assign looptag structure */
	BMO_ITER (e, &siter, bm, op, "geom", BM_EDGE) {
		BMLoop *l;
		BMIter liter;

		BMO_elem_flag_enable(bm, e->v1, BEVEL_FLAG | BEVEL_DEL);
		BMO_elem_flag_enable(bm, e->v2, BEVEL_FLAG | BEVEL_DEL);
		
		if (BM_edge_face_count(e) < 2) {
			BMO_elem_flag_disable(bm, e, BEVEL_DEL);
			BMO_elem_flag_disable(bm, e->v1, BEVEL_DEL);
			BMO_elem_flag_disable(bm, e->v2, BEVEL_DEL);
		}
		
		if (!BLI_smallhash_haskey(&hash, (intptr_t)e)) {
			BLI_array_grow_one(etags);
			BM_elem_index_set(e, BLI_array_count(etags) - 1); /* set_dirty! */
			BLI_smallhash_insert(&hash, (intptr_t)e, NULL);
			BMO_elem_flag_enable(bm, e, EDGE_OLD);
		}
		
		/* find all faces surrounding e->v1 and, e->v2 */
		for (i = 0; i < 2; i++) {
			BM_ITER_ELEM (l, &liter, i ? e->v2:e->v1, BM_LOOPS_OF_VERT) {
				BMLoop *l2;
				BMIter liter2;
				
				/* see if we've already processed this loop's fac */
				if (BLI_smallhash_haskey(&hash, (intptr_t)l->f))
					continue;
				
				/* create tags for all loops in l-> */
				BM_ITER_ELEM (l2, &liter2, l->f, BM_LOOPS_OF_FACE) {
					BLI_array_grow_one(tags);
					BM_elem_index_set(l2, BLI_array_count(tags) - 1); /* set_loop */
					
					if (!BLI_smallhash_haskey(&hash, (intptr_t)l2->e)) {
						BLI_array_grow_one(etags);
						BM_elem_index_set(l2->e, BLI_array_count(etags) - 1); /* set_dirty! */
						BLI_smallhash_insert(&hash, (intptr_t)l2->e, NULL);
						BMO_elem_flag_enable(bm, l2->e, EDGE_OLD);
					}
				}

				BLI_smallhash_insert(&hash, (intptr_t)l->f, NULL);
				BMO_elem_flag_enable(bm, l->f, BEVEL_FLAG);
				BLI_array_append(faces, l->f);
			}
		}
	}

	bm->elem_index_dirty |= BM_EDGE;
	
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		BMIter eiter;
		
		if (!BMO_elem_flag_test(bm, v, BEVEL_FLAG))
			continue;
		
		BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
			if (!BMO_elem_flag_test(bm, e, BEVEL_FLAG) && !ETAG_GET(e, v)) {
				BMVert *v2;
				float co[3];
				
				v2 = BM_edge_other_vert(e, v);
				sub_v3_v3v3(co, v2->co, v->co);
				if (has_elens) {
					float elen = *(float *)CustomData_bmesh_get_n(&bm->edata, e->head.data, CD_PROP_FLT, li);

					normalize_v3(co);
					mul_v3_fl(co, elen);
				}
				
				mul_v3_fl(co, fac);
				add_v3_v3(co, v->co);
				
				v2 = BM_vert_create(bm, co, v);
				ETAG_SET(e, v, v2);
			}
		}
	}
	
	for (i = 0; i < BLI_array_count(faces); i++) {
		BMLoop *l;
		BMIter liter;
		
		BMO_elem_flag_enable(bm, faces[i], FACE_OLD);
		
		BM_ITER_ELEM (l, &liter, faces[i], BM_LOOPS_OF_FACE) {
			float co[3];

			if (BMO_elem_flag_test(bm, l->e, BEVEL_FLAG)) {
				if (BMO_elem_flag_test(bm, l->prev->e, BEVEL_FLAG)) {
					tag = tags + BM_elem_index_get(l);
					calc_corner_co(l, fac, co, do_dist, do_even);
					tag->newv = BM_vert_create(bm, co, l->v);
				}
				else {
					tag = tags + BM_elem_index_get(l);
					tag->newv = ETAG_GET(l->prev->e, l->v);
					
					if (!tag->newv) {
						sub_v3_v3v3(co, l->prev->v->co, l->v->co);
						if (has_elens) {
							float elen = *(float *)CustomData_bmesh_get_n(&bm->edata, l->prev->e->head.data,
							                                              CD_PROP_FLT, li);

							normalize_v3(co);
							mul_v3_fl(co, elen);
						}

						mul_v3_fl(co, fac);
						add_v3_v3(co, l->v->co);

						tag->newv = BM_vert_create(bm, co, l->v);
						
						ETAG_SET(l->prev->e, l->v, tag->newv);
					}
				}
			}
			else if (BMO_elem_flag_test(bm, l->v, BEVEL_FLAG)) {
				tag = tags + BM_elem_index_get(l);
				tag->newv = ETAG_GET(l->e, l->v);

				if (!tag->newv) {
					sub_v3_v3v3(co, l->next->v->co, l->v->co);
					if (has_elens) {
						float elen = *(float *)CustomData_bmesh_get_n(&bm->edata, l->e->head.data, CD_PROP_FLT, li);

						normalize_v3(co);
						mul_v3_fl(co, elen);
					}
					
					mul_v3_fl(co, fac);
					add_v3_v3(co, l->v->co);

					tag = tags + BM_elem_index_get(l);
					tag->newv = BM_vert_create(bm, co, l->v);
					
					ETAG_SET(l->e, l->v, tag->newv);
				}
			}
			else {
				tag = tags + BM_elem_index_get(l);
				tag->newv = l->v;
				BMO_elem_flag_disable(bm, l->v, BEVEL_DEL);
			}
		}
	}
	
	/* create new faces inset from original face */
	for (i = 0; i < BLI_array_count(faces); i++) {
		BMLoop *l;
		BMIter liter;
		BMFace *f;
		BMVert *lastv = NULL, *firstv = NULL;

		BMO_elem_flag_enable(bm, faces[i], BEVEL_DEL);
		
		BLI_array_empty(verts);
		BLI_array_empty(edges);
		
		BM_ITER_ELEM (l, &liter, faces[i], BM_LOOPS_OF_FACE) {
			BMVert *v2;
			
			tag = tags + BM_elem_index_get(l);
			BLI_array_append(verts, tag->newv);
			
			if (!firstv)
				firstv = tag->newv;
			
			if (lastv) {
				e = BM_edge_create(bm, lastv, tag->newv, l->e, TRUE);
				BM_elem_attrs_copy(bm, bm, l->prev->e, e);
				BLI_array_append(edges, e);
			}
			lastv = tag->newv;
			
			v2 = ETAG_GET(l->e, l->next->v);
			
			tag = &tags[BM_elem_index_get(l->next)];
			if (!BMO_elem_flag_test(bm, l->e, BEVEL_FLAG) && v2 && v2 != tag->newv) {
				BLI_array_append(verts, v2);
				
				e = BM_edge_create(bm, lastv, v2, l->e, TRUE);
				BM_elem_attrs_copy(bm, bm, l->e, e);
				
				BLI_array_append(edges, e);
				lastv = v2;
			}
		}
		
		e = BM_edge_create(bm, firstv, lastv, BM_FACE_FIRST_LOOP(faces[i])->e, TRUE);
		if (BM_FACE_FIRST_LOOP(faces[i])->prev->e != e) {
			BM_elem_attrs_copy(bm, bm, BM_FACE_FIRST_LOOP(faces[i])->prev->e, e);
		}
		BLI_array_append(edges, e);
		
		f = BM_face_create_ngon(bm, verts[0], verts[1], edges, BLI_array_count(edges), FALSE);
		if (!f) {
			printf("%s: could not make face!\n", __func__);
			continue;
		}

		BMO_elem_flag_enable(bm, f, FACE_NEW);
	}

	for (i = 0; i < BLI_array_count(faces); i++) {
		BMLoop *l;
		BMIter liter;
		int j;
		
		/* create quad spans between split edge */
		BM_ITER_ELEM (l, &liter, faces[i], BM_LOOPS_OF_FACE) {
			BMVert *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL;
			
			if (!BMO_elem_flag_test(bm, l->e, BEVEL_FLAG))
				continue;
			
			v1 = tags[BM_elem_index_get(l)].newv;
			v2 = tags[BM_elem_index_get(l->next)].newv;
			if (l->radial_next != l) {
				v3 = tags[BM_elem_index_get(l->radial_next)].newv;
				if (l->radial_next->next->v == l->next->v) {
					v4 = v3;
					v3 = tags[BM_elem_index_get(l->radial_next->next)].newv;
				}
				else {
					v4 = tags[BM_elem_index_get(l->radial_next->next)].newv;
				}
			}
			else {
				/* the loop is on a boundar */
				v3 = l->next->v;
				v4 = l->v;
				
				for (j = 0; j < 2; j++) {
					BMIter eiter;
					BMVert *v = j ? v4 : v3;

					BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
						if (!BM_vert_in_edge(e, v3) || !BM_vert_in_edge(e, v4))
							continue;
						
						if (!BMO_elem_flag_test(bm, e, BEVEL_FLAG) && BMO_elem_flag_test(bm, e, EDGE_OLD)) {
							BMVert *vv;
							
							vv = ETAG_GET(e, v);
							if (!vv || BMO_elem_flag_test(bm, vv, BEVEL_FLAG))
								continue;
							
							if (j) {
								v1 = vv;
							}
							else {
								v2 = vv;
							}
							break;
						}
					}
				}

				BMO_elem_flag_disable(bm, v3, BEVEL_DEL);
				BMO_elem_flag_disable(bm, v4, BEVEL_DEL);
			}
			
			if (v1 != v2 && v2 != v3 && v3 != v4) {
				BMIter liter2;
				BMLoop *l2, *l3;
				BMEdge *e1, *e2;
				float d1, d2, *d3;
				
				f = BM_face_create_quad_tri(bm, v4, v3, v2, v1, l->f, TRUE);

				e1 = BM_edge_exists(v4, v3);
				e2 = BM_edge_exists(v2, v1);
				BM_elem_attrs_copy(bm, bm, l->e, e1);
				BM_elem_attrs_copy(bm, bm, l->e, e2);
				
				/* set edge lengths of cross edges as the average of the cross edges they're based o */
				if (has_elens) {
					/* angle happens not to be used. why? - not sure it just isn't - campbell.
					 * leave this in in case we need to use it later */
#if 0
					float ang;
#endif
					e1 = BM_edge_exists(v1, v4);
					e2 = BM_edge_exists(v2, v3);
					
					if (l->radial_next->v == l->v) {
						l2 = l->radial_next->prev;
						l3 = l->radial_next->next;
					}
					else {
						l2 = l->radial_next->next;
						l3 = l->radial_next->prev;
					}
					
					d3 = CustomData_bmesh_get_n(&bm->edata, e1->head.data, CD_PROP_FLT, li);
					d1 = *(float *)CustomData_bmesh_get_n(&bm->edata, l->prev->e->head.data, CD_PROP_FLT, li);
					d2 = *(float *)CustomData_bmesh_get_n(&bm->edata, l2->e->head.data, CD_PROP_FLT, li);
#if 0
					ang = angle_v3v3v3(l->prev->v->co, l->v->co, BM_edge_other_vert(l2->e, l->v)->co);
#endif
					*d3 = (d1 + d2) * 0.5f;
					
					d3 = CustomData_bmesh_get_n(&bm->edata, e2->head.data, CD_PROP_FLT, li);
					d1 = *(float *)CustomData_bmesh_get_n(&bm->edata, l->next->e->head.data, CD_PROP_FLT, li);
					d2 = *(float *)CustomData_bmesh_get_n(&bm->edata, l3->e->head.data, CD_PROP_FLT, li);
#if 0
					ang = angle_v3v3v3(BM_edge_other_vert(l->next->e, l->next->v)->co, l->next->v->co,
					                   BM_edge_other_vert(l3->e, l->next->v)->co);
#endif
					*d3 = (d1 + d2) * 0.5f;
				}

				if (!f) {
					fprintf(stderr, "%s: face index out of range! (bmesh internal error)\n", __func__);
					continue;
				}
				
				BMO_elem_flag_enable(bm, f, FACE_NEW | FACE_SPAN);
				
				/* un-tag edges in f for deletio */
				BM_ITER_ELEM (l2, &liter2, f, BM_LOOPS_OF_FACE) {
					BMO_elem_flag_disable(bm, l2->e, BEVEL_DEL);
				}
			}
			else {
				f = NULL;
			}
		}
	}
	
	/* fill in holes at vertices */
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		BMIter eiter;
		BMVert *vv, *vstart = NULL, *lastv = NULL;
		SmallHash tmphash;
		int rad, insorig = 0, err = 0;

		BLI_smallhash_init(&tmphash);

		if (!BMO_elem_flag_test(bm, v, BEVEL_FLAG))
			continue;
		
		BLI_array_empty(verts);
		BLI_array_empty(edges);
		
		BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
			BMIter liter;
			BMVert *v1 = NULL, *v2 = NULL;
			BMLoop *l;
			
			if (BM_edge_face_count(e) < 2)
				insorig = 1;
			
			if (BM_elem_index_get(e) == -1)
				continue;
			
			rad = 0;
			BM_ITER_ELEM (l, &liter, e, BM_LOOPS_OF_EDGE) {
				if (!BMO_elem_flag_test(bm, l->f, FACE_OLD))
					continue;
				
				rad++;
				
				tag = tags + BM_elem_index_get((l->v == v) ? l : l->next);
				
				if (!v1)
					v1 = tag->newv;
				else if (!v2)
					v2 = tag->newv;
			}
			
			if (rad < 2)
				insorig = 1;
			
			if (!v1)
				v1 = ETAG_GET(e, v);
			if (!v2 || v1 == v2)
				v2 = ETAG_GET(e, v);
			
			if (v1) {
				if (!BLI_smallhash_haskey(&tmphash, (intptr_t)v1)) {
					BLI_array_append(verts, v1);
					BLI_smallhash_insert(&tmphash, (intptr_t)v1, NULL);
				}
				
				if (v2 && v1 != v2 && !BLI_smallhash_haskey(&tmphash, (intptr_t)v2)) {
					BLI_array_append(verts, v2);
					BLI_smallhash_insert(&tmphash, (intptr_t)v2, NULL);
				}
			}
		}
		
		if (!BLI_array_count(verts))
			continue;
		
		if (insorig) {
			BLI_array_append(verts, v);
			BLI_smallhash_insert(&tmphash, (intptr_t)v, NULL);
		}
		
		/* find edges that exist between vertices in verts.  this is basically
		 * a topological walk of the edges connecting them */
		vstart = vstart ? vstart : verts[0];
		vv = vstart;
		do {
			BM_ITER_ELEM (e, &eiter, vv, BM_EDGES_OF_VERT) {
				BMVert *vv2 = BM_edge_other_vert(e, vv);
				
				if (vv2 != lastv && BLI_smallhash_haskey(&tmphash, (intptr_t)vv2)) {
					/* if we've go over the same vert twice, break out of outer loop */
					if (BLI_smallhash_lookup(&tmphash, (intptr_t)vv2) != NULL) {
						e = NULL;
						err = 1;
						break;
					}
					
					/* use self pointer as ta */
					BLI_smallhash_remove(&tmphash, (intptr_t)vv2);
					BLI_smallhash_insert(&tmphash, (intptr_t)vv2, vv2);
					
					lastv = vv;
					BLI_array_append(edges, e);
					vv = vv2;
					break;
				}
			}

			if (e == NULL) {
				break;
			}
		} while (vv != vstart);
		
		if (err) {
			continue;
		}

		/* there may not be a complete loop of edges, so start again and make
		 * final edge afterwards.  in this case, the previous loop worked to
		 * find one of the two edges at the extremes. */
		if (vv != vstart) {
			/* undo previous taggin */
			for (i = 0; i < BLI_array_count(verts); i++) {
				BLI_smallhash_remove(&tmphash, (intptr_t)verts[i]);
				BLI_smallhash_insert(&tmphash, (intptr_t)verts[i], NULL);
			}

			vstart = vv;
			lastv = NULL;
			BLI_array_empty(edges);
			do {
				BM_ITER_ELEM (e, &eiter, vv, BM_EDGES_OF_VERT) {
					BMVert *vv2 = BM_edge_other_vert(e, vv);
					
					if (vv2 != lastv && BLI_smallhash_haskey(&tmphash, (intptr_t)vv2)) {
						/* if we've go over the same vert twice, break out of outer loo */
						if (BLI_smallhash_lookup(&tmphash, (intptr_t)vv2) != NULL) {
							e = NULL;
							err = 1;
							break;
						}
						
						/* use self pointer as ta */
						BLI_smallhash_remove(&tmphash, (intptr_t)vv2);
						BLI_smallhash_insert(&tmphash, (intptr_t)vv2, vv2);
						
						lastv = vv;
						BLI_array_append(edges, e);
						vv = vv2;
						break;
					}
				}
				if (e == NULL)
					break;
			} while (vv != vstart);
			
			if (!err) {
				e = BM_edge_create(bm, vv, vstart, NULL, TRUE);
				BLI_array_append(edges, e);
			}
		}
		
		if (err)
			continue;
		
		if (BLI_array_count(edges) >= 3) {
			BMFace *f;
			
			if (BM_face_exists(bm, verts, BLI_array_count(verts), &f))
				continue;
			
			f = BM_face_create_ngon(bm, lastv, vstart, edges, BLI_array_count(edges), FALSE);
			if (!f) {
				fprintf(stderr, "%s: in bevel vert fill! (bmesh internal error)\n", __func__);
			}
			else {
				BMO_elem_flag_enable(bm, f, FACE_NEW | FACE_HOLE);
			}
		}
		BLI_smallhash_release(&tmphash);
	}
	
	/* copy over customdat */
	for (i = 0; i < BLI_array_count(faces); i++) {
		BMLoop *l;
		BMIter liter;
		BMFace *f = faces[i];
		
		BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
			BMLoop *l2;
			BMIter liter2;
			
			tag = tags + BM_elem_index_get(l);
			if (!tag->newv)
				continue;
			
			BM_ITER_ELEM (l2, &liter2, tag->newv, BM_LOOPS_OF_VERT) {
				if (!BMO_elem_flag_test(bm, l2->f, FACE_NEW) || (l2->v != tag->newv && l2->v != l->v))
					continue;
				
				if (tag->newv != l->v || HasMDisps) {
					BM_elem_attrs_copy(bm, bm, l->f, l2->f);
					BM_loop_interp_from_face(bm, l2, l->f, TRUE, TRUE);
				}
				else {
					BM_elem_attrs_copy(bm, bm, l->f, l2->f);
					BM_elem_attrs_copy(bm, bm, l, l2);
				}
				
				if (HasMDisps) {
					BMLoop *l3;
					BMIter liter3;
					
					BM_ITER_ELEM (l3, &liter3, l2->f, BM_LOOPS_OF_FACE) {
						BM_loop_interp_multires(bm, l3, l->f);
					}
				}
			}
		}
	}
	
	/* handle vertices along boundary edge */
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		if (BMO_elem_flag_test(bm, v, VERT_OLD) &&
		    BMO_elem_flag_test(bm, v, BEVEL_FLAG) &&
		    !BMO_elem_flag_test(bm, v, BEVEL_DEL))
		{
			BMLoop *l;
			BMLoop *lorig = NULL;
			BMIter liter;
			
			BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
				// BMIter liter2;
				// BMLoop *l2 = l->v == v ? l : l->next, *l3;
				
				if (BMO_elem_flag_test(bm, l->f, FACE_OLD)) {
					lorig = l;
					break;
				}
			}
			
			if (!lorig)
				continue;
			
			BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
				BMLoop *l2 = l->v == v ? l : l->next;
				
				BM_elem_attrs_copy(bm, bm, lorig->f, l2->f);
				BM_elem_attrs_copy(bm, bm, lorig, l2);
			}
		}
	}
#if 0
	/* clean up any remaining 2-edged face */
	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		if (f->len == 2) {
			BMFace *faces[2] = {f, BM_FACE_FIRST_LOOP(f)->radial_next->f};
			
			if (faces[0] == faces[1])
				BM_face_kill(bm, f);
			else
				BM_faces_join(bm, faces, 2);
		}
	}
#endif

	BMO_op_callf(bm, "delete geom=%fv context=%i", BEVEL_DEL, DEL_VERTS);

	/* clean up any edges that might not get properly delete */
	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		if (BMO_elem_flag_test(bm, e, EDGE_OLD) && !e->l)
			BMO_elem_flag_enable(bm, e, BEVEL_DEL);
	}

	BMO_op_callf(bm, "delete geom=%fe context=%i", BEVEL_DEL, DEL_EDGES);
	BMO_op_callf(bm, "delete geom=%ff context=%i", BEVEL_DEL, DEL_FACES);
	
	BLI_smallhash_release(&hash);
	BLI_array_free(tags);
	BLI_array_free(etags);
	BLI_array_free(verts);
	BLI_array_free(edges);
	BLI_array_free(faces);
	
	BMO_slot_buffer_from_enabled_flag(bm, op, "face_spans", BM_FACE, FACE_SPAN);
	BMO_slot_buffer_from_enabled_flag(bm, op, "face_holes", BM_FACE, FACE_HOLE);
}
