/* 
 * $Id$
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA    02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Alex Mole, Yehoshua Sapir
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/
#include "MTex.h" /*This must come first*/

#include "BKE_utildefines.h"
#include "BLI_blenlib.h"
#include "Texture.h"
#include "Object.h"
#include "gen_utils.h"
#include "gen_library.h"

#include <DNA_material_types.h>

/*****************************************************************************/
/* Python V24_BPy_MTex methods declarations:                                     */
/*****************************************************************************/
static PyObject *V24_MTex_setTexMethod( V24_BPy_MTex * self, PyObject * args );

/*****************************************************************************/
/* Python method structure definition for Blender.Texture.MTex module:       */
/*****************************************************************************/
struct PyMethodDef V24_M_MTex_methods[] = {
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python V24_BPy_MTex methods table:                                            */
/*****************************************************************************/
static PyMethodDef V24_BPy_MTex_methods[] = {
	/* name, method, flags, doc */
	{"setTex", ( PyCFunction ) V24_MTex_setTexMethod, METH_VARARGS,
	 "(i) - Set MTex Texture"},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python V24_MTex_Type callback function prototypes:                            */
/*****************************************************************************/
static int V24_MTex_compare( V24_BPy_MTex * a, V24_BPy_MTex * b );
static PyObject *V24_MTex_repr( V24_BPy_MTex * self );

#define MTEXGET(x) \
	static PyObject *V24_MTex_get##x( V24_BPy_MTex *self, void *closure );
#define MTEXSET(x) \
	static int V24_MTex_set##x( V24_BPy_MTex *self, PyObject *value, void *closure);
#define MTEXGETSET(x) \
	MTEXGET(x) \
	MTEXSET(x)

MTEXGETSET(Tex)
MTEXGETSET(TexCo)
MTEXGETSET(Object)
MTEXGETSET(UVLayer)
MTEXGETSET(MapTo)
MTEXGETSET(Col)
MTEXGETSET(DVar)
MTEXGETSET(BlendMode)
MTEXGETSET(ColFac)
MTEXGETSET(NorFac)
MTEXGETSET(VarFac)
MTEXGETSET(DispFac)
MTEXGETSET(WarpFac)
MTEXGETSET(Ofs)
MTEXGETSET(Size)
MTEXGETSET(Mapping)
MTEXGETSET(Flag)
MTEXGETSET(ProjX)
MTEXGETSET(ProjY)
MTEXGETSET(ProjZ)
MTEXGETSET(MapToFlag)

/*****************************************************************************/
/* Python get/set methods table                                              */
/*****************************************************************************/

static PyGetSetDef V24_MTex_getseters[] = {
	{ "tex", (getter) V24_MTex_getTex, (setter) V24_MTex_setTex,
		"Texture whose mapping this MTex describes", NULL },
	{ "texco", (getter) V24_MTex_getTexCo, (setter) V24_MTex_setTexCo,
		"Texture coordinate space (UV, Global, etc.)", NULL },
	{ "object", (getter) V24_MTex_getObject, (setter) V24_MTex_setObject,
		"Object whose space to use when texco is Object", NULL },
	{ "uvlayer", (getter) V24_MTex_getUVLayer, (setter) V24_MTex_setUVLayer,
		"Name of the UV layer to use", NULL },
	{ "mapto", (getter) V24_MTex_getMapTo, (setter) V24_MTex_setMapTo,
		"What values the texture affects", NULL },
	{ "col", (getter) V24_MTex_getCol, (setter) V24_MTex_setCol,
		"Color that the texture blends with", NULL },
	{ "dvar", (getter) V24_MTex_getDVar, (setter) V24_MTex_setDVar,
		"Value that the texture blends with when not blending colors", NULL },
	{ "blendmode", (getter) V24_MTex_getBlendMode, (setter) V24_MTex_setBlendMode,
		"Texture blending mode", NULL },
	{ "colfac", (getter) V24_MTex_getColFac, (setter) V24_MTex_setColFac,
		"Factor by which texture affects color", NULL },
	{ "norfac", (getter) V24_MTex_getNorFac, (setter) V24_MTex_setNorFac,
		"Factor by which texture affects normal", NULL },
	{ "varfac", (getter) V24_MTex_getVarFac, (setter) V24_MTex_setVarFac,
		"Factor by which texture affects most variables", NULL },
	{ "dispfac", (getter) V24_MTex_getDispFac, (setter) V24_MTex_setDispFac,
		"Factor by which texture affects displacement", NULL },
	{ "warpfac", (getter) V24_MTex_getWarpFac, (setter) V24_MTex_setWarpFac,
		"Factor by which texture affects warp", NULL },
	{ "ofs", (getter) V24_MTex_getOfs, (setter) V24_MTex_setOfs,
		"Offset to adjust texture space", NULL },
	{ "size", (getter) V24_MTex_getSize, (setter) V24_MTex_setSize,
		"Size to scale texture space", NULL },
	{ "mapping", (getter) V24_MTex_getMapping, (setter) V24_MTex_setMapping,
		"Mapping of texture coordinates (flat, cube, etc.)", NULL },
	{ "stencil", (getter) V24_MTex_getFlag, (setter) V24_MTex_setFlag,
		"Stencil mode", (void*) MTEX_STENCIL },
	{ "neg", (getter) V24_MTex_getFlag, (setter) V24_MTex_setFlag,
		"Negate texture values mode", (void*) MTEX_NEGATIVE },
	{ "noRGB", (getter) V24_MTex_getFlag, (setter) V24_MTex_setFlag,
		"Convert texture RGB values to intensity values",
		(void*) MTEX_RGBTOINT },
	{ "correctNor", (getter) V24_MTex_getFlag, (setter) V24_MTex_setFlag,
		"Correct normal mapping for Texture space and Object space",
		(void*) MTEX_VIEWSPACE },
	{ "xproj", (getter) V24_MTex_getProjX, (setter) V24_MTex_setProjX,
		"Projection of X axis to Texture space", NULL },
	{ "yproj", (getter) V24_MTex_getProjY, (setter) V24_MTex_setProjY,
		"Projection of Y axis to Texture space", NULL },
	{ "zproj", (getter) V24_MTex_getProjZ, (setter) V24_MTex_setProjZ,
		"Projection of Z axis to Texture space", NULL },
	{ "mtCol", (getter) V24_MTex_getMapToFlag, (setter) V24_MTex_setMapToFlag,
		"How texture maps to color", (void*) MAP_COL },
	{ "mtNor", (getter) V24_MTex_getMapToFlag, (setter) V24_MTex_setMapToFlag,
		"How texture maps to normals", (void*) MAP_NORM },
	{ "mtCsp", (getter) V24_MTex_getMapToFlag, (setter) V24_MTex_setMapToFlag,
		"How texture maps to specularity color", (void*) MAP_COLSPEC },
	{ "mtCmir", (getter) V24_MTex_getMapToFlag, (setter) V24_MTex_setMapToFlag,
		"How texture maps to mirror color", (void*) MAP_COLMIR },
	{ "mtRef", (getter) V24_MTex_getMapToFlag, (setter) V24_MTex_setMapToFlag,
		"How texture maps to reflectivity", (void*) MAP_REF },
	{ "mtSpec", (getter) V24_MTex_getMapToFlag, (setter) V24_MTex_setMapToFlag,
		"How texture maps to specularity", (void*) MAP_SPEC },
	{ "mtEmit", (getter) V24_MTex_getMapToFlag, (setter) V24_MTex_setMapToFlag,
		"How texture maps to emit value", (void*) MAP_EMIT },
	{ "mtAlpha", (getter) V24_MTex_getMapToFlag, (setter) V24_MTex_setMapToFlag,
		"How texture maps to alpha value", (void*) MAP_ALPHA },
	{ "mtHard", (getter) V24_MTex_getMapToFlag, (setter) V24_MTex_setMapToFlag,
		"How texture maps to hardness", (void*) MAP_HAR },
	{ "mtRayMir", (getter) V24_MTex_getMapToFlag, (setter) V24_MTex_setMapToFlag,
		"How texture maps to RayMir value", (void*) MAP_RAYMIRR },
	{ "mtTranslu", (getter) V24_MTex_getMapToFlag, (setter) V24_MTex_setMapToFlag,
		"How texture maps to translucency", (void*) MAP_TRANSLU },
	{ "mtAmb", (getter) V24_MTex_getMapToFlag, (setter) V24_MTex_setMapToFlag,
		"How texture maps to ambient value", (void*) MAP_AMB },
	{ "mtDisp", (getter) V24_MTex_getMapToFlag, (setter) V24_MTex_setMapToFlag,
		"How texture maps to displacement", (void*) MAP_DISPLACE },
	{ "mtWarp", (getter) V24_MTex_getMapToFlag, (setter) V24_MTex_setMapToFlag,
		"How texture maps to warp", (void*) MAP_WARP },
	{ NULL, NULL, NULL, NULL, NULL }
};



/*****************************************************************************/
/* Python V24_MTex_Type structure definition:                                    */
/*****************************************************************************/

PyTypeObject V24_MTex_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,	/* ob_size */
	"Blender MTex",		/* tp_name */
	sizeof( V24_BPy_MTex ),	/* tp_basicsize */
	0,			/* tp_itemsize */
	/* methods */
	NULL,		/* tp_dealloc */
	0,			/* tp_print */
	0,	/* tp_getattr */
	0,	/* tp_setattr */
	( cmpfunc ) V24_MTex_compare,	/* tp_compare */
	( reprfunc ) V24_MTex_repr,	/* tp_repr */
	0,			/* tp_as_number */
	0,			/* tp_as_sequence */
	0,			/* tp_as_mapping */
	0,			/* tp_as_hash */
	0, 0, 0, 0, 0,
  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,	/*    long tp_flags; */
	0,			/* tp_doc */
	0, 0, 0, 0, 0, 0,
	V24_BPy_MTex_methods,			/* tp_methods */
	0,			/* tp_members */
	V24_MTex_getseters,  /*    struct PyGetSetDef *tp_getset; */
	0,			/*    struct _typeobject *tp_base; */
	0,			/*    PyObject *tp_dict; */
	0,			/*    descrgetfunc tp_descr_get; */
	0,			/*    descrsetfunc tp_descr_set; */
	0,			/*    long tp_dictoffset; */
	0,			/*    initproc tp_init; */
	0,			/*    allocfunc tp_alloc; */
	0,			/*    newfunc tp_new; */
	/*  Low-level free-memory routine */
	0,			/*    freefunc tp_free;  */
	/* For PyObject_IS_GC */
	0,			/*    inquiry tp_is_gc;  */
	0,			/*    PyObject *tp_bases; */
	/* method resolution order */
	0,			/*    PyObject *tp_mro;  */
	0,			/*    PyObject *tp_cache; */
	0,			/*    PyObject *tp_subclasses; */
	0,			/*    PyObject *tp_weaklist; */
	0
};


PyObject *V24_MTex_Init( void )
{
	PyObject *V24_submodule;
/*	PyObject *dict; */

	/* call PyType_Ready() to init dictionaries & such */
	if( PyType_Ready( &V24_MTex_Type) < 0)
		Py_RETURN_NONE;

	V24_submodule = Py_InitModule( "Blender.Texture.MTex", V24_M_MTex_methods );

	return V24_submodule;
}

PyObject *V24_MTex_CreatePyObject( MTex * mtex )
{
	V24_BPy_MTex *pymtex;

	pymtex = ( V24_BPy_MTex * ) PyObject_NEW( V24_BPy_MTex, &V24_MTex_Type );
	if( !pymtex )
		return V24_EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create V24_BPy_MTex PyObject" );

	pymtex->mtex = mtex;
	return ( PyObject * ) pymtex;
}

MTex *V24_MTex_FromPyObject( PyObject * pyobj )
{
	return ( ( V24_BPy_MTex * ) pyobj )->mtex;
}

/*****************************************************************************/
/* Python V24_BPy_MTex methods:                                                  */
/*****************************************************************************/

static PyObject *V24_MTex_setTexMethod( V24_BPy_MTex * self, PyObject * args )
{
	return V24_EXPP_setterWrapper( (void *)self, args, (setter)V24_MTex_setTex );
}

static int V24_MTex_compare( V24_BPy_MTex * a, V24_BPy_MTex * b )
{
	return ( a->mtex == b->mtex ) ? 0 : -1;
}

static PyObject *V24_MTex_repr( V24_BPy_MTex * self )
{
	return PyString_FromFormat( "[MTex]" );
}


/*****************************************************************************/
/* Python V24_BPy_MTex get and set functions:                                    */
/*****************************************************************************/

static PyObject *V24_MTex_getTex( V24_BPy_MTex *self, void *closure )
{
	if( self->mtex->tex )
		return V24_Texture_CreatePyObject( self->mtex->tex );
	else
		Py_RETURN_NONE;
}

static int V24_MTex_setTex( V24_BPy_MTex *self, PyObject *value, void *closure)
{
	return V24_GenericLib_assignData(value, (void **) &self->mtex->tex, 0, 1, ID_TE, 0);
}

static PyObject *V24_MTex_getTexCo( V24_BPy_MTex *self, void *closure )
{
	return PyInt_FromLong( self->mtex->texco );
}

static int V24_MTex_setTexCo( V24_BPy_MTex *self, PyObject *value, void *closure)
{
	int texco;

	if( !PyInt_Check( value ) ) {
		return V24_EXPP_ReturnIntError( PyExc_TypeError,
			"Value must be a member of Texture.TexCo dictionary" );
	}

	texco = PyInt_AsLong( value ) ;

	if (texco != TEXCO_ORCO && texco != TEXCO_REFL && texco != TEXCO_NORM &&
		texco != TEXCO_GLOB && texco != TEXCO_UV && texco != TEXCO_OBJECT &&
		texco != TEXCO_STRESS && texco != TEXCO_TANGENT && texco != TEXCO_WINDOW &&
		texco != TEXCO_VIEW && texco != TEXCO_STICKY )
		return V24_EXPP_ReturnIntError( PyExc_ValueError,
			"Value must be a member of Texture.TexCo dictionary" );

	self->mtex->texco = (short)texco;

	return 0;
}

static PyObject *V24_MTex_getObject( V24_BPy_MTex *self, void *closure )
{
	if( self->mtex->object )
		return V24_Object_CreatePyObject( self->mtex->object );
	else
		Py_RETURN_NONE;
}

static int V24_MTex_setObject( V24_BPy_MTex *self, PyObject *value, void *closure)
{
	return V24_GenericLib_assignData(value, (void **) &self->mtex->object, 0, 1, ID_OB, 0);
}

static PyObject *V24_MTex_getUVLayer( V24_BPy_MTex *self, void *closure )
{
	return PyString_FromString(self->mtex->uvname);
}

static int V24_MTex_setUVLayer( V24_BPy_MTex *self, PyObject *value, void *closure)
{
	if ( !PyString_Check(value) )
		return V24_EXPP_ReturnIntError( PyExc_TypeError,
					    "expected string value" );
	BLI_strncpy(self->mtex->uvname, PyString_AsString(value), 31);
	return 0;
}

static PyObject *V24_MTex_getMapTo( V24_BPy_MTex *self, void *closure )
{
	return PyInt_FromLong( self->mtex->mapto );
}

static int V24_MTex_setMapTo( V24_BPy_MTex *self, PyObject *value, void *closure)
{
	int mapto;

	if( !PyInt_Check( value ) ) {
		return V24_EXPP_ReturnIntError( PyExc_TypeError,
			"expected an int" );
	}

	mapto = PyInt_AsLong( value );

	/* This method is deprecated anyway. */
	if ( mapto < 0 || mapto > 16383 ) {
		return V24_EXPP_ReturnIntError( PyExc_ValueError,
			"Value must be a sum of values from Texture.MapTo dictionary" );
	}

	self->mtex->mapto = (short)mapto;

	return 0;
}

static PyObject *V24_MTex_getCol( V24_BPy_MTex *self, void *closure )
{
	return Py_BuildValue( "(f,f,f)", self->mtex->r, self->mtex->g,
		self->mtex->b );
}

static int V24_MTex_setCol( V24_BPy_MTex *self, PyObject *value, void *closure)
{
	float rgb[3];
	int i;

	if( !PyArg_ParseTuple( value, "fff",
		&rgb[0], &rgb[1], &rgb[2] ) )

		return V24_EXPP_ReturnIntError( PyExc_TypeError,
					      "expected tuple of 3 floats" );

	for( i = 0; i < 3; ++i )
		if( rgb[i] < 0 || rgb[i] > 1 )
			return V24_EXPP_ReturnIntError( PyExc_ValueError,
					      "values must be in range [0,1]" );

	self->mtex->r = rgb[0];
	self->mtex->g = rgb[1];
	self->mtex->b = rgb[2];

	return 0;
}

static PyObject *V24_MTex_getDVar( V24_BPy_MTex *self, void *closure )
{
	return PyFloat_FromDouble(self->mtex->def_var);
}

static int V24_MTex_setDVar( V24_BPy_MTex *self, PyObject *value, void *closure)
{
	float f;

	if ( !PyFloat_Check( value ) )
		return V24_EXPP_ReturnIntError( PyExc_TypeError,
						"expected a float" );

	f = (float)PyFloat_AsDouble(value);

	if (f < 0 || f > 1)
		return V24_EXPP_ReturnIntError( PyExc_ValueError,
					      "values must be in range [0,1]" );

	self->mtex->def_var = f;

	return 0;
}

static PyObject *V24_MTex_getBlendMode( V24_BPy_MTex *self, void *closure )
{
	return PyInt_FromLong(self->mtex->blendtype);
}

static int V24_MTex_setBlendMode( V24_BPy_MTex *self, PyObject *value, void *closure)
{
	int n;

	if ( !PyInt_Check( value ) )
		return V24_EXPP_ReturnIntError( PyExc_TypeError,
					    "Value must be member of Texture.BlendModes dictionary" );

	n = PyInt_AsLong(value);

/*	if (n != MTEX_BLEND && n != MTEX_MUL && n != MTEX_ADD &&
		n != MTEX_SUB && n != MTEX_DIV && n != MTEX_DARK &&
		n != MTEX_DIFF && n != MTEX_LIGHT && n != MTEX_SCREEN)*/
	if (n < 0 || n > 8)
	{
		return V24_EXPP_ReturnIntError( PyExc_ValueError,
					    "Value must be member of Texture.BlendModes dictionary" );
	}

	self->mtex->blendtype = (short)n;

	return 0;
}

static PyObject *V24_MTex_getColFac( V24_BPy_MTex *self, void *closure )
{
	return PyFloat_FromDouble(self->mtex->colfac);
}

static int V24_MTex_setColFac( V24_BPy_MTex *self, PyObject *value, void *closure)
{
	float f;

	if ( !PyFloat_Check( value ) )
		return V24_EXPP_ReturnIntError( PyExc_TypeError,
						"expected a float" );

	f = (float)PyFloat_AsDouble(value);

	if (f < 0 || f > 1)
		return V24_EXPP_ReturnIntError( PyExc_ValueError,
					      "values must be in range [0,1]" );

	self->mtex->colfac = f;

	return 0;
}

static PyObject *V24_MTex_getNorFac( V24_BPy_MTex *self, void *closure )
{
	return PyFloat_FromDouble(self->mtex->norfac);
}

static int V24_MTex_setNorFac( V24_BPy_MTex *self, PyObject *value, void *closure)
{
	float f;

	if ( !PyFloat_Check( value ) )
		return V24_EXPP_ReturnIntError( PyExc_TypeError,
						"expected a float" );

	f = (float)PyFloat_AsDouble(value);

	if (f < 0 || f > 25)
		return V24_EXPP_ReturnIntError( PyExc_ValueError,
					      "values must be in range [0,25]" );

	self->mtex->norfac = f;

	return 0;
}

static PyObject *V24_MTex_getVarFac( V24_BPy_MTex *self, void *closure )
{
	return PyFloat_FromDouble(self->mtex->varfac);
}

static int V24_MTex_setVarFac( V24_BPy_MTex *self, PyObject *value, void *closure)
{
	float f;

	if ( !PyFloat_Check( value ) )
		return V24_EXPP_ReturnIntError( PyExc_TypeError,
						"expected a float" );

	f = (float)PyFloat_AsDouble(value);

	if (f < 0 || f > 1)
		return V24_EXPP_ReturnIntError( PyExc_ValueError,
					      "values must be in range [0,1]" );

	self->mtex->varfac = f;

	return 0;
}

static PyObject *V24_MTex_getDispFac( V24_BPy_MTex *self, void *closure )
{
	return PyFloat_FromDouble(self->mtex->dispfac);
}

static int V24_MTex_setDispFac( V24_BPy_MTex *self, PyObject *value, void *closure)
{
	float f;

	if ( !PyFloat_Check( value ) )
		return V24_EXPP_ReturnIntError( PyExc_TypeError,
						"expected a float" );

	f = (float)PyFloat_AsDouble(value);

	if (f < 0 || f > 1)
		return V24_EXPP_ReturnIntError( PyExc_ValueError,
					      "values must be in range [0,1]" );

	self->mtex->dispfac = f;

	return 0;
}

static PyObject *V24_MTex_getWarpFac( V24_BPy_MTex *self, void *closure )
{
	return PyFloat_FromDouble(self->mtex->warpfac);
}

static int V24_MTex_setWarpFac( V24_BPy_MTex *self, PyObject *value, void *closure)
{
	float f;

	if ( !PyFloat_Check( value ) )
		return V24_EXPP_ReturnIntError( PyExc_TypeError,
						"expected a float" );

	f = (float)PyFloat_AsDouble(value);

	if (f < 0 || f > 1)
		return V24_EXPP_ReturnIntError( PyExc_ValueError,
					      "values must be in range [0,1]" );

	self->mtex->warpfac = f;

	return 0;
}

static PyObject *V24_MTex_getOfs( V24_BPy_MTex *self, void *closure )
{
	return Py_BuildValue( "(f,f,f)", self->mtex->ofs[0], self->mtex->ofs[1],
		self->mtex->ofs[2] );
}

static int V24_MTex_setOfs( V24_BPy_MTex *self, PyObject *value, void *closure)
{
	float f[3];
	int i;

	if( !PyArg_ParseTuple( value, "fff", &f[0], &f[1], &f[2] ) )

		return V24_EXPP_ReturnIntError( PyExc_TypeError,
					      "expected tuple of 3 floats" );

	for( i = 0; i < 3; ++i )
		if( f[i] < -10 || f[i] > 10 )
			return V24_EXPP_ReturnIntError( PyExc_ValueError,
					      "values must be in range [-10,10]" );

	self->mtex->ofs[0] = f[0];
	self->mtex->ofs[1] = f[1];
	self->mtex->ofs[2] = f[2];

	return 0;
}

static PyObject *V24_MTex_getSize( V24_BPy_MTex *self, void *closure )
{
	return Py_BuildValue( "(f,f,f)", self->mtex->size[0], self->mtex->size[1],
		self->mtex->size[2] );
}

static int V24_MTex_setSize( V24_BPy_MTex *self, PyObject *value, void *closure)
{
	float f[3];
	int i;

	if( !PyArg_ParseTuple( value, "fff", &f[0], &f[1], &f[2] ) )

		return V24_EXPP_ReturnIntError( PyExc_TypeError,
					      "expected tuple of 3 floats" );

	for( i = 0; i < 3; ++i )
		if( f[i] < -100 || f[i] > 100 )
			return V24_EXPP_ReturnIntError( PyExc_ValueError,
					      "values must be in range [-100,100]" );

	self->mtex->size[0] = f[0];
	self->mtex->size[1] = f[1];
	self->mtex->size[2] = f[2];

	return 0;
}

static PyObject *V24_MTex_getMapping( V24_BPy_MTex *self, void *closure )
{
	return PyInt_FromLong( self->mtex->mapping );
}

static int V24_MTex_setMapping( V24_BPy_MTex *self, PyObject *value, void *closure)
{
	int n;

	if ( !PyInt_Check( value ) )
		return V24_EXPP_ReturnIntError( PyExc_TypeError,
				"Value must be member of Texture.Mappings dictionary" );

	n = PyInt_AsLong(value);

/*	if (n != MTEX_FLAT && n != MTEX_TUBE && n != MTEX_CUBE &&
		n != MTEX_SPHERE) */
	if (n < 0 || n > 3)
	{
		return V24_EXPP_ReturnIntError( PyExc_ValueError,
			    "Value must be member of Texture.Mappings dictionary" );
	}

	self->mtex->mapping = (char)n;

	return 0;
}

static PyObject *V24_MTex_getFlag( V24_BPy_MTex *self, void *closure )
{
	return PyBool_FromLong( self->mtex->texflag & ((int) closure) );
}

static int V24_MTex_setFlag( V24_BPy_MTex *self, PyObject *value, void *closure)
{
	if ( !PyBool_Check( value ) )
		return V24_EXPP_ReturnIntError( PyExc_TypeError,
				"expected a bool");

	if ( value == Py_True )
		self->mtex->texflag |= (int)closure;
	else
		self->mtex->texflag &= ~((int) closure);

	return 0;
}

static PyObject *V24_MTex_getProjX( V24_BPy_MTex *self, void *closure )
{
	return PyInt_FromLong( self->mtex->projx );
}

static int V24_MTex_setProjX( V24_BPy_MTex *self, PyObject *value, void *closure)
{
	int proj;

	if( !PyInt_Check( value ) ) {
		return V24_EXPP_ReturnIntError( PyExc_TypeError,
			"Value must be a member of Texture.Proj dictionary" );
	}

	proj = PyInt_AsLong( value ) ;

	/* valid values are from PROJ_N to PROJ_Z = 0 to 3 */
	if (proj < 0 || proj > 3)
		return V24_EXPP_ReturnIntError( PyExc_ValueError,
			"Value must be a member of Texture.Proj dictionary" );

	self->mtex->projx = (char)proj;

	return 0;
}

static PyObject *V24_MTex_getProjY( V24_BPy_MTex *self, void *closure )
{
	return PyInt_FromLong( self->mtex->projy );
}

static int V24_MTex_setProjY( V24_BPy_MTex *self, PyObject *value, void *closure )
{
	int proj;

	if( !PyInt_Check( value ) ) {
		return V24_EXPP_ReturnIntError( PyExc_TypeError,
			"Value must be a member of Texture.Proj dictionary" );
	}

	proj = PyInt_AsLong( value ) ;

	/* valid values are from PROJ_N to PROJ_Z = 0 to 3 */
	if (proj < 0 || proj > 3)
		return V24_EXPP_ReturnIntError( PyExc_ValueError,
			"Value must be a member of Texture.Proj dictionary" );

	self->mtex->projy = (char)proj;

	return 0;
}

static PyObject *V24_MTex_getProjZ( V24_BPy_MTex *self, void *closure )
{
	return PyInt_FromLong( self->mtex->projz );
}

static int V24_MTex_setProjZ( V24_BPy_MTex *self, PyObject *value, void *closure)
{
	int proj;

	if( !PyInt_Check( value ) ) {
		return V24_EXPP_ReturnIntError( PyExc_TypeError,
			"Value must be a member of Texture.Proj dictionary" );
	}

	proj = PyInt_AsLong( value ) ;

	/* valid values are from PROJ_N to PROJ_Z = 0 to 3 */
	if (proj < 0 || proj > 3)
		return V24_EXPP_ReturnIntError( PyExc_ValueError,
			"Value must be a member of Texture.Proj dictionary" );

	self->mtex->projz = (char)proj;

	return 0;
}

static PyObject *V24_MTex_getMapToFlag( V24_BPy_MTex *self, void *closure )
{
	int flag = (int) closure;

	if ( self->mtex->mapto & flag )
	{
		return PyInt_FromLong( ( self->mtex->maptoneg & flag ) ? -1 : 1 );
	} else {
		return PyInt_FromLong( 0 );
	}
}

static int V24_MTex_setMapToFlag( V24_BPy_MTex *self, PyObject *value, void *closure)
{
	int flag = (int) closure;
	int intVal;

	if ( !PyInt_Check( value ) )
		return V24_EXPP_ReturnIntError( PyExc_TypeError,
				"expected an int");

	intVal = PyInt_AsLong( value );

	if (flag == MAP_COL || flag == MAP_COLSPEC || flag == MAP_COLMIR ||
		flag == MAP_WARP) {
		if (intVal < 0 || intVal > 1) {
			return V24_EXPP_ReturnIntError( PyExc_ValueError,
				"value for that mapping must be 0 or 1" );
		}
	} else {
		if (intVal < -1 || intVal > 1) {
			return V24_EXPP_ReturnIntError( PyExc_ValueError,
				"value for that mapping must be -1, 0 or 1" );
		}
	}

	switch (intVal)
	{
	case 0:
		self->mtex->mapto &= ~flag;
		self->mtex->maptoneg &= ~flag;
		break;

	case 1:
		self->mtex->mapto |= flag;
		self->mtex->maptoneg &= ~flag;
		break;

	case -1:
		self->mtex->mapto |= flag;
		self->mtex->maptoneg |= flag;
		break;
	}

	return 0;
}
