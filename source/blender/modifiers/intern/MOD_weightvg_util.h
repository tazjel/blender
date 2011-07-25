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
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
* The Original Code is Copyright (C) 2011 by Bastien Montagne.
* All rights reserved.
*
* Contributor(s): None yet.
*
* ***** END GPL LICENSE BLOCK *****
*
*/

/** \file blender/modifiers/intern/MOD_util.h
 *  \ingroup modifiers
 */


#ifndef MOD_WEIGHTVG_UTIL_H
#define MOD_WEIGHTVG_UTIL_H

/* so modifier types match their defines */
#include "MOD_modifiertypes.h"

struct Tex;
struct DerivedMesh;
struct Object;
/*struct ModifierData;
struct MappingInfoModifierData;*/

/*
 * XXX I’d like to make modified weights visible in WeightPaint mode,
 *     but couldn’t figure a way to do this…
 *     Maybe this will need changes in mesh_calc_modifiers (DerivedMesh.c)?
 *     Or the WeightPaint mode code itself?
 */

/**************************************
 * Util functions.                    *
 **************************************/

/* We cannot divide by zero (what a surprise…).
 * So if -MOD_WEIGHTVGROUP_DIVMODE_ZEROFLOOR < weightf < MOD_WEIGHTVGROUP_DIVMODE_ZEROFLOOR,
 * we clamp weightf to this value (or its negative version).
 * Also used to avoid null power factor.
 */
#define MOD_WVG_ZEROFLOOR		1.0e-32f

/* Applies new_w weights to org_w ones, using either a texture, vgroup or constant value as factor.
 * Return values are in org_w.
 * If indices is not NULL, it must be a table of same length as org_w and new_w, mapping to the real
 * vertex index (in case the weight tables do not cover the whole vertices...).
 * XXX The standard “factor” value is assumed in [0.0, 1.0] range. Else, weird results might appear.
 */
void weightvg_do_mask(int num, int *indices, float *org_w, float *new_w, Object *ob,
                      struct DerivedMesh *dm, float fact, const char *defgrp_name, Tex *texture,
                      int tex_use_channel, int tex_mapping, Object *tex_map_object,
                      const char *tex_uvlayer_name);

/* Applies weights to given vgroup (defgroup), and optionnaly add/remove vertices from the group.
 * If indices is not NULL, it must be a table of same length as weights, mapping to the real
 * vertex index (in case the weight table does not cover the whole vertices...).
 */
void weightvg_update_vg(MDeformVert *dvert, int defgrp_idx, int num, int *indices, float *weights,
                        int do_add, float add_thresh, int do_rem, float rem_thresh);

#endif /* MOD_WEIGHTVG_UTIL_H */
