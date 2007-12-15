/* 
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Willian P. Germano, Jacques Guignot, Joseph Gilbert,
 * Campbell Barton, Ken Hughes
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/
struct View3D;

#include "Scene.h" /*This must come first */

#include "BKE_global.h"
#include "BKE_main.h"
#include "MEM_guardedalloc.h"	/* for MEM_callocN */
#include "DNA_space_types.h"	/* SPACE_VIEW3D, SPACE_SEQ */
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h" /* U.userdefs */
#include "DNA_object_types.h" /* V24_SceneObSeq_new */
#include "BKE_depsgraph.h"
#include "BKE_library.h"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BKE_font.h"
#include "BKE_idprop.h"
#include "BLI_blenlib.h" /* only for V24_SceneObSeq_new */
#include "BSE_drawview.h"	/* for play_anim */
#include "BSE_headerbuttons.h"	/* for copy_scene */
#include "BIF_drawscene.h"	/* for set_scene */
#include "BIF_space.h"		/* for copy_view3d_lock() */
#include "BIF_screen.h"		/* curarea */
#include "BDR_editobject.h"		/* free_and_unlink_base() */
#include "mydevice.h"		/* for #define REDRAW */
#include "DNA_view3d_types.h"
/* python types */
#include "Object.h"
#include "Camera.h"
/* only for V24_SceneObSeq_new */
#include "BKE_material.h"
#include "BLI_arithb.h"
#include "Armature.h"
#include "Lamp.h"
#include "Curve.h"
#include "NMesh.h"
#include "Mesh.h"
#include "World.h"
#include "Lattice.h"
#include "Metaball.h"
#include "IDProp.h"
#include "Text3d.h"
#include "Library.h"

#include "gen_utils.h"
#include "gen_library.h"
#include "sceneRender.h"
#include "sceneRadio.h"
#include "sceneTimeLine.h"
#include "sceneSequence.h"


#include "BKE_utildefines.h" /* vec copy */
#include "vector.h"

PyObject *V24_M_Object_Get( PyObject * self, PyObject * args ); /* from Object.c */

/* checks for the scene being removed */
#define SCENE_DEL_CHECK_PY(bpy_scene) if (!(bpy_scene->scene)) return ( V24_EXPP_ReturnPyObjError( PyExc_RuntimeError, "Scene has been removed" ) )
#define SCENE_DEL_CHECK_INT(bpy_scene) if (!(bpy_scene->scene)) return ( V24_EXPP_ReturnIntError( PyExc_RuntimeError, "Scene has been removed" ) )


enum obj_consts {
	EXPP_OBSEQ_NORMAL = 0,
	EXPP_OBSEQ_SELECTED,
	EXPP_OBSEQ_CONTEXT
};


/*-----------------------Python API function prototypes for the Scene module--*/
static PyObject *V24_M_Scene_New( PyObject * self, PyObject * args,
			      PyObject * keywords );
static PyObject *V24_M_Scene_Get( PyObject * self, PyObject * args );
static PyObject *V24_M_Scene_GetCurrent( PyObject * self );
static PyObject *V24_M_Scene_getCurrent_deprecated( PyObject * self );
static PyObject *V24_M_Scene_Unlink( PyObject * self, PyObject * arg );
/*-----------------------Scene module doc strings-----------------------------*/
static char V24_M_Scene_doc[] = "The Blender.Scene V24_submodule";
static char V24_M_Scene_New_doc[] =
	"(name = 'Scene') - Create a new Scene called 'name' in Blender.";
static char V24_M_Scene_Get_doc[] =
	"(name = None) - Return the scene called 'name'. If 'name' is None, return a list with all Scenes.";
static char V24_M_Scene_GetCurrent_doc[] =
	"() - Return the currently active Scene in Blender.";
static char V24_M_Scene_Unlink_doc[] =
	"(scene) - Unlink (delete) scene 'Scene' from Blender. (scene) is of type Blender scene.";
/*----------------------Scene module method def----------------------------*/
struct PyMethodDef V24_M_Scene_methods[] = {
	{"New", ( PyCFunction ) V24_M_Scene_New, METH_VARARGS | METH_KEYWORDS,
	 V24_M_Scene_New_doc},
	{"Get", V24_M_Scene_Get, METH_VARARGS, V24_M_Scene_Get_doc},
	{"get", V24_M_Scene_Get, METH_VARARGS, V24_M_Scene_Get_doc},
	{"GetCurrent", ( PyCFunction ) V24_M_Scene_GetCurrent,
	 METH_NOARGS, V24_M_Scene_GetCurrent_doc},
	{"getCurrent", ( PyCFunction ) V24_M_Scene_getCurrent_deprecated,
	 METH_NOARGS, V24_M_Scene_GetCurrent_doc},
	{"Unlink", V24_M_Scene_Unlink, METH_VARARGS, V24_M_Scene_Unlink_doc},
	{"unlink", V24_M_Scene_Unlink, METH_VARARGS, V24_M_Scene_Unlink_doc},
	{NULL, NULL, 0, NULL}
};
/*-----------------------V24_BPy_Scene  method declarations--------------------*/
static PyObject *V24_Scene_getLayerList( V24_BPy_Scene * self );
static PyObject *V24_Scene_oldsetLayers( V24_BPy_Scene * self, PyObject * arg );
static PyObject *V24_Scene_copy( V24_BPy_Scene * self, PyObject * arg );
static PyObject *V24_Scene_makeCurrent( V24_BPy_Scene * self );
static PyObject *V24_Scene_update( V24_BPy_Scene * self, PyObject * args );
static PyObject *V24_Scene_link( V24_BPy_Scene * self, PyObject * args );
static PyObject *V24_Scene_unlink( V24_BPy_Scene * self, PyObject * args );
static PyObject *V24_Scene_getChildren( V24_BPy_Scene * self );
static PyObject *V24_Scene_getActiveObject(V24_BPy_Scene *self);
static PyObject *V24_Scene_getCurrentCamera( V24_BPy_Scene * self );
static PyObject *V24_Scene_setCurrentCamera( V24_BPy_Scene * self, PyObject * args );
static PyObject *V24_Scene_getRenderingContext( V24_BPy_Scene * self );
static PyObject *V24_Scene_getRadiosityContext( V24_BPy_Scene * self );
static PyObject *V24_Scene_getScriptLinks( V24_BPy_Scene * self, PyObject * value );
static PyObject *V24_Scene_getSequence( V24_BPy_Scene * self );
static PyObject *V24_Scene_addScriptLink( V24_BPy_Scene * self, PyObject * args );
static PyObject *V24_Scene_clearScriptLinks( V24_BPy_Scene * self, PyObject * args );
static PyObject *V24_Scene_play( V24_BPy_Scene * self, PyObject * args );
static PyObject *V24_Scene_getTimeLine( V24_BPy_Scene * self );


/*internal*/
static int V24_Scene_compare( V24_BPy_Scene * a, V24_BPy_Scene * b );
static PyObject *V24_Scene_repr( V24_BPy_Scene * self );

/*object seq*/
static PyObject *V24_SceneObSeq_CreatePyObject( V24_BPy_Scene *self, Base *iter, int mode);

/*-----------------------V24_BPy_Scene method def------------------------------*/
static PyMethodDef V24_BPy_Scene_methods[] = {
	/* name, method, flags, doc */
	{"getName", ( PyCFunction ) V24_GenericLib_getName, METH_NOARGS,
	 "() - Return Scene name"},
	{"setName", ( PyCFunction ) V24_GenericLib_setName_with_method, METH_VARARGS,
	 "(str) - Change Scene name"},
	{"getLayers", ( PyCFunction ) V24_Scene_getLayerList, METH_NOARGS,
	 "() - Return a list of layers int indices which are set in this scene "},
	{"setLayers", ( PyCFunction ) V24_Scene_oldsetLayers, METH_VARARGS,
	 "(layers) - Change layers which are set in this scene\n"
	 "(layers) - list of integers in the range [1, 20]."},
	{"copy", ( PyCFunction ) V24_Scene_copy, METH_VARARGS,
	 "(duplicate_objects = 1) - Return a copy of this scene\n"
	 "The optional argument duplicate_objects defines how the scene\n"
	 "children are duplicated:\n\t0: Link Objects\n\t1: Link Object Data"
	 "\n\t2: Full copy\n"},
	{"makeCurrent", ( PyCFunction ) V24_Scene_makeCurrent, METH_NOARGS,
	 "() - Make self the current scene"},
	{"update", ( PyCFunction ) V24_Scene_update, METH_VARARGS,
	 "(full = 0) - Update scene self.\n"
	 "full = 0: sort the base list of objects."
	 "full = 1: full update -- also regroups, does ipos, keys"},
	{"link", ( PyCFunction ) V24_Scene_link, METH_VARARGS,
	 "(obj) - Link Object obj to this scene"},
	{"unlink", ( PyCFunction ) V24_Scene_unlink, METH_VARARGS,
	 "(obj) - Unlink Object obj from this scene"},
	{"getChildren", ( PyCFunction ) V24_Scene_getChildren, METH_NOARGS,
	 "() - Return list of all objects linked to this scene"},
	{"getActiveObject", (PyCFunction)V24_Scene_getActiveObject, METH_NOARGS,
	 "() - Return this scene's active object"},
	{"getCurrentCamera", ( PyCFunction ) V24_Scene_getCurrentCamera,
	 METH_NOARGS,
	 "() - Return current active Camera"},
	{"getScriptLinks", ( PyCFunction ) V24_Scene_getScriptLinks, METH_O,
	 "(eventname) - Get a list of this scene's scriptlinks (Text names) "
	 "of the given type\n"
	 "(eventname) - string: FrameChanged, OnLoad, OnSave, Redraw or Render."},
	{"addScriptLink", ( PyCFunction ) V24_Scene_addScriptLink, METH_VARARGS,
	 "(text, evt) - Add a new scene scriptlink.\n"
	 "(text) - string: an existing Blender Text name;\n"
	 "(evt) string: FrameChanged, OnLoad, OnSave, Redraw or Render."},
	{"clearScriptLinks", ( PyCFunction ) V24_Scene_clearScriptLinks,
	 METH_VARARGS,
	 "() - Delete all scriptlinks from this scene.\n"
	 "([s1<,s2,s3...>]) - Delete specified scriptlinks from this scene."},
	{"setCurrentCamera", ( PyCFunction ) V24_Scene_setCurrentCamera,
	 METH_VARARGS,
	 "() - Set the currently active Camera"},
	{"getRenderingContext", ( PyCFunction ) V24_Scene_getRenderingContext,
	 METH_NOARGS,
	 "() - Get the rendering context for the scene and return it as a V24_BPy_RenderData"},
	{"getRadiosityContext", ( PyCFunction ) V24_Scene_getRadiosityContext,
	 METH_NOARGS,
	 "() - Get the radiosity context for this scene."},
	{"play", ( PyCFunction ) V24_Scene_play, METH_VARARGS,
	 "(mode = 0, win = VIEW3D) - Play realtime animation in Blender"
	 " (not rendered).\n"
	 "(mode) - int:\n"
	 "\t0 - keep playing in biggest given 'win';\n"
	 "\t1 - keep playing in all 'win', VIEW3D and SEQ windows;\n"
	 "\t2 - play once in biggest given 'win';\n"
	 "\t3 - play once in all 'win', VIEW3D and SEQ windows.\n"
	 "(win) - int: see Blender.Window.Types. Only these are meaningful here:"
	 "VIEW3D, SEQ,	IPO, ACTION, NLA, SOUND.  But others are also accepted, "
	 "since they can be used just as an interruptible timer.  If 'win' is not"
	 "available or invalid, VIEW3D is tried, then any bigger window."
	 "Returns 0 for normal exit or 1 when canceled by user input."},
	{"getTimeLine", ( PyCFunction ) V24_Scene_getTimeLine, METH_NOARGS,
	"() - Get time line of this Scene"},
	{NULL, NULL, 0, NULL}
};


/*****************************************************************************/
/* Python V24_BPy_Scene getsetattr funcs:                                        */
/*****************************************************************************/
static PyObject *V24_Scene_getLayerMask( V24_BPy_Scene * self )
{
	SCENE_DEL_CHECK_PY(self);
	return PyInt_FromLong( self->scene->lay & ((1<<20)-1) );
}

static int V24_Scene_setLayerMask( V24_BPy_Scene * self, PyObject * value )
{
	int laymask = 0;
	
	SCENE_DEL_CHECK_INT(self);
	
	if (!PyInt_Check(value)) {
		return V24_EXPP_ReturnIntError( PyExc_AttributeError,
			"expected an integer (bitmask) as argument" );
	}
	
	laymask = PyInt_AsLong(value);

	if (laymask <= 0 || laymask > (1<<20) - 1) /* binary: 1111 1111 1111 1111 1111 */
		return V24_EXPP_ReturnIntError( PyExc_AttributeError,
			"bitmask must have from 1 up to 20 bits set");

	self->scene->lay = laymask;
	/* if this is the current scene then apply the scene layers value
	 * to the view layers value: */
	if (G.vd && (self->scene == G.scene)) {
		int val, bit = 0;
		G.vd->lay = laymask;

		while( bit < 20 ) {
			val = 1 << bit;
			if( laymask & val ) {
				G.vd->layact = val;
				break;
			}
			bit++;
		}
	}

	return 0;
}

static PyObject *V24_Scene_getLayerList( V24_BPy_Scene * self )
{
	PyObject *laylist, *item;
	int layers, bit = 0, val = 0;
	
	SCENE_DEL_CHECK_PY(self);
	
	laylist = PyList_New( 0 );
	
	if( !laylist )
		return ( V24_EXPP_ReturnPyObjError( PyExc_MemoryError,
			"couldn't create pylist!" ) );

	layers = self->scene->lay;

	while( bit < 20 ) {
		val = 1 << bit;
		if( layers & val ) {
			item = Py_BuildValue( "i", bit + 1 );
			PyList_Append( laylist, item );
			Py_DECREF( item );
		}
		bit++;
	}
	return laylist;
}

static int V24_Scene_setLayerList( V24_BPy_Scene * self, PyObject * value )
{
	PyObject *item = NULL;
	int layers = 0, val, i, len_list;
	
	SCENE_DEL_CHECK_INT(self);
	
	if( !PySequence_Check( value ) )
		return ( V24_EXPP_ReturnIntError( PyExc_TypeError,
			"expected a list of integers in the range [1, 20]" ) );

	len_list = PySequence_Size(value);

	if (len_list == 0)
		return ( V24_EXPP_ReturnIntError( PyExc_AttributeError,
			"list can't be empty, at least one layer must be set" ) );

	for( i = 0; i < len_list; i++ ) {
		item = PySequence_GetItem( value, i );
		
		if( !PyInt_Check( item ) ) {
			Py_DECREF( item );
			return V24_EXPP_ReturnIntError
				( PyExc_AttributeError,
				  "list must contain only integer numbers" );
		}
		
		val = ( int ) PyInt_AsLong( item );
		if( val < 1 || val > 20 )
			return V24_EXPP_ReturnIntError
				( PyExc_AttributeError,
				  "layer values must be in the range [1, 20]" );

		layers |= 1 << ( val - 1 );
	}
	self->scene->lay = layers;

	if (G.vd && (self->scene == G.scene)) {
		int bit = 0;
		G.vd->lay = layers;

		while( bit < 20 ) {
			val = 1 << bit;
			if( layers & val ) {
				G.vd->layact = val;
				break;
			}
			bit++;
		}
	}

	return 0;
}

static PyObject *V24_Scene_getWorld( V24_BPy_Scene * self )
{
	SCENE_DEL_CHECK_PY(self);
	
	if (!self->scene->world)
		Py_RETURN_NONE;
	return V24_World_CreatePyObject(self->scene->world);
}

static int V24_Scene_setWorld( V24_BPy_Scene * self, PyObject * value )
{
	SCENE_DEL_CHECK_INT(self);
	return V24_GenericLib_assignData(value, (void **) &self->scene->world, NULL, 1, ID_WO, 0);
}

/* accessed from scn.objects */
static PyObject *V24_Scene_getObjects( V24_BPy_Scene *self) 
{
	SCENE_DEL_CHECK_PY(self);
	return V24_SceneObSeq_CreatePyObject(self, NULL, 0);
}

static PyObject *V24_Scene_getCursor( V24_BPy_Scene * self )
{
	SCENE_DEL_CHECK_PY(self);
	return V24_newVectorObject( self->scene->cursor, 3, Py_WRAP );
}

static int V24_Scene_setCursor( V24_BPy_Scene * self, PyObject * value )
{	
	V24_VectorObject *bpy_vec;
	SCENE_DEL_CHECK_INT(self);
	if (!VectorObject_Check(value))
		return ( V24_EXPP_ReturnIntError( PyExc_TypeError,
			"expected a vector" ) );
	
	bpy_vec = (V24_VectorObject *)value;
	
	if (bpy_vec->size != 3)
		return ( V24_EXPP_ReturnIntError( PyExc_ValueError,
			"can only assign a 3D vector" ) );
	
	VECCOPY(self->scene->cursor, bpy_vec->vec);
	return 0;
}

/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef V24_BPy_Scene_getseters[] = {
	GENERIC_LIB_GETSETATTR,
	{"Layers",
	 (getter)V24_Scene_getLayerMask, (setter)V24_Scene_setLayerMask,
	 "Scene layer bitmask",
	 NULL},
	{"layers",
	 (getter)V24_Scene_getLayerList, (setter)V24_Scene_setLayerList,
	 "Scene layer list",
	 NULL},
	{"world",
	 (getter)V24_Scene_getWorld, (setter)V24_Scene_setWorld,
	 "Scene layer bitmask",
	 NULL},
	{"cursor",
	 (getter)V24_Scene_getCursor, (setter)V24_Scene_setCursor,
	 "Scene layer bitmask",
	 NULL},
	{"timeline",
	 (getter)V24_Scene_getTimeLine, (setter)NULL,
	 "Scenes timeline (read only)",
	 NULL},
	{"render",
	 (getter)V24_Scene_getRenderingContext, (setter)NULL,
	 "Scenes rendering context (read only)",
	 NULL},
	{"radiosity",
	 (getter)V24_Scene_getRadiosityContext, (setter)NULL,
	 "Scenes radiosity context (read only)",
	 NULL},
	{"sequence",
	 (getter)V24_Scene_getSequence, (setter)NULL,
	 "Scene sequencer data (read only)",
	 NULL},
	 
	{"objects",
	 (getter)V24_Scene_getObjects, (setter)NULL,
	 "Scene object iterator",
	 NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};



/*-----------------------V24_BPy_Scene method def------------------------------*/
PyTypeObject V24_Scene_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,	/* ob_size */
	"Scene",		/* tp_name */
	sizeof( V24_BPy_Scene ),	/* tp_basicsize */
	0,			/* tp_itemsize */
	/* methods */
	NULL,						/* tp_dealloc */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) V24_Scene_compare,	/* tp_compare */
	( reprfunc ) V24_Scene_repr,	/* tp_repr */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	( hashfunc ) V24_GenericLib_hash,	/* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	V24_BPy_Scene_methods,           /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	V24_BPy_Scene_getseters,         /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
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

/*-----------------------Scene module Init())-----------------------------*/
PyObject *V24_Scene_Init( void )
{

	PyObject *V24_submodule;
	PyObject *dict;
	
	if( PyType_Ready( &V24_Scene_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &V24_SceneObSeq_Type ) < 0 )
		return NULL;
	
	V24_submodule = Py_InitModule3( "Blender.Scene", V24_M_Scene_methods, V24_M_Scene_doc );

	dict = PyModule_GetDict( V24_submodule );
	PyDict_SetItemString( dict, "Render", V24_Render_Init(  ) );
	PyDict_SetItemString( dict, "Radio", V24_Radio_Init(  ) );
	PyDict_SetItemString( dict, "Sequence", V24_Sequence_Init(  ) );
	
	return V24_submodule;
}

/*-----------------------compare----------------------------------------*/
static int V24_Scene_compare( V24_BPy_Scene * a, V24_BPy_Scene * b )
{
	return ( a->scene == b->scene ) ? 0 : -1;
}

/*----------------------repr--------------------------------------------*/
static PyObject *V24_Scene_repr( V24_BPy_Scene * self )
{
	if( !(self->scene) )
		return PyString_FromString( "[Scene - Removed]");
	else
		return PyString_FromFormat( "[Scene \"%s\"]",
					self->scene->id.name + 2 );
}

/*-----------------------CreatePyObject---------------------------------*/
PyObject *V24_Scene_CreatePyObject( Scene * scene )
{
	V24_BPy_Scene *pyscene;

	pyscene = ( V24_BPy_Scene * ) PyObject_NEW( V24_BPy_Scene, &V24_Scene_Type );

	if( !pyscene )
		return V24_EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create V24_BPy_Scene object" );

	pyscene->scene = scene;

	return ( PyObject * ) pyscene;
}

/*-----------------------FromPyObject-----------------------------------*/
Scene *V24_Scene_FromPyObject( PyObject * pyobj )
{
	return ( ( V24_BPy_Scene * ) pyobj )->scene;
}

/*-----------------------Scene module function defintions---------------*/
/*-----------------------Scene.New()------------------------------------*/
static PyObject *V24_M_Scene_New( PyObject * self, PyObject * args,
			      PyObject * kword )
{
	char *name = "Scene";
	char *kw[] = { "name", NULL };
	PyObject *pyscene;	/* for the Scene object wrapper in Python */
	Scene *blscene;		/* for the actual Scene we create in Blender */

	if( !PyArg_ParseTupleAndKeywords( args, kword, "|s", kw, &name ) )
		return ( V24_EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected a string or an empty argument list" ) );

	blscene = add_scene( name );	/* first create the Scene in Blender */

	if( blscene ) {
		/* normally, for most objects, we set the user count to zero here.
		 * Scene is different than most objs since it is the container
		 * for all the others. Since add_scene() has already set 
		 * the user count to one, we leave it alone.
		 */

		/* now create the wrapper obj in Python */
		pyscene = V24_Scene_CreatePyObject( blscene );
	} else
		return ( V24_EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"couldn't create Scene obj in Blender" ) );

	if( pyscene == NULL )
		return ( V24_EXPP_ReturnPyObjError( PyExc_MemoryError,
						"couldn't create Scene PyObject" ) );

	return pyscene;
}

/*-----------------------Scene.Get()------------------------------------*/
static PyObject *V24_M_Scene_Get( PyObject * self, PyObject * args )
{
	char *name = NULL;
	Scene *scene_iter;

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return ( V24_EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected string argument (or nothing)" ) );

	scene_iter = G.main->scene.first;

	if( name ) {		/* (name) - Search scene by name */

		PyObject *wanted_scene = NULL;

		while( ( scene_iter ) && ( wanted_scene == NULL ) ) {

			if( strcmp( name, scene_iter->id.name + 2 ) == 0 )
				wanted_scene =
					V24_Scene_CreatePyObject( scene_iter );

			scene_iter = scene_iter->id.next;
		}

		if( wanted_scene == NULL ) {	/* Requested scene doesn't exist */
			char error_msg[64];
			PyOS_snprintf( error_msg, sizeof( error_msg ),
				       "Scene \"%s\" not found", name );
			return ( V24_EXPP_ReturnPyObjError
				 ( PyExc_NameError, error_msg ) );
		}

		return wanted_scene;
	}

	else {	/* () - return a list with wrappers for all scenes in Blender */
		int index = 0;
		PyObject *sce_pylist, *pyobj;

		sce_pylist = PyList_New( BLI_countlist( &( G.main->scene ) ) );

		if( sce_pylist == NULL )
			return ( V24_EXPP_ReturnPyObjError( PyExc_MemoryError,
							"couldn't create PyList" ) );

		while( scene_iter ) {
			pyobj = V24_Scene_CreatePyObject( scene_iter );

			if( !pyobj ) {
				Py_DECREF(sce_pylist);
				return ( V24_EXPP_ReturnPyObjError
					 ( PyExc_MemoryError,
					   "couldn't create PyString" ) );
			}
			PyList_SET_ITEM( sce_pylist, index, pyobj );

			scene_iter = scene_iter->id.next;
			index++;
		}

		return sce_pylist;
	}
}

/*-----------------------Scene.GetCurrent()------------------------------*/
static PyObject *V24_M_Scene_GetCurrent( PyObject * self )
{
	return V24_Scene_CreatePyObject( ( Scene * ) G.scene );
}

static PyObject *V24_M_Scene_getCurrent_deprecated( PyObject * self )
{
	static char warning = 1;
	if( warning ) {
		printf("Blender.Scene.getCurrent() is deprecated,\n\tuse Blender.Scene.GetCurrent() instead.\n");
		--warning;
	}

	return V24_Scene_CreatePyObject( ( Scene * ) G.scene );
}


/*-----------------------Scene.Unlink()----------------------------------*/
static PyObject *V24_M_Scene_Unlink( PyObject * self, PyObject * args )
{
	PyObject *pyobj;
	V24_BPy_Scene *pyscn;
	Scene *scene;

	if( !PyArg_ParseTuple( args, "O!", &V24_Scene_Type, &pyobj ) )
		return V24_EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected Scene PyType object" );
	
	pyscn = (V24_BPy_Scene *)pyobj;
	scene = pyscn->scene;
	
	SCENE_DEL_CHECK_PY(pyscn);
	
	if( scene == G.scene )
		return V24_EXPP_ReturnPyObjError( PyExc_SystemError,
					      "current Scene cannot be removed!" );

	free_libblock( &G.main->scene, scene );
	
	pyscn->scene= NULL;
	Py_RETURN_NONE;
}

/* DEPRECATE ME !!! */
/*-----------------------V24_BPy_Scene function defintions-------------------*/

/*-----------------------Scene.setLayers()---------------------------------*/
static PyObject *V24_Scene_oldsetLayers( V24_BPy_Scene * self, PyObject * args )
{
	return V24_EXPP_setterWrapper( (void *)self, args, (setter)V24_Scene_setLayerList );
}
/* END DEPRECATE CODE */


/*-----------------------Scene.copy()------------------------------------*/
static PyObject *V24_Scene_copy( V24_BPy_Scene * self, PyObject * args )
{
	short dup_objs = 1;
	Scene *scene = self->scene;

	SCENE_DEL_CHECK_PY(self);

	if( !PyArg_ParseTuple( args, "|h", &dup_objs ) )
		return V24_EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected int in [0,2] or nothing as argument" );

	return V24_Scene_CreatePyObject( copy_scene( scene, dup_objs ) );
}

/*-----------------------Scene.makeCurrent()-----------------------------*/
static PyObject *V24_Scene_makeCurrent( V24_BPy_Scene * self )
{
	Scene *scene = self->scene;
#if 0	/* add back in when bpy becomes "official" */
	static char warning = 1;
	if( warning ) {
		printf("scene.makeCurrent() deprecated!\n\tuse bpy.scenes.active = scene instead\n");
		--warning;
	}
#endif

	SCENE_DEL_CHECK_PY(self);
	
	if( scene && scene != G.scene) {
		set_scene( scene );
		scene_update_for_newframe(scene, scene->lay);
	}

	Py_RETURN_NONE;
}

/*-----------------------Scene.update()----------------------------------*/
static PyObject *V24_Scene_update( V24_BPy_Scene * self, PyObject * args )
{
	Scene *scene = self->scene;
	int full = 0;
	
	SCENE_DEL_CHECK_PY(self);
	if( !PyArg_ParseTuple( args, "|i", &full ) )
		return V24_EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected nothing or int (0 or 1) argument" );

/* Under certain circunstances, DAG_scene_sort *here* can crash Blender.
 * A "RuntimeError: max recursion limit" happens when a scriptlink
 * on frame change has scene.update(1).
 * Investigate better how to avoid this. */
	if( !full )
		DAG_scene_sort( scene );

	else if( full == 1 ) {
		int enablescripts = G.f & G_DOSCRIPTLINKS;
		
		/*Disable scriptlinks to prevent firing off newframe scriptlink
		  events.*/
		G.f &= ~G_DOSCRIPTLINKS;
		set_scene_bg( scene );
		scene_update_for_newframe( scene, scene->lay );
		
		/*re-enabled scriptlinks if necassary.*/
		if (enablescripts) G.f |= G_DOSCRIPTLINKS;
	} else
		return V24_EXPP_ReturnPyObjError( PyExc_ValueError,
					      "in method scene.update(full), full should be:\n"
					      "0: to only sort scene elements (old behavior); or\n"
					      "1: for a full update (regroups, does ipos, keys, etc.)" );

	Py_RETURN_NONE;
}

/*-----------------------Scene.link()------------------------------------*/
static PyObject *V24_Scene_link( V24_BPy_Scene * self, PyObject * args )
{
	Scene *scene = self->scene;
	V24_BPy_Object *bpy_obj;
	Object *object = NULL;
	static char warning = 1;

	if( warning ) {
		printf("scene.link(ob) deprecated!\n\tuse scene.objects.link(ob) instead\n");
		--warning;
	}

	SCENE_DEL_CHECK_PY(self);
	
	if( !PyArg_ParseTuple( args, "O!", &V24_Object_Type, &bpy_obj ) )
		return V24_EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected Object argument" );
	
	
		/*return V24_EXPP_ReturnPyObjError( PyExc_RuntimeError,
					          "Could not create data on demand for this object type!" );*/
	
	object = bpy_obj->object;
	
	/* Object.c's V24_EXPP_add_obdata does not support these objects */
	if (!object->data && (object->type == OB_SURF || object->type == OB_FONT || object->type == OB_WAVE )) {
		return V24_EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Object has no data and new data cant be automaticaly created for Surf, Text or Wave type objects!" );
	} else {
		/* Ok, all is fine, let's try to link it */
		Base *base;

		/* We need to link the object to a 'Base', then link this base
		 * to the scene.        See DNA_scene_types.h ... */

		/* First, check if the object isn't already in the scene */
		base = object_in_scene( object, scene );
		/* if base is not NULL ... */
		if( base )	/* ... the object is already in one of the Scene Bases */
			return V24_EXPP_ReturnPyObjError( PyExc_RuntimeError,
						      "object already in scene!" );

		/* not linked, go get mem for a new base object */

		base = MEM_callocN( sizeof( Base ), "pynewbase" );

		if( !base )
			return V24_EXPP_ReturnPyObjError( PyExc_MemoryError,
						      "couldn't allocate new Base for object" );

		/*  if the object has not yet been linked to object data, then
		 *  set the real type before we try creating data */

		if( bpy_obj->realtype != OB_EMPTY ) {
			object->type = bpy_obj->realtype;
			bpy_obj->realtype = OB_EMPTY;
		}

		/* check if this object has obdata, case not, try to create it */

		if( !object->data && ( object->type != OB_EMPTY ) )
			V24_EXPP_add_obdata( object ); /* returns -1 on error, defined in Object.c */

		base->object = object;	/* link object to the new base */
		base->lay = object->lay;
		base->flag = object->flag;

		object->id.us += 1;	/* incref the object user count in Blender */

		BLI_addhead( &scene->base, base );	/* finally, link new base to scene */
	}

	Py_RETURN_NONE;
}

/*-----------------------Scene.unlink()----------------------------------*/
static PyObject *V24_Scene_unlink( V24_BPy_Scene * self, PyObject * args )
{
	V24_BPy_Object *bpy_obj = NULL;
	Scene *scene = self->scene;
	Base *base;
	static char warning = 1;

	if( warning ) {
		printf("scene.unlink(ob) deprecated!\n\tuse scene.objects.unlink(ob) instead\n");
		--warning;
	}

	SCENE_DEL_CHECK_PY(self);
	
	if( !PyArg_ParseTuple( args, "O!", &V24_Object_Type, &bpy_obj ) )
		return V24_EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected Object as argument" );

	/* is the object really in the scene? */
	base = object_in_scene( bpy_obj->object, scene );

	if( base ) {		/* if it is, remove it */
		if (scene->basact==base)
			scene->basact= NULL;	/* in case the object was selected */
		
		free_and_unlink_base_from_scene( scene, base );
		Py_RETURN_TRUE;
	}
	else
		Py_RETURN_FALSE;
}

/*-----------------------Scene.getChildren()-----------------------------*/
static PyObject *V24_Scene_getChildren( V24_BPy_Scene * self )
{
	Scene *scene = self->scene;
	PyObject *pylist;
	PyObject *bpy_obj;
	Object *object;
	Base *base;
	static char warning = 1;

	if( warning ) {
		printf("scene.getChildren() deprecated!\n\tuse scene.objects instead\n");
		--warning;
	}

	SCENE_DEL_CHECK_PY(self);

	pylist = PyList_New( 0 );
	
	base = scene->base.first;

	while( base ) {
		object = base->object;
		
		bpy_obj = V24_Object_CreatePyObject( object );
		
		if( !bpy_obj ) {
			Py_DECREF(pylist);
			return V24_EXPP_ReturnPyObjError( PyExc_RuntimeError,
						      "couldn't create new object wrapper" );
		}
		PyList_Append( pylist, bpy_obj );
		Py_DECREF( bpy_obj );	/* PyList_Append incref'ed it */
		
		base = base->next;
	}

	return pylist;
}

/*-----------------------Scene.getActiveObject()------------------------*/
static PyObject *V24_Scene_getActiveObject(V24_BPy_Scene *self)
{
	Scene *scene = self->scene;
	PyObject *pyob;
	Object *ob;
	static char warning = 1;

	if( warning ) {
		printf("scene.getActiveObject() deprecated!\n\tuse scene.objects.active instead\n");
		--warning;
	}

	SCENE_DEL_CHECK_PY(self);
	
	ob = ((scene->basact) ? (scene->basact->object) : 0);

	if (ob) {
		pyob = V24_Object_CreatePyObject( ob );

		if (!pyob)
			return V24_EXPP_ReturnPyObjError(PyExc_MemoryError,
					"couldn't create new object wrapper!");

		return pyob;
	}

	Py_RETURN_NONE; /* no active object */
}

/*-----------------------Scene.getCurrentCamera()------------------------*/
static PyObject *V24_Scene_getCurrentCamera( V24_BPy_Scene * self )
{
	static char warning = 1;
	
	if( warning ) {
		printf("scene.getCurrentCamera() deprecated!\n\tuse scene.objects.camera instead\n");
		--warning;
	}

	SCENE_DEL_CHECK_PY(self);
	/* None is ok */
	return V24_Object_CreatePyObject( self->scene->camera );
}

/*-----------------------Scene.setCurrentCamera()------------------------*/
static PyObject *V24_Scene_setCurrentCamera( V24_BPy_Scene * self, PyObject * args )
{
	Object *object;
	V24_BPy_Object *cam_obj;
	Scene *scene = self->scene;
	static char warning = 1;

	if( warning ) {
		printf("scene.setCurrentCamera(ob) deprecated!\n\tSet scene.objects.camera = ob instead\n");
		--warning;
	}

	SCENE_DEL_CHECK_PY(self);
	
	if( !PyArg_ParseTuple( args, "O!", &V24_Object_Type, &cam_obj ) )
		return V24_EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected Camera Object as argument" );

	object = cam_obj->object;
	if( object->type != OB_CAMERA )
		return V24_EXPP_ReturnPyObjError( PyExc_ValueError,
					      "expected Camera Object as argument" );

	scene->camera = object;	/* set the current Camera */

	/* if this is the current scene, update its window now */
	if( !G.background && scene == G.scene ) /* Traced a crash to redrawing while in background mode -Campbell */
		copy_view3d_lock( REDRAW );

/* XXX copy_view3d_lock(REDRAW) prints "bad call to addqueue: 0 (18, 1)".
 * The same happens in bpython. */

	Py_RETURN_NONE;
}

/*-----------------------Scene.getRenderingContext()---------------------*/
static PyObject *V24_Scene_getRenderingContext( V24_BPy_Scene * self )
{
	SCENE_DEL_CHECK_PY(self);
	return V24_RenderData_CreatePyObject( self->scene );
}

static PyObject *V24_Scene_getRadiosityContext( V24_BPy_Scene * self )
{
	SCENE_DEL_CHECK_PY(self);
	return V24_Radio_CreatePyObject( self->scene );
}

static PyObject *V24_Scene_getSequence( V24_BPy_Scene * self )
{
	SCENE_DEL_CHECK_PY(self);
	if (self->scene->ed) /* we should create this if its not there :/ */
		return V24_SceneSeq_CreatePyObject( self->scene, NULL );
	else
		Py_RETURN_NONE;
}

/* scene.addScriptLink */
static PyObject *V24_Scene_addScriptLink( V24_BPy_Scene * self, PyObject * args )
{
	Scene *scene = self->scene;
	ScriptLink *slink = NULL;

	SCENE_DEL_CHECK_PY(self);

	slink = &( scene )->scriptlink;

	return V24_EXPP_addScriptLink( slink, args, 1 );
}

/* scene.clearScriptLinks */
static PyObject *V24_Scene_clearScriptLinks( V24_BPy_Scene * self, PyObject * args )
{
	Scene *scene = self->scene;
	ScriptLink *slink = NULL;

	SCENE_DEL_CHECK_PY(self);

	slink = &( scene )->scriptlink;

	return V24_EXPP_clearScriptLinks( slink, args );
}

/* scene.getScriptLinks */
static PyObject *V24_Scene_getScriptLinks( V24_BPy_Scene * self, PyObject * value )
{
	Scene *scene = self->scene;
	ScriptLink *slink = NULL;
	PyObject *ret = NULL;

	SCENE_DEL_CHECK_PY(self);

	slink = &( scene )->scriptlink;

	ret = V24_EXPP_getScriptLinks( slink, value, 1 );

	if( ret )
		return ret;
	else
		return NULL;
}

static PyObject *V24_Scene_play( V24_BPy_Scene * self, PyObject * args )
{
	int mode = 0, win = SPACE_VIEW3D;
	PyObject *ret = NULL;
	ScrArea *sa = NULL, *oldsa = curarea;

	SCENE_DEL_CHECK_PY(self);

	if( !PyArg_ParseTuple( args, "|ii", &mode, &win ) )
		return V24_EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected nothing, or or two ints as arguments." );

	if( mode < 0 || mode > 3 )
		return V24_EXPP_ReturnPyObjError( PyExc_TypeError,
					      "mode should be in range [0, 3]." );

	switch ( win ) {
	case SPACE_VIEW3D:
	case SPACE_SEQ:
	case SPACE_IPO:
	case SPACE_ACTION:
	case SPACE_NLA:
	case SPACE_SOUND:
	case SPACE_BUTS:	/* from here they don't 'play', but ... */
	case SPACE_TEXT:	/* ... might be used as a timer. */
	case SPACE_SCRIPT:
	case SPACE_OOPS:
	case SPACE_IMAGE:
	case SPACE_IMASEL:
	case SPACE_INFO:
	case SPACE_FILE:
		break;
	default:
		win = SPACE_VIEW3D;
	}

	/* we have to move to a proper win */
	sa = find_biggest_area_of_type( win );
	if( !sa && win != SPACE_VIEW3D )
		sa = find_biggest_area_of_type( SPACE_VIEW3D );

	if( !sa )
		sa = find_biggest_area(  );

	if( sa )
		areawinset( sa->win );

	/* play_anim returns 0 for normal exit or 1 if user canceled it */
	ret = PyInt_FromLong( (long)play_anim( mode ) );

	if( sa )
		areawinset( oldsa->win );

	return ret;
}

static PyObject *V24_Scene_getTimeLine( V24_BPy_Scene *self ) 
{
	V24_BPy_TimeLine *tm; 

	SCENE_DEL_CHECK_PY(self);
	
	tm= (V24_BPy_TimeLine *) PyObject_NEW (V24_BPy_TimeLine, &V24_TimeLine_Type);
	if (!tm)
		return V24_EXPP_ReturnPyObjError (PyExc_MemoryError,
					      "couldn't create V24_BPy_TimeLine object");
	tm->marker_list= &(self->scene->markers);
	tm->sfra= (int) self->scene->r.sfra;
	tm->efra= (int) self->scene->r.efra;

	return (PyObject *)tm;
}

/************************************************************************
 *
 * Object Sequence 
 *
 ************************************************************************/
/*
 * create a thin wrapper for the scenes objects
 */

/* accessed from scn.objects.selected or scn.objects.context */
static PyObject *V24_SceneObSeq_getObjects( V24_BPy_SceneObSeq *self, void *mode) 
{
	SCENE_DEL_CHECK_PY(self->bpyscene);
	return V24_SceneObSeq_CreatePyObject(self->bpyscene, NULL, (int)((long)mode));
}

int V24_SceneObSeq_setObjects( V24_BPy_SceneObSeq *self, PyObject *value, void *_mode_) 
{
	/*
	ONLY SUPPORTS scn.objects.selected and scn.objects.context 
	cannot assign to scn.objects yet!!!
	*/
	PyObject *item;
	Scene *scene= self->bpyscene->scene;
	Object *blen_ob;
	Base *base;
	int size, mode = (int)_mode_;
	
	SCENE_DEL_CHECK_INT(self->bpyscene);
	
	/* scn.objects.selected = scn.objects  - shortcut to select all */
	if (BPy_SceneObSeq_Check(value)) {
		V24_BPy_SceneObSeq *bpy_sceneseq = (V24_BPy_SceneObSeq *)value;
		if (self->bpyscene->scene != bpy_sceneseq->bpyscene->scene)
			return V24_EXPP_ReturnIntError( PyExc_ValueError,
					"Cannot assign a SceneObSeq type from another scene" );
		if (bpy_sceneseq->mode != EXPP_OBSEQ_NORMAL)
			return V24_EXPP_ReturnIntError( PyExc_ValueError,
					"Can only assign scn.objects to scn.objects.context or scn.objects.selected" );
		
		for (base= scene->base.first; base; base= base->next) {
			base->flag |= SELECT;
			base->object->flag |= SELECT;
			
			if (mode==EXPP_OBSEQ_CONTEXT && G.vd) {
				base->object->lay= base->lay= G.vd->lay;
			}
		}
		return 0;
	}

	if (!PySequence_Check(value))
		return V24_EXPP_ReturnIntError( PyExc_ValueError,
				"Error, must assign a sequence of objects to scn.objects.selected" );
	
	/* for context and selected, just deselect, dont remove */
	for (base= scene->base.first; base; base= base->next) {
		base->flag &= ~SELECT;
		base->object->flag &= ~SELECT;
	}
	
	size = PySequence_Length(value);
	while (size) {
		size--;
		item = PySequence_GetItem(value, size);
		if ( PyObject_TypeCheck(item, &V24_Object_Type) ) {
			blen_ob= ((V24_BPy_Object *)item)->object;
			base = object_in_scene( blen_ob, scene );
			if (base) {
				blen_ob->flag |= SELECT;
				base->flag |= SELECT;
				if (mode==EXPP_OBSEQ_CONTEXT && G.vd) {
					blen_ob->restrictflag &= ~OB_RESTRICT_VIEW;
					blen_ob->lay= base->lay= G.vd->lay;
				}
			}
		}
		Py_DECREF(item);
	}
	return 0;
}


static PyObject *V24_SceneObSeq_CreatePyObject( V24_BPy_Scene *self, Base *iter, int mode )
{
	V24_BPy_SceneObSeq *seq = PyObject_NEW( V24_BPy_SceneObSeq, &V24_SceneObSeq_Type);
	seq->bpyscene = self; Py_INCREF(self);
	seq->iter = iter;
	seq->mode = mode;
	return (PyObject *)seq;
}

static int V24_SceneObSeq_len( V24_BPy_SceneObSeq * self )
{
	Scene *scene= self->bpyscene->scene;
	SCENE_DEL_CHECK_INT(self->bpyscene);
	
	if (self->mode == EXPP_OBSEQ_NORMAL)
		return BLI_countlist( &( scene->base ) );
	else if (self->mode == EXPP_OBSEQ_SELECTED) {
		int len=0;
		Base *base;
		for (base= scene->base.first; base; base= base->next) {
			if (base->flag & SELECT) {
				len++;
			}
		}
		return len;
	} else if (self->mode == EXPP_OBSEQ_CONTEXT) {
		int len=0;
		Base *base;
		
		if( G.vd == NULL ) /* No 3d view has been initialized yet, simply return an empty list */
			return 0;
		
		for (base= scene->base.first; base; base= base->next) {
			if TESTBASE(base) {
				len++;
			}
		}
		return len;
	}
	/*should never run this */
	return 0;
}

/*
 * retrive a single Object from somewhere in the Object list
 */

static PyObject *V24_SceneObSeq_item( V24_BPy_SceneObSeq * self, int i )
{
	int index=0;
	Base *base= NULL;
	Scene *scene= self->bpyscene->scene;
	
	SCENE_DEL_CHECK_PY(self->bpyscene);
	
	/* objects */
	if (self->mode==EXPP_OBSEQ_NORMAL)
		for (base= scene->base.first; base && i!=index; base= base->next, index++) {}
	/* selected */
	else if (self->mode==EXPP_OBSEQ_SELECTED) {
		for (base= scene->base.first; base && i!=index; base= base->next)
			if (base->flag & SELECT)
				index++;
	}
	/* context */
	else if (self->mode==EXPP_OBSEQ_CONTEXT) {
		if (G.vd)
			for (base= scene->base.first; base && i!=index; base= base->next)
				if TESTBASE(base)
					index++;
	}
	
	if (!(base))
		return V24_EXPP_ReturnPyObjError( PyExc_IndexError,
					      "array index out of range" );
	
	return V24_Object_CreatePyObject( base->object );
}

static PySequenceMethods V24_SceneObSeq_as_sequence = {
	( inquiry ) V24_SceneObSeq_len,	/* sq_length */
	( binaryfunc ) 0,	/* sq_concat */
	( intargfunc ) 0,	/* sq_repeat */
	( intargfunc ) V24_SceneObSeq_item,	/* sq_item */
	( intintargfunc ) 0,	/* sq_slice */
	( intobjargproc ) 0,	/* sq_ass_item */
	( intintobjargproc ) 0,	/* sq_ass_slice */
	0,0,0,
};


/************************************************************************
 *
 * Python V24_SceneObSeq_Type iterator (iterates over GroupObjects)
 *
 ************************************************************************/

/*
 * Initialize the interator index
 */

static PyObject *V24_SceneObSeq_getIter( V24_BPy_SceneObSeq * self )
{
	/* we need to get the first base, but for selected context we may need to advance
	to the first selected or first conext base */
	Base *base= self->bpyscene->scene->base.first;
	
	SCENE_DEL_CHECK_PY(self->bpyscene);
	
	if (self->mode==EXPP_OBSEQ_SELECTED)
		while (base && !(base->flag & SELECT))
			base= base->next;
	else if (self->mode==EXPP_OBSEQ_CONTEXT) {
		if (!G.vd)
			base= NULL; /* will never iterate if we have no */
		else
			while (base && !TESTBASE(base))
				base= base->next;	
	}
	/* create a new iterator if were alredy using this one */
	if (self->iter==NULL) {
		self->iter = base;
		return V24_EXPP_incr_ret ( (PyObject *) self );
	} else {
		return V24_SceneObSeq_CreatePyObject(self->bpyscene, base, self->mode);
	}
}

/*
 * Return next SceneOb.
 */

static PyObject *V24_SceneObSeq_nextIter( V24_BPy_SceneObSeq * self )
{
	PyObject *object;
	Base *base;
	if( !(self->iter) ||  !(self->bpyscene->scene) ) {
		self->iter= NULL;
		return V24_EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );
	}
	
	object= V24_Object_CreatePyObject( self->iter->object ); 
	base= self->iter->next;
	
	if (self->mode==EXPP_OBSEQ_SELECTED)
		while (base && !(base->flag & SELECT))
			base= base->next;
	else if (self->mode==EXPP_OBSEQ_CONTEXT) {
		if (!G.vd)
			base= NULL; /* will never iterate if we have no */
		else
			while (base && !TESTBASE(base))
				base= base->next;	
	}
	self->iter= base;
	return object;
}


static PyObject *V24_SceneObSeq_link( V24_BPy_SceneObSeq * self, PyObject *pyobj )
{	
	SCENE_DEL_CHECK_PY(self->bpyscene);
	
	/* this shold eventually replace V24_Scene_link */
	if (self->mode != EXPP_OBSEQ_NORMAL)
		return (V24_EXPP_ReturnPyObjError( PyExc_TypeError,
					      "Cannot link to objects.selection or objects.context!" ));	
	
	/*
	if (self->iter != NULL)
		return V24_EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Cannot modify scene objects while iterating" );
	*/
	
	if( PyTuple_Size(pyobj) == 1 ) {
		V24_BPy_LibraryData *seq = ( V24_BPy_LibraryData * )PyTuple_GET_ITEM( pyobj, 0 );
		if( BPy_LibraryData_Check( seq ) )
			return V24_LibraryData_importLibData( seq, seq->name,
					( seq->kind == OBJECT_IS_LINK ? FILE_LINK : 0 ),
					self->bpyscene->scene );
	}
	return V24_Scene_link(self->bpyscene, pyobj);
}

/* This is buggy with new object data not already linked to an object, for now use the above code */
static PyObject *V24_SceneObSeq_new( V24_BPy_SceneObSeq * self, PyObject *args )
{
	
	void *data = NULL;
	char *name = NULL;
	char *desc = NULL;
	short type = OB_EMPTY;
	struct Object *object;
	Base *base;
	PyObject *py_data;
	Scene *scene= self->bpyscene->scene;
	
	SCENE_DEL_CHECK_PY(self->bpyscene);
	
	if (self->mode != EXPP_OBSEQ_NORMAL)
		return V24_EXPP_ReturnPyObjError( PyExc_TypeError,
					"Cannot add new to objects.selection or objects.context!" );	

	if( !PyArg_ParseTuple( args, "O|s", &py_data, &name ) )
		return V24_EXPP_ReturnPyObjError( PyExc_TypeError,
				"scene.objects.new(obdata) - expected obdata to be\n\ta python obdata type or the string 'Empty'" );

	if( BPy_Armature_Check( py_data ) ) {
		data = ( void * ) V24_Armature_FromPyObject( py_data );
		type = OB_ARMATURE;
	} else if( BPy_Camera_Check( py_data ) ) {
		data = ( void * ) V24_Camera_FromPyObject( py_data );
		type = OB_CAMERA;
	} else if( BPy_Lamp_Check( py_data ) ) {
		data = ( void * ) V24_Lamp_FromPyObject( py_data );
		type = OB_LAMP;
	} else if( BPy_Curve_Check( py_data ) ) {
		data = ( void * ) V24_Curve_FromPyObject( py_data );
		type = OB_CURVE;
	} else if( BPy_NMesh_Check( py_data ) ) {
		data = ( void * ) NMesh_FromPyObject( py_data, NULL );
		type = OB_MESH;
		if( !data )		/* NULL means there is already an error */
			return NULL;
	} else if( BPy_Mesh_Check( py_data ) ) {
		data = ( void * ) V24_Mesh_FromPyObject( py_data, NULL );
		type = OB_MESH;
	} else if( BPy_Lattice_Check( py_data ) ) {
		data = ( void * ) V24_Lattice_FromPyObject( py_data );
		type = OB_LATTICE;
	} else if( BPy_Metaball_Check( py_data ) ) {
		data = ( void * ) V24_Metaball_FromPyObject( py_data );
		type = OB_MBALL;
	} else if( V24_BPy_Text3d_Check( py_data ) ) {
		data = ( void * ) V24_Text3d_FromPyObject( py_data );
		type = OB_FONT;
	} else if( ( desc = PyString_AsString( (PyObject *)py_data ) ) != NULL ) {
		if( !strcmp( desc, "Empty" ) ) {
			type = OB_EMPTY;
			data = NULL;
		} else
			goto typeError;
	} else {
typeError:
		return V24_EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected an object and optionally a string as arguments" );
	}

	if (!name) {
		if (type == OB_EMPTY)
			name = "Empty";
		else
			name = ((ID *)data)->name + 2;
	}
	
	object = add_only_object(type, name);
	
	if( data ) {
		object->data = data;
		id_us_plus((ID *)data);
	}
	
	object->flag = SELECT;
	
	/* creates the curve for the text object */
	if (type == OB_FONT) 
		text_to_curve(object, 0);
	
	/* link to scene */
	base = MEM_callocN( sizeof( Base ), "pynewbase" );

	if( !base )
		return V24_EXPP_ReturnPyObjError( PyExc_MemoryError,
						  "couldn't allocate new Base for object" );

	base->object = object;	/* link object to the new base */
	
	if (scene == G.scene && G.vd) {
		if (G.vd->localview) {
			object->lay= G.vd->layact + G.vd->lay;
		} else {
			object->lay= G.vd->layact;
		}
	} else {
		base->lay= object->lay = scene->lay & ((1<<20)-1);	/* Layer, by default visible*/	
	}
	
	base->lay= object->lay;
	
	base->flag = SELECT;
	object->id.us = 1; /* we will exist once in this scene */

	BLI_addhead( &(scene->base), base );	/* finally, link new base to scene */
	
	/* make sure data and object materials are consistent */
	test_object_materials( (ID *)object->data );
	
	/* so we can deal with vertex groups */
	if (type == OB_MESH)
		((V24_BPy_Mesh *)py_data)->object = object;
	
	return V24_Object_CreatePyObject( object );

}

static PyObject *V24_SceneObSeq_unlink( V24_BPy_SceneObSeq * self, PyObject *args )
{
	PyObject *pyobj;
	Object *blen_ob;
	Scene *scene;
	Base *base= NULL;
	
	SCENE_DEL_CHECK_PY(self->bpyscene);
	
	if (self->mode != EXPP_OBSEQ_NORMAL)
		return (V24_EXPP_ReturnPyObjError( PyExc_TypeError,
					      "Cannot add new to objects.selection or objects.context!" ));	
	
	if( !PyArg_ParseTuple( args, "O!", &V24_Object_Type, &pyobj ) )
		return ( V24_EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a python object as an argument" ) );
	
	blen_ob = ( ( V24_BPy_Object * ) pyobj )->object;
	
	scene = self->bpyscene->scene;
	
	/* is the object really in the scene? */
	base = object_in_scene( blen_ob, scene);
	if( base ) { /* if it is, remove it */
		if (scene->basact==base)
			scene->basact= NULL;	/* in case the object was selected */
		free_and_unlink_base_from_scene(scene, base);
		Py_RETURN_TRUE;
	}
	Py_RETURN_FALSE;
}

PyObject *V24_SceneObSeq_getActive(V24_BPy_SceneObSeq *self)
{
	Base *base;
	SCENE_DEL_CHECK_PY(self->bpyscene);
	
	if (self->mode!=EXPP_OBSEQ_NORMAL)
			return (V24_EXPP_ReturnPyObjError( PyExc_TypeError,
						"cannot get active from objects.selected or objects.context" ));
	
	base= self->bpyscene->scene->basact;
	if (!base)
		Py_RETURN_NONE;
	
	return V24_Object_CreatePyObject( base->object );
}

static int V24_SceneObSeq_setActive(V24_BPy_SceneObSeq *self, PyObject *value)
{
	Base *base;
	
	SCENE_DEL_CHECK_INT(self->bpyscene);
	
	if (self->mode!=EXPP_OBSEQ_NORMAL)
			return (V24_EXPP_ReturnIntError( PyExc_TypeError,
						"cannot set active from objects.selected or objects.context" ));
	
	if (value==Py_None) {
		self->bpyscene->scene->basact= NULL;
		return 0;
	}
	
	if (!BPy_Object_Check(value))
		return (V24_EXPP_ReturnIntError( PyExc_ValueError,
					      "Object or None types can only be assigned to active!" ));
	
	base = object_in_scene( ((V24_BPy_Object *)value)->object, self->bpyscene->scene );
	
	if (!base)
		return (V24_EXPP_ReturnIntError( PyExc_ValueError,
					"cannot assign an active object outside the scene." ));
	
	self->bpyscene->scene->basact= base;
	return 0;
}

PyObject *V24_SceneObSeq_getCamera(V24_BPy_SceneObSeq *self)
{
	SCENE_DEL_CHECK_PY(self->bpyscene);
	
	if (self->mode!=EXPP_OBSEQ_NORMAL)
			return (V24_EXPP_ReturnPyObjError( PyExc_TypeError,
						"cannot get camera from objects.selected or objects.context" ));
	
	return V24_Object_CreatePyObject( self->bpyscene->scene->camera );
}

static int V24_SceneObSeq_setCamera(V24_BPy_SceneObSeq *self, PyObject *value)
{
	int ret;

	SCENE_DEL_CHECK_INT(self->bpyscene);
	if (self->mode!=EXPP_OBSEQ_NORMAL)
			return V24_EXPP_ReturnIntError( PyExc_TypeError,
					"cannot set camera from objects.selected or objects.context" );
	
	ret = V24_GenericLib_assignData(value, (void **) &self->bpyscene->scene->camera, 0, 0, ID_OB, 0);
	
	/* if this is the current scene, update its window now */
	if( ret == 0 && !G.background && self->bpyscene->scene == G.scene ) /* Traced a crash to redrawing while in background mode -Campbell */
		copy_view3d_lock( REDRAW );

/* XXX copy_view3d_lock(REDRAW) prints "bad call to addqueue: 0 (18, 1)".
 * The same happens in bpython. */

	return ret;
}


static struct PyMethodDef V24_BPy_SceneObSeq_methods[] = {
	{"link", (PyCFunction)V24_SceneObSeq_link, METH_VARARGS,
		"link object to this scene"},
	{"new", (PyCFunction)V24_SceneObSeq_new, METH_VARARGS,
		"Create a new object in this scene from the obdata given and return a new object"},
	{"unlink", (PyCFunction)V24_SceneObSeq_unlink, METH_VARARGS,
		"unlinks the object from the scene"},
	{NULL, NULL, 0, NULL}
};

/************************************************************************
 *
 * Python V24_SceneObSeq_Type standard operations
 *
 ************************************************************************/

static void V24_SceneObSeq_dealloc( V24_BPy_SceneObSeq * self )
{
	Py_DECREF(self->bpyscene);
	PyObject_DEL( self );
}

static int V24_SceneObSeq_compare( V24_BPy_SceneObSeq * a, V24_BPy_SceneObSeq * b )
{
	return ( a->bpyscene->scene == b->bpyscene->scene && a->mode == b->mode) ? 0 : -1;	
}

/*
 * repr function
 * callback functions building meaninful string to representations
 */
static PyObject *V24_SceneObSeq_repr( V24_BPy_SceneObSeq * self )
{
	if( !(self->bpyscene->scene) )
		return PyString_FromFormat( "[Scene ObjectSeq Removed]" );
	else if (self->mode==EXPP_OBSEQ_SELECTED)
		return PyString_FromFormat( "[Scene ObjectSeq Selected \"%s\"]",
						self->bpyscene->scene->id.name + 2 );
	else if (self->mode==EXPP_OBSEQ_CONTEXT)
		return PyString_FromFormat( "[Scene ObjectSeq Context \"%s\"]",
						self->bpyscene->scene->id.name + 2 );
	
	/*self->mode==0*/
	return PyString_FromFormat( "[Scene ObjectSeq \"%s\"]",
					self->bpyscene->scene->id.name + 2 );
}

static PyGetSetDef V24_SceneObSeq_getseters[] = {
	{"selected",
	 (getter)V24_SceneObSeq_getObjects, (setter)V24_SceneObSeq_setObjects,
	 "sequence of selected objects",
	 (void *)EXPP_OBSEQ_SELECTED},
	{"context",
	 (getter)V24_SceneObSeq_getObjects, (setter)V24_SceneObSeq_setObjects,
	 "sequence of user context objects",
	 (void *)EXPP_OBSEQ_CONTEXT},
	{"active",
	 (getter)V24_SceneObSeq_getActive, (setter)V24_SceneObSeq_setActive,
	 "active object",
	 NULL},
	{"camera",
	 (getter)V24_SceneObSeq_getCamera, (setter)V24_SceneObSeq_setCamera,
	 "camera object",
	 NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*****************************************************************************/
/* Python V24_SceneObSeq_Type structure definition:                               */
/*****************************************************************************/
PyTypeObject V24_SceneObSeq_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender SceneObSeq",           /* char *tp_name; */
	sizeof( V24_BPy_SceneObSeq ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) V24_SceneObSeq_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) V24_SceneObSeq_compare, /* cmpfunc tp_compare; */
	( reprfunc ) V24_SceneObSeq_repr,   /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	&V24_SceneObSeq_as_sequence,	    /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	( getiterfunc) V24_SceneObSeq_getIter, /* getiterfunc tp_iter; */
	( iternextfunc ) V24_SceneObSeq_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	V24_BPy_SceneObSeq_methods,       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	V24_SceneObSeq_getseters,       /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
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
