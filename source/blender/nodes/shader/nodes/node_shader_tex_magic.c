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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../node_shader_util.h"

/* **************** OUTPUT ******************** */

static bNodeSocketTemplate sh_node_tex_magic_in[]= {
	{	SOCK_VECTOR, 1, N_("Vector"),		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE, SOCK_HIDE_VALUE},
	{	SOCK_FLOAT, 1, N_("Scale"),			5.0f, 0.0f, 0.0f, 0.0f, -1000.0f, 1000.0f},
	{	SOCK_FLOAT, 1, N_("Distortion"),	1.0f, 0.0f, 0.0f, 0.0f, -1000.0f, 1000.0f},
	{	-1, 0, ""	}
};

static bNodeSocketTemplate sh_node_tex_magic_out[]= {
	{	SOCK_RGBA, 0, N_("Color"),		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	SOCK_FLOAT, 0, N_("Fac"),		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

static void node_shader_init_tex_magic(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeTexMagic *tex = MEM_callocN(sizeof(NodeTexMagic), "NodeTexMagic");
	default_tex_mapping(&tex->base.tex_mapping);
	default_color_mapping(&tex->base.color_mapping);
	tex->depth = 2;

	node->storage = tex;
}

static int node_shader_gpu_tex_magic(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *out)
{
	NodeTexMagic *tex = (NodeTexMagic*)node->storage;
	float depth = tex->depth;

	if (!in[0].link)
		in[0].link = GPU_attribute(CD_ORCO, "");

	node_shader_gpu_tex_mapping(mat, node, in, out);

	return GPU_stack_link(mat, "node_tex_magic", in, out, GPU_uniform(&depth));
}

/* node type definition */
void register_node_type_sh_tex_magic(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, SH_NODE_TEX_MAGIC, "Magic Texture", NODE_CLASS_TEXTURE, NODE_OPTIONS);
	node_type_compatibility(&ntype, NODE_NEW_SHADING);
	node_type_socket_templates(&ntype, sh_node_tex_magic_in, sh_node_tex_magic_out);
	node_type_size(&ntype, 150, 60, 200);
	node_type_init(&ntype, node_shader_init_tex_magic);
	node_type_storage(&ntype, "NodeTexMagic", node_free_standard_storage, node_copy_standard_storage);
	node_type_exec(&ntype, NULL);
	node_type_gpu(&ntype, node_shader_gpu_tex_magic);

	nodeRegisterType(ttype, &ntype);
}
