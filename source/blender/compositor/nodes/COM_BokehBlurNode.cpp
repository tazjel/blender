/*
 * Copyright 2011, Blender Foundation.
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
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#include "COM_BokehBlurNode.h"
#include "DNA_scene_types.h"
#include "DNA_camera_types.h"
#include "DNA_object_types.h"
#include "DNA_node_types.h"
#include "COM_ExecutionSystem.h"
#include "COM_BokehBlurOperation.h"
#include "COM_VariableSizeBokehBlurOperation.h"
#include "COM_ConvertDepthToRadiusOperation.h"

BokehBlurNode::BokehBlurNode(bNode *editorNode): Node(editorNode) {
}

void BokehBlurNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
	bNode *node = this->getbNode();
	Scene *scene= (Scene*)node->id;
	Object* camob = (scene)? scene->camera: NULL;

	if (this->getInputSocket(2)->isConnected()) {
		VariableSizeBokehBlurOperation *operation = new VariableSizeBokehBlurOperation();
		ConvertDepthToRadiusOperation *converter = new ConvertDepthToRadiusOperation();
		converter->setCameraObject(camob);
		this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), true, 0, graph);
		this->getInputSocket(1)->relinkConnections(operation->getInputSocket(1), true, 1, graph);
		this->getInputSocket(2)->relinkConnections(converter->getInputSocket(0), true, 2, graph);
		addLink(graph, converter->getOutputSocket(), operation->getInputSocket(2));
		graph->addOperation(operation);
		graph->addOperation(converter);
		this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket());
	} else {
		BokehBlurOperation *operation = new BokehBlurOperation();
		this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), true, 0, graph);
		this->getInputSocket(1)->relinkConnections(operation->getInputSocket(1), true, 1, graph);
		this->getInputSocket(3)->relinkConnections(operation->getInputSocket(2), true, 3, graph);
		operation->setSize(this->getInputSocket(2)->getbNodeSocket()->ns.vec[0]);
		operation->setQuality(context->getQuality());
		graph->addOperation(operation);
		this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket());
	}
}
