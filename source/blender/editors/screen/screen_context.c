/**
 * $Id:
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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"

int ed_screen_context(const bContext *C, const char *member, bContextDataResult *result)
{
	bScreen *sc= CTX_wm_screen(C);
	Scene *scene= sc->scene;
	Base *base;

	if(CTX_data_equals(member, "scene")) {
		CTX_data_id_pointer_set(result, &scene->id);
		return 1;
	}
	else if(CTX_data_equals(member, "selected_objects") || CTX_data_equals(member, "selected_bases")) {
		int selected_objects= CTX_data_equals(member, "selected_objects");

		for(base=scene->base.first; base; base=base->next) {
			if((base->flag & SELECT) && (base->lay & scene->lay)) {
				if(selected_objects)
					CTX_data_id_list_add(result, &base->object->id);
				else
					CTX_data_list_add(result, &scene->id, &RNA_UnknownType, base);
			}
		}

		return 1;
	}
	else if(CTX_data_equals(member, "active_base")) {
		if(scene->basact)
			CTX_data_pointer_set(result, &scene->id, &RNA_UnknownType, &scene->basact);

		return 1;
	}
	else if(CTX_data_equals(member, "active_object")) {
		if(scene->basact)
			CTX_data_id_pointer_set(result, &scene->basact->object->id);

		return 1;
	}
	else if(CTX_data_equals(member, "edit_object")) {
		/* convenience for now, 1 object per scene in editmode */
		if(scene->obedit)
			CTX_data_id_pointer_set(result, &scene->obedit->id);
		
		return 1;
	}
	
	return 0;
}

