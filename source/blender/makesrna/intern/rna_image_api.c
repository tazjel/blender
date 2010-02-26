/**
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Arystanbek Dyussenov
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "RNA_define.h"
#include "RNA_types.h"

#ifdef RNA_RUNTIME

#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_utildefines.h"

#include "DNA_image_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

static void rna_Image_save(Image *image, bContext *C, ReportList *reports, char *path, Scene *scene)
{
	ImBuf *ibuf;

	if (scene == NULL) {
		scene = CTX_data_scene(C);
	}

	if (scene) {
		ImageUser iuser;
		void *lock;

		iuser.scene = scene;
		iuser.ok = 1;

		ibuf = BKE_image_acquire_ibuf(image, &iuser, &lock);

		if (ibuf == NULL) {
			BKE_reportf(reports, RPT_ERROR, "Couldn't acquire buffer from image");
		}

		if (!BKE_write_ibuf(NULL, ibuf, path, scene->r.imtype, scene->r.subimtype, scene->r.quality)) {
			BKE_reportf(reports, RPT_ERROR, "Couldn't write image: %s", path);
		}
	} else {
		BKE_reportf(reports, RPT_ERROR, "Scene not in context, couldn't get save parameters");
	}
}

#else

void RNA_api_image(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func= RNA_def_function(srna, "save", "rna_Image_save");
	RNA_def_function_ui_description(func, "Save image to a specific path.");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT|FUNC_USE_REPORTS);
	parm= RNA_def_string(func, "path", "", 0, "", "Save path.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_pointer(func, "scene", "Scene", "", "Scene to take image parameters from.");
}

#endif

