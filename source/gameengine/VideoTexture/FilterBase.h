/* $Id$
-----------------------------------------------------------------------------
This source file is part of VideoTexture library

Copyright (c) 2007 The Zdeno Ash Miklas

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place - Suite 330, Boston, MA 02111-1307, USA, or go to
http://www.gnu.org/copyleft/lesser.txt.
-----------------------------------------------------------------------------
*/

#if !defined FILTERBASE_H
#define FILTERBASE_H

#include "Common.h"

#include <PyObjectPlus.h>

#include "PyTypeList.h"


// forward declaration
class FilterBase;


// python structure for filter
struct PyFilter
{
	PyObject_HEAD
	// source object
	FilterBase * m_filter;
};


/// base class for pixel filters
class FilterBase
{
public:
	/// constructor
	FilterBase (void);
	/// destructor
	virtual ~FilterBase (void);
	// release python objects
	virtual void release (void);

	/// convert pixel
	template <class SRC> unsigned int convert (SRC src, short x, short y,
		short * size, unsigned int pixSize)
	{
		return filter(src, x, y, size, pixSize,
			convertPrevious(src, x, y, size, pixSize));
	}

	/// get previous filter
	PyFilter * getPrevious (void) { return m_previous; }
	/// set previous filter
	void setPrevious (PyFilter * filt, bool useRefCnt = true);

	/// find first filter in chain
	FilterBase * findFirst (void);

	/// get first filter's source pixel size
	unsigned int firstPixelSize (void) { return findFirst()->getPixelSize(); }

protected:
	/// previous pixel filter
	PyFilter * m_previous;

	/// filter pixel, source byte buffer
	virtual unsigned int filter (unsigned char * src, short x, short y,
		short * size, unsigned int pixSize, unsigned int val = 0)
	{ return val; }
	/// filter pixel, source int buffer
	virtual unsigned int filter (unsigned int * src, short x, short y,
		short * size, unsigned int pixSize, unsigned int val = 0)
	{ return val; }

	/// get source pixel size
	virtual unsigned int getPixelSize (void) { return 1; }

	/// get converted pixel from previous filters
	template <class SRC> unsigned int convertPrevious (SRC src, short x, short y,
		short * size, unsigned int pixSize)
	{
		// if previous filter doesn't exists, return source pixel
		if (m_previous == NULL) return *src;
		// otherwise return converted pixel
		return m_previous->m_filter->convert(src, x, y, size, pixSize);
	}
};


// list of python filter types
extern PyTypeList pyFilterTypes;


// functions for python interface

// object initialization
template <class T> static int Filter_init (PyObject * pySelf, PyObject * args, PyObject * kwds)
{
	PyFilter * self = reinterpret_cast<PyFilter*>(pySelf);
	// create filter object
	if (self->m_filter != NULL) delete self->m_filter;
	self->m_filter = new T();
	// initialization succeded
	return 0;
}

// object allocation
PyObject * Filter_allocNew (PyTypeObject * type, PyObject * args, PyObject * kwds);
// object deallocation
void Filter_dealloc (PyFilter * self);

// get previous pixel filter object
PyObject * Filter_getPrevious (PyFilter * self, void * closure);
// set previous pixel filter object
int Filter_setPrevious (PyFilter * self, PyObject * value, void * closure);


#endif
