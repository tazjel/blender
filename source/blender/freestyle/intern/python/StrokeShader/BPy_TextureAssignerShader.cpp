#include "BPy_TextureAssignerShader.h"

#include "../../stroke/BasicStrokeShaders.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char TextureAssignerShader___doc__[] =
"[Texture shader]\n"
"\n"
".. method:: __init__(id)\n"
"\n"
"   Builds a TextureAssignerShader object.\n"
"\n"
"   :arg id: The preset number to use.\n"
"   :type id: int\n"
"\n"
".. method:: shade(s)\n"
"\n"
"   Assigns a texture to the stroke in order to simulate its marks\n"
"   system.  This shader takes as input an integer value telling which\n"
"   texture and blending mode to use among a set of predefined\n"
"   textures.  Here are the different presets:\n"
"\n"
"   * 0: `/brushes/charcoalAlpha.bmp`, `MediumType.HUMID_MEDIUM`\n"
"   * 1: `/brushes/washbrushAlpha.bmp`, `MediumType.HUMID_MEDIUM`\n"
"   * 2: `/brushes/oil.bmp`, `MediumType.HUMID_MEDIUM`\n"
"   * 3: `/brushes/oilnoblend.bmp`, `MediumType.HUMID_MEDIUM`\n"
"   * 4: `/brushes/charcoalAlpha.bmp`, `MediumType.DRY_MEDIUM`\n"
"   * 5: `/brushes/washbrushAlpha.bmp`, `MediumType.DRY_MEDIUM`\n"
"   * 6: `/brushes/opaqueDryBrushAlpha.bmp`, `MediumType.OPAQUE_MEDIUM`\n"
"   * 7: `/brushes/opaqueBrushAlpha.bmp`, `MediumType.OPAQUE_MEDIUM`\n"
"\n"
"   Any other value will lead to the following preset:\n"
"\n"
"   * Default: `/brushes/smoothAlpha.bmp`, `MediumType.OPAQUE_MEDIUM`\n"
"\n"
"   :arg s: A Stroke object.\n"
"   :type s: :class:`Stroke`\n";

static int TextureAssignerShader___init__( BPy_TextureAssignerShader* self, PyObject *args)
{
	int i;

	if(!( PyArg_ParseTuple(args, "i", &i) ))
		return -1;

	self->py_ss.ss = new StrokeShaders::TextureAssignerShader(i);
	return 0;
}

/*-----------------------BPy_TextureAssignerShader type definition ------------------------------*/

PyTypeObject TextureAssignerShader_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"TextureAssignerShader",        /* tp_name */
	sizeof(BPy_TextureAssignerShader), /* tp_basicsize */
	0,                              /* tp_itemsize */
	0,                              /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	0,                              /* tp_repr */
	0,                              /* tp_as_number */
	0,                              /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	0,                              /* tp_hash  */
	0,                              /* tp_call */
	0,                              /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	TextureAssignerShader___doc__,  /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	0,                              /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&StrokeShader_Type,             /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)TextureAssignerShader___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
