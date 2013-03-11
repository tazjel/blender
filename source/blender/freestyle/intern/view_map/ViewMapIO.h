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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __FREESTYLE_VIEW_MAP_IO_H__
#define __FREESTYLE_VIEW_MAP_IO_H__

/** \file blender/freestyle/intern/view_map/ViewMapIO.h
 *  \ingroup freestyle
 *  \brief Functions to manage I/O for the view map
 *  \author Emmanuel Turquin
 *  \date 09/01/2003
 */

#include <fstream>
#include <string>

#include "ViewMap.h"

#include "../system/FreestyleConfig.h"
#include "../system/ProgressBar.h"

namespace ViewMapIO {

static const unsigned ZERO = UINT_MAX;

LIB_VIEW_MAP_EXPORT
int load(istream& in, ViewMap *vm, ProgressBar *pb = NULL);

LIB_VIEW_MAP_EXPORT
int save(ostream& out, ViewMap *vm, ProgressBar *pb = NULL);

namespace Options {

static const unsigned char FLOAT_VECTORS = 1;
static const unsigned char NO_OCCLUDERS = 2;

LIB_VIEW_MAP_EXPORT
void setFlags(const unsigned char flags);

LIB_VIEW_MAP_EXPORT
void addFlags(const unsigned char flags);

LIB_VIEW_MAP_EXPORT
void rmFlags(const unsigned char flags);

LIB_VIEW_MAP_EXPORT
unsigned char getFlags();

LIB_VIEW_MAP_EXPORT
void setModelsPath(const string& path);

LIB_VIEW_MAP_EXPORT
string getModelsPath();

}; // End of namepace Options

#ifdef IRIX

namespace Internal {

template <unsigned S>
ostream& write(ostream& out, const char *str)
{
	out.put(str[S - 1]);
	return write<S - 1>(out, str);
}

template<>
ostream& write<1>(ostream& out, const char *str)
{
	return out.put(str[0]);
}

template<>
ostream& write<0>(ostream& out, const char *)
{
	return out;
}

template <unsigned S>
istream& read(istream& in, char *str)
{
	in.get(str[S - 1]);
	return read<S - 1>(in, str);
}

template<>
istream& read<1>(istream& in, char *str)
{
	return in.get(str[0]);
}

template<>
istream& read<0>(istream& in, char *)
{
	return in;
}

} // End of namespace Internal

#endif // IRIX

} // End of namespace ViewMapIO

#endif // __FREESTYLE_VIEW_MAP_IO_H__
