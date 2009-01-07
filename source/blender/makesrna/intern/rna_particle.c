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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_particle_types.h"

#ifdef RNA_RUNTIME

#else

static void rna_def_particlesettings(BlenderRNA *brna)
{
	StructRNA *srna;

	srna= RNA_def_struct(brna, "ParticleSettings", "ID");
	RNA_def_struct_ui_text(srna, "Particle Settings", "Particle settings, reusable by multiple particle systems.");
}

static void rna_def_particlesystem(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "ParticleSystem", NULL);
	RNA_def_struct_ui_text(srna, "Particle System", "Particle system in an object.");

	prop= RNA_def_property(srna, "settings", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "part");
	RNA_def_property_ui_text(prop, "Settings", "Particle system settings.");
}

void RNA_def_particle(BlenderRNA *brna)
{
	rna_def_particlesystem(brna);
	rna_def_particlesettings(brna);
}

#endif

