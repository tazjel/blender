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
 * Contributor(s): Matt Ebb.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_colorbalance.c
 *  \ingroup cmpnodes
 */



#include "node_composite_util.h"


/* ******************* Color Balance ********************************* */
static bNodeSocketTemplate cmp_node_colorbalance_in[] = {
	{SOCK_FLOAT, 1, N_("Fac"),	1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
	{SOCK_RGBA, 1, N_("Image"), 1.0f, 1.0f, 1.0f, 1.0f},
	{-1, 0, ""}
};

static bNodeSocketTemplate cmp_node_colorbalance_out[] = {
	{SOCK_RGBA, 0, N_("Image")},
	{-1, 0, ""}
};

static void node_composit_init_colorbalance(bNodeTree *UNUSED(ntree), bNode *node)
{
	NodeColorBalance *n = node->storage = MEM_callocN(sizeof(NodeColorBalance), "node colorbalance");

	n->lift[0] = n->lift[1] = n->lift[2] = 1.0f;
	n->gamma[0] = n->gamma[1] = n->gamma[2] = 1.0f;
	n->gain[0] = n->gain[1] = n->gain[2] = 1.0f;
}

void register_node_type_cmp_colorbalance(void)
{
	static bNodeType ntype;

	cmp_node_type_base(&ntype, CMP_NODE_COLORBALANCE, "Color Balance", NODE_CLASS_OP_COLOR, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_colorbalance_in, cmp_node_colorbalance_out);
	node_type_size(&ntype, 400, 200, 400);
	node_type_init(&ntype, node_composit_init_colorbalance);
	node_type_storage(&ntype, "NodeColorBalance", node_free_standard_storage, node_copy_standard_storage);

	nodeRegisterType(&ntype);
}
