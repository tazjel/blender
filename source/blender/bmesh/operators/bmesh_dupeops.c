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

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_ghash.h"
#include "BLI_memarena.h"
#include "BLI_array.h"
#include "BLI_math.h"

#include "bmesh.h"
#include "bmesh_operators_private.h"

/* local flag define */
#define DUPE_INPUT		1 /* input from operato */
#define DUPE_NEW		2
#define DUPE_DONE		4
#define DUPE_MAPPED		8

/*
 *  COPY VERTEX
 *
 *   Copy an existing vertex from one bmesh to another.
 *
 */
static BMVert *copy_vertex(BMesh *source_mesh, BMVert *source_vertex, BMesh *target_mesh, GHash *vhash)
{
	BMVert *target_vertex = NULL;

	/* Create a new verte */
	target_vertex = BM_Make_Vert(target_mesh, source_vertex->co,  NULL);
	
	/* Insert new vertex into the vert has */
	BLI_ghash_insert(vhash, source_vertex, target_vertex);
	
	/* Copy attribute */
	BM_Copy_Attributes(source_mesh, target_mesh, source_vertex, target_vertex);
	
	/* Set internal op flag */
	BMO_SetFlag(target_mesh, (BMHeader *)target_vertex, DUPE_NEW);
	
	return target_vertex;
}

/*
 * COPY EDGE
 *
 * Copy an existing edge from one bmesh to another.
 *
 */
static BMEdge *copy_edge(BMOperator *op, BMesh *source_mesh,
			 BMEdge *source_edge, BMesh *target_mesh,
			 GHash *vhash, GHash *ehash)
{
	BMEdge *target_edge = NULL;
	BMVert *target_vert1, *target_vert2;
	BMFace *face;
	BMIter fiter;
	int rlen;

	/* see if any of the neighboring faces are
	 * not being duplicated.  in that case,
	 * add it to the new/old map. */
	rlen = 0;
	for (face = BMIter_New(&fiter, source_mesh, BM_FACES_OF_EDGE, source_edge);
		face; face = BMIter_Step(&fiter)) {
		if (BMO_TestFlag(source_mesh, face, DUPE_INPUT)) {
			rlen++;
		}
	}

	/* Lookup v1 and v2 */
	target_vert1 = BLI_ghash_lookup(vhash, source_edge->v1);
	target_vert2 = BLI_ghash_lookup(vhash, source_edge->v2);
	
	/* Create a new edg */
	target_edge = BM_Make_Edge(target_mesh, target_vert1, target_vert2, NULL, FALSE);
	
	/* add to new/old edge map if necassar */
	if (rlen < 2) {
		/* not sure what non-manifold cases of greater then three
		 * radial should do. */
		BMO_Insert_MapPointer(source_mesh, op, "boundarymap",
		                      source_edge, target_edge);
	}

	/* Insert new edge into the edge hash */
	BLI_ghash_insert(ehash, source_edge, target_edge);
	
	/* Copy attributes */
	BM_Copy_Attributes(source_mesh, target_mesh, source_edge, target_edge);
	
	/* Set internal op flags */
	BMO_SetFlag(target_mesh, (BMHeader *)target_edge, DUPE_NEW);
	
	return target_edge;
}

/*
 * COPY FACE
 *
 *  Copy an existing face from one bmesh to another.
 */

static BMFace *copy_face(BMOperator *op, BMesh *source_mesh,
                         BMFace *source_face, BMesh *target_mesh,
                         BMVert **vtar, BMEdge **edar, GHash *vhash, GHash *ehash)
{
	/* BMVert *target_vert1, *target_vert2; */ /* UNUSED */
	BMLoop *source_loop, *target_loop;
	BMFace *target_face = NULL;
	BMIter iter, iter2;
	int i;
	
	/* lookup the first and second vert */
#if 0 /* UNUSED */
	target_vert1 = BLI_ghash_lookup(vhash, BMIter_New(&iter, source_mesh, BM_VERTS_OF_FACE, source_face));
	target_vert2 = BLI_ghash_lookup(vhash, BMIter_Step(&iter));
#else
	BMIter_New(&iter, source_mesh, BM_VERTS_OF_FACE, source_face);
	BMIter_Step(&iter);
#endif

	/* lookup edge */
	for (i = 0, source_loop = BMIter_New(&iter, source_mesh, BM_LOOPS_OF_FACE, source_face);
	     source_loop;
	     source_loop = BMIter_Step(&iter), i++)
	{
		vtar[i] = BLI_ghash_lookup(vhash, source_loop->v);
		edar[i] = BLI_ghash_lookup(ehash, source_loop->e);
	}
	
	/* create new fac */
	target_face = BM_Make_Face(target_mesh, vtar, edar, source_face->len, FALSE);
	BMO_Insert_MapPointer(source_mesh, op,
	                      "facemap", source_face, target_face);
	BMO_Insert_MapPointer(source_mesh, op,
	                      "facemap", target_face, source_face);

	BM_Copy_Attributes(source_mesh, target_mesh, source_face, target_face);

	/* mark the face for outpu */
	BMO_SetFlag(target_mesh, (BMHeader *)target_face, DUPE_NEW);
	
	/* copy per-loop custom dat */
	BM_ITER(source_loop, &iter, source_mesh, BM_LOOPS_OF_FACE, source_face) {
		BM_ITER(target_loop, &iter2, target_mesh, BM_LOOPS_OF_FACE, target_face) {
			if (BLI_ghash_lookup(vhash, source_loop->v) == target_loop->v) {
				BM_Copy_Attributes(source_mesh, target_mesh, source_loop, target_loop);
				break;
			}
		}
	}

	return target_face;
}

/*
 * COPY MESH
 *
 * Internal Copy function.
 */
static void copy_mesh(BMOperator *op, BMesh *source, BMesh *target)
{

	BMVert *v = NULL, *v2;
	BMEdge *e = NULL;
	BMFace *f = NULL;

	BLI_array_declare(vtar);
	BLI_array_declare(edar);
	BMVert **vtar = NULL;
	BMEdge **edar = NULL;
	
	BMIter verts;
	BMIter edges;
	BMIter faces;
	
	GHash *vhash;
	GHash *ehash;

	/* initialize pointer hashe */
	vhash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "bmesh dupeops v");
	ehash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "bmesh dupeops e");
	
	for (v = BMIter_New(&verts, source, BM_VERTS_OF_MESH, source); v; v = BMIter_Step(&verts)) {
		if (BMO_TestFlag(source, (BMHeader *)v, DUPE_INPUT) && (!BMO_TestFlag(source, (BMHeader *)v, DUPE_DONE))) {
			BMIter iter;
			int iso = 1;

			v2 = copy_vertex(source, v, target, vhash);

			BM_ITER(f, &iter, source, BM_FACES_OF_VERT, v) {
				if (BMO_TestFlag(source, f, DUPE_INPUT)) {
					iso = 0;
					break;
				}
			}

			if (iso) {
				BM_ITER(e, &iter, source, BM_EDGES_OF_VERT, v) {
					if (BMO_TestFlag(source, e, DUPE_INPUT)) {
						iso = 0;
						break;
					}
				}
			}

			if (iso) {
				BMO_Insert_MapPointer(source, op, "isovertmap", v, v2);
			}

			BMO_SetFlag(source, (BMHeader *)v, DUPE_DONE);
		}
	}

	/* now we dupe all the edge */
	for (e = BMIter_New(&edges, source, BM_EDGES_OF_MESH, source); e; e = BMIter_Step(&edges)) {
		if (BMO_TestFlag(source, (BMHeader *)e, DUPE_INPUT) && (!BMO_TestFlag(source, (BMHeader *)e, DUPE_DONE))) {
			/* make sure that verts are copie */
			if (!BMO_TestFlag(source, (BMHeader *)e->v1, DUPE_DONE)) {
				copy_vertex(source, e->v1, target, vhash);
				BMO_SetFlag(source, (BMHeader *)e->v1, DUPE_DONE);
			}
			if (!BMO_TestFlag(source, (BMHeader *)e->v2, DUPE_DONE)) {
				copy_vertex(source, e->v2, target, vhash);
				BMO_SetFlag(source, (BMHeader *)e->v2, DUPE_DONE);
			}
			/* now copy the actual edg */
			copy_edge(op, source, e, target,  vhash,  ehash);
			BMO_SetFlag(source, (BMHeader *)e, DUPE_DONE);
		}
	}

	/* first we dupe all flagged faces and their elements from sourc */
	for (f = BMIter_New(&faces, source, BM_FACES_OF_MESH, source); f; f = BMIter_Step(&faces)) {
		if (BMO_TestFlag(source, (BMHeader *)f, DUPE_INPUT)) {
			/* vertex pas */
			for (v = BMIter_New(&verts, source, BM_VERTS_OF_FACE, f); v; v = BMIter_Step(&verts)) {
				if (!BMO_TestFlag(source, (BMHeader *)v, DUPE_DONE)) {
					copy_vertex(source, v, target, vhash);
					BMO_SetFlag(source, (BMHeader *)v, DUPE_DONE);
				}
			}

			/* edge pas */
			for (e = BMIter_New(&edges, source, BM_EDGES_OF_FACE, f); e; e = BMIter_Step(&edges)) {
				if (!BMO_TestFlag(source, (BMHeader *)e, DUPE_DONE)) {
					copy_edge(op, source, e, target,  vhash,  ehash);
					BMO_SetFlag(source, (BMHeader *)e, DUPE_DONE);
				}
			}

			/* ensure arrays are the right size */
			BLI_array_empty(vtar);
			BLI_array_empty(edar);

			BLI_array_growitems(vtar, f->len);
			BLI_array_growitems(edar, f->len);

			copy_face(op, source, f, target, vtar, edar, vhash, ehash);
			BMO_SetFlag(source, (BMHeader *)f, DUPE_DONE);
		}
	}
	
	/* free pointer hashe */
	BLI_ghash_free(vhash, NULL, NULL);
	BLI_ghash_free(ehash, NULL, NULL);

	BLI_array_free(vtar); /* free vert pointer array */
	BLI_array_free(edar); /* free edge pointer array */
}

/*
 * Duplicate Operator
 *
 * Duplicates verts, edges and faces of a mesh.
 *
 * INPUT SLOTS:
 *
 * BMOP_DUPE_VINPUT: Buffer containing pointers to mesh vertices to be duplicated
 * BMOP_DUPE_EINPUT: Buffer containing pointers to mesh edges to be duplicated
 * BMOP_DUPE_FINPUT: Buffer containing pointers to mesh faces to be duplicated
 *
 * OUTPUT SLOTS:
 *
 * BMOP_DUPE_VORIGINAL: Buffer containing pointers to the original mesh vertices
 * BMOP_DUPE_EORIGINAL: Buffer containing pointers to the original mesh edges
 * BMOP_DUPE_FORIGINAL: Buffer containing pointers to the original mesh faces
 * BMOP_DUPE_VNEW: Buffer containing pointers to the new mesh vertices
 * BMOP_DUPE_ENEW: Buffer containing pointers to the new mesh edges
 * BMOP_DUPE_FNEW: Buffer containing pointers to the new mesh faces
 *
 */

void dupeop_exec(BMesh *bm, BMOperator *op)
{
	BMOperator *dupeop = op;
	BMesh *bm2 = BMO_Get_Pnt(op, "dest");
	
	if (!bm2)
		bm2 = bm;
		
	/* flag inpu */
	BMO_Flag_Buffer(bm, dupeop, "geom", DUPE_INPUT, BM_ALL);

	/* use the internal copy functio */
	copy_mesh(dupeop, bm, bm2);
	
	/* Outpu */
	/* First copy the input buffers to output buffers - original dat */
	BMO_CopySlot(dupeop, dupeop, "geom", "origout");

	/* Now alloc the new output buffer */
	BMO_Flag_To_Slot(bm, dupeop, "newout", DUPE_NEW, BM_ALL);
}

/* executes the duplicate operation, feeding elements of
 * type flag etypeflag and header flag flag to it.  note,
 * to get more useful information (such as the mapping from
 * original to new elements) you should run the dupe op manually */
void BMOP_DupeFromFlag(BMesh *bm, int etypeflag, const char hflag)
{
	BMOperator dupeop;

	BMO_Init_Op(bm, &dupeop, "dupe");
	BMO_HeaderFlag_To_Slot(bm, &dupeop, "geom", hflag, etypeflag);

	BMO_Exec_Op(bm, &dupeop);
	BMO_Finish_Op(bm, &dupeop);
}

/*
 * Split Operator
 *
 * Duplicates verts, edges and faces of a mesh but also deletes the originals.
 *
 * INPUT SLOTS:
 *
 * BMOP_DUPE_VINPUT: Buffer containing pointers to mesh vertices to be split
 * BMOP_DUPE_EINPUT: Buffer containing pointers to mesh edges to be split
 * BMOP_DUPE_FINPUT: Buffer containing pointers to mesh faces to be split
 *
 * OUTPUT SLOTS:
 *
 * BMOP_DUPE_VOUTPUT: Buffer containing pointers to the split mesh vertices
 * BMOP_DUPE_EOUTPUT: Buffer containing pointers to the split mesh edges
 * BMOP_DUPE_FOUTPUT: Buffer containing pointers to the split mesh faces
 */

#define SPLIT_INPUT	1
void splitop_exec(BMesh *bm, BMOperator *op)
{
	BMOperator *splitop = op;
	BMOperator dupeop;
	BMOperator delop;
	BMVert *v;
	BMEdge *e;
	BMFace *f;
	BMIter iter, iter2;
	int found;

	/* initialize our sub-operator */
	BMO_Init_Op(bm, &dupeop, "dupe");
	BMO_Init_Op(bm, &delop, "del");
	
	BMO_CopySlot(splitop, &dupeop, "geom", "geom");
	BMO_Exec_Op(bm, &dupeop);
	
	BMO_Flag_Buffer(bm, splitop, "geom", SPLIT_INPUT, BM_ALL);

	/* make sure to remove edges and verts we don't need */
	for (e = BMIter_New(&iter, bm, BM_EDGES_OF_MESH, NULL); e; e = BMIter_Step(&iter)) {
		found = 0;
		f = BMIter_New(&iter2, bm, BM_FACES_OF_EDGE, e);
		for ( ; f; f = BMIter_Step(&iter2)) {
			if (!BMO_TestFlag(bm, f, SPLIT_INPUT)) {
				found = 1;
				break;
			}
		}
		if (!found) BMO_SetFlag(bm, e, SPLIT_INPUT);
	}
	
	for (v = BMIter_New(&iter, bm, BM_VERTS_OF_MESH, NULL); v; v = BMIter_Step(&iter)) {
		found = 0;
		e = BMIter_New(&iter2, bm, BM_EDGES_OF_VERT, v);
		for ( ; e; e = BMIter_Step(&iter2)) {
			if (!BMO_TestFlag(bm, e, SPLIT_INPUT)) {
				found = 1;
				break;
			}
		}
		if (!found) BMO_SetFlag(bm, v, SPLIT_INPUT);

	}

	/* connect outputs of dupe to delete, exluding keep geometr */
	BMO_Set_Int(&delop, "context", DEL_FACES);
	BMO_Flag_To_Slot(bm, &delop, "geom", SPLIT_INPUT, BM_ALL);
	
	BMO_Exec_Op(bm, &delop);

	/* now we make our outputs by copying the dupe output */
	BMO_CopySlot(&dupeop, splitop, "newout", "geomout");
	BMO_CopySlot(&dupeop, splitop, "boundarymap",
	             "boundarymap");
	BMO_CopySlot(&dupeop, splitop, "isovertmap",
	             "isovertmap");
	
	/* cleanu */
	BMO_Finish_Op(bm, &delop);
	BMO_Finish_Op(bm, &dupeop);
}


void delop_exec(BMesh *bm, BMOperator *op)
{
#define DEL_INPUT 1

	BMOperator *delop = op;

	/* Mark Buffer */
	BMO_Flag_Buffer(bm, delop, "geom", DEL_INPUT, BM_ALL);

	BMO_remove_tagged_context(bm, DEL_INPUT, BMO_Get_Int(op, "context"));

#undef DEL_INPUT
}

/*
 * Spin Operator
 *
 * Extrude or duplicate geometry a number of times,
 * rotating and possibly translating after each step
 */

void spinop_exec(BMesh *bm, BMOperator *op)
{
    BMOperator dupop, extop;
	float cent[3], dvec[3];
	float axis[3] = {0.0f, 0.0f, 1.0f};
	float q[4];
	float rmat[3][3];
	float phi, si;
	int steps, dupli, a, usedvec;

	BMO_Get_Vec(op, "cent", cent);
	BMO_Get_Vec(op, "axis", axis);
	normalize_v3(axis);
	BMO_Get_Vec(op, "dvec", dvec);
	usedvec = !is_zero_v3(dvec);
	steps = BMO_Get_Int(op, "steps");
	phi = BMO_Get_Float(op, "ang") * (float)M_PI / (360.0f * steps);
	dupli = BMO_Get_Int(op, "dupli");

	si = (float)sin(phi);
	q[0] = (float)cos(phi);
	q[1] = axis[0]*si;
	q[2] = axis[1]*si;
	q[3] = axis[2]*si;
	quat_to_mat3(rmat, q);

	BMO_CopySlot(op, op, "geom", "lastout");
	for (a = 0; a < steps; a++) {
		if (dupli) {
			BMO_InitOpf(bm, &dupop, "dupe geom=%s", op, "lastout");
			BMO_Exec_Op(bm, &dupop);
			BMO_CallOpf(bm, "rotate cent=%v mat=%m3 verts=%s",
				cent, rmat, &dupop, "newout");
			BMO_CopySlot(&dupop, op, "newout", "lastout");
			BMO_Finish_Op(bm, &dupop);
		}
		else {
			BMO_InitOpf(bm, &extop, "extrudefaceregion edgefacein=%s",
				op, "lastout");
			BMO_Exec_Op(bm, &extop);
			BMO_CallOpf(bm, "rotate cent=%v mat=%m3 verts=%s",
				cent, rmat, &extop, "geomout");
			BMO_CopySlot(&extop, op, "geomout", "lastout");
			BMO_Finish_Op(bm, &extop);
		}

		if (usedvec)
			BMO_CallOpf(bm, "translate vec=%v verts=%s", dvec, op, "lastout");
	}
}
