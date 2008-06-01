/**
 * BME_mesh.c    jan 2007
 *
 *	BMesh mesh level functions.
 *
 * $Id: BME_eulers.c,v 1.00 2007/01/17 17:42:01 Briggs Exp $
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * about this.	
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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Geoffrey Bantle.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"


#include "BKE_utildefines.h"
#include "BKE_bmesh.h"
#include "BKE_global.h"
#include "BKE_depsgraph.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BIF_editmesh.h"
#include "BIF_space.h"
#include "editmesh.h"
#include "bmesh_private.h"
#include "mydevice.h"

#include "BSE_edit.h"


/*	
 *	BME MAKE MESH
 *
 *  Allocates a new BME_Mesh structure
*/



BME_Mesh *BME_make_mesh(int valloc, int ealloc, int lalloc, int palloc){
	/*allocate the structure*/
	BME_Mesh *bm = MEM_callocN(sizeof(BME_Mesh),"BMesh");
	/*allocate the memory pools for the mesh elements*/
	bm->vpool = BME_mempool_create(sizeof(BME_Vert), valloc, valloc);
	bm->epool = BME_mempool_create(sizeof(BME_Edge), ealloc, ealloc);
	bm->ppool = BME_mempool_create(sizeof(BME_Poly), palloc, palloc);
	bm->lpool = BME_mempool_create(sizeof(BME_Loop), lalloc, lalloc);
	/*allocate the customdata pools*/
	return bm;
}


/*	
 *	BME FREE MESH
 *
 *	Frees a BME_Mesh structure.
*/

void BME_free_mesh(BME_Mesh *bm)
{
	/*destroy element pools*/
	BME_mempool_destroy(bm->vpool);
	BME_mempool_destroy(bm->epool);
	BME_mempool_destroy(bm->ppool);
	BME_mempool_destroy(bm->lpool);
	/*destroy custom data pools*/

	MEM_freeN(bm);	
}

/*	
 *	BME MODEL BEGIN AND END
 *
 *	These two functions represent the 'point of entry' for tools. Every BMesh tool
 *	must begin with a call to BME_model_end() and finish with a call to BME_model_end().
 *	No modification of mesh data is allowed except in between these two calls.
 *
 *	TODO 
 *		FOR BME_MODEL_BEGIN:
 *		-integrate euler undo system.
 *		-make full copy of structure to safely recover from errors.
 *		-accept a toolname string.
 *		-accept param to turn off full copy if just selection tool. (perhaps check for this in eulers...)
 *
 *		BME_MODEL_END:
 *		-full mesh validation if debugging turned on
 *		-free structure copy or use it to restore.
 *		-do euler undo push.
 *
*/

int BME_model_begin(BME_Mesh *bm){
	/*Initialize some scratch pointer arrays used by eulers*/
	bm->vtar = MEM_callocN(sizeof(BME_Vert *) * 1024, "BMesh scratch vert array");
	bm->edar = MEM_callocN(sizeof(BME_Edge *) * 1024, "BMesh scratch edge array");
	bm->lpar = MEM_callocN(sizeof(BME_Loop *) * 1024, "BMesh scratch loop array");
	bm->plar = MEM_callocN(sizeof(BME_Poly *) * 1024, "BMesh scratch poly array");

	bm->vtarlen = bm->edarlen = bm->lparlen = bm->plarlen = 1024;

	return 1;
}

void BME_model_end(BME_Mesh *bm){
	int meshok, totvert, totedge, totpoly, totloop;

	totvert = BLI_countlist(&(bm->verts));
	totedge = BLI_countlist(&(bm->edges));
	totpoly = BLI_countlist(&(bm->polys));

	if(bm->vtar) MEM_freeN(bm->vtar);
	if(bm->edar) MEM_freeN(bm->edar);
	if(bm->lpar) MEM_freeN(bm->lpar);
	if(bm->plar) MEM_freeN(bm->plar);
	
	bm->vtar = NULL;
	bm->edar = NULL;
	bm->lpar = NULL;
	bm->plar = NULL;
	bm->vtarlen = bm->edarlen = bm->lparlen = bm->plarlen = 0;
	
	
	if(bm->totvert!=totvert || bm->totedge!=totedge || bm->totpoly!=totpoly)
		BME_error();
	
	meshok = BME_validate_mesh(bm, 1);
	if(!meshok){
		BME_error();
	}
}

/*	
 *	BME VALIDATE MESH
 *
 *	There are several levels of validation for meshes. At the 
 *  Euler level, some basic validation is done to local topology.
 *  To catch more subtle problems however, BME_validate_mesh() is 
 *  called by BME_model_end() whenever a tool is done executing.
 *  The purpose of this function is to insure that during the course 
 *  of tool execution that nothing has been done to invalidate the 
 *  structure, and if it has, provide a way of reporting that so that
 *  we can restore the proper structure from a backup. Since a full mesh
 *  validation would be too expensive, this is presented as a compromise.
 *
 *	TODO 
 *	
 *	-Write a full mesh validation function for debugging purposes.
 */

#define VHALT(halt) {BME_error(); if(halt) return 0;}

int BME_validate_mesh(struct BME_Mesh *bm, int halt)
{
	BME_Vert *v;
	BME_Edge *e;
	BME_Poly *f;
	BME_Loop *l;
	BME_CycleNode *diskbase;
	int i, ok;
	
	/*Simple edge verification*/
	for(e=bm->edges.first; e; e=e->next){
		if(e->v1 == e->v2) VHALT(halt);
		/*validate e->d1.data and e->d2.data*/
		if(e->d1.data != e || e->d2.data != e) VHALT(halt);
		/*validate e->loop->e*/
		if(e->loop){
			if(e->loop->e != e) VHALT(halt);
		}
	}
	
	/*calculate disk cycle lengths*/
	for(v=bm->verts.first; v; v=v->next) v->tflag1 = v->tflag2 = 0;
	for(e=bm->edges.first; e; e=e->next){ 
		e->v1->tflag1++;
		e->v2->tflag1++;
	}
	/*Validate vertices and disk cycle*/
	for(v=bm->verts.first; v; v=v->next){
		/*validate v->edge pointer*/
		if(v->tflag1){
			if(v->edge){
				ok = BME_vert_in_edge(v->edge,v);
				if(!ok) VHALT(halt);
				/*validate length of disk cycle*/
				diskbase = BME_disk_getpointer(v->edge, v);
				ok = BME_cycle_validate(v->tflag1, diskbase);
				if(!ok) VHALT(halt);
				/*validate that each edge in disk cycle contains V*/
				for(i=0, e=v->edge; i < v->tflag1; i++, e = BME_disk_nextedge(e,v)){
					ok = BME_vert_in_edge(e, v);
					if(!ok) VHALT(halt);
				}
			}
			else VHALT(halt);
		}
	}
	/*validate edges*/
	for(e=bm->edges.first; e; e=e->next){
		/*seperate these into BME_disk_hasedge (takes pointer to edge)*/
		/*search v1 disk cycle for edge*/
		ok = BME_disk_hasedge(e->v1,e);
		if(!ok) VHALT(halt);
		/*search v2 disk cycle for edge*/
		ok = BME_disk_hasedge(e->v2,e);
		if(!ok) VHALT(halt);
	}
	
	for(e=bm->edges.first; e; e=e->next) e->tflag2 = 0; //store incident faces
	/*Validate the loop cycle integrity.*/
	for(f=bm->polys.first; f; f=f->next){
		ok = BME_cycle_length(f->loopbase);
		if(ok > 1){
			f->tflag1 = ok;
		}
		else VHALT(halt);
		for(i=0, l=f->loopbase; i < f->tflag1; i++, l=l->next){
			/*verify loop->v pointers*/
			ok = BME_verts_in_edge(l->v, l->next->v, l->e);
			if(!ok) VHALT(halt);
			/*verify radial node data pointer*/
			if(l->radial.data != l) VHALT(halt);
			/*validate l->e->loop poitner*/
			if(l->e->loop == NULL) VHALT(halt);
			/*validate l->f pointer*/
			if(l->f != f) VHALT(halt);
			/*see if l->e->loop is actually in radial cycle*/
			
			l->e->tflag2++;
		 }
	}
	
	/*validate length of radial cycle*/
	for(e=bm->edges.first; e; e=e->next){
		if(e->loop){
			ok = BME_cycle_validate(e->tflag2,&(e->loop->radial));
			if(!ok) VHALT(halt);
		}
	}
	
	/*validate that EIDs are within range... if not indicates corrupted mem*/

	/*if we get this far, pretty safe to return 1*/
	return 1;
}

/*	Currently just a convient place for a breakpoint.
	Probably should take an error string
*/
void BME_error(void){
	printf("BME modelling error!");
}
