/* 
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Michel Selten, Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_modules_h
#define EXPP_modules_h

#include <Python.h>

#include <DNA_scene_types.h>
#include <DNA_object_types.h>
#include <DNA_camera_types.h>
#include <DNA_lamp_types.h>
#include <DNA_curve_types.h>
#include <DNA_armature_types.h>
#include <DNA_ipo_types.h>
#include <DNA_effect_types.h>
#include <DNA_meta_types.h>
#include <DNA_image_types.h>
#include <DNA_text_types.h>

/*****************************************************************************/
/* Global variables                                                          */
/*****************************************************************************/
extern PyObject *g_blenderdict;


/*****************************************************************************/
/* Module Init functions and Data Object helper functions (used by the       */
/* Object module to work with its .data field for the various Data objs      */
/*****************************************************************************/
void M_Blender_Init (void);

/* sys */
PyObject * sys_Init (void);

/* Object itself */
PyObject * Object_Init (void);
PyObject * Object_CreatePyObject (struct Object *obj);
Object   * Object_FromPyObject (PyObject *py_obj);
int        Object_CheckPyObject (PyObject *py_obj);

/* Scene */
PyObject * Scene_Init (void);
PyObject * Scene_CreatePyObject (struct Scene *sce);
Scene    * Scene_FromPyObject   (PyObject *pyobj);
int        Scene_CheckPyObject  (PyObject *pyobj);

/* Types */
PyObject * Types_Init (void);

/* NMesh Data */
PyObject * NMesh_Init (void);
PyObject * NMesh_CreatePyObject (struct Camera *cam);
Camera   * NMesh_FromPyObject   (PyObject *pyobj);
int        NMesh_CheckPyObject  (PyObject *pyobj);

/* Material */
PyObject * Material_Init (void);

/* Camera Data */
PyObject * Camera_Init (void);
PyObject * Camera_CreatePyObject (struct Camera *cam);
Camera   * Camera_FromPyObject   (PyObject *pyobj);
int        Camera_CheckPyObject  (PyObject *pyobj);

/* Lamp Data */
PyObject * Lamp_Init (void);
PyObject * Lamp_CreatePyObject (struct Lamp *lamp);
Lamp     * Lamp_FromPyObject   (PyObject *pyobj);
int        Lamp_CheckPyObject  (PyObject *pyobj);

/* Curve Data */
PyObject * Curve_Init (void);
PyObject * CurveCreatePyObject (struct Curve *curve);
Curve    * CurveFromPyObject   (PyObject *py_obj);
int        CurveCheckPyObject  (PyObject *py_obj);

/* Armature Data */
PyObject  * Armature_Init (void);
PyObject  * Armature_CreatePyObject (bArmature *armature);
bArmature * Armature_FromPyObject   (PyObject *py_obj);
int         Armature_CheckPyObject  (PyObject *py_obj);

/* Ipo Data */
PyObject * Ipo_Init (void);
PyObject * Ipo_CreatePyObject (struct Ipo *ipo);
Ipo      * Ipo_FromPyObject   (PyObject *py_obj);
int        Ipo_CheckPyObject  (PyObject *py_obj);

/* Metaball Data */
PyObject * Metaball_Init (void);
PyObject * Metaball_CreatePyObject (MetaBall *metaball);
MetaBall * Metaball_FromPyObject   (PyObject *py_obj);
int        Metaball_CheckPyObject  (PyObject *py_obj);

/* Particle Effects Data */
PyObject * Effect_Init (void);
PyObject * Effect_CreatePyObject (struct Effect *effect);
Effect   * Effect_FromPyObject (PyObject *py_obj);
int        Effect_CheckPyObject (PyObject *py_obj);

/* Image */
PyObject * Image_Init (void);
PyObject * Image_CreatePyObject (Image *image);
int        Image_CheckPyObject (PyObject *pyobj);

/* Text */
PyObject * Text_Init (void);
PyObject * Text_CreatePyObject (Text *txt);

/* Init functions for other modules */
PyObject * Window_Init (void);
PyObject * Draw_Init (void);
PyObject * BGL_Init (void);
PyObject * World_Init (void);

#endif /* EXPP_modules_h */
