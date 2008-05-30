/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __GPU_CODEGEN_H__
#define __GPU_CODEGEN_H__

#include "DNA_listBase.h"

struct ListBase;
struct GPUShader;
struct GPUOutput;
struct GPUNode;
struct GPUVertexAttribs;

#define MAX_FUNCTION_NAME	64
#define MAX_PARAMETER		32

typedef struct GPUFunction {
	char name[MAX_FUNCTION_NAME];
	int paramtype[MAX_PARAMETER];
	int paramout[MAX_PARAMETER];
	int totparam;
} GPUFunction;

GPUFunction *GPU_lookup_function(char *name);

/* Pass Generation
   - Takes a list of nodes and a desired output, and makes a pass. This
     will take ownership of the nodes and free them early if unused or
	 at the end if used.
*/

struct GPUPass;
typedef struct GPUPass GPUPass;

GPUPass *GPU_generate_pass(ListBase *nodes, struct GPUNodeLink *outlink, int vertexshader);

struct GPUShader *GPU_pass_shader(GPUPass *pass);

void GPU_pass_bind(GPUPass *pass);
void GPU_pass_unbind(GPUPass *pass);

void GPU_pass_free(GPUPass *pass);

/* Node Functions */

GPUNodeLink *GPU_node_link_create(int type);

GPUNode *GPU_node_begin(char *name);
void GPU_node_input(GPUNode *node, int type, char *name, void *p1, void *p2);
void GPU_node_input_array(GPUNode *node, int type, char *name, void *ptr1, void *ptr2, void *ptr3);
void GPU_node_output(GPUNode *node, int type, char *name, GPUNodeLink **link);
void GPU_node_end(GPUNode *node);

void GPU_node_free(GPUNode *node);
void GPU_nodes_free(ListBase *nodes);

void GPU_nodes_create_vertex_attributes(ListBase *nodes, struct GPUVertexAttribs *attribs);

/* Material calls */

void gpu_material_add_node(struct GPUMaterial *material, struct GPUNode *node);

#endif

