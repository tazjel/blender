/**
 *
 * $Id: BOP_Misc.h 14444 2008-04-16 22:40:48Z khughes $
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
 * Contributor(s): Ken Hughes
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
/*
 * This file contains various definitions used across the modules
 */

/*
 * define operator>> for faces, edges and vertices, and also add some
 * debugging functions for displaying various internal data structures
 */

// #define	BOP_DEBUG

#define HASH(x) ((x) >> 5)		/* each "hash" covers 32 indices */
// #define HASH_PRINTF_DEBUG	/* uncomment to enable debug output */
