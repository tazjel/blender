/* $Id$
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): Willian P. Germano & Joseph Gilbert
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 *
 */

#ifndef EXPP_vector_h
#define EXPP_vector_h

#include "Python.h"
#include "gen_utils.h"
#include "Types.h"
#include "matrix.h"
#include "BKE_utildefines.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/*****************************/
//    Vector Python Object   
/*****************************/

#define VectorObject_Check(v) ((v)->ob_type == &vector_Type)

typedef struct {
	PyObject_VAR_HEAD float *vec;
	int size;
	int flag;
	//0 - no coercion
	//1 - coerced from int
	//2 - coerced from float
	int delete_pymem;	/* flag to delete the memory vec points at */
} VectorObject;

//prototypes
PyObject *newVectorObject( float *vec, int size );
PyObject *newVectorProxy( float *vec, int size );
PyObject *Vector_Zero( VectorObject * self );
PyObject *Vector_Normalize( VectorObject * self );
PyObject *Vector_Negate( VectorObject * self );
PyObject *Vector_Resize2D( VectorObject * self );
PyObject *Vector_Resize3D( VectorObject * self );
PyObject *Vector_Resize4D( VectorObject * self );

#endif				/* EXPP_vector_h */
