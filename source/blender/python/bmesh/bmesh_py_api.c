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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/bmesh/bmesh_py_api.c
 *  \ingroup pybmesh
 *
 * This file defines the 'bmesh' module.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "bmesh.h"

#include "bmesh_py_types.h"
#include "bmesh_py_types_select.h"
#include "bmesh_py_types_customdata.h"
#include "bmesh_py_types_meshdata.h"

#include "bmesh_py_ops.h"
#include "bmesh_py_utils.h"

#include "BKE_tessmesh.h"

#include "DNA_mesh_types.h"

#include "../generic/py_capi_utils.h"

#include "bmesh_py_api.h" /* own include */

PyDoc_STRVAR(bpy_bm_new_doc,
".. method:: new()\n"
"\n"
"   :return: Return a new, empty BMesh.\n"
"   :rtype: :class:`bmesh.types.BMesh`\n"
);

static PyObject *bpy_bm_new(PyObject *UNUSED(self))
{
	BMesh *bm;

	bm = BM_mesh_create(&bm_mesh_allocsize_default);

	return BPy_BMesh_CreatePyObject(bm, BPY_BMFLAG_NOP);
}

PyDoc_STRVAR(bpy_bm_from_edit_mesh_doc,
".. method:: from_edit_mesh(mesh)\n"
"\n"
"   Return a BMesh from this mesh, currently the mesh must already be in editmode.\n"
"\n"
"   :arg mesh: The editmode mesh.\n"
"   :type mesh: :class:`bpy.types.Mesh`\n"
"   :return: the BMesh associated with this mesh.\n"
"   :rtype: :class:`bmesh.types.BMesh`\n"
);
static PyObject *bpy_bm_from_edit_mesh(PyObject *UNUSED(self), PyObject *value)
{
	BMesh *bm;
	Mesh *me = PyC_RNA_AsPointer(value, "Mesh");

	if (me == NULL) {
		return NULL;
	}

	if (me->edit_btmesh == NULL) {
		PyErr_SetString(PyExc_ValueError,
		                "The mesh must be in editmode");
		return NULL;
	}

	bm = me->edit_btmesh->bm;

	return BPy_BMesh_CreatePyObject(bm, BPY_BMFLAG_IS_WRAPPED);
}

PyDoc_STRVAR(bpy_bm_update_edit_mesh_doc,
".. method:: update_edit_mesh(mesh, tessface=True)\n"
"\n"
"   Update the mesh after changes to the BMesh in editmode, \n"
"   optionally recalculating n-gon tessellation.\n"
"\n"
"   :arg mesh: The editmode mesh.\n"
"   :type mesh: :class:`bpy.types.Mesh`\n"
"   :arg tessface: Option to recalculate n-gon tessellation.\n"
"   :type tessface: boolean\n"
);
static PyObject *bpy_bm_update_edit_mesh(PyObject *UNUSED(self), PyObject *args)
{
	PyObject *py_me;
	Mesh *me;
	int do_tessface = TRUE;

	if (!PyArg_ParseTuple(args, "O|i:update_edit_mesh", &py_me, &do_tessface)) {
		return NULL;
	}

	me = PyC_RNA_AsPointer(py_me, "Mesh");

	if (me == NULL) {
		return NULL;
	}

	if (me->edit_btmesh == NULL) {
		PyErr_SetString(PyExc_ValueError,
		                "The mesh must be in editmode");
		return NULL;
	}

	{
		/* XXX, not great - infact this function could just not use the context at all
		 * postpone that change until after release: BMESH_TODO - campbell */
		extern struct bContext *BPy_GetContext(void);
		extern void EDBM_update_generic(struct bContext *C, BMEditMesh *em, const short do_tessface);

		struct bContext *C = BPy_GetContext();
		EDBM_update_generic(C, me->edit_btmesh, do_tessface);
	}

	Py_RETURN_NONE;
}

static struct PyMethodDef BPy_BM_methods[] = {
	{"new",            (PyCFunction)bpy_bm_new,            METH_NOARGS,  bpy_bm_new_doc},
	{"from_edit_mesh", (PyCFunction)bpy_bm_from_edit_mesh, METH_O,       bpy_bm_from_edit_mesh_doc},
	{"update_edit_mesh", (PyCFunction)bpy_bm_update_edit_mesh, METH_VARARGS, bpy_bm_update_edit_mesh_doc},
	{NULL, NULL, 0, NULL}
};

PyDoc_STRVAR(BPy_BM_doc,
"This module provides access to blenders bmesh data structures.\n"
"\n"
"\n"
"Submodules:\n"
"\n"
"* :mod:`bmesh.utils`\n"
"* :mod:`bmesh.types`\n"
"\n"
"\n"
".. include:: include__bmesh.rst\n"
);
static struct PyModuleDef BPy_BM_module_def = {
	PyModuleDef_HEAD_INIT,
	"bmesh",  /* m_name */
	BPy_BM_doc,  /* m_doc */
	0,  /* m_size */
	BPy_BM_methods,  /* m_methods */
	NULL,  /* m_reload */
	NULL,  /* m_traverse */
	NULL,  /* m_clear */
	NULL,  /* m_free */
};

PyObject *BPyInit_bmesh(void)
{
	PyObject *mod;
	PyObject *submodule;
	PyObject *sys_modules = PyThreadState_GET()->interp->modules;

	BPy_BM_init_types();
	BPy_BM_init_types_select();
	BPy_BM_init_types_customdata();
	BPy_BM_init_types_meshdata();

	mod = PyModule_Create(&BPy_BM_module_def);

	/* bmesh.types */
	PyModule_AddObject(mod, "types", (submodule = BPyInit_bmesh_types()));
	PyDict_SetItemString(sys_modules, PyModule_GetName(submodule), submodule);
	Py_INCREF(submodule);

	PyModule_AddObject(mod, "ops", (submodule = BPyInit_bmesh_ops()));
	/* PyDict_SetItemString(sys_modules, PyModule_GetName(submodule), submodule); */
	PyDict_SetItemString(sys_modules, "bmesh.ops", submodule); /* fake module */
	Py_INCREF(submodule);

	PyModule_AddObject(mod, "utils", (submodule = BPyInit_bmesh_utils()));
	PyDict_SetItemString(sys_modules, PyModule_GetName(submodule), submodule);
	Py_INCREF(submodule);

	return mod;
}
