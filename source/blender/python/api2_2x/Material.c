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
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include "Material.h"

/*****************************************************************************/
/* Function:              M_Material_New                                     */
/* Python equivalent:     Blender.Material.New                               */
/*****************************************************************************/
static PyObject *M_Material_New(PyObject *self, PyObject *args, PyObject *keywords)
{
  char        *name = "Mat";
  static char *kwlist[] = {"name", NULL};
  C_Material    *pymat; /* for Material Data object wrapper in Python */
  Material      *blmat; /* for actual Material Data we create in Blender */
  char        buf[21];

  if (!PyArg_ParseTupleAndKeywords(args, keywords, "|s", kwlist, &name))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
            "expected string or nothing as argument"));

  if (strcmp(name, "Mat") != 0) /* use gave us a name ?*/
    PyOS_snprintf(buf, sizeof(buf), "%s", name);

  blmat = add_material(name); /* first create the Material Data in Blender */

  if (blmat) /* now create the wrapper obj in Python */
    pymat = (C_Material *)Material_createPyObject (blmat);
  else
    return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                            "couldn't create Material Data in Blender"));

  if (pymat == NULL)
    return (EXPP_ReturnPyObjError (PyExc_MemoryError,
                            "couldn't create Material Data object"));

  return (PyObject *)pymat;
}

/*****************************************************************************/
/* Function:              M_Material_Get                                     */
/* Python equivalent:     Blender.Material.Get                               */
/* Description:           Receives a string and returns the material whose   */
/*                        name matches the string.  If no argument is        */
/*                        passed in, a list of all materials in the          */
/*                        current scene is returned.                         */
/*****************************************************************************/
static PyObject *M_Material_Get(PyObject *self, PyObject *args)
{
  char   *name = NULL;
  Material *mat_iter;

	if (!PyArg_ParseTuple(args, "|s", &name))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected string argument (or nothing)"));

  mat_iter = G.main->mat.first;

	if (name) { /* (name) - Search material by name */

    C_Material *wanted_mat = NULL;

    while ((mat_iter) && (wanted_mat == NULL)) {

      if (strcmp (name, mat_iter->id.name+2) == 0)
        wanted_mat = (C_Material *)Material_createPyObject (mat_iter);

      mat_iter = mat_iter->id.next;
    }

    if (wanted_mat == NULL) { /* Requested material doesn't exist */
      char error_msg[64];
      PyOS_snprintf(error_msg, sizeof(error_msg),
                      "Material \"%s\" not found", name);
      return (EXPP_ReturnPyObjError (PyExc_NameError, error_msg));
    }

    return (PyObject *)wanted_mat;
	}

	else { /* () - return a list of all materials in the scene */
    int index = 0;
    PyObject *matlist, *pystr;

    matlist = PyList_New (BLI_countlist (&(G.main->mat)));

    if (matlist == NULL)
      return (PythonReturnErrorObject (PyExc_MemoryError,
              "couldn't create PyList"));

		while (mat_iter) {
      pystr = PyString_FromString (mat_iter->id.name+2);

			if (!pystr)
				return (PythonReturnErrorObject (PyExc_MemoryError,
									"couldn't create PyString"));

			PyList_SET_ITEM (matlist, index, pystr);

      mat_iter = mat_iter->id.next;
      index++;
		}

		return (matlist);
	}
}

/*****************************************************************************/
/* Function:              M_Material_Init                                      */
/*****************************************************************************/
PyObject *M_Material_Init (void)
{
  PyObject  *submodule;

  submodule = Py_InitModule3("Blender.Material",
                  M_Material_methods, M_Material_doc);

  return (submodule);
}

/*****************************************************************************/
/* Python C_Material methods:                                                  */
/*****************************************************************************/
static PyObject *Material_getName(C_Material *self)
{
  PyObject *attr = PyString_FromString(self->material->id.name+2);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                   "couldn't get Material.name attribute"));
}

static PyObject *Material_getMode(C_Material *self)
{
  PyObject *attr = PyInt_FromLong((long)self->material->mode);

  if (attr) return attr;

  return EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                   "couldn't get Material.Mode attribute");
}

static PyObject *Material_getRGBCol(C_Material *self)
{
	return rgbTuple_getCol(self->rgb);
}

static PyObject *Material_getAmbCol(C_Material *self)
{
	return rgbTuple_getCol(self->amb);
}

static PyObject *Material_getSpecCol(C_Material *self)
{
	return rgbTuple_getCol(self->spec);
}

static PyObject *Material_getMirCol(C_Material *self)
{
	return rgbTuple_getCol(self->mir);
}

static PyObject *Material_getAmb(C_Material *self)
{
  PyObject *attr = PyFloat_FromDouble((double)self->material->amb);

  if (attr) return attr;

  return EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                   "couldn't get Material.amb attribute");
}

static PyObject *Material_getEmit(C_Material *self)
{
  PyObject *attr = PyFloat_FromDouble((double)self->material->emit);

  if (attr) return attr;

  return EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                   "couldn't get Material.emit attribute");
}

static PyObject *Material_getAng(C_Material *self)
{
	PyObject *attr = PyFloat_FromDouble((double)self->material->ang);

  if (attr) return attr;

  return EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                   "couldn't get Material.ang attribute");
}

static PyObject *Material_getAlpha(C_Material *self)
{
  PyObject *attr = PyFloat_FromDouble((double)self->material->alpha);

  if (attr) return attr;

  return EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                   "couldn't get Material.alpha attribute");
}

static PyObject *Material_getRef(C_Material *self)
{
  PyObject *attr = PyFloat_FromDouble((double)self->material->ref);

  if (attr) return attr;

  return EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                   "couldn't get Material.ref attribute");
}

static PyObject *Material_getSpec(C_Material *self)
{
  PyObject *attr = PyFloat_FromDouble((double)self->material->spec);

  if (attr) return attr;

  return EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                   "couldn't get Material.spec attribute");
}

static PyObject *Material_getSpecTransp(C_Material *self)
{
  PyObject *attr = PyFloat_FromDouble((double)self->material->spectra);

  if (attr) return attr;

  return EXPP_ReturnPyObjError (PyExc_RuntimeError,
              "couldn't get Material.specTransp attribute");
}

static PyObject *Material_getAdd(C_Material *self)
{
  PyObject *attr = PyFloat_FromDouble((double)self->material->add);

  if (attr) return attr;

  return EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                   "couldn't get Material.add attribute");
}

static PyObject *Material_getZOffset(C_Material *self)
{
  PyObject *attr = PyFloat_FromDouble((double)self->material->zoffs);

  if (attr) return attr;

  return EXPP_ReturnPyObjError (PyExc_RuntimeError,
                        "couldn't get Material.zOffset attribute");
}

static PyObject *Material_getHaloSize(C_Material *self)
{
  PyObject *attr = PyFloat_FromDouble((double)self->material->hasize);

  if (attr) return attr;

  return EXPP_ReturnPyObjError (PyExc_RuntimeError,
                   "couldn't get Material.haloSize attribute");
}

static PyObject *Material_getFlareSize(C_Material *self)
{
  PyObject *attr = PyFloat_FromDouble((double)self->material->flaresize);

  if (attr) return attr;

  return EXPP_ReturnPyObjError (PyExc_RuntimeError,
                     "couldn't get Material.flareSize attribute");
}

static PyObject *Material_getFlareBoost(C_Material *self)
{
  PyObject *attr = PyFloat_FromDouble((double)self->material->flareboost);

  if (attr) return attr;

  return EXPP_ReturnPyObjError (PyExc_RuntimeError,
                     "couldn't get Material.flareBoost attribute");
}

static PyObject *Material_getSubSize(C_Material *self)
{
  PyObject *attr = PyFloat_FromDouble((double)self->material->subsize);

  if (attr) return attr;

  return EXPP_ReturnPyObjError (PyExc_RuntimeError,
               "couldn't get Material.subSize attribute");
}

static PyObject *Material_getHardness(C_Material *self)
{
  PyObject *attr = PyInt_FromLong((long)self->material->har);

  if (attr) return attr;

  return EXPP_ReturnPyObjError (PyExc_RuntimeError,
                       "couldn't get Material.hard attribute");
}

static PyObject *Material_getNFlares(C_Material *self)
{
  PyObject *attr = PyInt_FromLong((long)self->material->flarec);

  if (attr) return attr;

  return EXPP_ReturnPyObjError (PyExc_RuntimeError,
                       "couldn't get Material.nFlares attribute");
}

static PyObject *Material_getNStars(C_Material *self)
{
  PyObject *attr = PyInt_FromLong((long)self->material->starc);

  if (attr) return attr;

  return EXPP_ReturnPyObjError (PyExc_RuntimeError,
                       "couldn't get Material.nStars attribute");
}

static PyObject *Material_getNLines(C_Material *self)
{
  PyObject *attr = PyInt_FromLong((long)self->material->linec);

  if (attr) return attr;

  return EXPP_ReturnPyObjError (PyExc_RuntimeError,
                       "couldn't get Material.nLines attribute");
}

static PyObject *Material_getNRings(C_Material *self)
{
  PyObject *attr = PyInt_FromLong((long)self->material->ringc);

  if (attr) return attr;

  return EXPP_ReturnPyObjError (PyExc_RuntimeError,
                       "couldn't get Material.nRings attribute");
}

static PyObject *Material_setName(C_Material *self, PyObject *args)
{
  char *name;
  char buf[21];

  if (!PyArg_ParseTuple(args, "s", &name))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
                                     "expected string argument"));

  PyOS_snprintf(buf, sizeof(buf), "%s", name);

  rename_id(&self->material->id, buf);

  Py_INCREF(Py_None);
  return Py_None;
}

/* Possible modes are traceable, shadow, shadeless, wire, vcolLight,
 * vcolPaint, halo, ztransp, zinvert, haloRings, env, haloLines,
 * onlyShadow, xalpha, star, faceTexture, haloTex, haloPuno, noMist,
 * haloShade, haloFlare */
static PyObject *Material_setMode(C_Material *self, PyObject *args)
{
  int i, flag = 0;
  char *m[21] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL,
					       NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	               NULL, NULL, NULL, NULL, NULL, NULL, NULL};

  if (!PyArg_ParseTuple(args, "|sssssssssssssssssssss",
					&m[0], &m[1], &m[2], &m[3],  &m[4],  &m[5],  &m[6],
					&m[7], &m[8], &m[9], &m[10], &m[11], &m[12], &m[13],
					&m[14], &m[15], &m[16], &m[17], &m[18], &m[19], &m[20]))
	{
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
            "expected from none to 21 string argument(s)"));
	}

  for (i = 0; i < 21; i++) {
    if (m[i] == NULL) break;
    if (strcmp(m[i], "Traceable") == 0)
      flag |= (short)EXPP_MAT_MODE_TRACEABLE;
    else if (strcmp(m[i], "Shadow") == 0)
      flag |= (short)EXPP_MAT_MODE_SHADOW;
    else if (strcmp(m[i], "Shadeless") == 0)
      flag |= (short)EXPP_MAT_MODE_SHADELESS;
    else if (strcmp(m[i], "Wire") == 0)
      flag |= (short)EXPP_MAT_MODE_WIRE;
    else if (strcmp(m[i], "VColLight") == 0)
      flag |= (short)EXPP_MAT_MODE_VCOLLIGHT;
    else if (strcmp(m[i], "VColPaint") == 0)
      flag |= (short)EXPP_MAT_MODE_VCOLPAINT;
    else if (strcmp(m[i], "Halo") == 0)
      flag |= (short)EXPP_MAT_MODE_HALO;
    else if (strcmp(m[i], "ZTransp") == 0)
      flag |= (short)EXPP_MAT_MODE_ZTRANSP;
    else if (strcmp(m[i], "ZInvert") == 0)
      flag |= (short)EXPP_MAT_MODE_ZINVERT;
    else if (strcmp(m[i], "HaloRings") == 0)
      flag |= (short)EXPP_MAT_MODE_HALORINGS;
    else if (strcmp(m[i], "Env") == 0)
      flag |= (short)EXPP_MAT_MODE_ENV;
    else if (strcmp(m[i], "HaloLines") == 0)
      flag |= (short)EXPP_MAT_MODE_HALOLINES;
    else if (strcmp(m[i], "OnlyShadow") == 0)
      flag |= (short)EXPP_MAT_MODE_ONLYSHADOW;
    else if (strcmp(m[i], "XAlpha") == 0)
      flag |= (short)EXPP_MAT_MODE_XALPHA;
    else if (strcmp(m[i], "Star") == 0)
      flag |= (short)EXPP_MAT_MODE_STAR;
    else if (strcmp(m[i], "FaceTex") == 0)
      flag |= (short)EXPP_MAT_MODE_FACETEX;
    else if (strcmp(m[i], "HaloTex") == 0)
      flag |= (short)EXPP_MAT_MODE_HALOTEX;
    else if (strcmp(m[i], "HaloPuno") == 0)
      flag |= (short)EXPP_MAT_MODE_HALOPUNO;
    else if (strcmp(m[i], "NoMist") == 0)
      flag |= (short)EXPP_MAT_MODE_NOMIST;
    else if (strcmp(m[i], "HaloShade") == 0)
      flag |= (short)EXPP_MAT_MODE_HALOSHADE;
    else if (strcmp(m[i], "HaloFlare") == 0)
      flag |= (short)EXPP_MAT_MODE_HALOFLARE;
    else
      return (EXPP_ReturnPyObjError (PyExc_AttributeError,
              "unknown Material mode argument"));
  }

  self->material->mode = flag;

  Py_INCREF(Py_None);
  return Py_None;
}

/* Another helper function, for the same reason.
 * (See comment before Material_setIntType above). */
static PyObject *Material_setIntMode(C_Material *self, PyObject *args)
{
  int value;

  if (!PyArg_ParseTuple(args, "i", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
                                     "expected int argument"));

  self->material->mode = value;

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Material_setRGBCol (C_Material *self, PyObject *args)
{
	return rgbTuple_setCol(self->rgb, args);
}

static PyObject *Material_setAmbCol (C_Material *self, PyObject *args)
{
	return rgbTuple_setCol(self->amb, args);
}

static PyObject *Material_setSpecCol (C_Material *self, PyObject *args)
{
	return rgbTuple_setCol(self->spec, args);
}

static PyObject *Material_setMirCol (C_Material *self, PyObject *args)
{
	return rgbTuple_setCol(self->mir, args);
}

static PyObject *Material_setColorComponent(C_Material *self, char *key,
								PyObject *args)
{ /* for compatibility with old bpython */
  float value;

  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument in [0.0, 1.0]"));

  value = EXPP_ClampFloat (value, EXPP_MAT_COL_MIN,
									EXPP_MAT_COL_MAX);

  if (!strcmp(key, "R"))
    self->material->r = value;
  else if (!strcmp(key, "G"))
    self->material->g = value;
  else if (!strcmp(key, "B"))
    self->material->b = value;

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setAmb(C_Material *self, PyObject *args)
{
  float value;

  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument in [0.0, 1.0]"));

  self->material->amb = EXPP_ClampFloat (value, EXPP_MAT_AMB_MIN,
									EXPP_MAT_AMB_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setEmit(C_Material *self, PyObject *args)
{
  float value;

  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument in [0.0, 1.0]"));

  self->material->emit = EXPP_ClampFloat (value, EXPP_MAT_EMIT_MIN,
									EXPP_MAT_EMIT_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setAng(C_Material *self, PyObject *args)
{
  float value;

  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument in [0.0, 1.0]"));

  self->material->ang = EXPP_ClampFloat (value, EXPP_MAT_ANG_MIN,
									EXPP_MAT_ANG_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setSpecTransp(C_Material *self, PyObject *args)
{
  float value;

  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument in [0.0, 1.0]"));

  self->material->spectra = EXPP_ClampFloat (value, EXPP_MAT_SPECTRA_MIN,
									EXPP_MAT_SPECTRA_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setAlpha(C_Material *self, PyObject *args)
{
  float value;

  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument in [0.0, 1.0]"));

  self->material->alpha = EXPP_ClampFloat (value, EXPP_MAT_ALPHA_MIN,
									EXPP_MAT_ALPHA_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setRef(C_Material *self, PyObject *args)
{
  float value;

  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument in [0.0, 1.0]"));

  self->material->ref = EXPP_ClampFloat (value, EXPP_MAT_REF_MIN,
									EXPP_MAT_REF_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setSpec(C_Material *self, PyObject *args)
{
  float value;

  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument in [0.0, 1.0]"));

  self->material->spec = EXPP_ClampFloat (value, EXPP_MAT_SPEC_MIN,
									EXPP_MAT_SPEC_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setZOffset(C_Material *self, PyObject *args)
{
  float value;

  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument in [0.0, 10.0]"));

  self->material->zoffs = EXPP_ClampFloat (value, EXPP_MAT_ZOFFS_MIN,
									EXPP_MAT_ZOFFS_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setAdd(C_Material *self, PyObject *args)
{
  float value;

  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument in [0.0, 1.0]"));

  self->material->add = EXPP_ClampFloat (value, EXPP_MAT_ADD_MIN,
									EXPP_MAT_ADD_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setHaloSize(C_Material *self, PyObject *args)
{
	float value;

  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument in [0.0, 100.0]"));

  self->material->hasize = EXPP_ClampFloat (value, EXPP_MAT_HALOSIZE_MIN,
									EXPP_MAT_HALOSIZE_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setFlareSize(C_Material *self, PyObject *args)
{
	 float value;

  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument in [0.1, 25.0]"));

  self->material->flaresize = EXPP_ClampFloat (value, EXPP_MAT_FLARESIZE_MIN,
									EXPP_MAT_FLARESIZE_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setFlareBoost(C_Material *self, PyObject *args)
{
	 float value;

  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument in [0.1, 10.0]"));

  self->material->flareboost = EXPP_ClampFloat(value, EXPP_MAT_FLAREBOOST_MIN,
									EXPP_MAT_FLAREBOOST_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setSubSize(C_Material *self, PyObject *args)
{
	 float value;

  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument in [0.1, 25.0]"));

  self->material->subsize = EXPP_ClampFloat (value, EXPP_MAT_SUBSIZE_MIN,
									EXPP_MAT_SUBSIZE_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setHardness(C_Material *self, PyObject *args)
{
	short value;

  if (!PyArg_ParseTuple(args, "h", &value))			 
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected int argument in [1, 255]"));

  self->material->har = EXPP_ClampInt (value, EXPP_MAT_HARD_MIN,
									EXPP_MAT_HARD_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setNFlares(C_Material *self, PyObject *args)
{
	short value;

  if (!PyArg_ParseTuple(args, "h", &value))			 
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected int argument in [1, 32]"));

  self->material->flarec = EXPP_ClampInt (value, EXPP_MAT_NFLARES_MIN,
									EXPP_MAT_NFLARES_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setNStars(C_Material *self, PyObject *args)
{
	short value;

  if (!PyArg_ParseTuple(args, "h", &value))			 
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected int argument in [3, 50]"));

  self->material->starc = EXPP_ClampInt (value, EXPP_MAT_NSTARS_MIN,
									EXPP_MAT_NSTARS_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setNLines(C_Material *self, PyObject *args)
{
	short value;

  if (!PyArg_ParseTuple(args, "h", &value))			 
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected int argument in [0, 250]"));

  self->material->linec = EXPP_ClampInt (value, EXPP_MAT_NLINES_MIN,
									EXPP_MAT_NLINES_MAX);

	return EXPP_incr_ret (Py_None);
}

static PyObject *Material_setNRings(C_Material *self, PyObject *args)
{
	short value;

  if (!PyArg_ParseTuple(args, "h", &value))			 
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected int argument in [0, 24]"));

  self->material->ringc = EXPP_ClampInt (value, EXPP_MAT_NRINGS_MIN,
									EXPP_MAT_NRINGS_MAX);

	return EXPP_incr_ret (Py_None);
}

/*****************************************************************************/
/* Function:    Material_createPyObject                                      */
/* Description: This function will create a new C_Material from an existing  */
/*              Blender material structure.                                  */
/*****************************************************************************/
static PyObject *Material_createPyObject (Material *mat)
{
	C_Material *pymat;
	float *rgb[3], *amb[3], *spec[3], *mir[3];

	pymat = (C_Material *)PyObject_NEW (C_Material, &Material_Type);

	if (!pymat)
		return EXPP_ReturnPyObjError (PyExc_MemoryError,
						"couldn't create C_Material object");

	pymat->material = mat;

	rgb[0] = &mat->r;
	rgb[1] = &mat->g;
	rgb[2] = &mat->b;

	amb[0] = &mat->ambr;
	amb[1] = &mat->ambg;
	amb[2] = &mat->ambb;

	spec[0] = &mat->specr;
	spec[1] = &mat->specg;
	spec[2] = &mat->specb;

	mir[0] = &mat->mirr;
	mir[1] = &mat->mirg;
	mir[2] = &mat->mirb;

	pymat->rgb  = (C_rgbTuple *)rgbTuple_New(rgb);
	pymat->amb  = (C_rgbTuple *)rgbTuple_New(amb);
	pymat->spec = (C_rgbTuple *)rgbTuple_New(spec);
	pymat->mir  = (C_rgbTuple *)rgbTuple_New(mir);

	return (PyObject *)pymat;
}

/*****************************************************************************/
/* Function:    MaterialDeAlloc                                              */
/* Description: This is a callback function for the C_Material type. It is   */
/*              the destructor function.                                     */
/*****************************************************************************/
static void MaterialDeAlloc (C_Material *self)
{
  PyObject_DEL (self);
}

/*****************************************************************************/
/* Function:    MaterialGetAttr                                              */
/* Description: This is a callback function for the C_Material type. It is   */
/*              the function that accesses C_Material "member variables" and */
/*              methods.                                                     */
/*****************************************************************************/
static PyObject *MaterialGetAttr (C_Material *self, char *name)
{
  PyObject *attr = Py_None;

  if (strcmp(name, "name") == 0)
    attr = PyString_FromString(self->material->id.name+2);
  else if (strcmp(name, "mode") == 0)
    attr = PyInt_FromLong(self->material->mode);
  else if (strcmp(name, "rgbCol") == 0)
    attr = Material_getRGBCol(self);
  else if (strcmp(name, "ambCol") == 0)
    attr = Material_getAmbCol(self);
  else if (strcmp(name, "specCol") == 0)
    attr = Material_getSpecCol(self);
  else if (strcmp(name, "mirCol") == 0)
    attr = Material_getMirCol(self);
  else if (strcmp(name, "R") == 0)
    attr = PyFloat_FromDouble((double)self->material->r);
  else if (strcmp(name, "G") == 0)
    attr = PyFloat_FromDouble((double)self->material->g);
  else if (strcmp(name, "B") == 0)
    attr = PyFloat_FromDouble((double)self->material->b);
  else if (strcmp(name, "amb") == 0)
    attr = PyFloat_FromDouble((double)self->material->amb);
  else if (strcmp(name, "ang") == 0)
    attr = PyFloat_FromDouble((double)self->material->ang);
  else if (strcmp(name, "emit") == 0)
    attr = PyFloat_FromDouble((double)self->material->emit);
  else if (strcmp(name, "alpha") == 0)
    attr = PyFloat_FromDouble((double)self->material->alpha);
  else if (strcmp(name, "ref") == 0)
    attr = PyFloat_FromDouble((double)self->material->ref);
  else if (strcmp(name, "spec") == 0)
    attr = PyFloat_FromDouble((double)self->material->spec);
  else if (strcmp(name, "specTransp") == 0)
    attr = PyFloat_FromDouble((double)self->material->spectra);
  else if (strcmp(name, "add") == 0)
    attr = PyFloat_FromDouble((double)self->material->add);
  else if (strcmp(name, "zOffset") == 0)
    attr = PyFloat_FromDouble((double)self->material->zoffs);
  else if (strcmp(name, "haloSize") == 0)
    attr = PyFloat_FromDouble((double)self->material->hasize);
  else if (strcmp(name, "flareSize") == 0)
    attr = PyFloat_FromDouble((double)self->material->flaresize);
  else if (strcmp(name, "flareBoost") == 0)
    attr = PyFloat_FromDouble((double)self->material->flareboost);
  else if (strcmp(name, "subSize") == 0)
    attr = PyFloat_FromDouble((double)self->material->subsize);
  else if (strcmp(name, "hard") == 0)
    attr = PyInt_FromLong((long)self->material->har);
  else if (strcmp(name, "nFlares") == 0)
    attr = PyInt_FromLong((long)self->material->flarec);
  else if (strcmp(name, "nStars") == 0)
    attr = PyInt_FromLong((long)self->material->starc);
  else if (strcmp(name, "nLines") == 0)
    attr = PyInt_FromLong((long)self->material->linec);
  else if (strcmp(name, "nRings") == 0)
    attr = PyInt_FromLong((long)self->material->ringc);

  else if (strcmp(name, "__members__") == 0) {
    attr = /* 27 items */
		  Py_BuildValue("[s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s]",
                    "name", "mode", "rgbCol", "ambCol", "specCol", "mirCol",
										"R", "G", "B", "alpha", "amb", "ang", "emit", "ref",
										"spec", "specTransp", "add", "zOffset", "haloSize",
										"flareSize", "flareBoost", "subSize", "hard", "nFlares",
										"nStars", "nLines", "nRings");
  }

  if (!attr)
    return (EXPP_ReturnPyObjError (PyExc_MemoryError,
                      "couldn't create PyObject"));

  if (attr != Py_None) return attr; /* member attribute found, return it */

  /* not an attribute, search the methods table */
  return Py_FindMethod(C_Material_methods, (PyObject *)self, name);
}

/****************************************************************************/
/* Function:    MaterialSetAttr                                             */
/* Description: This is a callback function for the C_Material type.        */
/*              It is the function that sets Material attributes (member    */
/*              variables).                                                 */
/****************************************************************************/
static int MaterialSetAttr (C_Material *self, char *name, PyObject *value)
{
  PyObject *valtuple; 
  PyObject *error = NULL;

/* We're playing a trick on the Python API users here.  Even if they use
 * Material.member = val instead of Material.setMember(val), we end up using the
 * function anyway, since it already has error checking, clamps to the right
 * interval and updates the Blender Material structure when necessary. */

/* First we put "value" in a tuple, because we want to pass it to functions
 * that only accept PyTuples. Using "N" doesn't increment value's ref count */
  valtuple = Py_BuildValue("(N)", value);

  if (!valtuple) /* everything OK with our PyObject? */
    return EXPP_ReturnIntError(PyExc_MemoryError,
                         "MaterialSetAttr: couldn't create PyTuple");

/* Now we just compare "name" with all possible C_Material member variables */
  if (strcmp (name, "name") == 0)
    error = Material_setName (self, valtuple);
  else if (strcmp (name, "mode") == 0)
    error = Material_setIntMode (self, valtuple); /* special case */
  else if (strcmp (name, "rgbCol") == 0)
    error = Material_setRGBCol (self, valtuple);
  else if (strcmp (name, "ambCol") == 0)
    error = Material_setAmbCol (self, valtuple);
  else if (strcmp (name, "specCol") == 0)
    error = Material_setSpecCol (self, valtuple);
  else if (strcmp (name, "mirCol") == 0)
    error = Material_setMirCol (self, valtuple);
  else if (strcmp (name, "R") == 0)
    error = Material_setColorComponent (self, "R", valtuple);
  else if (strcmp (name, "G") == 0)
    error = Material_setColorComponent (self, "G", valtuple);
  else if (strcmp (name, "B") == 0)
    error = Material_setColorComponent (self, "B", valtuple);
  else if (strcmp (name, "amb") == 0)
    error = Material_setAmb (self, valtuple);
  else if (strcmp (name, "ang") == 0)
    error = Material_setAng (self, valtuple);
  else if (strcmp (name, "emit") == 0)
    error = Material_setEmit (self, valtuple);
  else if (strcmp (name, "alpha") == 0)
    error = Material_setAlpha (self, valtuple);
  else if (strcmp (name, "ref") == 0)
    error = Material_setRef (self, valtuple);
  else if (strcmp (name, "spec") == 0)
    error = Material_setSpec (self, valtuple);
  else if (strcmp (name, "specTransp") == 0)
    error = Material_setSpecTransp (self, valtuple);
  else if (strcmp (name, "add") == 0)
    error = Material_setAdd (self, valtuple);
  else if (strcmp (name, "zOffset") == 0)
    error = Material_setZOffset (self, valtuple);
  else if (strcmp (name, "haloSize") == 0)
    error = Material_setHaloSize (self, valtuple);
  else if (strcmp (name, "flareSize") == 0)
    error = Material_setFlareSize (self, valtuple);
  else if (strcmp (name, "flareBoost") == 0)
    error = Material_setFlareBoost (self, valtuple);
  else if (strcmp (name, "subSize") == 0)
    error = Material_setSubSize (self, valtuple);
  else if (strcmp (name, "hard") == 0)
    error = Material_setHardness (self, valtuple);
  else if (strcmp (name, "nFlares") == 0)
    error = Material_setNFlares (self, valtuple);
  else if (strcmp (name, "nStars") == 0)
    error = Material_setNStars (self, valtuple);
  else if (strcmp (name, "nLines") == 0)
    error = Material_setNLines (self, valtuple);
  else if (strcmp (name, "nRings") == 0)
    error = Material_setNRings (self, valtuple);

  else { /* Error */
    Py_DECREF(valtuple);
    return (EXPP_ReturnIntError (PyExc_AttributeError, name));
  }

/* valtuple won't be returned to the caller, so we need to DECREF it */
  Py_DECREF(valtuple);

  if (error != Py_None) return -1;

/* Py_None was incref'ed by the called Material_set* function. We probably
 * don't need to decref Py_None (!), but since Python/C API manual tells us
 * to treat it like any other PyObject regarding ref counting ... */
  Py_DECREF(Py_None);
  return 0; /* normal exit */
}

/*****************************************************************************/
/* Function:    MaterialPrint                                                */
/* Description: This is a callback function for the C_Material type. It      */
/*              builds a meaninful string to 'print' material objects.       */
/*****************************************************************************/
static int MaterialPrint(C_Material *self, FILE *fp, int flags)
{ 
  fprintf(fp, "[Material \"%s\"]", self->material->id.name+2);
  return 0;
}

/*****************************************************************************/
/* Function:    MaterialRepr                                                 */
/* Description: This is a callback function for the C_Material type. It      */
/*              builds a meaninful string to represent material objects.     */
/*****************************************************************************/
static PyObject *MaterialRepr (C_Material *self)
{
	char buf[40];

	PyOS_snprintf(buf, sizeof(buf), "[Material \"%s\"]",
									                self->material->id.name+2);

	return PyString_FromString(buf);
}
