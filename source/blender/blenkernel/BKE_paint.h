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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */ 

#ifndef BKE_PAINT_H
#define BKE_PAINT_H

struct Brush;
struct MFace;
struct MultireModifierData;
struct MVert;
struct Object;
struct Paint;
struct PBVH;
struct Scene;
struct StrokeCache;

extern const char PAINT_CURSOR_SCULPT[3];
extern const char PAINT_CURSOR_VERTEX_PAINT[3];
extern const char PAINT_CURSOR_WEIGHT_PAINT[3];
extern const char PAINT_CURSOR_TEXTURE_PAINT[3];

void paint_init(struct Paint *p, const char col[3]);
void free_paint(struct Paint *p);
void copy_paint(struct Paint *orig, struct Paint *new);

struct Paint *paint_get_active(struct Scene *sce);
struct Brush *paint_brush(struct Paint *paint);
void paint_brush_set(struct Paint *paint, struct Brush *br);
void paint_brush_slot_add(struct Paint *p);
void paint_brush_slot_remove(struct Paint *p);

/* testing face select mode
 * Texture paint could be removed since selected faces are not used
 * however hiding faces is useful */
int paint_facesel_test(struct Object *ob);

void paint_refresh_mask_display(struct Object *ob);

typedef struct SculptSession {
	/* Mesh data (not copied) can come either directly from a Mesh, or from a MultiresDM */
	struct MultiresModifierData *multires; /* Special handling for multires meshes */
	struct MVert *mvert;
	struct MFace *mface;
	int totvert, totface;
	float *face_normals;
	struct KeyBlock *kb;
	
	/* Mesh connectivity */
	struct ListBase *fmap;

	/* Area hiding */
	ListBase hidden_areas;
	
	/* Used to cache the render of the active texture */
	unsigned int texcache_side, *texcache, texcache_actual;

	/* Layer brush persistence between strokes */
	 float (*layer_co)[3]; /* Copy of the mesh vertices' locations */

	struct SculptStroke *stroke;
	struct StrokeCache *cache;

	int modifiers_active;
} SculptSession;

typedef struct PaintSession {
	/* mode-specific data (just sculpt for now */
	SculptSession *sculpt;

	/* PBVH acceleration structure */
	struct PBVH *pbvh;

	/* Partial redraw */
	int partial_redraw;
} PaintSession;

void create_paintsession(struct Object *ob);
void free_paintsession(struct Object *ob);

#endif
