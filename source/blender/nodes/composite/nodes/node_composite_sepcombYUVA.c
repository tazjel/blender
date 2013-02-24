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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_sepcombYUVA.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"


/* **************** SEPARATE YUVA ******************** */
static bNodeSocketTemplate cmp_node_sepyuva_in[] = {
	{  SOCK_RGBA, 1, N_("Image"),        1.0f, 1.0f, 1.0f, 1.0f},
	{  -1, 0, ""   }
};
static bNodeSocketTemplate cmp_node_sepyuva_out[] = {
	{  SOCK_FLOAT, 0, N_("Y")},
	{  SOCK_FLOAT, 0, N_("U")},
	{  SOCK_FLOAT, 0, N_("V")},
	{  SOCK_FLOAT, 0, N_("A")},
	{  -1, 0, ""   }
};


void register_node_type_cmp_sepyuva(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_SEPYUVA, "Separate YUVA", NODE_CLASS_CONVERTOR, 0);
	node_type_socket_templates(&ntype, cmp_node_sepyuva_in, cmp_node_sepyuva_out);

	nodeRegisterType(ttype, &ntype);
}



/* **************** COMBINE YUVA ******************** */
static bNodeSocketTemplate cmp_node_combyuva_in[] = {
	{	SOCK_FLOAT, 1, N_("Y"),			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
	{	SOCK_FLOAT, 1, N_("U"),			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
	{	SOCK_FLOAT, 1, N_("V"),			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
	{	SOCK_FLOAT, 1, N_("A"),			1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate cmp_node_combyuva_out[] = {
	{	SOCK_RGBA, 0, N_("Image")},
	{	-1, 0, ""	}
};

void register_node_type_cmp_combyuva(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_COMBYUVA, "Combine YUVA", NODE_CLASS_CONVERTOR, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_combyuva_in, cmp_node_combyuva_out);

	nodeRegisterType(ttype, &ntype);
}
