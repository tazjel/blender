/*
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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2006 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Implements the Sculpt Mode tools
 *
 * BDR_sculptmode.h
 *
 */

#include "GHOST_Types.h"

#include "MEM_guardedalloc.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_dynstr.h"

#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_view3d_types.h"
#include "DNA_userdef_types.h"

#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "BIF_glutil.h"
#include "BIF_gl.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_previewrender.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"

#include "BDR_drawobject.h"
#include "BDR_sculptmode.h"

#include "BSE_drawview.h"
#include "BSE_edit.h"
#include "BSE_view.h"

#include "IMB_imbuf_types.h"

#include "blendef.h"
#include "mydevice.h"

#include "RE_render_ext.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ===== STRUCTS =====
 *
 */

/* Used by vertex_users to store face indices in a list */
typedef struct IndexNode {
	struct IndexNode* next,* prev;
	int Index;
} IndexNode;


/* ActiveData stores an Index into the mvert array of Mesh, plus Fade, which
   stores how far the vertex is from the brush center, scaled to the range [0,1]. */
typedef struct ActiveData {
	struct ActiveData *next, *prev;
	unsigned int Index;
	float Fade;
} ActiveData;

typedef struct GrabData {
	char firsttime;
	ListBase active_verts[8];
	unsigned char index;
	vec3f delta, delta_symm;
	float depth;
} GrabData;

typedef struct ProjVert {
	short co[2];
	char inside;
} ProjVert;

typedef struct EditData {
	vec3f center;
	float size;
	char flip;
	short mouse[2];

	/* Normals */
	vec3f up, right;

	GrabData *grabdata;
	float *layer_disps;
	vec3f *layer_store;
} EditData;

typedef struct RectNode {
	struct RectNode *next, *prev;
	rcti r;
} RectNode;

static ProjVert *projverts= NULL;


/* ===== MEMORY =====
 * 
 * Allocate/initialize/free data
 */

void sculptmode_init(Scene *sce)
{
	SculptData *sd;

	if(!sce) {
		error("Unable to initialize sculptmode: bad scene");
		return;
	}

	sd= &sce->sculptdata;

	memset(sd, 0, sizeof(SculptData));

	sd->drawbrush.size=sd->smoothbrush.size=sd->pinchbrush.size=sd->inflatebrush.size=sd->grabbrush.size=sd->layerbrush.size= 50;
	sd->drawbrush.strength=sd->smoothbrush.strength=sd->pinchbrush.strength=sd->inflatebrush.strength=sd->grabbrush.strength=sd->layerbrush.strength= 25;
	sd->drawbrush.dir=sd->pinchbrush.dir=sd->inflatebrush.dir=sd->layerbrush.dir= 1;
	sd->drawbrush.airbrush=sd->smoothbrush.airbrush=sd->pinchbrush.airbrush=sd->inflatebrush.airbrush=sd->layerbrush.airbrush= 0;
	sd->brush_type= DRAW_BRUSH;
	sd->texact= -1;
	sd->texfade= 1;
	sd->averaging= 1;
	sd->texscale= 100;
	sd->texrept= SCULPTREPT_DRAG;
}

/* Free G.sculptdata->vertexusers */
void sculptmode_free_vertexusers(struct Scene *sce)
{
	SculptData *sd;

	if(!sce) return;

	sd= &sce->sculptdata;
	if(sd->vertex_users){
		int i;
		for(i=0; i<sd->vertex_users_size; ++i){
			BLI_freelistN(&sd->vertex_users[i]);
		}
		MEM_freeN(sd->vertex_users);
		sd->vertex_users= NULL;
		sd->vertex_users_size= 0;
	}
}

typedef struct SculptUndoStep {
	struct SculptUndoStep *next, *prev;
	char *str;

	MVert *verts;
	
	MEdge *edges;
	MFace *faces;
	int totvert, totedge, totface;
	
	int pv_stored;
	PartialVisibility *pv;
} SculptUndoStep;
typedef struct SculptUndo {
	ListBase steps;
	SculptUndoStep *cur;
} SculptUndo;

void sculptmode_undo_init()
{
	G.scene->sculptdata.undo= MEM_callocN(sizeof(SculptUndo), "Sculpt Undo");
	sculptmode_undo_push("Original", 1, 1, 1);
}

void sculptmode_undo_free_link(SculptUndoStep *sus)
{
	if(sus->verts)
		MEM_freeN(sus->verts);
	if(sus->edges)
		MEM_freeN(sus->edges);
	if(sus->faces)
		MEM_freeN(sus->faces);
	if(sus->pv) {
		MEM_freeN(sus->pv->vert_map);
		MEM_freeN(sus->pv->edge_map);
		MEM_freeN(sus->pv->old_faces);
		MEM_freeN(sus->pv->old_edges);
		MEM_freeN(sus->pv);
	}
}

void sculptmode_undo_pull_chopped(SculptUndoStep *sus)
{
	SculptUndoStep *f;
	
	for(f= sus; f && !(sus->edges || sus->faces); f= f->prev)
		if(f->edges || f->faces) {
			sus->edges= f->edges;
			f->edges= NULL;
			sus->faces= f->faces;
			f->faces= NULL;
			sus->totvert= f->totvert;
			sus->totedge= f->totedge;
			sus->totface= f->totface;
			break;
		}
	
	for(f= sus; f && !sus->pv_stored; f= f->prev)
		if(f->pv_stored) {
			sus->pv= f->pv;
			f->pv= NULL;
			sus->pv_stored= 1;
			break;
		}
}

void sculptmode_undo_free(Scene *sce)
{
	SculptUndoStep *sus;
	if(!sce->sculptdata.undo) return;
	for(sus= sce->sculptdata.undo->steps.first; sus; sus= sus->next)
		sculptmode_undo_free_link(sus);
	BLI_freelistN(&sce->sculptdata.undo->steps);
	MEM_freeN(sce->sculptdata.undo);
}

PartialVisibility *sculptmode_copy_pmv(PartialVisibility *);
void sculptmode_undo_push(char *str, int verts, int fe, int pv)
{
	int cnt= 7;
	SculptUndo *su= G.scene->sculptdata.undo;
	SculptUndoStep *n= MEM_callocN(sizeof(SculptUndoStep), "SculptUndo"), *sus, *chop, *path;
	Mesh *me= get_mesh(G.scene->sculptdata.active_ob);

	/* Chop off undo data after cur */
	for(sus= su->steps.last; sus && sus != su->cur; sus= path) {
		path= sus->prev;
		sculptmode_undo_free_link(sus);
		BLI_freelinkN(&su->steps, sus);
	}

	/* Initialize undo data */
	n->str= str;
	if(verts)
		n->verts= MEM_dupallocN(me->mvert);
	if(fe) {
		n->edges= MEM_dupallocN(me->medge);
		n->faces= MEM_dupallocN(me->mface);
		n->totvert= me->totvert;
		n->totedge= me->totedge;
		n->totface= me->totface;
	}
	if(pv) {
		n->pv_stored= 1;
		if(me->pv)
			n->pv= sculptmode_copy_pmv(me->pv);
	}

	/* Add new undo step */
	BLI_addtail(&su->steps, n);

	/* Chop off undo steps pass MAXSZ */
	for(chop= su->steps.last; chop && cnt; chop= chop->prev, --cnt);
	if(!cnt && chop) {
		/* Make sure that non-vert data isn't lost */
		sculptmode_undo_pull_chopped(chop);
	
		for(sus= su->steps.first; sus && sus != chop; sus= path) {
			path= sus->next;
			sculptmode_undo_free_link(sus);
			BLI_freelinkN(&su->steps, sus);
		}
	}

	su->cur= n;
}

void sculptmode_undo_update(SculptUndoStep *newcur)
{
	SculptData *sd= &G.scene->sculptdata;
	Mesh *me= get_mesh(sd->active_ob);
	SculptUndoStep *oldcur= sd->undo->cur, *sus;
	Object *ob= sd->active_ob;
	int forward= 0, do_fe, do_pv;
	
	/* No update if undo step hasn't changed */
	if(newcur == oldcur) return;
	/* Discover direction of undo */
	for(sus= oldcur; sus && sus != newcur; sus= sus->next);
	if(sus == newcur) forward= 1;
	
	set_sculpt_object(NULL);

	/* Verts */
	if(newcur->verts) {
		CustomData_free_layer(&me->vdata, CD_MVERT, me->totvert);
		me->mvert= MEM_dupallocN(newcur->verts);
		CustomData_add_layer(&me->vdata, CD_MVERT, 0, me->mvert, newcur->totvert);
	}
	
	/* Check if faces/edges have been modified between oldcur and newcur */
	do_fe= 0;
	for(sus= forward?oldcur->next:newcur->next;
	    sus && sus != (forward?newcur->next:oldcur->next); sus= sus->next)
		if(sus->edges || sus->faces) {
			do_fe= 1;
			break;
		}
	do_pv= 0;
	for(sus= forward?oldcur->next:newcur->next;
	    sus && sus != (forward?newcur->next:oldcur->next); sus= sus->next)
		if(sus->pv_stored) {
			do_pv= 1;
			break;
		}
	
	if(do_fe)
		for(sus= newcur; sus; sus= sus->prev) {
			if(sus->edges || sus->faces) {
				CustomData_free_layer(&me->edata, CD_MEDGE, me->totedge);
				CustomData_free_layer(&me->fdata, CD_MFACE, me->totface);

				me->medge= MEM_dupallocN(sus->edges);
				me->mface= MEM_dupallocN(sus->faces);
				CustomData_add_layer(&me->edata, CD_MEDGE, 0, me->medge, sus->totedge);
				CustomData_add_layer(&me->fdata, CD_MFACE, 0, me->mface, sus->totface);

				me->totvert= sus->totvert;
				me->totedge= sus->totedge;
				me->totface= sus->totface;
				break;
			}
		}
	
	if(do_pv)
		for(sus= newcur; sus; sus= sus->prev)
			if(sus->pv_stored) {
				if(me->pv) {
					MEM_freeN(me->pv->vert_map);
					MEM_freeN(me->pv->edge_map);
					MEM_freeN(me->pv->old_faces);
					MEM_freeN(me->pv->old_edges);
					MEM_freeN(me->pv);
				}
				me->pv= NULL;
				if(newcur->pv)
					me->pv= sculptmode_copy_pmv(newcur->pv);
			}
	
	sd->undo->cur= newcur;
	
	set_sculpt_object(ob);
	
	if(modifiers_getVirtualModifierList(ob))
		DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);

	if(G.vd->depths) G.vd->depths->damaged= 1;
	allqueue(REDRAWVIEW3D, 0);
}

void sculptmode_undo()
{
	SculptUndo *su= G.scene->sculptdata.undo;

	if(su->cur->prev)
		sculptmode_undo_update(su->cur->prev);
}

void sculptmode_redo()
{
	SculptUndo *su= G.scene->sculptdata.undo;

	if(su->cur->next)
		sculptmode_undo_update(su->cur->next);
}

void sculptmode_undo_menu()
{
	SculptUndo *su= G.scene->sculptdata.undo;
	SculptUndoStep *sus;
	DynStr *ds= BLI_dynstr_new();
	char *menu;
	
	BLI_dynstr_append(ds, "Sculpt Mode Undo History %t");
	for(sus= su->steps.first; sus; sus= sus->next) {
		BLI_dynstr_append(ds, "|");
		BLI_dynstr_append(ds, sus->str);
	}
	menu= BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);
	
	if(menu) {
		short event= pupmenu_col(menu, 20);
		MEM_freeN(menu);

		if(event>0) {
			int a= 1;
			for(sus= su->steps.first; sus; sus= sus->next, a++)
				if(a==event) break;
			sculptmode_undo_update(sus);
		}
	}
}

void sculptmode_free_all(Scene *sce)
{
	SculptData *sd= &sce->sculptdata;
	int a;

	sculptmode_free_vertexusers(sce);
	if(sd->texrndr) {
		if(sd->texrndr->rect) MEM_freeN(sd->texrndr->rect);
		MEM_freeN(sd->texrndr);
	}
	
	for(a=0; a<MAX_MTEX; a++) {
		MTex *mtex= sd->mtex[a];
		if(mtex) {
			if(mtex->tex) mtex->tex->id.us--;
			MEM_freeN(mtex);
		}
	}

	sculptmode_undo_free(sce);
}

void calc_vertex_users()
{
	int i,j;
	IndexNode *node= 0;
	Mesh *me= get_mesh(G.scene->sculptdata.active_ob);

	sculptmode_free_vertexusers(G.scene);

	/* Allocate an array of ListBases, one per vertex */
	G.scene->sculptdata.vertex_users= (ListBase*)MEM_mallocN(sizeof(ListBase) * me->totvert, "vertex_users");
	G.scene->sculptdata.vertex_users_size= me->totvert;

	/* Initialize */
	for(i=0; i<me->totvert; ++i){
		G.scene->sculptdata.vertex_users[i].first=G.scene->sculptdata.vertex_users[i].last= 0;
	}

	/* Find the users */
	for(i=0; i<me->totface; ++i){
		for(j=0; j<(me->mface[i].v4?4:3); ++j){
			node= (IndexNode*)MEM_mallocN(sizeof(IndexNode), "faceindex");
			node->Index=i;
			BLI_addtail(&G.scene->sculptdata.vertex_users[((unsigned int*)(&me->mface[i]))[j]], node);
		}
	}
}

void set_sculpt_object(struct Object *ob)
{
	G.scene->sculptdata.active_ob= ob;

	if(ob)
		calc_vertex_users();
}

/* ===== INTERFACE =====
 */

void sculptmode_rem_tex(void *junk0,void *junk1)
{
	MTex *mtex= G.scene->sculptdata.mtex[G.scene->sculptdata.texact];
	if(mtex) {
		if(mtex->tex) mtex->tex->id.us--;
		MEM_freeN(mtex);
		G.scene->sculptdata.mtex[G.scene->sculptdata.texact]= NULL;
		BIF_undo_push("Unlink brush texture");
		allqueue(REDRAWBUTSEDIT, 0);
		allqueue(REDRAWOOPS, 0);
	}
}

/* ===== OPENGL =====
 *
 * Simple functions to get data from the GL
 */

void init_sculptmatrices()
{
	glPushMatrix();

	glMatrixMode(GL_MODELVIEW);
	glMultMatrixf(OBACT->obmat);

	glGetDoublev(GL_MODELVIEW_MATRIX, G.scene->sculptdata.modelviewmat);
	glGetDoublev(GL_PROJECTION_MATRIX, G.scene->sculptdata.projectionmat);
	glGetIntegerv(GL_VIEWPORT, (GLint *)G.scene->sculptdata.viewport);
	/* Set up viewport so that gluUnProject will give correct values */
	G.scene->sculptdata.viewport[0] = 0;
	G.scene->sculptdata.viewport[1] = 0;

	glPopMatrix();

}

/* Uses window coordinates (x,y) to find the depth in the GL depth buffer */
float get_depth(short x, short y)
{
	float depth;

	if(x<0 || y<0) return 1;
	if(x>=curarea->winx || y>=curarea->winy) return 1;

	if(G.vd->depths && x<G.vd->depths->w && y<G.vd->depths->h)
		return G.vd->depths->depths[y*G.vd->depths->w+x];
	
	x+= curarea->winrct.xmin;
	y+= curarea->winrct.ymin;
	
	glReadPixels(x, y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);

	return depth;
}

/* Uses window coordinates (x,y) and depth component z to find a point in
   modelspace */
vec3f unproject(const short x, const short y, const float z)
{
	double ux, uy, uz;
	vec3f p;

        gluUnProject(x,y,z, G.scene->sculptdata.modelviewmat,
		     G.scene->sculptdata.projectionmat,
		     (GLint *)G.scene->sculptdata.viewport, &ux, &uy, &uz );
	p.x= ux;
	p.y= uy;
	p.z= uz;
	return p;
}

void project(const float v[3], short p[2])
{
	double ux, uy, uz;

	gluProject(v[0],v[1],v[2], G.scene->sculptdata.modelviewmat,
		   G.scene->sculptdata.projectionmat,
		   (GLint *)G.scene->sculptdata.viewport, &ux, &uy, &uz);
	p[0]= ux;
	p[1]= uy;
}

/* ===== Sculpting =====
 *
 */

float brush_strength(EditData *e)
{
	const BrushData* b= sculptmode_brush();
	float dir= b->dir==1 ? 1 : -1;
	float pressure= 1;
	const GHOST_TabletData *td= get_tablet_data();
	float flip= e->flip ? -1:1;

	if(td) {
		switch(td->Active) {
		case 1:
			pressure= td->Pressure;
			break;
		case 2:
			pressure= td->Pressure;
			dir = -dir;
			break;
		default:
			break;
		}
	}

	switch(G.scene->sculptdata.brush_type){
	case DRAW_BRUSH:
	case LAYER_BRUSH:
		return b->strength / 5000.0f * dir * pressure * flip;
	case SMOOTH_BRUSH:
		return b->strength / 50.0f * pressure;
	case PINCH_BRUSH:
		return b->strength / 1000.0f * dir * pressure * flip;
	case GRAB_BRUSH:
		return 1;
	case INFLATE_BRUSH:
		return b->strength / 5000.0f * dir * pressure * flip;
	default:
		return 0;
	}
}

/* Currently only for the draw brush; finds average normal for all active
   vertices */
vec3f calc_area_normal(const ListBase* active_verts)
{
	Mesh *me= get_mesh(G.scene->sculptdata.active_ob);
	vec3f area_normal= {0,0,0};
	ActiveData *node= active_verts->first;

	while(node){
		area_normal.x+= me->mvert[node->Index].no[0];
		area_normal.y+= me->mvert[node->Index].no[1];
		area_normal.z+= me->mvert[node->Index].no[2];
		node= node->next;
	}
	Normalise(&area_normal.x);
	return area_normal;
}
void do_draw_brush(const ListBase* active_verts)
{
	Mesh *me= get_mesh(G.scene->sculptdata.active_ob);
	const vec3f area_normal= calc_area_normal(active_verts);
	ActiveData *node= active_verts->first;

	while(node){
		me->mvert[node->Index].co[0] += area_normal.x * node->Fade;
		me->mvert[node->Index].co[1] += area_normal.y * node->Fade;
		me->mvert[node->Index].co[2] += area_normal.z * node->Fade;
		node= node->next;
	}
}

vec3f neighbor_average(const int vert)
{
	Mesh *me= get_mesh(G.scene->sculptdata.active_ob);
	int i, skip= -1, total=0;
	IndexNode *node= G.scene->sculptdata.vertex_users[vert].first;
	vec3f avg= {0,0,0};
	MFace *f;

	while(node){
		f= &me->mface[node->Index];
		if(f->v4) {
			skip= (f->v1==vert?2:
			       f->v2==vert?3:
			       f->v3==vert?0:
			       f->v4==vert?1:-1);
		}

		for(i=0; i<(f->v4?4:3); ++i) {
			if(i != skip) {
				VecAddf(&avg.x,&avg.x,me->mvert[(&f->v1)[i]].co);
				++total;
			}
		}

		node= node->next;
	}

	avg.x/= total;
	avg.y/= total;
	avg.z/= total;

	return avg;
}

void do_smooth_brush(const ListBase* active_verts)
{
	int cur;
	ActiveData *node= active_verts->first;
	Mesh *me= get_mesh(G.scene->sculptdata.active_ob);
	vec3f avg;

	while(node){
		cur= node->Index;

		if(BLI_countlist(&G.scene->sculptdata.vertex_users[cur]) > 2) {
			avg.x=avg.y=avg.z= 0;
			
			avg= neighbor_average(cur);
			
			me->mvert[cur].co[0]+= (avg.x - me->mvert[cur].co[0])*node->Fade;
			me->mvert[cur].co[1]+= (avg.y - me->mvert[cur].co[1])*node->Fade;
			me->mvert[cur].co[2]+= (avg.z - me->mvert[cur].co[2])*node->Fade;
		}

		node= node->next;
	}
}

void do_pinch_brush(const ListBase* active_verts, const vec3f* center)
{
	Mesh *me= get_mesh(G.scene->sculptdata.active_ob);
 	ActiveData *node= active_verts->first;
	float* co;

	while(node) {
		co= me->mvert[node->Index].co;
		co[0] += (center->x - co[0]) * node->Fade;
		co[1] += (center->y - co[1]) * node->Fade;
		co[2] += (center->z - co[2]) * node->Fade;
		node= node->next;
	}
}

void do_grab_brush(EditData *e)
{
	Mesh *me= get_mesh(G.scene->sculptdata.active_ob);
	ActiveData *node= e->grabdata->active_verts[e->grabdata->index].first;
	float add[3];

	while(node) {
		VecCopyf(add,&e->grabdata->delta_symm.x);
		VecMulf(add,node->Fade);
		VecAddf(me->mvert[node->Index].co,me->mvert[node->Index].co,add);

		node= node->next;
	}
}

void do_layer_brush(EditData *e, const ListBase *active_verts)
{
	Mesh *me= get_mesh(G.scene->sculptdata.active_ob);
	vec3f area_normal= calc_area_normal(active_verts);
	ActiveData *node= active_verts->first;
	const float bstr= brush_strength(e);

	while(node){
		float *disp= &e->layer_disps[node->Index];
		
		if((bstr > 0 && *disp < bstr) ||
		  (bstr < 0 && *disp > bstr)) {
			*disp+= node->Fade;

			if(bstr < 0) {
				if(*disp < bstr)
					*disp = bstr;
			} else {
				if(*disp > bstr)
					*disp = bstr;
			}

			me->mvert[node->Index].co[0]= e->layer_store[node->Index].x + area_normal.x * *disp;
			me->mvert[node->Index].co[1]= e->layer_store[node->Index].y + area_normal.y * *disp;
			me->mvert[node->Index].co[2]= e->layer_store[node->Index].z + area_normal.z * *disp;
		}

		node= node->next;
	}
}

void do_inflate_brush(const ListBase *active_verts)
{
	ActiveData *node= active_verts->first;
	int cur;
	float add[3];
	Mesh *me= get_mesh(G.scene->sculptdata.active_ob);
	
	while(node) {
		cur= node->Index;

		add[0]= me->mvert[cur].no[0]/ 32767.0f;
		add[1]= me->mvert[cur].no[1]/ 32767.0f;
		add[2]= me->mvert[cur].no[2]/ 32767.0f;
		VecMulf(add,node->Fade);
		VecAddf(me->mvert[cur].co,me->mvert[cur].co,add);

		node= node->next;
	}
}

float simple_strength(float p, const float len)
{
	if(p > len) p= len;
	return 0.5f * (cos(3*p/len) + 1);
}

float tex_strength(EditData *e, float *point, const float len,const unsigned vindex)
{
	float avg= 0;

	if(G.scene->sculptdata.texact==-1)
		avg= 1;
	else if(G.scene->sculptdata.texrept==SCULPTREPT_3D) {
		float jnk;
		const float factor= 0.01;
		MTex mtex;
		memset(&mtex,0,sizeof(MTex));
		mtex.tex= G.scene->sculptdata.mtex[G.scene->sculptdata.texact]->tex;
		mtex.projx= 1;
		mtex.projy= 2;
		mtex.projz= 3;
		mtex.size[0]= G.scene->sculptdata.texscale * factor;
		mtex.size[1]= G.scene->sculptdata.texscale * factor;
		mtex.size[2]= G.scene->sculptdata.texscale * factor;
		
		externtex(&mtex,point,&avg,&jnk,&jnk,&jnk,&jnk);
	} else {
		vec3f t2;
		float theta, magn;
		float cx;
		int px, py;
		unsigned i;
		unsigned int *p;
		RenderInfo *ri= G.scene->sculptdata.texrndr;

		/* If no texture or Default, use smooth curve */
		if(G.scene->sculptdata.texact == -1 || !G.scene->sculptdata.mtex[G.scene->sculptdata.texact] ||
		   !G.scene->sculptdata.mtex[G.scene->sculptdata.texact]->tex->type)
			return simple_strength(len,e->size);

		/* Find direction from center to point */
		VecSubf(&t2.x,point,&e->center.x);
		Normalise(&t2.x);

		theta= e->right.x*t2.x+e->right.y*t2.y+e->right.z*t2.z;

		/* Avoid NaN errors */
		if( theta < -1 )
			theta = -1;
		else if( theta > 1 )
			theta = 1;

		theta = acos( theta );

		/* Checks whether theta should be in the III/IV quadrants using the
		   dot product with the Up vector */
		if(e->up.x*t2.x+e->up.y*t2.y+e->up.z*t2.z > 0)
			theta = 2 * M_PI - theta;

		magn= len/e->size;

		/* XXX: This code assumes that the texture can be treated as a square */

		/* Find alpha's center, we assume a square */
		cx= ri->pr_rectx/2.0f;

		/* Scale the magnitude to match the size of the tex */
		magn*= cx;
	
		/* XXX: not sure if this +c business is correct....
	   
		Find the pixel in the tex */
		px= magn * cos(theta) + cx;
		py= magn * sin(theta) + cx;

		if(G.scene->sculptdata.texrept==SCULPTREPT_TILE) {
			const float scale= G.scene->sculptdata.texscale;
			px+= e->mouse[0];
			py+= e->mouse[1];
			px%= (int)scale;
			py%= (int)scale;
			p= ri->rect + (int)(ri->pr_recty*py/scale) * ri->pr_rectx + (int)(ri->pr_rectx*px/scale);
		}
		else p= ri->rect + py * ri->pr_rectx + px;
		
		for(i=0; i<3; ++i)
			avg+= ((unsigned char*)(p))[i] / 255.0f;

		avg/= 3;
	}

	if(G.scene->sculptdata.texfade)
		avg*= simple_strength(len,e->size); /* Smooth curve */

	return avg;
}

void sculptmode_add_damaged_rect(EditData *e, ListBase *damaged_rects)
{
	short p[2];
	const float radius= sculptmode_brush()->size;
	RectNode *rn= MEM_mallocN(sizeof(RectNode),"RectNode");
	Mesh *me= get_mesh(G.scene->sculptdata.active_ob);
	unsigned i;

	/* Find center */
	project(&e->center.x, p);
	rn->r.xmin= p[0]-radius;
	rn->r.ymin= p[1]-radius;
	rn->r.xmax= p[0]+radius;
	rn->r.ymax= p[1]+radius;

	BLI_addtail(damaged_rects,rn);

	/* Update insides */
	for(i=0; i<me->totvert; ++i) {
		if(!projverts[i].inside) {
			if(projverts[i].co[0] > rn->r.xmin && projverts[i].co[1] > rn->r.ymin &&
			   projverts[i].co[0] < rn->r.xmax && projverts[i].co[1] < rn->r.ymax) {
				projverts[i].inside= 1;
			}
		}
	}
}

void do_brush_action(float *vertexcosnos, EditData e,
		     ListBase *damaged_verts, ListBase *damaged_rects)
{
	int i;
	float av_dist;
	ListBase active_verts={0,0};
	ActiveData *adata= 0;
	float *vert;
	Mesh *me= get_mesh(G.scene->sculptdata.active_ob);
	const float bstrength= brush_strength(&e);

	sculptmode_add_damaged_rect(&e,damaged_rects);

	if(!e.grabdata || (e.grabdata && e.grabdata->firsttime)) {
		/* Find active vertices */
		for(i=0; i<me->totvert; ++i)
		{
			if(projverts[i].inside) {
				vert= vertexcosnos ? &vertexcosnos[i*6] : me->mvert[i].co;
				av_dist= VecLenf(&e.center.x,vert);
				if( av_dist < e.size )
				{
					adata= (ActiveData*)MEM_mallocN(sizeof(ActiveData), "ActiveData");
					adata->Index = i;
					adata->Fade= tex_strength(&e,vert,av_dist,i) * bstrength;
					if(e.grabdata && e.grabdata->firsttime)
						BLI_addtail(&e.grabdata->active_verts[e.grabdata->index], adata);
					else
						BLI_addtail(&active_verts, adata);
				}
			}
		}
	}

	switch(G.scene->sculptdata.brush_type){
	case DRAW_BRUSH:
		do_draw_brush(&active_verts);
		break;
	case SMOOTH_BRUSH:
		do_smooth_brush(&active_verts);
		break;
	case PINCH_BRUSH:
		do_pinch_brush(&active_verts, &e.center);
		break;
	case INFLATE_BRUSH:
		do_inflate_brush(&active_verts);
		break;
	case GRAB_BRUSH:
		do_grab_brush(&e);
		break;
	case LAYER_BRUSH:
		do_layer_brush(&e, &active_verts);
		break;
	}

	if(vertexcosnos)
		BLI_freelistN(&active_verts);
	else {
		if(!e.grabdata)
			addlisttolist(damaged_verts, &active_verts);
	}
}

EditData flip_editdata(EditData *e, short x, short y, short z)
{
	EditData fe= *e;
	GrabData *gd= fe.grabdata;
	if(x) {
		fe.center.x= -fe.center.x;
		fe.up.x= -fe.up.x;
		fe.right.x= -fe.right.x;
	}

	if(y) {
		fe.center.y= -fe.center.y;
		fe.up.y= -fe.up.y;
		fe.right.y= -fe.right.y;
	}

	if(z) {
		fe.center.z= -fe.center.z;
		fe.up.z= -fe.up.z;
		fe.right.z= -fe.right.z;
	}

	project(&fe.center.x,fe.mouse);

	if(gd) {
		gd->index= x + y*2 + z*4;
		gd->delta_symm= gd->delta;
		if(x) gd->delta_symm.x= -gd->delta_symm.x;
		if(y) gd->delta_symm.y= -gd->delta_symm.y;
		if(z) gd->delta_symm.z= -gd->delta_symm.z;
	}

	return fe;
}

void do_symmetrical_brush_actions(float *vertexcosnos, EditData *e,
				  ListBase *damaged_verts, ListBase *damaged_rects)
{
	const SculptData *sd= &G.scene->sculptdata;

	do_brush_action(vertexcosnos,flip_editdata(e,0,0,0),damaged_verts,damaged_rects);

	if(sd->symm_x)
		do_brush_action(vertexcosnos,flip_editdata(e,1,0,0),damaged_verts,damaged_rects);
	if(sd->symm_y)
		do_brush_action(vertexcosnos,flip_editdata(e,0,1,0),damaged_verts,damaged_rects);
	if(sd->symm_z)
		do_brush_action(vertexcosnos,flip_editdata(e,0,0,1),damaged_verts,damaged_rects);
	if(sd->symm_x && sd->symm_y)
		do_brush_action(vertexcosnos,flip_editdata(e,1,1,0),damaged_verts,damaged_rects);
	if(sd->symm_x && sd->symm_z)
		do_brush_action(vertexcosnos,flip_editdata(e,1,0,1),damaged_verts,damaged_rects);
	if(sd->symm_y && sd->symm_z)
		do_brush_action(vertexcosnos,flip_editdata(e,0,1,1),damaged_verts,damaged_rects);
	if(sd->symm_x && sd->symm_y && sd->symm_z)
		do_brush_action(vertexcosnos,flip_editdata(e,1,1,1),damaged_verts,damaged_rects);
}

void add_face_normal(vec3f *norm, const MFace* face)
{
	Mesh *me= get_mesh(G.scene->sculptdata.active_ob);

	vec3f c= {me->mvert[face->v1].co[0],me->mvert[face->v1].co[1],me->mvert[face->v1].co[2]};
	vec3f b= {me->mvert[face->v2].co[0],me->mvert[face->v2].co[1],me->mvert[face->v2].co[2]};
	vec3f a= {me->mvert[face->v3].co[0],me->mvert[face->v3].co[1],me->mvert[face->v3].co[2]};
	vec3f s1, s2;

	VecSubf(&s1.x,&a.x,&b.x);
	VecSubf(&s2.x,&c.x,&b.x);

	norm->x+= s1.y * s2.z - s1.z * s2.y;
	norm->y+= s1.z * s2.x - s1.x * s2.z;
	norm->z+= s1.x * s2.y - s1.y * s2.x;
}

void update_damaged_vert(Mesh *me, ListBase *lb)
{
	ActiveData *vert;
       
	for(vert= lb->first; vert; vert= vert->next) {
		vec3f norm= {0,0,0};		
		IndexNode *face= G.scene->sculptdata.vertex_users[vert->Index].first;

		while(face){
			add_face_normal(&norm,&me->mface[face->Index]);
			face= face->next;
		}
		Normalise(&norm.x);
		
		me->mvert[vert->Index].no[0]=norm.x*32767;
		me->mvert[vert->Index].no[1]=norm.y*32767;
		me->mvert[vert->Index].no[2]=norm.z*32767;
	}
}

void calc_damaged_verts(ListBase *damaged_verts, GrabData *grabdata)
{
	Mesh *me= get_mesh(G.scene->sculptdata.active_ob);

	if(grabdata) {
		int i;
		for(i=0; i<8; ++i)
			update_damaged_vert(me,&grabdata->active_verts[i]);
	} else {
		update_damaged_vert(me,damaged_verts);
		BLI_freelistN(damaged_verts);
	}
}

BrushData *sculptmode_brush()
{
	SculptData *sd= &G.scene->sculptdata;
	return (sd->brush_type==DRAW_BRUSH ? &sd->drawbrush :
		sd->brush_type==SMOOTH_BRUSH ? &sd->smoothbrush :
		sd->brush_type==PINCH_BRUSH ? &sd->pinchbrush :
		sd->brush_type==INFLATE_BRUSH ? &sd->inflatebrush :
		sd->brush_type==GRAB_BRUSH ? &sd->grabbrush :
		sd->brush_type==LAYER_BRUSH ? &sd->layerbrush : NULL);
}

void sculptmode_update_tex()
{
	SculptData *sd= &G.scene->sculptdata;
	RenderInfo *ri= sd->texrndr;

	/* Skip Default brush shape and non-textures */
	if(sd->texact == -1 || !sd->mtex[sd->texact]) return;

	if(!ri) {
		ri= MEM_callocN(sizeof(RenderInfo),"brush texture render");
		sd->texrndr= ri;
	}

	if(ri->rect) {
		MEM_freeN(ri->rect);
		ri->rect= NULL;
	}

	ri->curtile= 0;
	ri->tottile= 0;
	if(ri->rect) MEM_freeN(ri->rect);
	ri->rect = NULL;
	ri->pr_rectx = 128; /* FIXME: might want to allow higher/lower sizes */
	ri->pr_recty = 128;

	BIF_previewrender(&sd->mtex[sd->texact]->tex->id, ri, NULL, PR_ICON_RENDER);
}

void init_editdata(SculptData *sd, EditData *e, short *mouse, short *pr_mouse, const char flip)
{
	const float mouse_depth= get_depth(mouse[0],mouse[1]);
	vec3f brush_edge_loc, zero_loc, oldloc;

	e->flip= flip;
	
	/* Convert the location and size of the brush to
	   modelspace coords */
	e->center= unproject(mouse[0],mouse[1],mouse_depth);
	brush_edge_loc= unproject(mouse[0] +
				  sculptmode_brush()->size,mouse[1],
				  mouse_depth);
	e->size= VecLenf(&e->center.x,&brush_edge_loc.x);

	/* Now project the Up and Right normals from view to model coords */
	zero_loc= unproject(0,0,0);
	e->up= unproject(0,-1,0);
	e->right= unproject(1,0,0);
	VecSubf(&e->up.x,&e->up.x,&zero_loc.x);
	VecSubf(&e->right.x,&e->right.x,&zero_loc.x);
	Normalise(&e->up.x);
	Normalise(&e->right.x);

	if(sd->brush_type == GRAB_BRUSH) {
		vec3f gcenter;
		if(!e->grabdata) {
			e->grabdata= MEM_callocN(sizeof(GrabData),"grab data");
			e->grabdata->firsttime= 1;
			e->grabdata->depth= mouse_depth;
		}
		else
			e->grabdata->firsttime= 0;
		
		/* Find the delta */
		gcenter= unproject(mouse[0],mouse[1],e->grabdata->depth);
		oldloc= unproject(pr_mouse[0],pr_mouse[1],e->grabdata->depth);
		VecSubf(&e->grabdata->delta.x,&gcenter.x,&oldloc.x);
	}
	else if(sd->brush_type == LAYER_BRUSH) {
		Mesh *me= get_mesh(sd->active_ob);

		if(!e->layer_disps)
			e->layer_disps= MEM_callocN(sizeof(float)*me->totvert,"Layer disps");
		if(!e->layer_store) {
			unsigned i;
			e->layer_store= MEM_mallocN(sizeof(vec3f)*me->totvert,"Layer store");
			for(i=0; i<me->totvert; ++i)
				VecCopyf(&e->layer_store[i].x,me->mvert[i].co);
		}
	}
}

void sculptmode_set_strength(const int delta)
{
	int val = sculptmode_brush()->strength + delta;
	if(val < 1) val = 1;
	if(val > 100) val = 100;
	sculptmode_brush()->strength= val;
}

void sculptmode_propset_calctex()
{
	PropsetData *pd= G.scene->sculptdata.propset;
	if(pd) {
		int i, j;
		const int tsz = 128;
		float *d;
		if(!pd->texdata) {
			pd->texdata= MEM_mallocN(sizeof(float)*tsz*tsz, "Brush preview");
			if(G.scene->sculptdata.texrept!=SCULPTREPT_3D)
				sculptmode_update_tex();
			for(i=0; i<tsz; ++i)
				for(j=0; j<tsz; ++j)
					pd->texdata[i*tsz+j]= simple_strength(sqrt(pow(i-tsz/2,2)+pow(j-tsz/2,2)),tsz/2);
			if(G.scene->sculptdata.texact != -1 && G.scene->sculptdata.texrndr) {
				for(i=0; i<tsz; ++i)
					for(j=0; j<tsz; ++j) {
						const int col= G.scene->sculptdata.texrndr->rect[i*tsz+j];
						pd->texdata[i*tsz+j]*= (((char*)&col)[0]+((char*)&col)[1]+((char*)&col)[2])/3.0f/255.0f;
					}
			}
		}
		
		/* Adjust alpha with brush strength */
		d= MEM_dupallocN(pd->texdata);
		for(i=0; i<tsz; ++i)
			for(j=0; j<tsz; ++j)
				d[i*tsz+j]*= sculptmode_brush()->strength/200.0f+0.5f;
		
			
		if(!pd->tex)
			glGenTextures(1, (GLint *)&pd->tex);
		glBindTexture(GL_TEXTURE_2D, pd->tex);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, tsz, tsz, 0, GL_ALPHA, GL_FLOAT, d);
		MEM_freeN(d);
	}
}

void sculptmode_propset_end(int cancel)
{
	PropsetData *pd= G.scene->sculptdata.propset;
	if(pd) {
		if(cancel) {
			sculptmode_brush()->size= pd->origsize;
			sculptmode_brush()->strength= pd->origstrength;
		} else {	
			if(pd->mode != PropsetSize)
				sculptmode_brush()->size= pd->origsize;
			if(pd->mode != PropsetStrength)
				sculptmode_brush()->strength= pd->origstrength;
		}
		glDeleteTextures(1, &pd->tex);
		MEM_freeN(pd->texdata);
		MEM_freeN(pd);
		G.scene->sculptdata.propset= NULL;
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSEDIT, 0);
	}
}

void sculptmode_propset_init(unsigned short key)
{
	PropsetData *pd= G.scene->sculptdata.propset;
	
	if(!pd) {
		short mouse[2];
		
		pd= MEM_callocN(sizeof(PropsetData),"PropsetSize");
		G.scene->sculptdata.propset= pd;
		
		getmouseco_areawin(mouse);
		pd->origloc[0]= mouse[0];
		pd->origloc[1]= mouse[1];
		
		pd->origsize= sculptmode_brush()->size;
		pd->origstrength= sculptmode_brush()->strength;
		
		sculptmode_propset_calctex();
	}

	switch(key) {
	case DKEY:
		if(pd->mode == PropsetNone)
			pd->mode= PropsetSize;
		else if(pd->mode == PropsetSize)
			pd->mode= PropsetStrength;
		else
			sculptmode_propset_end(1);
	}
}

void sculptmode_propset(unsigned short event)
{
	PropsetData *pd= G.scene->sculptdata.propset;
	short mouse[2];
	short tmp[2];
	float dist;

	switch(event) {
	case DKEY:
		sculptmode_propset_init(DKEY);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case MOUSEX:
	case MOUSEY:
		getmouseco_areawin(mouse);
		tmp[0]= pd->origloc[0]-mouse[0];
		tmp[1]= pd->origloc[1]-mouse[1];
		dist= sqrt(tmp[0]*tmp[0]+tmp[1]*tmp[1]);
		if(pd->mode == PropsetSize) {
			sculptmode_brush()->size= dist;
			if(sculptmode_brush()->size>200) sculptmode_brush()->size= 200;
		} else if(pd->mode == PropsetStrength) {
			float fin= (200.0f - dist) * 0.5f;
			sculptmode_brush()->strength= fin>=0 ? fin : 0;
			sculptmode_propset_calctex();
		}
		allqueue(REDRAWVIEW3D, 0);
		break;
	/*case WHEELUPMOUSE:
		sculptmode_set_strength(5);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case WHEELDOWNMOUSE:
		sculptmode_set_strength(-5);
		allqueue(REDRAWVIEW3D, 0);
		break;*/
	case ESCKEY:
	case RIGHTMOUSE:
		sculptmode_brush()->size= pd->origsize;
		sculptmode_brush()->strength= pd->origstrength;
	case LEFTMOUSE:
		while(get_mbut()==L_MOUSE);
	case RETKEY:
	case PADENTER:
		sculptmode_propset_end(0);
		break;
	default:
		break;
	};
}

void sculptmode_selectbrush_menu()
{
	SculptData *sd= &G.scene->sculptdata;
	int val;
	
	pupmenu_set_active(sd->brush_type);
	
	val= pupmenu("Select Brush%t|Draw|Smooth|Pinch|Inflate|Grab|Layer");

	if(val>0) {
		sd->brush_type= val;
		
		allqueue(REDRAWVIEW3D, 1);
		allqueue(REDRAWBUTSEDIT, 1);
	}
}

void sculptmode_update_all_projverts()
{
	Mesh *me= get_mesh(G.scene->sculptdata.active_ob);
	unsigned i;

	if(projverts) MEM_freeN(projverts);
	projverts= MEM_mallocN(sizeof(ProjVert)*me->totvert,"ProjVerts");
	for(i=0; i<me->totvert; ++i) {
		project(me->mvert[i].co, projverts[i].co);
		projverts[i].inside= 0;
	}
}

void sculptmode_draw_wires(char only_damaged, Mesh *me)
{
	int i;

	bglPolygonOffset(1.0);
	glDepthMask(0);
	BIF_ThemeColor((G.scene->sculptdata.active_ob==OBACT)?TH_ACTIVE:TH_SELECT);

	for(i=0; i<me->totedge; i++) {
		MEdge *med= &me->medge[i];

		if((!only_damaged || (projverts[med->v1].inside || projverts[med->v2].inside)) &&
		   (med->flag & ME_EDGEDRAW)) {
			glDrawElements(GL_LINES, 2, GL_UNSIGNED_INT, &med->v1);
		}
	}

	glDepthMask(1);
	bglPolygonOffset(0.0);
}

void sculptmode_draw_mesh(ListBase *damaged_rects) {
	Mesh *me= get_mesh(G.scene->sculptdata.active_ob);
	SculptData *sd= &G.scene->sculptdata;
	int i, j, dt;

	persp(PERSP_VIEW);
	mymultmatrix(sd->active_ob->obmat);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	init_gl_materials(sd->active_ob, 0);

	glShadeModel(GL_SMOOTH);

	glVertexPointer(3, GL_FLOAT, sizeof(MVert), &me->mvert[0].co);
	glNormalPointer(GL_SHORT, sizeof(MVert), &me->mvert[0].no);

	dt= MIN2(G.vd->drawtype, G.scene->sculptdata.active_ob->dt);
	if(dt==OB_WIRE)
		glColorMask(0,0,0,0);

	/* Only draw faces within the modified areas of the screen */
	if(damaged_rects) {
		for(i=0; i<me->totface; ++i) {
			MFace *f= &me->mface[i];
			char inside= 0;
			for(j=0; j<(f->v4?4:3); ++j) {
				if(projverts[*((&f->v1)+j)].inside) {
					inside= 1;
					break;
				}
			}
			if(inside)
				glDrawElements(f->v4?GL_QUADS:GL_TRIANGLES,f->v4?4:3,GL_UNSIGNED_INT,&f->v1);
		}
		glEnd();
	}
	else { /* Draw entire model */
		for(i=0; i<me->totface; ++i) {
			const char q= me->mface[i].v4?1:0;
			glDrawElements(q?GL_QUADS:GL_TRIANGLES,q?4:3,GL_UNSIGNED_INT,&me->mface[i].v1);
		}
	}

	glDisable(GL_LIGHTING);
	glColorMask(1,1,1,1);

	if(dt==OB_WIRE || (sd->active_ob->dtx & OB_DRAWWIRE))
		sculptmode_draw_wires(0, me);

	glDisable(GL_DEPTH_TEST);
}

void sculptmode_correct_state()
{
	if(get_mesh(G.scene->sculptdata.active_ob) != get_mesh(OBACT))
		set_sculpt_object(OBACT);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	
	if(!G.scene->sculptdata.vertex_users) calc_vertex_users();
	if(!G.scene->sculptdata.undo) sculptmode_undo_init();
}

void sculpt()
{
	Object *ob= 0;
	short mouse[2], mvalo[2], firsttime=1, mousebut;
	ListBase damaged_verts= {0,0};
	ListBase damaged_rects= {0,0};
	float *vertexcosnos= 0;
	short modifier_calculations= 0;
	EditData e;
	RectNode *rn= NULL;
	SculptData *sd= &G.scene->sculptdata;
	short spacing= 32000;

	if((G.f & G_SCULPTMODE)==0) return;
	if(G.obedit) return;
	
	ob= OBACT;
	if(ob->id.lib) return;

	/* Make sure that the active mesh is set correctly */
	if(get_mesh(G.scene->sculptdata.active_ob) != get_mesh(ob))
		set_sculpt_object(ob);
		
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);

	if(!G.scene->sculptdata.active_ob || !get_mesh(G.scene->sculptdata.active_ob) ||
	   get_mesh(G.scene->sculptdata.active_ob)->totface==0) return;

	if(ob->lay & G.vd->lay); else error("Active object is not in this layer");

	persp(PERSP_VIEW);
	
	getmouseco_areawin(mvalo);

	/* Make sure sculptdata has been init'd properly */
	if(!G.scene->sculptdata.vertex_users) calc_vertex_users();
	if(!G.scene->sculptdata.undo) sculptmode_undo_init();
	
	/* Init texture
	   FIXME: Shouldn't be doing this every time! */
	if(sd->texrept!=SCULPTREPT_3D)
		sculptmode_update_tex();

	getmouseco_areawin(mouse);
	mvalo[0]= mouse[0];
	mvalo[1]= mouse[1];

	if (U.flag & USER_LMOUSESELECT) mousebut = R_MOUSE;
	else mousebut = L_MOUSE;

	/* If modifier_calculations is true, then extra time must be spent
	   updating the mesh. This takes a *lot* longer, so it's worth
	   skipping if the modifier stack is empty. */
	modifier_calculations= modifiers_getVirtualModifierList(ob) != NULL;

	init_sculptmatrices();

	sculptmode_update_all_projverts();

	e.grabdata= NULL;
	e.layer_disps= NULL;
	e.layer_store= NULL;

	/* Capture original copy */
	glAccum(GL_LOAD, 1);

	while (get_mbut() & mousebut) {
		getmouseco_areawin(mouse);
		
		if(firsttime || mouse[0]!=mvalo[0] || mouse[1]!=mvalo[1] || sculptmode_brush()->airbrush) {
			firsttime= 0;

			spacing+= sqrt(pow(mvalo[0]-mouse[0],2)+pow(mvalo[1]-mouse[1],2));

			if(modifier_calculations)
				vertexcosnos= mesh_get_mapped_verts_nors(ob);

			if(G.scene->sculptdata.brush_type != GRAB_BRUSH && (sd->spacing==0 || spacing>sd->spacing)) {
				char i;
				float t= G.scene->sculptdata.averaging-1;
				const float sub= 1/(t+1);
				t/= (t+1);
				for(i=0; i<G.scene->sculptdata.averaging; ++i) {
					short avgco[2]= {mvalo[0]*t+mouse[0]*(1-t),
					                 mvalo[1]*t+mouse[1]*(1-t)};
					
					init_editdata(&G.scene->sculptdata,&e,avgco,mvalo,get_qual()==LR_SHIFTKEY);
					
					if(get_depth(mouse[0],mouse[1]) < 1.0)
						G.scene->sculptdata.pivot= e.center;
					
					/* The brush always has at least one area it affects,
					   right beneath the mouse. It can have up to seven
					   other areas that must also be modified, if all three
					   axes of symmetry are on. */
					do_symmetrical_brush_actions(vertexcosnos,&e,&damaged_verts,&damaged_rects);

					t-= sub;
				}
				spacing= 0;
			}
			else if(sd->brush_type==GRAB_BRUSH) {
				init_editdata(&G.scene->sculptdata,&e,mouse,mvalo,0);
				G.scene->sculptdata.pivot= unproject(mouse[0],mouse[1],e.grabdata->depth);
				do_symmetrical_brush_actions(vertexcosnos,&e,&damaged_verts,&damaged_rects);
			}
			
			if(modifier_calculations)
				DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);

			if(modifier_calculations || sd->brush_type == GRAB_BRUSH) {
				calc_damaged_verts(&damaged_verts,e.grabdata);

				scrarea_do_windraw(curarea);
				screen_swapbuffers();
			} else {
				calc_damaged_verts(&damaged_verts,e.grabdata);

				for(rn=damaged_rects.first; rn; rn= rn->next) {
					float col[3];
					rcti clp= rn->r;
					rcti *win= &curarea->winrct;
					
					clp.xmin+= win->xmin;
					clp.xmax+= win->xmin;
					clp.ymin+= win->ymin;
					clp.ymax+= win->ymin;
					
					if(clp.xmin<win->xmax && clp.xmax>win->xmin &&
					   clp.ymin<win->ymax && clp.ymax>win->ymin) {
						if(clp.xmin<win->xmin) clp.xmin= win->xmin;
						if(clp.ymin<win->ymin) clp.ymin= win->ymin;
						if(clp.xmax>win->xmax) clp.xmax= win->xmax;
						if(clp.ymax>win->ymax) clp.ymax= win->ymax;
						glScissor(clp.xmin+1, clp.ymin+1,
							  clp.xmax-clp.xmin-2,clp.ymax-clp.ymin-2);
					}
					
					BIF_GetThemeColor3fv(TH_BACK, col);
					glClearColor(col[0], col[1], col[2], 0.0);
					glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
				}
				glDisable(GL_SCISSOR_TEST);
				
				sculptmode_draw_mesh(&damaged_rects);

				glAccum(GL_LOAD, 1);
				
				/* Draw cursor */
				persp(PERSP_WIN);
				glDisable(GL_DEPTH_TEST);
				fdrawXORcirc((float)mouse[0],(float)mouse[1],sculptmode_brush()->size);
				glRasterPos2i(0, 0);
				myswapbuffers();
				glAccum(GL_RETURN, 1);
				
				glEnable(GL_SCISSOR_TEST);
			}

			BLI_freelistN(&damaged_rects);
	
			mvalo[0]= mouse[0];
			mvalo[1]= mouse[1];

			if(modifier_calculations)
				MEM_freeN(vertexcosnos);
		}
		else BIF_wait_for_statechange();
	}

	if(projverts) MEM_freeN(projverts);
	projverts= NULL;
	if(e.layer_disps) MEM_freeN(e.layer_disps);
	if(e.layer_store) MEM_freeN(e.layer_store);
	/* Free GrabData */
	if(e.grabdata) {
		int i;
		for(i=0; i<8; ++i)
			BLI_freelistN(&e.grabdata->active_verts[i]);
		MEM_freeN(e.grabdata);
	}

	switch(G.scene->sculptdata.brush_type) {
	case DRAW_BRUSH:
		sculptmode_undo_push("Draw Brush", 1,0,0); break;
	case SMOOTH_BRUSH:
		sculptmode_undo_push("Smooth Brush", 1,0,0); break;
	case PINCH_BRUSH:
		sculptmode_undo_push("Pinch Brush", 1,0,0); break;
	case INFLATE_BRUSH:
		sculptmode_undo_push("Inflate Brush", 1,0,0); break;
	case GRAB_BRUSH:
		sculptmode_undo_push("Grab Brush", 1,0,0); break;
	case LAYER_BRUSH:
		sculptmode_undo_push("Layer Brush", 1,0,0); break;
	default:
		sculptmode_undo_push("Sculpting", 1,0,0); break;
	}

	if(G.vd->depths) G.vd->depths->damaged= 1;
	
	allqueue(REDRAWVIEW3D, 0);
}

void set_sculptmode()
{
	if(G.f & G_SCULPTMODE) {
		G.f &= ~G_SCULPTMODE;

		set_sculpt_object(NULL);

		sculptmode_undo_free(G.scene);

		DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);
	} else {
		G.f |= G_SCULPTMODE;

		if(!sculptmode_brush())
			sculptmode_init(G.scene);

		if(G.vd->twflag) G.vd->twflag= 0;

		set_sculpt_object(OBACT);

		sculptmode_undo_init();
		
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_NORMAL_ARRAY);

		allqueue(REDRAWVIEW3D,0);
	}

	allqueue(REDRAWBUTSEDIT, 0);
}

/* Partial Mesh Visibility */
PartialVisibility *sculptmode_copy_pmv(PartialVisibility *pmv)
{
	PartialVisibility *n= MEM_dupallocN(pmv);
	n->vert_map= MEM_dupallocN(pmv->vert_map);
	n->edge_map= MEM_dupallocN(pmv->edge_map);
	n->old_edges= MEM_dupallocN(pmv->old_edges);
	n->old_faces= MEM_dupallocN(pmv->old_faces);
	return n;
}

void sculptmode_revert_pmv(Mesh *me)
{
	if(me->pv) {
		unsigned i;
		MVert *nve, *old_verts;
		Object *ob= G.scene->sculptdata.active_ob;

		/* Temporarily exit sculptmode */
		set_sculpt_object(NULL);

		/* Reorder vertices */
		nve= me->mvert;
		old_verts = MEM_mallocN(sizeof(MVert)*me->pv->totvert,"PMV revert verts");
		for(i=0; i<me->pv->totvert; ++i)
			old_verts[i]= nve[me->pv->vert_map[i]];

		/* Restore verts, edges and faces */
		CustomData_free_layer(&me->vdata, CD_MVERT, me->totvert);
		CustomData_free_layer(&me->edata, CD_MEDGE, me->totedge);
		CustomData_free_layer(&me->fdata, CD_MFACE, me->totface);

		me->mvert= CustomData_add_layer(&me->vdata, CD_MVERT, 0, old_verts, me->pv->totvert);
		me->medge= CustomData_add_layer(&me->edata, CD_MEDGE, 0, me->pv->old_edges, me->pv->totedge);
		me->mface= CustomData_add_layer(&me->fdata, CD_MFACE, 0, me->pv->old_faces, me->pv->totface);

		me->totvert= me->pv->totvert;
		me->totedge= me->pv->totedge;
		me->totface= me->pv->totface;

		me->pv->old_edges= NULL;
		me->pv->old_faces= NULL;

		/* Free maps */
		MEM_freeN(me->pv->edge_map);
		me->pv->edge_map= NULL;
		MEM_freeN(me->pv->vert_map);
		me->pv->vert_map= NULL;
		
		set_sculpt_object(ob);

		DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);
	}
}

void sculptmode_pmv_off(Mesh *me)
{
	if(me->pv) {
		sculptmode_revert_pmv(me);
		MEM_freeN(me->pv);
		me->pv= NULL;
	}
}

/* mode: 0=hide outside selection, 1=hide inside selection */
void sculptmode_do_pmv(Object *ob, rcti *hb_2d, int mode)
{
	Mesh *me= get_mesh(ob);
	vec3f hidebox[6];
	vec3f plane_normals[4];
	float plane_ds[4];
	unsigned i, j;
	unsigned ndx_show, ndx_hide;
	MVert *nve;
	unsigned face_cnt_show= 0, face_ndx_show= 0;
	unsigned edge_cnt_show= 0, edge_ndx_show= 0;
	unsigned *old_map= NULL;
	const unsigned SHOW= 0, HIDE=1;

	/* Convert hide box from 2D to 3D */
	hidebox[0]= unproject(hb_2d->xmin, hb_2d->ymax, 1);
	hidebox[1]= unproject(hb_2d->xmax, hb_2d->ymax, 1);
	hidebox[2]= unproject(hb_2d->xmax, hb_2d->ymin, 1);
	hidebox[3]= unproject(hb_2d->xmin, hb_2d->ymin, 1);
	hidebox[4]= unproject(hb_2d->xmin, hb_2d->ymax, 0);
	hidebox[5]= unproject(hb_2d->xmax, hb_2d->ymin, 0);
	
	/* Calculate normals for each side of hide box */
	CalcNormFloat(&hidebox[0].x,&hidebox[1].x,&hidebox[4].x,&plane_normals[0].x);
	CalcNormFloat(&hidebox[1].x,&hidebox[2].x,&hidebox[5].x,&plane_normals[1].x);
	CalcNormFloat(&hidebox[2].x,&hidebox[3].x,&hidebox[5].x,&plane_normals[2].x);
	CalcNormFloat(&hidebox[3].x,&hidebox[0].x,&hidebox[4].x,&plane_normals[3].x);
	
	/* Calculate D for each side of hide box */
	for(i= 0; i<4; ++i)
		plane_ds[i]= hidebox[i].x*plane_normals[i].x + hidebox[i].y*plane_normals[i].y +
			hidebox[i].z*plane_normals[i].z;
	
	/* Add partial visibility to mesh */
	if(!me->pv) {
		me->pv= MEM_callocN(sizeof(PartialVisibility),"PartialVisibility");
	} else {
		old_map= MEM_callocN(sizeof(unsigned)*me->pv->totvert,"PMV oldmap");
		for(i=0; i<me->pv->totvert; ++i) {
			old_map[i]= me->pv->vert_map[i]<me->totvert?0:1;
		}
		sculptmode_revert_pmv(me);
	}
	
	/* Kill sculpt data */
	set_sculpt_object(NULL);
	
	/* Initalize map with which verts are to be hidden */
	me->pv->vert_map= MEM_mallocN(sizeof(unsigned)*me->totvert, "PMV vertmap");
	me->pv->totvert= me->totvert;
	me->totvert= 0;
	for(i=0; i<me->pv->totvert; ++i) {
		me->pv->vert_map[i]= mode ? HIDE:SHOW;
		for(j=0; j<4; ++j) {
			if(me->mvert[i].co[0] * plane_normals[j].x +
			   me->mvert[i].co[1] * plane_normals[j].y +
			   me->mvert[i].co[2] * plane_normals[j].z < plane_ds[j] ) {
				me->pv->vert_map[i]= mode ? SHOW:HIDE; /* Vert is outside the hide box */
				break;
			}
		}
		if(old_map && old_map[i]) me->pv->vert_map[i]= 1;
		if(!me->pv->vert_map[i]) ++me->totvert;

	}
	if(old_map) MEM_freeN(old_map);

	/* Find out how many faces to show */
	for(i=0; i<me->totface; ++i) {
		if(!me->pv->vert_map[me->mface[i].v1] &&
		   !me->pv->vert_map[me->mface[i].v2] &&
		   !me->pv->vert_map[me->mface[i].v3]) {
			if(me->mface[i].v4) {
				if(!me->pv->vert_map[me->mface[i].v4])
					++face_cnt_show;
			}
			else ++face_cnt_show;
		}
	}
	/* Find out how many edges to show */
	for(i=0; i<me->totedge; ++i) {
		if(!me->pv->vert_map[me->medge[i].v1] &&
		   !me->pv->vert_map[me->medge[i].v2])
			++edge_cnt_show;
	}

	/* Create new vert array and reset each vert's map with map[old]=new index */
	nve= MEM_mallocN(sizeof(MVert)*me->pv->totvert, "PMV verts");
	ndx_show= 0; ndx_hide= me->totvert;
	for(i=0; i<me->pv->totvert; ++i) {
		if(me->pv->vert_map[i]) {
			me->pv->vert_map[i]= ndx_hide;
			nve[me->pv->vert_map[i]]= me->mvert[i];
			++ndx_hide;
		} else {
			me->pv->vert_map[i]= ndx_show;
			nve[me->pv->vert_map[i]]= me->mvert[i];
			++ndx_show;
		}
	}
	CustomData_free_layer(&me->vdata, CD_MVERT, me->pv->totvert);
	me->mvert= CustomData_add_layer(&me->vdata, CD_MVERT, 0, nve, me->totvert);

	/* Create new face array */
	me->pv->old_faces= me->mface;
	me->pv->totface= me->totface;
	me->mface= MEM_mallocN(sizeof(MFace)*face_cnt_show, "PMV faces");
	for(i=0; i<me->totface; ++i) {
		MFace *pr_f= &me->pv->old_faces[i];
		char show= 0;

		if(me->pv->vert_map[pr_f->v1] < me->totvert &&
		   me->pv->vert_map[pr_f->v2] < me->totvert &&
		   me->pv->vert_map[pr_f->v3] < me->totvert) {
			if(pr_f->v4) {
				if(me->pv->vert_map[pr_f->v4] < me->totvert)
					show= 1;
			}
			else show= 1;
		}

		if(show) {
			MFace *cr_f= &me->mface[face_ndx_show];
			*cr_f= *pr_f;
			cr_f->v1= me->pv->vert_map[pr_f->v1];
			cr_f->v2= me->pv->vert_map[pr_f->v2];
			cr_f->v3= me->pv->vert_map[pr_f->v3];
			cr_f->v4= pr_f->v4 ? me->pv->vert_map[pr_f->v4] : 0;
			test_index_face(cr_f,NULL,0,pr_f->v4?4:3);
			++face_ndx_show;
		}
	}
	me->totface= face_cnt_show;
	CustomData_set_layer(&me->fdata, CD_MFACE, me->mface);

	/* Create new edge array */
	me->pv->old_edges= me->medge;
	me->pv->totedge= me->totedge;
	me->medge= MEM_mallocN(sizeof(MEdge)*edge_cnt_show, "PMV edges");
	me->pv->edge_map= MEM_mallocN(sizeof(int)*me->pv->totedge,"PMV edgemap");
	for(i=0; i<me->totedge; ++i) {
		if(me->pv->vert_map[me->pv->old_edges[i].v1] < me->totvert &&
		   me->pv->vert_map[me->pv->old_edges[i].v2] < me->totvert) {
			MEdge *cr_e= &me->medge[edge_ndx_show];
			me->pv->edge_map[i]= edge_ndx_show;
			*cr_e= me->pv->old_edges[i];
			cr_e->v1= me->pv->vert_map[me->pv->old_edges[i].v1];
			cr_e->v2= me->pv->vert_map[me->pv->old_edges[i].v2];
			++edge_ndx_show;
		}
		else me->pv->edge_map[i]= -1;
	}
	me->totedge= edge_cnt_show;
	CustomData_set_layer(&me->edata, CD_MEDGE, me->medge);
	
	set_sculpt_object(ob);

	DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);
}

rcti sculptmode_pmv_box()
{
	short down[2], mouse[2];
	rcti ret;

	getmouseco_areawin(down);

	while((get_mbut()&L_MOUSE) || (get_mbut()&R_MOUSE)) {
		getmouseco_areawin(mouse);

		scrarea_do_windraw(curarea);

		persp(PERSP_WIN);
		glLineWidth(2);
		setlinestyle(2);
		sdrawXORline(down[0],down[1],mouse[0],down[1]);
		sdrawXORline(mouse[0],down[1],mouse[0],mouse[1]);
		sdrawXORline(mouse[0],mouse[1],down[0],mouse[1]);
		sdrawXORline(down[0],mouse[1],down[0],down[1]);
		setlinestyle(0);
		glLineWidth(1);
		persp(PERSP_VIEW);

		screen_swapbuffers();
		backdrawview3d(0);
	}

	ret.xmin= down[0]<mouse[0]?down[0]:mouse[0];
	ret.ymin= down[1]<mouse[1]?down[1]:mouse[1];
	ret.xmax= down[0]>mouse[0]?down[0]:mouse[0];
	ret.ymax= down[1]>mouse[1]?down[1]:mouse[1];
	return ret;
}

void sculptmode_pmv(int mode)
{
	Object *ob= OBACT;
	rcti hb_2d= sculptmode_pmv_box(); /* Get 2D hide box */
	
	sculptmode_correct_state();

	waitcursor(1);

	if(hb_2d.xmax-hb_2d.xmin > 3 && hb_2d.ymax-hb_2d.ymin > 3) {
		init_sculptmatrices();

		sculptmode_do_pmv(ob,&hb_2d,mode);
	}
	else sculptmode_pmv_off(get_mesh(ob));

	scrarea_do_windraw(curarea);

	sculptmode_undo_push("Partial mesh hide", 1, 1, 1);

	waitcursor(0);
}
