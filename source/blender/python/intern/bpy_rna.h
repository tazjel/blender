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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef BPY_RNA_H
#define BPY_RNA_H

#include <Python.h>

#include "RNA_access.h"
#include "RNA_types.h"
#include "BKE_idprop.h"

extern PyTypeObject pyrna_struct_Type;
extern PyTypeObject pyrna_prop_Type;

typedef struct {
	PyObject_VAR_HEAD /* required python macro   */
	PointerRNA ptr;
	IDProperty *properties; /* needed in some cases for RNA_pointer_create(), free when deallocing */
} BPy_StructRNA;

typedef struct {
	PyObject_VAR_HEAD /* required python macro   */
	PointerRNA ptr;
	PropertyRNA *prop;
} BPy_PropertyRNA;

PyObject *BPY_rna_module( void );
PyObject *BPY_rna_doc( void );

PyObject *pyrna_struct_CreatePyObject( PointerRNA *ptr );
PyObject *pyrna_prop_CreatePyObject( PointerRNA *ptr, PropertyRNA *prop );

/* operators also need this to set args */
int pyrna_py_to_prop(PointerRNA *ptr, PropertyRNA *prop, PyObject *value);
PyObject * pyrna_prop_to_py(PointerRNA *ptr, PropertyRNA *prop);
#endif
