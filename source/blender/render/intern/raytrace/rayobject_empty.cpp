/**
 * $Id: rayobject_empty.cpp 34664 2011-02-06 00:49:58Z gsrb3d $
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
 * The Original Code is Copyright (C) 1990-1998 NeoGeo BV.
 * All rights reserved.
 *
 * Contributors: 2004/2005 Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "rayobject.h"

/*
 * Empty raytree
 */

static int RE_rayobject_empty_intersect(RayObject *o, Isect *is)
{
	return 0;
}

static void RE_rayobject_empty_free(RayObject *o)
{
}

static void RE_rayobject_empty_bb(RayObject *o, float *min, float *max)
{
	return;
}

static float RE_rayobject_empty_cost(RayObject *o)
{
	return 0.0;
}

static void RE_rayobject_empty_hint_bb(RayObject *o, RayHint *hint, float *min, float *max)
{}

static RayObjectAPI empty_api =
{
	RE_rayobject_empty_intersect,
	NULL, //static void RE_rayobject_instance_add(RayObject *o, RayObject *ob);
	NULL, //static void RE_rayobject_instance_done(RayObject *o);
	RE_rayobject_empty_free,
	RE_rayobject_empty_bb,
	RE_rayobject_empty_cost,
	RE_rayobject_empty_hint_bb
};

static RayObject empty_raytree = { &empty_api, {0, 0} };

RayObject *RE_rayobject_empty_create()
{
	return RE_rayobject_unalignRayAPI( &empty_raytree );
}

