/*
 * $Id$
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 *
 * Contributor(s): Willian P. Germano, Joseph Gilbert, Ken Hughes, Alex Fraser, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/generic/mathutils_Vector.c
 *  \ingroup pygen
 */


#include <Python.h>

#include "mathutils.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#define MAX_DIMENSIONS 4

/* Swizzle axes get packed into a single value that is used as a closure. Each
   axis uses SWIZZLE_BITS_PER_AXIS bits. The first bit (SWIZZLE_VALID_AXIS) is
   used as a sentinel: if it is unset, the axis is not valid. */
#define SWIZZLE_BITS_PER_AXIS 3
#define SWIZZLE_VALID_AXIS 0x4
#define SWIZZLE_AXIS       0x3

static PyObject *Vector_copy(VectorObject *self);
static PyObject *Vector_to_tuple_ext(VectorObject *self, int ndigits);

/* Supports 2D, 3D, and 4D vector objects both int and float values
 * accepted. Mixed float and int values accepted. Ints are parsed to float
 */
static PyObject *Vector_new(PyTypeObject *type, PyObject *args, PyObject *UNUSED(kwds))
{
	float vec[4]= {0.0f, 0.0f, 0.0f, 0.0f};
	int size= 3; /* default to a 3D vector */

	switch(PyTuple_GET_SIZE(args)) {
	case 0:
		break;
	case 1:
		if((size=mathutils_array_parse(vec, 2, 4, PyTuple_GET_ITEM(args, 0), "mathutils.Vector()")) == -1)
			return NULL;
		break;
	default:
		PyErr_SetString(PyExc_TypeError,
		                "mathutils.Vector(): "
		                "more then a single arg given");
		return NULL;
	}
	return newVectorObject(vec, size, Py_NEW, type);
}

static PyObject *vec__apply_to_copy(PyNoArgsFunction vec_func, VectorObject *self)
{
	PyObject *ret= Vector_copy(self);
	PyObject *ret_dummy= vec_func(ret);
	if(ret_dummy) {
		Py_DECREF(ret_dummy);
		return (PyObject *)ret;
	}
	else { /* error */
		Py_DECREF(ret);
		return NULL;
	}
}

/*-----------------------------METHODS---------------------------- */
PyDoc_STRVAR(Vector_zero_doc,
".. method:: zero()\n"
"\n"
"   Set all values to zero.\n"
);
static PyObject *Vector_zero(VectorObject *self)
{
	fill_vn(self->vec, self->size, 0.0f);

	if(BaseMath_WriteCallback(self) == -1)
		return NULL;

	Py_RETURN_NONE;
}

PyDoc_STRVAR(Vector_normalize_doc,
".. method:: normalize()\n"
"\n"
"   Normalize the vector, making the length of the vector always 1.0.\n"
"\n"
"   .. warning:: Normalizing a vector where all values are zero results\n"
"      in all axis having a nan value (not a number).\n"
"\n"
"   .. note:: Normalize works for vectors of all sizes,\n"
"      however 4D Vectors w axis is left untouched.\n"
);
static PyObject *Vector_normalize(VectorObject *self)
{
	int i;
	float norm = 0.0f;

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	for(i = 0; i < self->size; i++) {
		norm += self->vec[i] * self->vec[i];
	}
	norm = (float) sqrt(norm);
	for(i = 0; i < self->size; i++) {
		self->vec[i] /= norm;
	}

	(void)BaseMath_WriteCallback(self);
	Py_RETURN_NONE;
}
PyDoc_STRVAR(Vector_normalized_doc,
".. method:: normalized()\n"
"\n"
"   Return a new, normalized vector.\n"
"\n"
"   :return: a normalized copy of the vector\n"
"   :rtype: :class:`Vector`\n"
);
static PyObject *Vector_normalized(VectorObject *self)
{
	return vec__apply_to_copy((PyNoArgsFunction)Vector_normalize, self);
}

PyDoc_STRVAR(Vector_resize_2d_doc,
".. method:: resize_2d()\n"
"\n"
"   Resize the vector to 2D  (x, y).\n"
"\n"
"   :return: an instance of itself\n"
"   :rtype: :class:`Vector`\n"
);
static PyObject *Vector_resize_2d(VectorObject *self)
{
	if(self->wrapped==Py_WRAP) {
		PyErr_SetString(PyExc_TypeError,
		                "vector.resize_2d(): "
		                "cannot resize wrapped data - only python vectors");
		return NULL;
	}
	if(self->cb_user) {
		PyErr_SetString(PyExc_TypeError,
		                "vector.resize_2d(): "
		                "cannot resize a vector that has an owner");
		return NULL;
	}

	self->vec = PyMem_Realloc(self->vec, (sizeof(float) * 2));
	if(self->vec == NULL) {
		PyErr_SetString(PyExc_MemoryError,
		                "vector.resize_2d(): "
		                "problem allocating pointer space");
		return NULL;
	}

	self->size = 2;
	Py_RETURN_NONE;
}

PyDoc_STRVAR(Vector_resize_3d_doc,
".. method:: resize_3d()\n"
"\n"
"   Resize the vector to 3D  (x, y, z).\n"
"\n"
"   :return: an instance of itself\n"
"   :rtype: :class:`Vector`\n"
);
static PyObject *Vector_resize_3d(VectorObject *self)
{
	if (self->wrapped==Py_WRAP) {
		PyErr_SetString(PyExc_TypeError,
		                "vector.resize_3d(): "
		                "cannot resize wrapped data - only python vectors");
		return NULL;
	}
	if(self->cb_user) {
		PyErr_SetString(PyExc_TypeError,
		                "vector.resize_3d(): "
		                "cannot resize a vector that has an owner");
		return NULL;
	}

	self->vec = PyMem_Realloc(self->vec, (sizeof(float) * 3));
	if(self->vec == NULL) {
		PyErr_SetString(PyExc_MemoryError,
		                "vector.resize_3d(): "
		                "problem allocating pointer space");
		return NULL;
	}

	if(self->size == 2)
		self->vec[2] = 0.0f;

	self->size = 3;
	Py_RETURN_NONE;
}

PyDoc_STRVAR(Vector_resize_4d_doc,
".. method:: resize_4d()\n"
"\n"
"   Resize the vector to 4D (x, y, z, w).\n"
"\n"
"   :return: an instance of itself\n"
"   :rtype: :class:`Vector`\n"
);
static PyObject *Vector_resize_4d(VectorObject *self)
{
	if(self->wrapped==Py_WRAP) {
		PyErr_SetString(PyExc_TypeError,
		                "vector.resize_4d(): "
		                "cannot resize wrapped data - only python vectors");
		return NULL;
	}
	if(self->cb_user) {
		PyErr_SetString(PyExc_TypeError,
		                "vector.resize_4d(): "
		                "cannot resize a vector that has an owner");
		return NULL;
	}

	self->vec = PyMem_Realloc(self->vec, (sizeof(float) * 4));
	if(self->vec == NULL) {
		PyErr_SetString(PyExc_MemoryError,
		                "vector.resize_4d(): "
		                "problem allocating pointer space");
		return NULL;
	}

	if(self->size == 2){
		self->vec[2] = 0.0f;
		self->vec[3] = 1.0f;
	}
	else if(self->size == 3){
		self->vec[3] = 1.0f;
	}
	self->size = 4;
	Py_RETURN_NONE;
}
PyDoc_STRVAR(Vector_to_2d_doc,
".. method:: to_2d()\n"
"\n"
"   Return a 2d copy of the vector.\n"
"\n"
"   :return: a new vector\n"
"   :rtype: :class:`Vector`\n"
);
static PyObject *Vector_to_2d(VectorObject *self)
{
	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	return newVectorObject(self->vec, 2, Py_NEW, Py_TYPE(self));
}
PyDoc_STRVAR(Vector_to_3d_doc,
".. method:: to_3d()\n"
"\n"
"   Return a 3d copy of the vector.\n"
"\n"
"   :return: a new vector\n"
"   :rtype: :class:`Vector`\n"
);
static PyObject *Vector_to_3d(VectorObject *self)
{
	float tvec[3]= {0.0f};

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	memcpy(tvec, self->vec, sizeof(float) * MIN2(self->size, 3));
	return newVectorObject(tvec, 3, Py_NEW, Py_TYPE(self));
}
PyDoc_STRVAR(Vector_to_4d_doc,
".. method:: to_4d()\n"
"\n"
"   Return a 4d copy of the vector.\n"
"\n"
"   :return: a new vector\n"
"   :rtype: :class:`Vector`\n"
);
static PyObject *Vector_to_4d(VectorObject *self)
{
	float tvec[4]= {0.0f, 0.0f, 0.0f, 1.0f};

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	memcpy(tvec, self->vec, sizeof(float) * MIN2(self->size, 4));
	return newVectorObject(tvec, 4, Py_NEW, Py_TYPE(self));
}

PyDoc_STRVAR(Vector_to_tuple_doc,
".. method:: to_tuple(precision=-1)\n"
"\n"
"   Return this vector as a tuple with.\n"
"\n"
"   :arg precision: The number to round the value to in [-1, 21].\n"
"   :type precision: int\n"
"   :return: the values of the vector rounded by *precision*\n"
"   :rtype: tuple\n"
);
/* note: BaseMath_ReadCallback must be called beforehand */
static PyObject *Vector_to_tuple_ext(VectorObject *self, int ndigits)
{
	PyObject *ret;
	int i;

	ret= PyTuple_New(self->size);

	if(ndigits >= 0) {
		for(i = 0; i < self->size; i++) {
			PyTuple_SET_ITEM(ret, i, PyFloat_FromDouble(double_round((double)self->vec[i], ndigits)));
		}
	}
	else {
		for(i = 0; i < self->size; i++) {
			PyTuple_SET_ITEM(ret, i, PyFloat_FromDouble(self->vec[i]));
		}
	}

	return ret;
}

static PyObject *Vector_to_tuple(VectorObject *self, PyObject *args)
{
	int ndigits= 0;

	if(!PyArg_ParseTuple(args, "|i:to_tuple", &ndigits))
		return NULL;

	if(ndigits > 22 || ndigits < 0) {
		PyErr_SetString(PyExc_ValueError,
		                "vector.to_tuple(ndigits): "
		                "ndigits must be between 0 and 21");
		return NULL;
	}

	if(PyTuple_GET_SIZE(args)==0)
		ndigits= -1;

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	return Vector_to_tuple_ext(self, ndigits);
}

PyDoc_STRVAR(Vector_to_track_quat_doc,
".. method:: to_track_quat(track, up)\n"
"\n"
"   Return a quaternion rotation from the vector and the track and up axis.\n"
"\n"
"   :arg track: Track axis in ['X', 'Y', 'Z', '-X', '-Y', '-Z'].\n"
"   :type track: string\n"
"   :arg up: Up axis in ['X', 'Y', 'Z'].\n"
"   :type up: string\n"
"   :return: rotation from the vector and the track and up axis.\n"
"   :rtype: :class:`Quaternion`\n"
);
static PyObject *Vector_to_track_quat(VectorObject *self, PyObject *args)
{
	float vec[3], quat[4];
	const char *strack, *sup;
	short track = 2, up = 1;

	if(!PyArg_ParseTuple(args, "|ss:to_track_quat", &strack, &sup))
		return NULL;

	if (self->size != 3) {
		PyErr_SetString(PyExc_TypeError,
		                "vector.to_track_quat(): "
		                "only for 3D vectors");
		return NULL;
	}

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	if (strack) {
		const char *axis_err_msg= "only X, -X, Y, -Y, Z or -Z for track axis";

		if (strlen(strack) == 2) {
			if (strack[0] == '-') {
				switch(strack[1]) {
					case 'X':
						track = 3;
						break;
					case 'Y':
						track = 4;
						break;
					case 'Z':
						track = 5;
						break;
					default:
						PyErr_SetString(PyExc_ValueError, axis_err_msg);
						return NULL;
				}
			}
			else {
				PyErr_SetString(PyExc_ValueError, axis_err_msg);
				return NULL;
			}
		}
		else if (strlen(strack) == 1) {
			switch(strack[0]) {
			case '-':
			case 'X':
				track = 0;
				break;
			case 'Y':
				track = 1;
				break;
			case 'Z':
				track = 2;
				break;
			default:
				PyErr_SetString(PyExc_ValueError, axis_err_msg);
				return NULL;
			}
		}
		else {
			PyErr_SetString(PyExc_ValueError, axis_err_msg);
			return NULL;
		}
	}

	if (sup) {
		const char *axis_err_msg= "only X, Y or Z for up axis";
		if (strlen(sup) == 1) {
			switch(*sup) {
			case 'X':
				up = 0;
				break;
			case 'Y':
				up = 1;
				break;
			case 'Z':
				up = 2;
				break;
			default:
				PyErr_SetString(PyExc_ValueError, axis_err_msg);
				return NULL;
			}
		}
		else {
			PyErr_SetString(PyExc_ValueError, axis_err_msg);
			return NULL;
		}
	}

	if (track == up) {
		PyErr_SetString(PyExc_ValueError,
		                "Can't have the same axis for track and up");
		return NULL;
	}

	/*
		flip vector around, since vectoquat expect a vector from target to tracking object
		and the python function expects the inverse (a vector to the target).
	*/
	negate_v3_v3(vec, self->vec);

	vec_to_quat(quat, vec, track, up);

	return newQuaternionObject(quat, Py_NEW, NULL);
}

/*
 * Vector.reflect(mirror): return a reflected vector on the mirror normal
 *  vec - ((2 * DotVecs(vec, mirror)) * mirror)
 */
PyDoc_STRVAR(Vector_reflect_doc,
".. method:: reflect(mirror)\n"
"\n"
"   Return the reflection vector from the *mirror* argument.\n"
"\n"
"   :arg mirror: This vector could be a normal from the reflecting surface.\n"
"   :type mirror: :class:`Vector`\n"
"   :return: The reflected vector matching the size of this vector.\n"
"   :rtype: :class:`Vector`\n"
);
static PyObject *Vector_reflect(VectorObject *self, PyObject *value)
{
	int value_size;
	float mirror[3], vec[3];
	float reflect[3] = {0.0f};
	float tvec[MAX_DIMENSIONS];

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	if((value_size= mathutils_array_parse(tvec, 2, 4, value, "vector.reflect(other), invalid 'other' arg")) == -1)
		return NULL;

	mirror[0] = tvec[0];
	mirror[1] = tvec[1];
	if (value_size > 2)		mirror[2] = tvec[2];
	else					mirror[2] = 0.0;

	vec[0] = self->vec[0];
	vec[1] = self->vec[1];
	if (self->size > 2)		vec[2] = self->vec[2];
	else					vec[2] = 0.0;

	normalize_v3(mirror);
	reflect_v3_v3v3(reflect, vec, mirror);

	return newVectorObject(reflect, self->size, Py_NEW, Py_TYPE(self));
}

PyDoc_STRVAR(Vector_cross_doc,
".. method:: cross(other)\n"
"\n"
"   Return the cross product of this vector and another.\n"
"\n"
"   :arg other: The other vector to perform the cross product with.\n"
"   :type other: :class:`Vector`\n"
"   :return: The cross product.\n"
"   :rtype: :class:`Vector`\n"
"\n"
"   .. note:: both vectors must be 3D\n"
);
static PyObject *Vector_cross(VectorObject *self, PyObject *value)
{
	VectorObject *ret;
	float tvec[MAX_DIMENSIONS];

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	if(mathutils_array_parse(tvec, self->size, self->size, value, "vector.cross(other), invalid 'other' arg") == -1)
		return NULL;

	ret= (VectorObject *)newVectorObject(NULL, 3, Py_NEW, Py_TYPE(self));
	cross_v3_v3v3(ret->vec, self->vec, tvec);
	return (PyObject *)ret;
}

PyDoc_STRVAR(Vector_dot_doc,
".. method:: dot(other)\n"
"\n"
"   Return the dot product of this vector and another.\n"
"\n"
"   :arg other: The other vector to perform the dot product with.\n"
"   :type other: :class:`Vector`\n"
"   :return: The dot product.\n"
"   :rtype: :class:`Vector`\n"
);
static PyObject *Vector_dot(VectorObject *self, PyObject *value)
{
	float tvec[MAX_DIMENSIONS];
	double dot = 0.0;
	int x;

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	if(mathutils_array_parse(tvec, self->size, self->size, value, "vector.dot(other), invalid 'other' arg") == -1)
		return NULL;

	for(x = 0; x < self->size; x++) {
		dot += (double)(self->vec[x] * tvec[x]);
	}

	return PyFloat_FromDouble(dot);
}

PyDoc_STRVAR(Vector_angle_doc,
".. function:: angle(other, fallback)\n"
"\n"
"   Return the angle between two vectors.\n"
"\n"
"   :arg other: another vector to compare the angle with\n"
"   :type other: :class:`Vector`\n"
"   :arg fallback: return this value when the angle cant be calculated\n"
"      (zero length vector)\n"
"   :type fallback: any\n"
"   :return: angle in radians or fallback when given\n"
"   :rtype: float\n"
"\n"
"   .. note:: Zero length vectors raise an :exc:`AttributeError`.\n"
);
static PyObject *Vector_angle(VectorObject *self, PyObject *args)
{
	const int size= self->size;
	float tvec[MAX_DIMENSIONS];
	PyObject *value;
	double dot = 0.0f, test_v1 = 0.0f, test_v2 = 0.0f;
	int x;
	PyObject *fallback= NULL;

	if(!PyArg_ParseTuple(args, "O|O:angle", &value, &fallback))
		return NULL;

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	if(mathutils_array_parse(tvec, size, size, value, "vector.angle(other), invalid 'other' arg") == -1)
		return NULL;

	for(x = 0; x < size; x++) {
		test_v1 += (double)(self->vec[x] * self->vec[x]);
		test_v2 += (double)(tvec[x] * tvec[x]);
	}
	if (!test_v1 || !test_v2){
		/* avoid exception */
		if(fallback) {
			Py_INCREF(fallback);
			return fallback;
		}
		else {
			PyErr_SetString(PyExc_ValueError,
			                "vector.angle(other): "
			                "zero length vectors have no valid angle");
			return NULL;
		}
	}

	//dot product
	for(x = 0; x < self->size; x++) {
		dot += (double)(self->vec[x] * tvec[x]);
	}
	dot /= (sqrt(test_v1) * sqrt(test_v2));

	return PyFloat_FromDouble(saacos(dot));
}

PyDoc_STRVAR(Vector_rotation_difference_doc,
".. function:: difference(other)\n"
"\n"
"   Returns a quaternion representing the rotational difference between this\n"
"   vector and another.\n"
"\n"
"   :arg other: second vector.\n"
"   :type other: :class:`Vector`\n"
"   :return: the rotational difference between the two vectors.\n"
"   :rtype: :class:`Quaternion`\n"
"\n"
"   .. note:: 2D vectors raise an :exc:`AttributeError`.\n"
);
static PyObject *Vector_rotation_difference(VectorObject *self, PyObject *value)
{
	float quat[4], vec_a[3], vec_b[3];

	if(self->size < 3) {
		PyErr_SetString(PyExc_ValueError,
		                "vec.difference(value): "
		                "expects both vectors to be size 3 or 4");
		return NULL;
	}

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	if(mathutils_array_parse(vec_b, 3, MAX_DIMENSIONS, value, "vector.difference(other), invalid 'other' arg") == -1)
		return NULL;

	normalize_v3_v3(vec_a, self->vec);
	normalize_v3(vec_b);

	rotation_between_vecs_to_quat(quat, vec_a, vec_b);

	return newQuaternionObject(quat, Py_NEW, NULL);
}

PyDoc_STRVAR(Vector_project_doc,
".. function:: project(other)\n"
"\n"
"   Return the projection of this vector onto the *other*.\n"
"\n"
"   :arg other: second vector.\n"
"   :type other: :class:`Vector`\n"
"   :return: the parallel projection vector\n"
"   :rtype: :class:`Vector`\n"
);
static PyObject *Vector_project(VectorObject *self, PyObject *value)
{
	const int size= self->size;
	float tvec[MAX_DIMENSIONS];
	float vec[MAX_DIMENSIONS];
	double dot = 0.0f, dot2 = 0.0f;
	int x;

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	if(mathutils_array_parse(tvec, size, size, value, "vector.project(other), invalid 'other' arg") == -1)
		return NULL;

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	//get dot products
	for(x = 0; x < size; x++) {
		dot += (double)(self->vec[x] * tvec[x]);
		dot2 += (double)(tvec[x] * tvec[x]);
	}
	//projection
	dot /= dot2;
	for(x = 0; x < size; x++) {
		vec[x] = (float)dot * tvec[x];
	}
	return newVectorObject(vec, size, Py_NEW, Py_TYPE(self));
}

PyDoc_STRVAR(Vector_lerp_doc,
".. function:: lerp(other, factor)\n"
"\n"
"   Returns the interpolation of two vectors.\n"
"\n"
"   :arg other: value to interpolate with.\n"
"   :type other: :class:`Vector`\n"
"   :arg factor: The interpolation value in [0.0, 1.0].\n"
"   :type factor: float\n"
"   :return: The interpolated rotation.\n"
"   :rtype: :class:`Vector`\n"
);
static PyObject *Vector_lerp(VectorObject *self, PyObject *args)
{
	const int size= self->size;
	PyObject *value= NULL;
	float fac, ifac;
	float tvec[MAX_DIMENSIONS], vec[MAX_DIMENSIONS];
	int x;

	if(!PyArg_ParseTuple(args, "Of:lerp", &value, &fac))
		return NULL;

	if(mathutils_array_parse(tvec, size, size, value, "vector.lerp(other), invalid 'other' arg") == -1)
		return NULL;

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	ifac= 1.0f - fac;

	for(x = 0; x < size; x++) {
		vec[x] = (ifac * self->vec[x]) + (fac * tvec[x]);
	}
	return newVectorObject(vec, size, Py_NEW, Py_TYPE(self));
}

PyDoc_STRVAR(Vector_rotate_doc,
".. function:: rotate(other)\n"
"\n"
"   Return vector by a rotation value.\n"
"\n"
"   :arg other: rotation component of mathutils value\n"
"   :type other: :class:`Euler`, :class:`Quaternion` or :class:`Matrix`\n"
);
static PyObject *Vector_rotate(VectorObject *self, PyObject *value)
{
	float other_rmat[3][3];

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	if(mathutils_any_to_rotmat(other_rmat, value, "vector.rotate(value)") == -1)
		return NULL;

	if(self->size < 3) {
		PyErr_SetString(PyExc_ValueError,
		                "Vector must be 3D or 4D");
		return NULL;
	}

	mul_m3_v3(other_rmat, self->vec);

	(void)BaseMath_WriteCallback(self);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(Vector_copy_doc,
".. function:: copy()\n"
"\n"
"   Returns a copy of this vector.\n"
"\n"
"   :return: A copy of the vector.\n"
"   :rtype: :class:`Vector`\n"
"\n"
"   .. note:: use this to get a copy of a wrapped vector with\n"
"      no reference to the original data.\n"
);
static PyObject *Vector_copy(VectorObject *self)
{
	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	return newVectorObject(self->vec, self->size, Py_NEW, Py_TYPE(self));
}

static PyObject *Vector_repr(VectorObject *self)
{
	PyObject *ret, *tuple;

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	tuple= Vector_to_tuple_ext(self, -1);
	ret= PyUnicode_FromFormat("Vector(%R)", tuple);
	Py_DECREF(tuple);
	return ret;
}

/* Sequence Protocol */
/* sequence length len(vector) */
static int Vector_len(VectorObject *self)
{
	return self->size;
}
/* sequence accessor (get): vector[index] */
static PyObject *vector_item_internal(VectorObject *self, int i, const int is_attr)
{
	if(i<0)	i= self->size-i;

	if(i < 0 || i >= self->size) {
		if(is_attr)	{
			PyErr_Format(PyExc_AttributeError,
			             "vector.%c: unavailable on %dd vector",
			             *(((char *)"xyzw") + i), self->size);
		}
		else {
			PyErr_SetString(PyExc_IndexError,
			                "vector[index]: out of range");
		}
		return NULL;
	}

	if(BaseMath_ReadIndexCallback(self, i) == -1)
		return NULL;

	return PyFloat_FromDouble(self->vec[i]);
}

static PyObject *Vector_item(VectorObject *self, int i)
{
	return vector_item_internal(self, i, FALSE);
}
/* sequence accessor (set): vector[index] = value */
static int vector_ass_item_internal(VectorObject *self, int i, PyObject *value, const int is_attr)
{
	float scalar;
	if((scalar=PyFloat_AsDouble(value))==-1.0f && PyErr_Occurred()) { /* parsed item not a number */
		PyErr_SetString(PyExc_TypeError,
		                "vector[index] = x: "
		                "index argument not a number");
		return -1;
	}

	if(i<0)	i= self->size-i;

	if(i < 0 || i >= self->size){
		if(is_attr) {
			PyErr_Format(PyExc_AttributeError,
			             "vector.%c = x: unavailable on %dd vector",
			             *(((char *)"xyzw") + i), self->size);
		}
		else {
			PyErr_SetString(PyExc_IndexError,
			                "vector[index] = x: "
			                "assignment index out of range");
		}
		return -1;
	}
	self->vec[i] = scalar;

	if(BaseMath_WriteIndexCallback(self, i) == -1)
		return -1;
	return 0;
}

static int Vector_ass_item(VectorObject *self, int i, PyObject *value)
{
	return vector_ass_item_internal(self, i, value, FALSE);
}

/* sequence slice (get): vector[a:b] */
static PyObject *Vector_slice(VectorObject *self, int begin, int end)
{
	PyObject *tuple;
	int count;

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	CLAMP(begin, 0, self->size);
	if (end<0) end= self->size+end+1;
	CLAMP(end, 0, self->size);
	begin= MIN2(begin, end);

	tuple= PyTuple_New(end - begin);
	for(count = begin; count < end; count++) {
		PyTuple_SET_ITEM(tuple, count - begin, PyFloat_FromDouble(self->vec[count]));
	}

	return tuple;
}
/* sequence slice (set): vector[a:b] = value */
static int Vector_ass_slice(VectorObject *self, int begin, int end, PyObject *seq)
{
	int y, size = 0;
	float vec[MAX_DIMENSIONS];

	if(BaseMath_ReadCallback(self) == -1)
		return -1;

	CLAMP(begin, 0, self->size);
	CLAMP(end, 0, self->size);
	begin = MIN2(begin, end);

	size = (end - begin);
	if(mathutils_array_parse(vec, size, size, seq, "vector[begin:end] = [...]") == -1)
		return -1;

	/*parsed well - now set in vector*/
	for(y = 0; y < size; y++){
		self->vec[begin + y] = vec[y];
	}

	if(BaseMath_WriteCallback(self) == -1)
		return -1;

	return 0;
}

/* Numeric Protocols */
/* addition: obj + obj */
static PyObject *Vector_add(PyObject *v1, PyObject *v2)
{
	VectorObject *vec1 = NULL, *vec2 = NULL;
	float vec[MAX_DIMENSIONS];

	if (!VectorObject_Check(v1) || !VectorObject_Check(v2)) {
		PyErr_SetString(PyExc_AttributeError,
		                "Vector addition: "
		                "arguments not valid for this operation");
		return NULL;
	}
	vec1 = (VectorObject*)v1;
	vec2 = (VectorObject*)v2;

	if(BaseMath_ReadCallback(vec1) == -1 || BaseMath_ReadCallback(vec2) == -1)
		return NULL;

	/*VECTOR + VECTOR*/
	if(vec1->size != vec2->size) {
		PyErr_SetString(PyExc_AttributeError,
		                "Vector addition: "
		                "vectors must have the same dimensions for this operation");
		return NULL;
	}

	add_vn_vnvn(vec, vec1->vec, vec2->vec, vec1->size);

	return newVectorObject(vec, vec1->size, Py_NEW, Py_TYPE(v1));
}

/* addition in-place: obj += obj */
static PyObject *Vector_iadd(PyObject *v1, PyObject *v2)
{
	VectorObject *vec1 = NULL, *vec2 = NULL;

	if (!VectorObject_Check(v1) || !VectorObject_Check(v2)) {
		PyErr_SetString(PyExc_AttributeError,
		                "Vector addition: "
		                "arguments not valid for this operation");
		return NULL;
	}
	vec1 = (VectorObject*)v1;
	vec2 = (VectorObject*)v2;

	if(vec1->size != vec2->size) {
		PyErr_SetString(PyExc_AttributeError,
		                "Vector addition: "
		                "vectors must have the same dimensions for this operation");
		return NULL;
	}

	if(BaseMath_ReadCallback(vec1) == -1 || BaseMath_ReadCallback(vec2) == -1)
		return NULL;

	add_vn_vn(vec1->vec, vec2->vec, vec1->size);

	(void)BaseMath_WriteCallback(vec1);
	Py_INCREF(v1);
	return v1;
}

/* subtraction: obj - obj */
static PyObject *Vector_sub(PyObject *v1, PyObject *v2)
{
	VectorObject *vec1 = NULL, *vec2 = NULL;
	float vec[MAX_DIMENSIONS];

	if (!VectorObject_Check(v1) || !VectorObject_Check(v2)) {
		PyErr_SetString(PyExc_AttributeError,
		                "Vector subtraction: "
		                "arguments not valid for this operation");
		return NULL;
	}
	vec1 = (VectorObject*)v1;
	vec2 = (VectorObject*)v2;

	if(BaseMath_ReadCallback(vec1) == -1 || BaseMath_ReadCallback(vec2) == -1)
		return NULL;

	if(vec1->size != vec2->size) {
		PyErr_SetString(PyExc_AttributeError,
		                "Vector subtraction: "
		                "vectors must have the same dimensions for this operation");
		return NULL;
	}

	sub_vn_vnvn(vec, vec1->vec, vec2->vec, vec1->size);

	return newVectorObject(vec, vec1->size, Py_NEW, Py_TYPE(v1));
}

/* subtraction in-place: obj -= obj */
static PyObject *Vector_isub(PyObject *v1, PyObject *v2)
{
	VectorObject *vec1= NULL, *vec2= NULL;

	if (!VectorObject_Check(v1) || !VectorObject_Check(v2)) {
		PyErr_SetString(PyExc_AttributeError,
		                "Vector subtraction: "
		                "arguments not valid for this operation");
		return NULL;
	}
	vec1 = (VectorObject*)v1;
	vec2 = (VectorObject*)v2;

	if(vec1->size != vec2->size) {
		PyErr_SetString(PyExc_AttributeError,
		                "Vector subtraction: "
		                "vectors must have the same dimensions for this operation");
		return NULL;
	}

	if(BaseMath_ReadCallback(vec1) == -1 || BaseMath_ReadCallback(vec2) == -1)
		return NULL;

	sub_vn_vn(vec1->vec, vec2->vec, vec1->size);

	(void)BaseMath_WriteCallback(vec1);
	Py_INCREF(v1);
	return v1;
}

/*------------------------obj * obj------------------------------
  mulplication*/


/* COLUMN VECTOR Multiplication (Vector X Matrix)
 * [a] * [1][4][7]
 * [b] * [2][5][8]
 * [c] * [3][6][9]
 *
 * note: vector/matrix multiplication IS NOT COMMUTATIVE!!!!
 * note: assume read callbacks have been done first.
 */
static int column_vector_multiplication(float rvec[MAX_DIMENSIONS], VectorObject* vec, MatrixObject * mat)
{
	float vec_cpy[MAX_DIMENSIONS];
	double dot = 0.0f;
	int x, y, z = 0;

	if(mat->row_size != vec->size){
		if(mat->row_size == 4 && vec->size == 3) {
			vec_cpy[3] = 1.0f;
		}
		else {
			PyErr_SetString(PyExc_TypeError,
			                "matrix * vector: "
			                "matrix.row_size and len(vector) must be the same, "
			                "except for 3D vector * 4x4 matrix.");
			return -1;
		}
	}

	memcpy(vec_cpy, vec->vec, vec->size * sizeof(float));

	rvec[3] = 1.0f;

	for(x = 0; x < mat->col_size; x++) {
		for(y = 0; y < mat->row_size; y++) {
			dot += (double)(mat->matrix[y][x] * vec_cpy[y]);
		}
		rvec[z++] = (float)dot;
		dot = 0.0f;
	}

	return 0;
}

static PyObject *vector_mul_float(VectorObject *vec, const float scalar)
{
	float tvec[MAX_DIMENSIONS];
	mul_vn_vn_fl(tvec, vec->vec, vec->size, scalar);
	return newVectorObject(tvec, vec->size, Py_NEW, Py_TYPE(vec));
}

static PyObject *Vector_mul(PyObject *v1, PyObject *v2)
{
	VectorObject *vec1 = NULL, *vec2 = NULL;
	float scalar;

	if VectorObject_Check(v1) {
		vec1= (VectorObject *)v1;
		if(BaseMath_ReadCallback(vec1) == -1)
			return NULL;
	}
	if VectorObject_Check(v2) {
		vec2= (VectorObject *)v2;
		if(BaseMath_ReadCallback(vec2) == -1)
			return NULL;
	}


	/* make sure v1 is always the vector */
	if (vec1 && vec2) {
		int i;
		double dot = 0.0f;

		if(vec1->size != vec2->size) {
			PyErr_SetString(PyExc_ValueError,
			                "Vector multiplication: "
			                "vectors must have the same dimensions for this operation");
			return NULL;
		}

		/*dot product*/
		for(i = 0; i < vec1->size; i++) {
			dot += (double)(vec1->vec[i] * vec2->vec[i]);
		}
		return PyFloat_FromDouble(dot);
	}
	else if (vec1) {
		if (MatrixObject_Check(v2)) {
			/* VEC * MATRIX */
			float tvec[MAX_DIMENSIONS];
			if(BaseMath_ReadCallback((MatrixObject *)v2) == -1)
				return NULL;
			if(column_vector_multiplication(tvec, vec1, (MatrixObject*)v2) == -1) {
				return NULL;
			}

			return newVectorObject(tvec, vec1->size, Py_NEW, Py_TYPE(vec1));
		}
		else if (QuaternionObject_Check(v2)) {
			/* VEC * QUAT */
			QuaternionObject *quat2 = (QuaternionObject*)v2;
			float tvec[3];

			if(vec1->size != 3) {
				PyErr_SetString(PyExc_ValueError,
				                "Vector multiplication: "
				                "only 3D vector rotations (with quats) currently supported");
				return NULL;
			}
			if(BaseMath_ReadCallback(quat2) == -1) {
				return NULL;
			}
			copy_v3_v3(tvec, vec1->vec);
			mul_qt_v3(quat2->quat, tvec);
			return newVectorObject(tvec, 3, Py_NEW, Py_TYPE(vec1));
		}
		else if (((scalar= PyFloat_AsDouble(v2)) == -1.0f && PyErr_Occurred())==0) { /* VEC * FLOAT */
			return vector_mul_float(vec1, scalar);
		}
	}
	else if (vec2) {
		if (((scalar= PyFloat_AsDouble(v1)) == -1.0f && PyErr_Occurred())==0) { /* FLOAT * VEC */
			return vector_mul_float(vec2, scalar);
		}
	}
	else {
		BLI_assert(!"internal error");
	}

	PyErr_Format(PyExc_TypeError,
	             "Vector multiplication: "
	             "not supported between '%.200s' and '%.200s' types",
	             Py_TYPE(v1)->tp_name, Py_TYPE(v2)->tp_name);
	return NULL;
}

/* mulplication in-place: obj *= obj */
static PyObject *Vector_imul(PyObject *v1, PyObject *v2)
{
	VectorObject *vec = (VectorObject *)v1;
	float scalar;

	if(BaseMath_ReadCallback(vec) == -1)
		return NULL;

	/* only support vec*=float and vec*=mat
	   vec*=vec result is a float so that wont work */
	if (MatrixObject_Check(v2)) {
		float rvec[MAX_DIMENSIONS];
		if(BaseMath_ReadCallback((MatrixObject *)v2) == -1)
			return NULL;

		if(column_vector_multiplication(rvec, vec, (MatrixObject*)v2) == -1)
			return NULL;

		memcpy(vec->vec, rvec, sizeof(float) * vec->size);
	}
	else if (QuaternionObject_Check(v2)) {
		/* VEC *= QUAT */
		QuaternionObject *quat2 = (QuaternionObject*)v2;

		if(vec->size != 3) {
			PyErr_SetString(PyExc_ValueError,
			                "Vector multiplication: "
			                "only 3D vector rotations (with quats) currently supported");
			return NULL;
		}

		if(BaseMath_ReadCallback(quat2) == -1) {
			return NULL;
		}
		mul_qt_v3(quat2->quat, vec->vec);
	}
	else if (((scalar= PyFloat_AsDouble(v2)) == -1.0f && PyErr_Occurred())==0) { /* VEC *= FLOAT */
		mul_vn_fl(vec->vec, vec->size, scalar);
	}
	else {
		PyErr_SetString(PyExc_TypeError,
		                "Vector multiplication: "
		                "arguments not acceptable for this operation");
		return NULL;
	}

	(void)BaseMath_WriteCallback(vec);
	Py_INCREF(v1);
	return v1;
}

/* divid: obj / obj */
static PyObject *Vector_div(PyObject *v1, PyObject *v2)
{
	int i;
	float vec[4], scalar;
	VectorObject *vec1 = NULL;

	if(!VectorObject_Check(v1)) { /* not a vector */
		PyErr_SetString(PyExc_TypeError,
		                "Vector division: "
		                "Vector must be divided by a float");
		return NULL;
	}
	vec1 = (VectorObject*)v1; /* vector */

	if(BaseMath_ReadCallback(vec1) == -1)
		return NULL;

	if((scalar=PyFloat_AsDouble(v2)) == -1.0f && PyErr_Occurred()) { /* parsed item not a number */
		PyErr_SetString(PyExc_TypeError,
		                "Vector division: "
		                "Vector must be divided by a float");
		return NULL;
	}

	if(scalar==0.0f) {
		PyErr_SetString(PyExc_ZeroDivisionError,
		                "Vector division: "
		                "divide by zero error");
		return NULL;
	}

	for(i = 0; i < vec1->size; i++) {
		vec[i] = vec1->vec[i] /	scalar;
	}
	return newVectorObject(vec, vec1->size, Py_NEW, Py_TYPE(v1));
}

/* divide in-place: obj /= obj */
static PyObject *Vector_idiv(PyObject *v1, PyObject *v2)
{
	int i;
	float scalar;
	VectorObject *vec1 = (VectorObject*)v1;

	if(BaseMath_ReadCallback(vec1) == -1)
		return NULL;

	if((scalar=PyFloat_AsDouble(v2)) == -1.0f && PyErr_Occurred()) { /* parsed item not a number */
		PyErr_SetString(PyExc_TypeError,
		                "Vector division: "
		                "Vector must be divided by a float");
		return NULL;
	}

	if(scalar==0.0f) {
		PyErr_SetString(PyExc_ZeroDivisionError,
		                "Vector division: "
		                "divide by zero error");
		return NULL;
	}
	for(i = 0; i < vec1->size; i++) {
		vec1->vec[i] /=	scalar;
	}

	(void)BaseMath_WriteCallback(vec1);

	Py_INCREF(v1);
	return v1;
}

/* -obj
  returns the negative of this object*/
static PyObject *Vector_neg(VectorObject *self)
{
	float tvec[MAX_DIMENSIONS];

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	negate_vn_vn(tvec, self->vec, self->size);
	return newVectorObject(tvec, self->size, Py_NEW, Py_TYPE(self));
}

/*------------------------vec_magnitude_nosqrt (internal) - for comparing only */
static double vec_magnitude_nosqrt(float *data, int size)
{
	double dot = 0.0f;
	int i;

	for(i=0; i<size; i++){
		dot += (double)data[i];
	}
	/*return (double)sqrt(dot);*/
	/* warning, line above removed because we are not using the length,
	   rather the comparing the sizes and for this we do not need the sqrt
	   for the actual length, the dot must be sqrt'd */
	return dot;
}


/*------------------------tp_richcmpr
  returns -1 execption, 0 false, 1 true */
static PyObject* Vector_richcmpr(PyObject *objectA, PyObject *objectB, int comparison_type)
{
	VectorObject *vecA = NULL, *vecB = NULL;
	int result = 0;
	double epsilon = .000001f;
	double lenA, lenB;

	if (!VectorObject_Check(objectA) || !VectorObject_Check(objectB)){
		if (comparison_type == Py_NE){
			Py_RETURN_TRUE;
		}
		else {
			Py_RETURN_FALSE;
		}
	}
	vecA = (VectorObject*)objectA;
	vecB = (VectorObject*)objectB;

	if(BaseMath_ReadCallback(vecA) == -1 || BaseMath_ReadCallback(vecB) == -1)
		return NULL;

	if (vecA->size != vecB->size){
		if (comparison_type == Py_NE){
			Py_RETURN_TRUE;
		}
		else {
			Py_RETURN_FALSE;
		}
	}

	switch (comparison_type){
		case Py_LT:
			lenA = vec_magnitude_nosqrt(vecA->vec, vecA->size);
			lenB = vec_magnitude_nosqrt(vecB->vec, vecB->size);
			if(lenA < lenB){
				result = 1;
			}
			break;
		case Py_LE:
			lenA = vec_magnitude_nosqrt(vecA->vec, vecA->size);
			lenB = vec_magnitude_nosqrt(vecB->vec, vecB->size);
			if(lenA < lenB){
				result = 1;
			}
			else {
				result = (((lenA + epsilon) > lenB) && ((lenA - epsilon) < lenB));
			}
			break;
		case Py_EQ:
			result = EXPP_VectorsAreEqual(vecA->vec, vecB->vec, vecA->size, 1);
			break;
		case Py_NE:
			result = !EXPP_VectorsAreEqual(vecA->vec, vecB->vec, vecA->size, 1);
			break;
		case Py_GT:
			lenA = vec_magnitude_nosqrt(vecA->vec, vecA->size);
			lenB = vec_magnitude_nosqrt(vecB->vec, vecB->size);
			if(lenA > lenB){
				result = 1;
			}
			break;
		case Py_GE:
			lenA = vec_magnitude_nosqrt(vecA->vec, vecA->size);
			lenB = vec_magnitude_nosqrt(vecB->vec, vecB->size);
			if(lenA > lenB){
				result = 1;
			}
			else {
				result = (((lenA + epsilon) > lenB) && ((lenA - epsilon) < lenB));
			}
			break;
		default:
			printf("The result of the comparison could not be evaluated");
			break;
	}
	if (result == 1){
		Py_RETURN_TRUE;
	}
	else {
		Py_RETURN_FALSE;
	}
}

/*-----------------PROTCOL DECLARATIONS--------------------------*/
static PySequenceMethods Vector_SeqMethods = {
	(lenfunc) Vector_len,				/* sq_length */
	(binaryfunc) NULL,					/* sq_concat */
	(ssizeargfunc) NULL,				/* sq_repeat */
	(ssizeargfunc) Vector_item,			/* sq_item */
	NULL,								/* py3 deprecated slice func */
	(ssizeobjargproc) Vector_ass_item,	/* sq_ass_item */
	NULL,								/* py3 deprecated slice assign func */
	(objobjproc) NULL,					/* sq_contains */
	(binaryfunc) NULL,					/* sq_inplace_concat */
	(ssizeargfunc) NULL,				/* sq_inplace_repeat */
};

static PyObject *Vector_subscript(VectorObject* self, PyObject* item)
{
	if (PyIndex_Check(item)) {
		Py_ssize_t i;
		i = PyNumber_AsSsize_t(item, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return NULL;
		if (i < 0)
			i += self->size;
		return Vector_item(self, i);
	}
	else if (PySlice_Check(item)) {
		Py_ssize_t start, stop, step, slicelength;

		if (PySlice_GetIndicesEx((void *)item, self->size, &start, &stop, &step, &slicelength) < 0)
			return NULL;

		if (slicelength <= 0) {
			return PyTuple_New(0);
		}
		else if (step == 1) {
			return Vector_slice(self, start, stop);
		}
		else {
			PyErr_SetString(PyExc_IndexError,
			                "slice steps not supported with vectors");
			return NULL;
		}
	}
	else {
		PyErr_Format(PyExc_TypeError,
		             "vector indices must be integers, not %.200s",
		             Py_TYPE(item)->tp_name);
		return NULL;
	}
}

static int Vector_ass_subscript(VectorObject* self, PyObject* item, PyObject* value)
{
	if (PyIndex_Check(item)) {
		Py_ssize_t i = PyNumber_AsSsize_t(item, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return -1;
		if (i < 0)
			i += self->size;
		return Vector_ass_item(self, i, value);
	}
	else if (PySlice_Check(item)) {
		Py_ssize_t start, stop, step, slicelength;

		if (PySlice_GetIndicesEx((void *)item, self->size, &start, &stop, &step, &slicelength) < 0)
			return -1;

		if (step == 1)
			return Vector_ass_slice(self, start, stop, value);
		else {
			PyErr_SetString(PyExc_IndexError,
			                "slice steps not supported with vectors");
			return -1;
		}
	}
	else {
		PyErr_Format(PyExc_TypeError,
		             "vector indices must be integers, not %.200s",
		             Py_TYPE(item)->tp_name);
		return -1;
	}
}

static PyMappingMethods Vector_AsMapping = {
	(lenfunc)Vector_len,
	(binaryfunc)Vector_subscript,
	(objobjargproc)Vector_ass_subscript
};


static PyNumberMethods Vector_NumMethods = {
	(binaryfunc)	Vector_add,	/*nb_add*/
	(binaryfunc)	Vector_sub,	/*nb_subtract*/
	(binaryfunc)	Vector_mul,	/*nb_multiply*/
	NULL,							/*nb_remainder*/
	NULL,							/*nb_divmod*/
	NULL,							/*nb_power*/
	(unaryfunc) 	Vector_neg,	/*nb_negative*/
	(unaryfunc) 	NULL,	/*tp_positive*/
	(unaryfunc) 	NULL,	/*tp_absolute*/
	(inquiry)	NULL,	/*tp_bool*/
	(unaryfunc)	NULL,	/*nb_invert*/
	NULL,				/*nb_lshift*/
	(binaryfunc)NULL,	/*nb_rshift*/
	NULL,				/*nb_and*/
	NULL,				/*nb_xor*/
	NULL,				/*nb_or*/
	NULL,				/*nb_int*/
	NULL,				/*nb_reserved*/
	NULL,				/*nb_float*/
	Vector_iadd,	/* nb_inplace_add */
	Vector_isub,	/* nb_inplace_subtract */
	Vector_imul,	/* nb_inplace_multiply */
	NULL,				/* nb_inplace_remainder */
	NULL,				/* nb_inplace_power */
	NULL,				/* nb_inplace_lshift */
	NULL,				/* nb_inplace_rshift */
	NULL,				/* nb_inplace_and */
	NULL,				/* nb_inplace_xor */
	NULL,				/* nb_inplace_or */
	NULL,				/* nb_floor_divide */
	Vector_div,		/* nb_true_divide */
	NULL,				/* nb_inplace_floor_divide */
	Vector_idiv,	/* nb_inplace_true_divide */
	NULL,			/* nb_index */
};

/*------------------PY_OBECT DEFINITION--------------------------*/

/*
 * vector axis, vector.x/y/z/w
 */

static PyObject *Vector_getAxis(VectorObject *self, void *type)
{
	return vector_item_internal(self, GET_INT_FROM_POINTER(type), TRUE);
}

static int Vector_setAxis(VectorObject *self, PyObject *value, void *type)
{
	return vector_ass_item_internal(self, GET_INT_FROM_POINTER(type), value, TRUE);
}

/* vector.length */
static PyObject *Vector_getLength(VectorObject *self, void *UNUSED(closure))
{
	double dot = 0.0f;
	int i;

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	for(i = 0; i < self->size; i++){
		dot += (double)(self->vec[i] * self->vec[i]);
	}
	return PyFloat_FromDouble(sqrt(dot));
}

static int Vector_setLength(VectorObject *self, PyObject *value)
{
	double dot = 0.0f, param;
	int i;

	if(BaseMath_ReadCallback(self) == -1)
		return -1;

	if((param=PyFloat_AsDouble(value)) == -1.0 && PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError,
		                "length must be set to a number");
		return -1;
	}

	if (param < 0.0) {
		PyErr_SetString(PyExc_ValueError,
		                "cannot set a vectors length to a negative value");
		return -1;
	}
	if (param == 0.0) {
		fill_vn(self->vec, self->size, 0.0f);
		return 0;
	}

	for(i = 0; i < self->size; i++){
		dot += (double)(self->vec[i] * self->vec[i]);
	}

	if (!dot) /* cant sqrt zero */
		return 0;

	dot = sqrt(dot);

	if (dot==param)
		return 0;

	dot= dot/param;

	for(i = 0; i < self->size; i++){
		self->vec[i]= self->vec[i] / (float)dot;
	}

	(void)BaseMath_WriteCallback(self); /* checked already */

	return 0;
}

/* Get a new Vector according to the provided swizzle. This function has little
   error checking, as we are in control of the inputs: the closure is set by us
   in Vector_createSwizzleGetSeter. */
static PyObject *Vector_getSwizzle(VectorObject *self, void *closure)
{
	size_t axis_to;
	size_t axis_from;
	float vec[MAX_DIMENSIONS];
	unsigned int swizzleClosure;

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	/* Unpack the axes from the closure into an array. */
	axis_to = 0;
	swizzleClosure = GET_INT_FROM_POINTER(closure);
	while (swizzleClosure & SWIZZLE_VALID_AXIS)
	{
		axis_from = swizzleClosure & SWIZZLE_AXIS;
		if(axis_from >= self->size) {
			PyErr_SetString(PyExc_AttributeError,
			                "Vector swizzle: "
			                "specified axis not present");
			return NULL;
		}

		vec[axis_to] = self->vec[axis_from];
		swizzleClosure = swizzleClosure >> SWIZZLE_BITS_PER_AXIS;
		axis_to++;
	}

	return newVectorObject(vec, axis_to, Py_NEW, Py_TYPE(self));
}

/* Set the items of this vector using a swizzle.
   - If value is a vector or list this operates like an array copy, except that
	 the destination is effectively re-ordered as defined by the swizzle. At
	 most min(len(source), len(dest)) values will be copied.
   - If the value is scalar, it is copied to all axes listed in the swizzle.
   - If an axis appears more than once in the swizzle, the final occurrence is
	 the one that determines its value.

   Returns 0 on success and -1 on failure. On failure, the vector will be
   unchanged. */
static int Vector_setSwizzle(VectorObject *self, PyObject *value, void *closure)
{
	size_t size_from;
	float scalarVal;

	size_t axis_from;
	size_t axis_to;

	unsigned int swizzleClosure;

	float tvec[MAX_DIMENSIONS];
	float vec_assign[MAX_DIMENSIONS];

	if(BaseMath_ReadCallback(self) == -1)
		return -1;

	/* Check that the closure can be used with this vector: even 2D vectors have
	   swizzles defined for axes z and w, but they would be invalid. */
	swizzleClosure = GET_INT_FROM_POINTER(closure);
	axis_from= 0;
	while (swizzleClosure & SWIZZLE_VALID_AXIS) {
		axis_to = swizzleClosure & SWIZZLE_AXIS;
		if (axis_to >= self->size)
		{
			PyErr_SetString(PyExc_AttributeError,
			                "Vector swizzle: "
			                "specified axis not present");
			return -1;
		}
		swizzleClosure = swizzleClosure >> SWIZZLE_BITS_PER_AXIS;
		axis_from++;
	}

	if (((scalarVal=PyFloat_AsDouble(value)) == -1 && PyErr_Occurred())==0) {
		int i;
		for(i=0; i < MAX_DIMENSIONS; i++)
			vec_assign[i]= scalarVal;

		size_from= axis_from;
	}
	else if(PyErr_Clear(), (size_from=mathutils_array_parse(vec_assign, 2, 4, value, "mathutils.Vector.**** = swizzle assignment")) == -1) {
		return -1;
	}

	if(axis_from != size_from) {
		PyErr_SetString(PyExc_AttributeError,
		                "Vector swizzle: size does not match swizzle");
		return -1;
	}

	/* Copy vector contents onto swizzled axes. */
	axis_from = 0;
	swizzleClosure = GET_INT_FROM_POINTER(closure);
	while (swizzleClosure & SWIZZLE_VALID_AXIS)
	{
		axis_to = swizzleClosure & SWIZZLE_AXIS;
		tvec[axis_to] = vec_assign[axis_from];
		swizzleClosure = swizzleClosure >> SWIZZLE_BITS_PER_AXIS;
		axis_from++;
	}

	memcpy(self->vec, tvec, axis_from * sizeof(float));
	/* continue with BaseMathObject_WriteCallback at the end */

	if(BaseMath_WriteCallback(self) == -1)
		return -1;
	else
		return 0;
}

/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef Vector_getseters[] = {
	{(char *)"x", (getter)Vector_getAxis, (setter)Vector_setAxis, (char *)"Vector X axis.\n\n:type: float", (void *)0},
	{(char *)"y", (getter)Vector_getAxis, (setter)Vector_setAxis, (char *)"Vector Y axis.\n\n:type: float", (void *)1},
	{(char *)"z", (getter)Vector_getAxis, (setter)Vector_setAxis, (char *)"Vector Z axis (3D Vectors only).\n\n:type: float", (void *)2},
	{(char *)"w", (getter)Vector_getAxis, (setter)Vector_setAxis, (char *)"Vector W axis (4D Vectors only).\n\n:type: float", (void *)3},
	{(char *)"length", (getter)Vector_getLength, (setter)Vector_setLength, (char *)"Vector Length.\n\n:type: float", NULL},
	{(char *)"magnitude", (getter)Vector_getLength, (setter)Vector_setLength, (char *)"Vector Length.\n\n:type: float", NULL},
	{(char *)"is_wrapped", (getter)BaseMathObject_getWrapped, (setter)NULL, (char *)BaseMathObject_Wrapped_doc, NULL},
	{(char *)"owner", (getter)BaseMathObject_getOwner, (setter)NULL, (char *)BaseMathObject_Owner_doc, NULL},

	/* autogenerated swizzle attrs, see python script below */
	{(char *)"xx",   (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 36
	{(char *)"xxx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 292
	{(char *)"xxxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2340
	{(char *)"xxxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2852
	{(char *)"xxxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3364
	{(char *)"xxxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3876
	{(char *)"xxy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 356
	{(char *)"xxyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2404
	{(char *)"xxyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2916
	{(char *)"xxyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3428
	{(char *)"xxyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3940
	{(char *)"xxz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 420
	{(char *)"xxzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2468
	{(char *)"xxzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2980
	{(char *)"xxzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3492
	{(char *)"xxzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4004
	{(char *)"xxw",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 484
	{(char *)"xxwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2532
	{(char *)"xxwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3044
	{(char *)"xxwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3556
	{(char *)"xxww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4068
	{(char *)"xy",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 44
	{(char *)"xyx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 300
	{(char *)"xyxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2348
	{(char *)"xyxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2860
	{(char *)"xyxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3372
	{(char *)"xyxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3884
	{(char *)"xyy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 364
	{(char *)"xyyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2412
	{(char *)"xyyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2924
	{(char *)"xyyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3436
	{(char *)"xyyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3948
	{(char *)"xyz",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 428
	{(char *)"xyzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2476
	{(char *)"xyzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2988
	{(char *)"xyzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3500
	{(char *)"xyzw", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4012
	{(char *)"xyw",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 492
	{(char *)"xywx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2540
	{(char *)"xywy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3052
	{(char *)"xywz", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3564
	{(char *)"xyww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4076
	{(char *)"xz",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 52
	{(char *)"xzx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 308
	{(char *)"xzxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2356
	{(char *)"xzxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2868
	{(char *)"xzxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3380
	{(char *)"xzxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3892
	{(char *)"xzy",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 372
	{(char *)"xzyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2420
	{(char *)"xzyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2932
	{(char *)"xzyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3444
	{(char *)"xzyw", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3956
	{(char *)"xzz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 436
	{(char *)"xzzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2484
	{(char *)"xzzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2996
	{(char *)"xzzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3508
	{(char *)"xzzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4020
	{(char *)"xzw",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 500
	{(char *)"xzwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2548
	{(char *)"xzwy", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3060
	{(char *)"xzwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3572
	{(char *)"xzww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4084
	{(char *)"xw",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 60
	{(char *)"xwx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 316
	{(char *)"xwxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2364
	{(char *)"xwxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2876
	{(char *)"xwxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3388
	{(char *)"xwxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3900
	{(char *)"xwy",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 380
	{(char *)"xwyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2428
	{(char *)"xwyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2940
	{(char *)"xwyz", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3452
	{(char *)"xwyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3964
	{(char *)"xwz",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 444
	{(char *)"xwzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2492
	{(char *)"xwzy", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3004
	{(char *)"xwzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3516
	{(char *)"xwzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4028
	{(char *)"xww",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 508
	{(char *)"xwwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2556
	{(char *)"xwwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3068
	{(char *)"xwwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3580
	{(char *)"xwww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((0|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4092
	{(char *)"yx",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 37
	{(char *)"yxx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 293
	{(char *)"yxxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2341
	{(char *)"yxxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2853
	{(char *)"yxxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3365
	{(char *)"yxxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3877
	{(char *)"yxy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 357
	{(char *)"yxyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2405
	{(char *)"yxyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2917
	{(char *)"yxyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3429
	{(char *)"yxyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3941
	{(char *)"yxz",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 421
	{(char *)"yxzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2469
	{(char *)"yxzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2981
	{(char *)"yxzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3493
	{(char *)"yxzw", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4005
	{(char *)"yxw",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 485
	{(char *)"yxwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2533
	{(char *)"yxwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3045
	{(char *)"yxwz", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3557
	{(char *)"yxww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4069
	{(char *)"yy",   (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 45
	{(char *)"yyx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 301
	{(char *)"yyxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2349
	{(char *)"yyxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2861
	{(char *)"yyxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3373
	{(char *)"yyxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3885
	{(char *)"yyy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 365
	{(char *)"yyyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2413
	{(char *)"yyyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2925
	{(char *)"yyyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3437
	{(char *)"yyyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3949
	{(char *)"yyz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 429
	{(char *)"yyzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2477
	{(char *)"yyzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2989
	{(char *)"yyzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3501
	{(char *)"yyzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4013
	{(char *)"yyw",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 493
	{(char *)"yywx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2541
	{(char *)"yywy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3053
	{(char *)"yywz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3565
	{(char *)"yyww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4077
	{(char *)"yz",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 53
	{(char *)"yzx",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 309
	{(char *)"yzxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2357
	{(char *)"yzxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2869
	{(char *)"yzxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3381
	{(char *)"yzxw", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3893
	{(char *)"yzy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 373
	{(char *)"yzyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2421
	{(char *)"yzyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2933
	{(char *)"yzyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3445
	{(char *)"yzyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3957
	{(char *)"yzz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 437
	{(char *)"yzzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2485
	{(char *)"yzzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2997
	{(char *)"yzzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3509
	{(char *)"yzzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4021
	{(char *)"yzw",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 501
	{(char *)"yzwx", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2549
	{(char *)"yzwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3061
	{(char *)"yzwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3573
	{(char *)"yzww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4085
	{(char *)"yw",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 61
	{(char *)"ywx",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 317
	{(char *)"ywxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2365
	{(char *)"ywxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2877
	{(char *)"ywxz", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3389
	{(char *)"ywxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3901
	{(char *)"ywy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 381
	{(char *)"ywyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2429
	{(char *)"ywyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2941
	{(char *)"ywyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3453
	{(char *)"ywyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3965
	{(char *)"ywz",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 445
	{(char *)"ywzx", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2493
	{(char *)"ywzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3005
	{(char *)"ywzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3517
	{(char *)"ywzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4029
	{(char *)"yww",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 509
	{(char *)"ywwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2557
	{(char *)"ywwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3069
	{(char *)"ywwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3581
	{(char *)"ywww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((1|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4093
	{(char *)"zx",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 38
	{(char *)"zxx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 294
	{(char *)"zxxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2342
	{(char *)"zxxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2854
	{(char *)"zxxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3366
	{(char *)"zxxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3878
	{(char *)"zxy",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 358
	{(char *)"zxyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2406
	{(char *)"zxyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2918
	{(char *)"zxyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3430
	{(char *)"zxyw", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3942
	{(char *)"zxz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 422
	{(char *)"zxzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2470
	{(char *)"zxzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2982
	{(char *)"zxzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3494
	{(char *)"zxzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4006
	{(char *)"zxw",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 486
	{(char *)"zxwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2534
	{(char *)"zxwy", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3046
	{(char *)"zxwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3558
	{(char *)"zxww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4070
	{(char *)"zy",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 46
	{(char *)"zyx",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 302
	{(char *)"zyxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2350
	{(char *)"zyxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2862
	{(char *)"zyxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3374
	{(char *)"zyxw", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3886
	{(char *)"zyy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 366
	{(char *)"zyyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2414
	{(char *)"zyyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2926
	{(char *)"zyyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3438
	{(char *)"zyyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3950
	{(char *)"zyz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 430
	{(char *)"zyzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2478
	{(char *)"zyzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2990
	{(char *)"zyzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3502
	{(char *)"zyzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4014
	{(char *)"zyw",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 494
	{(char *)"zywx", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2542
	{(char *)"zywy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3054
	{(char *)"zywz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3566
	{(char *)"zyww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4078
	{(char *)"zz",   (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 54
	{(char *)"zzx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 310
	{(char *)"zzxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2358
	{(char *)"zzxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2870
	{(char *)"zzxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3382
	{(char *)"zzxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3894
	{(char *)"zzy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 374
	{(char *)"zzyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2422
	{(char *)"zzyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2934
	{(char *)"zzyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3446
	{(char *)"zzyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3958
	{(char *)"zzz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 438
	{(char *)"zzzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2486
	{(char *)"zzzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2998
	{(char *)"zzzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3510
	{(char *)"zzzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4022
	{(char *)"zzw",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 502
	{(char *)"zzwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2550
	{(char *)"zzwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3062
	{(char *)"zzwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3574
	{(char *)"zzww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4086
	{(char *)"zw",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 62
	{(char *)"zwx",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 318
	{(char *)"zwxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2366
	{(char *)"zwxy", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2878
	{(char *)"zwxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3390
	{(char *)"zwxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3902
	{(char *)"zwy",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 382
	{(char *)"zwyx", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2430
	{(char *)"zwyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2942
	{(char *)"zwyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3454
	{(char *)"zwyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3966
	{(char *)"zwz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 446
	{(char *)"zwzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2494
	{(char *)"zwzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3006
	{(char *)"zwzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3518
	{(char *)"zwzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4030
	{(char *)"zww",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 510
	{(char *)"zwwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2558
	{(char *)"zwwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3070
	{(char *)"zwwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3582
	{(char *)"zwww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((2|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4094
	{(char *)"wx",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 39
	{(char *)"wxx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 295
	{(char *)"wxxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2343
	{(char *)"wxxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2855
	{(char *)"wxxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3367
	{(char *)"wxxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3879
	{(char *)"wxy",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 359
	{(char *)"wxyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2407
	{(char *)"wxyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2919
	{(char *)"wxyz", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3431
	{(char *)"wxyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3943
	{(char *)"wxz",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 423
	{(char *)"wxzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2471
	{(char *)"wxzy", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2983
	{(char *)"wxzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3495
	{(char *)"wxzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4007
	{(char *)"wxw",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 487
	{(char *)"wxwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2535
	{(char *)"wxwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3047
	{(char *)"wxwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3559
	{(char *)"wxww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4071
	{(char *)"wy",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 47
	{(char *)"wyx",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 303
	{(char *)"wyxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2351
	{(char *)"wyxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2863
	{(char *)"wyxz", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3375
	{(char *)"wyxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3887
	{(char *)"wyy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 367
	{(char *)"wyyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2415
	{(char *)"wyyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2927
	{(char *)"wyyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3439
	{(char *)"wyyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3951
	{(char *)"wyz",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 431
	{(char *)"wyzx", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2479
	{(char *)"wyzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2991
	{(char *)"wyzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3503
	{(char *)"wyzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4015
	{(char *)"wyw",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 495
	{(char *)"wywx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2543
	{(char *)"wywy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3055
	{(char *)"wywz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3567
	{(char *)"wyww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4079
	{(char *)"wz",   (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 55
	{(char *)"wzx",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 311
	{(char *)"wzxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2359
	{(char *)"wzxy", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2871
	{(char *)"wzxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3383
	{(char *)"wzxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3895
	{(char *)"wzy",  (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 375
	{(char *)"wzyx", (getter)Vector_getSwizzle, (setter)Vector_setSwizzle, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2423
	{(char *)"wzyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2935
	{(char *)"wzyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3447
	{(char *)"wzyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3959
	{(char *)"wzz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 439
	{(char *)"wzzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2487
	{(char *)"wzzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2999
	{(char *)"wzzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3511
	{(char *)"wzzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4023
	{(char *)"wzw",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 503
	{(char *)"wzwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2551
	{(char *)"wzwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3063
	{(char *)"wzwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3575
	{(char *)"wzww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4087
	{(char *)"ww",   (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS)))}, // 63
	{(char *)"wwx",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 319
	{(char *)"wwxx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2367
	{(char *)"wwxy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2879
	{(char *)"wwxz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3391
	{(char *)"wwxw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3903
	{(char *)"wwy",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 383
	{(char *)"wwyx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2431
	{(char *)"wwyy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2943
	{(char *)"wwyz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3455
	{(char *)"wwyw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3967
	{(char *)"wwz",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 447
	{(char *)"wwzx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2495
	{(char *)"wwzy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3007
	{(char *)"wwzz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3519
	{(char *)"wwzw", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4031
	{(char *)"www",  (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2))))}, // 511
	{(char *)"wwwx", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((0|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 2559
	{(char *)"wwwy", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((1|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3071
	{(char *)"wwwz", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((2|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 3583
	{(char *)"wwww", (getter)Vector_getSwizzle, (setter)NULL, NULL, SET_INT_IN_POINTER(((3|SWIZZLE_VALID_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((3|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  )}, // 4095
	{NULL, NULL, NULL, NULL, NULL}  /* Sentinel */
};

/* Python script used to make swizzle array */
/*
SWIZZLE_BITS_PER_AXIS = 3
SWIZZLE_VALID_AXIS = 0x4

axis_dict = {}
axis_pos = {'x':0, 'y':1, 'z':2, 'w':3}
axises = 'xyzw'
while len(axises) >= 2:

	for axis_0 in axises:
		axis_0_pos = axis_pos[axis_0]
		for axis_1 in axises:
			axis_1_pos = axis_pos[axis_1]
			axis_dict[axis_0+axis_1] = '((%s|SWIZZLE_VALID_AXIS) | ((%s|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS))' % (axis_0_pos, axis_1_pos)
			if len(axises)>2:
				for axis_2 in axises:
					axis_2_pos = axis_pos[axis_2]
					axis_dict[axis_0+axis_1+axis_2] = '((%s|SWIZZLE_VALID_AXIS) | ((%s|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((%s|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)))' % (axis_0_pos, axis_1_pos, axis_2_pos)
					if len(axises)>3:
						for axis_3 in axises:
							axis_3_pos = axis_pos[axis_3]
							axis_dict[axis_0+axis_1+axis_2+axis_3] = '((%s|SWIZZLE_VALID_AXIS) | ((%s|SWIZZLE_VALID_AXIS)<<SWIZZLE_BITS_PER_AXIS) | ((%s|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*2)) | ((%s|SWIZZLE_VALID_AXIS)<<(SWIZZLE_BITS_PER_AXIS*3)))  ' % (axis_0_pos, axis_1_pos, axis_2_pos, axis_3_pos)

	axises = axises[:-1]


items = axis_dict.items()
items.sort(key = lambda a: a[0].replace('x', '0').replace('y', '1').replace('z', '2').replace('w', '3'))

unique = set()
for key, val in items:
	num = eval(val)
	set_str = 'Vector_setSwizzle' if (len(set(key)) == len(key)) else 'NULL'
	print '\t{"%s", %s(getter)Vector_getSwizzle, (setter)%s, NULL, SET_INT_IN_POINTER(%s)}, // %s' % (key, (' '*(4-len(key))), set_str, axis_dict[key], num)
	unique.add(num)

if len(unique) != len(items):
	print "ERROR"
*/

#if 0
//ROW VECTOR Multiplication - Vector X Matrix
//[x][y][z] *  [1][4][7]
//             [2][5][8]
//             [3][6][9]
//vector/matrix multiplication IS NOT COMMUTATIVE!!!!
static int row_vector_multiplication(float rvec[4], VectorObject* vec, MatrixObject * mat)
{
	float vec_cpy[4];
	double dot = 0.0f;
	int x, y, z = 0, vec_size = vec->size;

	if(mat->colSize != vec_size){
		if(mat->colSize == 4 && vec_size != 3){
			PyErr_SetString(PyExc_ValueError,
			                "vector * matrix: matrix column size "
			                "and the vector size must be the same");
			return -1;
		}
		else {
			vec_cpy[3] = 1.0f;
		}
	}

	if(BaseMath_ReadCallback(vec) == -1 || BaseMath_ReadCallback(mat) == -1)
		return -1;

	memcpy(vec_cpy, vec->vec, vec_size * sizeof(float));

	rvec[3] = 1.0f;
	//muliplication
	for(x = 0; x < mat->rowSize; x++) {
		for(y = 0; y < mat->colSize; y++) {
			dot += mat->matrix[x][y] * vec_cpy[y];
		}
		rvec[z++] = (float)dot;
		dot = 0.0f;
	}
	return 0;
}
#endif

/*----------------------------Vector.negate() -------------------- */
PyDoc_STRVAR(Vector_negate_doc,
".. method:: negate()\n"
"\n"
"   Set all values to their negative.\n"
"\n"
"   :return: an instance of itself\n"
"   :rtype: :class:`Vector`\n"
);
static PyObject *Vector_negate(VectorObject *self)
{
	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	negate_vn(self->vec, self->size);

	(void)BaseMath_WriteCallback(self); // already checked for error
	Py_RETURN_NONE;
}

static struct PyMethodDef Vector_methods[] = {
	/* in place only */
	{"zero", (PyCFunction) Vector_zero, METH_NOARGS, Vector_zero_doc},
	{"negate", (PyCFunction) Vector_negate, METH_NOARGS, Vector_negate_doc},

	/* operate on original or copy */
	{"normalize", (PyCFunction) Vector_normalize, METH_NOARGS, Vector_normalize_doc},
	{"normalized", (PyCFunction) Vector_normalized, METH_NOARGS, Vector_normalized_doc},

	{"to_2d", (PyCFunction) Vector_to_2d, METH_NOARGS, Vector_to_2d_doc},
	{"resize_2d", (PyCFunction) Vector_resize_2d, METH_NOARGS, Vector_resize_2d_doc},
	{"to_3d", (PyCFunction) Vector_to_3d, METH_NOARGS, Vector_to_3d_doc},
	{"resize_3d", (PyCFunction) Vector_resize_3d, METH_NOARGS, Vector_resize_3d_doc},
	{"to_4d", (PyCFunction) Vector_to_4d, METH_NOARGS, Vector_to_4d_doc},
	{"resize_4d", (PyCFunction) Vector_resize_4d, METH_NOARGS, Vector_resize_4d_doc},
	{"to_tuple", (PyCFunction) Vector_to_tuple, METH_VARARGS, Vector_to_tuple_doc},
	{"to_track_quat", (PyCFunction) Vector_to_track_quat, METH_VARARGS, Vector_to_track_quat_doc},

	/* operation between 2 or more types  */
	{"reflect", (PyCFunction) Vector_reflect, METH_O, Vector_reflect_doc},
	{"cross", (PyCFunction) Vector_cross, METH_O, Vector_cross_doc},
	{"dot", (PyCFunction) Vector_dot, METH_O, Vector_dot_doc},
	{"angle", (PyCFunction) Vector_angle, METH_VARARGS, Vector_angle_doc},
	{"rotation_difference", (PyCFunction) Vector_rotation_difference, METH_O, Vector_rotation_difference_doc},
	{"project", (PyCFunction) Vector_project, METH_O, Vector_project_doc},
	{"lerp", (PyCFunction) Vector_lerp, METH_VARARGS, Vector_lerp_doc},
	{"rotate", (PyCFunction) Vector_rotate, METH_O, Vector_rotate_doc},

	{"copy", (PyCFunction) Vector_copy, METH_NOARGS, Vector_copy_doc},
	{"__copy__", (PyCFunction) Vector_copy, METH_NOARGS, NULL},
	{NULL, NULL, 0, NULL}
};


/* Note
 Py_TPFLAGS_CHECKTYPES allows us to avoid casting all types to Vector when coercing
 but this means for eg that
 vec*mat and mat*vec both get sent to Vector_mul and it neesd to sort out the order
*/

PyDoc_STRVAR(vector_doc,
"This object gives access to Vectors in Blender."
);
PyTypeObject vector_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	/*  For printing, in format "<module>.<name>" */
	"mathutils.Vector",             /* char *tp_name; */
	sizeof(VectorObject),         /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	(destructor) BaseMathObject_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,   /* cmpfunc tp_compare; */
	(reprfunc)Vector_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	&Vector_NumMethods,                       /* PyNumberMethods *tp_as_number; */
	&Vector_SeqMethods,                       /* PySequenceMethods *tp_as_sequence; */
	&Vector_AsMapping,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC,
	vector_doc,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/

	/* call function for all accessible objects */
	(traverseproc)BaseMathObject_traverse,	//tp_traverse

	/* delete references to contained objects */
	(inquiry)BaseMathObject_clear,	//tp_clear

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	(richcmpfunc)Vector_richcmpr,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	Vector_methods,           /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	Vector_getseters,           /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	Vector_new,                 /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

/*------------------------newVectorObject (internal)-------------
  creates a new vector object
  pass Py_WRAP - if vector is a WRAPPER for data allocated by BLENDER
 (i.e. it was allocated elsewhere by MEM_mallocN())
  pass Py_NEW - if vector is not a WRAPPER and managed by PYTHON
 (i.e. it must be created here with PyMEM_malloc())*/
PyObject *newVectorObject(float *vec, const int size, const int type, PyTypeObject *base_type)
{
	VectorObject *self;

	if(size > 4 || size < 2) {
		PyErr_SetString(PyExc_RuntimeError,
		                "Vector(): invalid size");
		return NULL;
	}

	self= base_type ?	(VectorObject *)base_type->tp_alloc(base_type, 0) :
						(VectorObject *)PyObject_GC_New(VectorObject, &vector_Type);

	if(self) {
		self->size = size;

		/* init callbacks as NULL */
		self->cb_user= NULL;
		self->cb_type= self->cb_subtype= 0;

		if(type == Py_WRAP) {
			self->vec = vec;
			self->wrapped = Py_WRAP;
		}
		else if (type == Py_NEW) {
			self->vec= PyMem_Malloc(size * sizeof(float));
			if(vec) {
				memcpy(self->vec, vec, size * sizeof(float));
			}
			else { /* new empty */
				fill_vn(self->vec, size, 0.0f);
				if(size == 4) { /* do the homogenous thing */
					self->vec[3] = 1.0f;
				}
			}
			self->wrapped = Py_NEW;
		}
		else {
			Py_FatalError("Vector(): invalid type!");
		}
	}
	return (PyObject *) self;
}

PyObject *newVectorObject_cb(PyObject *cb_user, int size, int cb_type, int cb_subtype)
{
	float dummy[4] = {0.0, 0.0, 0.0, 0.0}; /* dummy init vector, callbacks will be used on access */
	VectorObject *self= (VectorObject *)newVectorObject(dummy, size, Py_NEW, NULL);
	if(self) {
		Py_INCREF(cb_user);
		self->cb_user=			cb_user;
		self->cb_type=			(unsigned char)cb_type;
		self->cb_subtype=		(unsigned char)cb_subtype;
		PyObject_GC_Track(self);
	}

	return (PyObject *)self;
}
