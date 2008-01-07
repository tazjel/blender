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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef NO_EXP_PYTHON_EMBEDDING

#ifndef _adr_py_lib_h_				// only process once,
#define _adr_py_lib_h_				// even if multiply included

#ifndef __cplusplus				// c++ only
#error Must be compiled with C++
#endif

#include "KX_Python.h"
#include "STR_String.h"

/*------------------------------
 * Python defines
------------------------------*/

								// some basic python macros
#define Py_NEWARGS 1			
#define Py_Return { Py_INCREF(Py_None); return Py_None;}
static inline PyObject* Py_Success(bool truth)
{
	if (truth)
	{
		Py_INCREF(Py_True);
		return Py_True;
	}
	Py_INCREF(Py_False);
	return Py_False;
}

#define Py_Error(E, M)   {PyErr_SetString(E, M); return NULL;}
#define Py_Try(F) {if (!(F)) return NULL;}
#define Py_Assert(A,E,M) {if (!(A)) {PyErr_SetString(E, M); return NULL;}}

static inline void Py_Fatal(char *M) {
	//cout << M << endl; 
	exit(-1);
};

								// This must be the first line of each 
								// PyC++ class
#define Py_Header \
 public: \
  static PyTypeObject   Type; \
  static PyMethodDef    Methods[]; \
  static PyParentObject Parents[]; \
  virtual PyTypeObject *GetType(void) {return &Type;}; \
  virtual PyParentObject *GetParents(void) {return Parents;}

								// This defines the _getattr_up macro
								// which allows attribute and method calls
								// to be properly passed up the hierarchy.
#define _getattr_up(Parent) \
  PyObject *rvalue = Py_FindMethod(Methods, this, const_cast<char*>(attr.ReadPtr())); \
  if (rvalue == NULL) \
    { \
      PyErr_Clear(); \
      return Parent::_getattr(attr); \
    } \
  else \
    return rvalue 

/**
 * These macros are helpfull when embedding Python routines. The second
 * macro is one that also requires a documentation string
 */
#define KX_PYMETHOD(class_name, method_name)			\
	PyObject* Py##method_name(PyObject* self, PyObject* args, PyObject* kwds); \
	static PyObject* sPy##method_name( PyObject* self, PyObject* args, PyObject* kwds) { \
		return ((class_name*) self)->Py##method_name(self, args, kwds);		\
	}; \

#define KX_PYMETHOD_DOC(class_name, method_name)			\
	PyObject* Py##method_name(PyObject* self, PyObject* args, PyObject* kwds); \
	static PyObject* sPy##method_name( PyObject* self, PyObject* args, PyObject* kwds) { \
		return ((class_name*) self)->Py##method_name(self, args, kwds);		\
	}; \
    static char method_name##_doc[]; \

/* The line above should remain empty */
/**
 * Method table macro (with doc)
 */
#define KX_PYMETHODTABLE(class_name, method_name) \
	{#method_name , (PyCFunction) class_name::sPy##method_name, METH_VARARGS, class_name::method_name##_doc}

/**
 * Function implementation macro
 */
#define KX_PYMETHODDEF_DOC(class_name, method_name, doc_string) \
char class_name::method_name##_doc[] = doc_string; \
PyObject* class_name::Py##method_name(PyObject*, PyObject* args, PyObject*)


/*------------------------------
 * PyObjectPlus
------------------------------*/
typedef PyTypeObject * PyParentObject;				// Define the PyParent Object

class PyObjectPlus : public PyObject 
{				// The PyObjectPlus abstract class
	Py_Header;							// Always start with Py_Header
	
public:
	PyObjectPlus(PyTypeObject *T);
	
	virtual ~PyObjectPlus();					// destructor
	static void PyDestructor(PyObject *P)				// python wrapper
	{  
		delete ((PyObjectPlus *) P);  
	};
	
//	void INCREF(void) {
//		  Py_INCREF(this);
//	  };				// incref method
//	void DECREF(void) {
//		  Py_DECREF(this);
//	  };				// decref method
	
	virtual PyObject *_getattr(const STR_String& attr);			// _getattr method
	static  PyObject *__getattr(PyObject * PyObj, char *attr) 	// This should be the entry in Type. 
	{
		return ((PyObjectPlus*) PyObj)->_getattr(STR_String(attr)); 
	}
	
	virtual int _delattr(const STR_String& attr);
	virtual int _setattr(const STR_String& attr, PyObject *value);		// _setattr method
	static  int __setattr(PyObject *PyObj, 			// This should be the entry in Type. 
				char *attr, 
				PyObject *value)
	{ 
		if (!value)
			return ((PyObjectPlus*) PyObj)->_delattr(attr);
		return ((PyObjectPlus*) PyObj)->_setattr(STR_String(attr), value);  
	}
	
	virtual PyObject *_repr(void);				// _repr method
	static  PyObject *__repr(PyObject *PyObj)			// This should be the entry in Type.
	{
		return ((PyObjectPlus*) PyObj)->_repr();  
	}
	
									// isA methods
	bool isA(PyTypeObject *T);
	bool isA(const char *mytypename);
	PyObject *Py_isA(PyObject *args);
	static PyObject *sPy_isA(PyObject *self, PyObject *args, PyObject *kwd)
	{
		return ((PyObjectPlus*)self)->Py_isA(args);
	}
};

#endif //  _adr_py_lib_h_

#endif //NO_EXP_PYTHON_EMBEDDING

