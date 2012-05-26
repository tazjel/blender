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

#include "COM_HueSaturationValueCorrectNode.h"

#include "COM_ConvertColourToValueProg.h"
#include "COM_ExecutionSystem.h"
#include "COM_ConvertRGBToHSVOperation.h"
#include "COM_ConvertHSVToRGBOperation.h"
#include "COM_MixBlendOperation.h"
#include "COM_SetColorOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_ChangeHSVOperation.h"
#include "DNA_node_types.h"
#include "COM_HueSaturationValueCorrectOperation.h"

HueSaturationValueCorrectNode::HueSaturationValueCorrectNode(bNode *editorNode): Node(editorNode)
{
}

void HueSaturationValueCorrectNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context)
{
	InputSocket *valueSocket = this->getInputSocket(0);
	InputSocket *colourSocket = this->getInputSocket(1);
	OutputSocket *outputSocket = this->getOutputSocket(0);
	bNode *editorsnode = getbNode();
	CurveMapping *storage = (CurveMapping*)editorsnode->storage;
	
	if (colourSocket->isConnected() && outputSocket->isConnected()) {
		ConvertRGBToHSVOperation * rgbToHSV = new ConvertRGBToHSVOperation();
		ConvertHSVToRGBOperation * hsvToRGB = new ConvertHSVToRGBOperation();
		HueSaturationValueCorrectOperation *changeHSV = new HueSaturationValueCorrectOperation();
		MixBlendOperation * blend = new MixBlendOperation();
	
		colourSocket->relinkConnections(rgbToHSV->getInputSocket(0), 1, graph);
		addLink(graph, rgbToHSV->getOutputSocket(), changeHSV->getInputSocket(0));
		addLink(graph, changeHSV->getOutputSocket(), hsvToRGB->getInputSocket(0));
		addLink(graph, hsvToRGB->getOutputSocket(), blend->getInputSocket(2));
		addLink(graph, rgbToHSV->getInputSocket(0)->getConnection()->getFromSocket(), blend->getInputSocket(1));
		valueSocket->relinkConnections(blend->getInputSocket(0), 0, graph);
		outputSocket->relinkConnections(blend->getOutputSocket());
	
		changeHSV->setCurveMapping(storage);
	
		blend->setResolutionInputSocketIndex(1);
	
		graph->addOperation(rgbToHSV);
		graph->addOperation(hsvToRGB);
		graph->addOperation(changeHSV);
		graph->addOperation(blend);
	}
}
