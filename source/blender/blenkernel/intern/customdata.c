/*
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
* along with this program; if not, write to the Free Software  Foundation,
* Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
* The Original Code is Copyright (C) 2006 Blender Foundation.
* All rights reserved.
*
* The Original Code is: all of this file.
*
* Contributor(s): Ben Batt <benbatt@gmail.com>
*
* ***** END GPL LICENSE BLOCK *****
*
* Implementation of CustomData.
*
* BKE_customdata.h contains the function prototypes for this file.
*
*/ 

#include "BKE_customdata.h"

#include "BLI_blenlib.h"
#include "BLI_linklist.h"

#include "DNA_customdata_types.h"
#include "DNA_listBase.h"
#include "DNA_meshdata_types.h"

#include "MEM_guardedalloc.h"

#include <string.h>

/* number of layers to add when growing a CustomData object */
#define CUSTOMDATA_GROW 5

/********************* Layer type information **********************/
typedef struct LayerTypeInfo {
	int size;          /* the memory size of one element of this layer's data */
	char *structname;  /* name of the struct used, for file writing */
	int structnum;     /* number of structs per element, for file writing */
	char *defaultname; /* default layer name */

	/* a function to copy count elements of this layer's data
	 * (deep copy if appropriate)
	 * if NULL, memcpy is used
	 */
	void (*copy)(const void *source, void *dest, int count);

	/* a function to free any dynamically allocated components of this
	 * layer's data (note the data pointer itself should not be freed)
	 * size should be the size of one element of this layer's data (e.g.
	 * LayerTypeInfo.size)
	 */
	void (*free)(void *data, int count, int size);

	/* a function to interpolate between count source elements of this
	 * layer's data and store the result in dest
	 * if weights == NULL or sub_weights == NULL, they should default to 1
	 *
	 * weights gives the weight for each element in sources
	 * sub_weights gives the sub-element weights for each element in sources
	 *    (there should be (sub element count)^2 weights per element)
	 * count gives the number of elements in sources
	 */
	void (*interp)(void **sources, float *weights, float *sub_weights,
	               int count, void *dest);

    /* a function to swap the data in corners of the element */
	void (*swap)(void *data, int *corner_indices);

    /* a function to set a layer's data to default values. if NULL, the
	   default is assumed to be all zeros */
	void (*set_default)(void *data, int count);
} LayerTypeInfo;

static void layerCopy_mdeformvert(const void *source, void *dest,
                                  int count)
{
	int i, size = sizeof(MDeformVert);

	memcpy(dest, source, count * size);

	for(i = 0; i < count; ++i) {
		MDeformVert *dvert = (MDeformVert *)((char *)dest + i * size);
		MDeformWeight *dw = MEM_callocN(dvert->totweight * sizeof(*dw),
		                                "layerCopy_mdeformvert dw");

		memcpy(dw, dvert->dw, dvert->totweight * sizeof(*dw));
		dvert->dw = dw;
	}
}

static void layerFree_mdeformvert(void *data, int count, int size)
{
	int i;

	for(i = 0; i < count; ++i) {
		MDeformVert *dvert = (MDeformVert *)((char *)data + i * size);

		if(dvert->dw) {
			MEM_freeN(dvert->dw);
			dvert->dw = NULL;
			dvert->totweight = 0;
		}
	}
}

static void linklist_free_simple(void *link)
{
	MEM_freeN(link);
}

static void layerInterp_mdeformvert(void **sources, float *weights,
                                    float *sub_weights, int count, void *dest)
{
	MDeformVert *dvert = dest;
	LinkNode *dest_dw = NULL; /* a list of lists of MDeformWeight pointers */
	LinkNode *node;
	int i, j, totweight;

	if(count <= 0) return;

	/* build a list of unique def_nrs for dest */
	totweight = 0;
	for(i = 0; i < count; ++i) {
		MDeformVert *source = sources[i];
		float interp_weight = weights ? weights[i] : 1.0f;

		for(j = 0; j < source->totweight; ++j) {
			MDeformWeight *dw = &source->dw[j];

			for(node = dest_dw; node; node = node->next) {
				MDeformWeight *tmp_dw = (MDeformWeight *)node->link;

				if(tmp_dw->def_nr == dw->def_nr) {
					tmp_dw->weight += dw->weight * interp_weight;
					break;
				}
			}

			/* if this def_nr is not in the list, add it */
			if(!node) {
				MDeformWeight *tmp_dw = MEM_callocN(sizeof(*tmp_dw),
				                            "layerInterp_mdeformvert tmp_dw");
				tmp_dw->def_nr = dw->def_nr;
				tmp_dw->weight = dw->weight * interp_weight;
				BLI_linklist_prepend(&dest_dw, tmp_dw);
				totweight++;
			}
		}
	}

	/* now we know how many unique deform weights there are, so realloc */
	if(dvert->dw) MEM_freeN(dvert->dw);

	if(totweight) {
		dvert->dw = MEM_callocN(sizeof(*dvert->dw) * totweight,
		                        "layerInterp_mdeformvert dvert->dw");
		dvert->totweight = totweight;

		for(i = 0, node = dest_dw; node; node = node->next, ++i)
			dvert->dw[i] = *((MDeformWeight *)node->link);
	}
	else
		memset(dvert, 0, sizeof(*dvert));

	BLI_linklist_free(dest_dw, linklist_free_simple);
}


static void layerInterp_msticky(void **sources, float *weights,
                                float *sub_weights, int count, void *dest)
{
	float co[2], w;
	MSticky *mst;
	int i;

	co[0] = co[1] = 0.0f;
	for(i = 0; i < count; i++) {
		w = weights ? weights[i] : 1.0f;
		mst = (MSticky*)sources[i];

		co[0] += w*mst->co[0];
		co[1] += w*mst->co[1];
	}

	mst = (MSticky*)dest;
	mst->co[0] = co[0];
	mst->co[1] = co[1];
}


static void layerCopy_tface(const void *source, void *dest, int count)
{
	const MTFace *source_tf = (const MTFace*)source;
	MTFace *dest_tf = (MTFace*)dest;
	int i;

	for(i = 0; i < count; ++i)
		dest_tf[i] = source_tf[i];
}

static void layerInterp_tface(void **sources, float *weights,
                              float *sub_weights, int count, void *dest)
{
	MTFace *tf = dest;
	int i, j, k;
	float uv[4][2];
	float *sub_weight;

	if(count <= 0) return;

	memset(uv, 0, sizeof(uv));

	sub_weight = sub_weights;
	for(i = 0; i < count; ++i) {
		float weight = weights ? weights[i] : 1;
		MTFace *src = sources[i];

		for(j = 0; j < 4; ++j) {
			if(sub_weights) {
				for(k = 0; k < 4; ++k, ++sub_weight) {
					float w = (*sub_weight) * weight;
					float *tmp_uv = src->uv[k];

					uv[j][0] += tmp_uv[0] * w;
					uv[j][1] += tmp_uv[1] * w;
				}
			} else {
				uv[j][0] += src->uv[j][0] * weight;
				uv[j][1] += src->uv[j][1] * weight;
			}
		}
	}

	*tf = *(MTFace *)sources[0];
	for(j = 0; j < 4; ++j) {
		tf->uv[j][0] = uv[j][0];
		tf->uv[j][1] = uv[j][1];
	}
}

static void layerSwap_tface(void *data, int *corner_indices)
{
	MTFace *tf = data;
	float uv[4][2];
	int j;

	for(j = 0; j < 4; ++j) {
		uv[j][0] = tf->uv[corner_indices[j]][0];
		uv[j][1] = tf->uv[corner_indices[j]][1];
	}

	memcpy(tf->uv, uv, sizeof(tf->uv));
}

static void layerDefault_tface(void *data, int count)
{
	static MTFace default_tf = {{{0, 1}, {0, 0}, {1, 0}, {1, 1}}, NULL,
	                           0, 0, TF_DYNAMIC, 0, 0};
	MTFace *tf = (MTFace*)data;
	int i;

	for(i = 0; i < count; i++)
		tf[i] = default_tf;
}

static void layerInterp_mcol(void **sources, float *weights,
                             float *sub_weights, int count, void *dest)
{
	MCol *mc = dest;
	int i, j, k;
	struct {
		float a;
		float r;
		float g;
		float b;
	} col[4];
	float *sub_weight;

	if(count <= 0) return;

	memset(col, 0, sizeof(col));
	
	sub_weight = sub_weights;
	for(i = 0; i < count; ++i) {
		float weight = weights ? weights[i] : 1;

		for(j = 0; j < 4; ++j) {
			if(sub_weights) {
				MCol *src = sources[i];
				for(k = 0; k < 4; ++k, ++sub_weight, ++src) {
					col[j].a += src->a * (*sub_weight) * weight;
					col[j].r += src->r * (*sub_weight) * weight;
					col[j].g += src->g * (*sub_weight) * weight;
					col[j].b += src->b * (*sub_weight) * weight;
				}
			} else {
				MCol *src = sources[i];
				col[j].a += src[j].a * weight;
				col[j].r += src[j].r * weight;
				col[j].g += src[j].g * weight;
				col[j].b += src[j].b * weight;
			}
		}
	}

	for(j = 0; j < 4; ++j) {
		mc[j].a = (int)col[j].a;
		mc[j].r = (int)col[j].r;
		mc[j].g = (int)col[j].g;
		mc[j].b = (int)col[j].b;
	}
}

static void layerSwap_mcol(void *data, int *corner_indices)
{
	MCol *mcol = data;
	MCol col[4];
	int j;

	for(j = 0; j < 4; ++j)
		col[j] = mcol[corner_indices[j]];

	memcpy(mcol, col, sizeof(col));
}

static void layerDefault_mcol(void *data, int count)
{
	static MCol default_mcol = {255, 255, 255, 255};
	MCol *mcol = (MCol*)data;
	int i;

	for(i = 0; i < 4*count; i++)
		mcol[i] = default_mcol;
}

const LayerTypeInfo LAYERTYPEINFO[CD_NUMTYPES] = {
	{sizeof(MVert), "MVert", 1, NULL, NULL, NULL, NULL, NULL, NULL},
	{sizeof(MSticky), "MSticky", 1, NULL, NULL, NULL, layerInterp_msticky, NULL,
	 NULL},
	{sizeof(MDeformVert), "MDeformVert", 1, NULL, layerCopy_mdeformvert,
	 layerFree_mdeformvert, layerInterp_mdeformvert, NULL, NULL},
	{sizeof(MEdge), "MEdge", 1, NULL, NULL, NULL, NULL, NULL, NULL},
	{sizeof(MFace), "MFace", 1, NULL, NULL, NULL, NULL, NULL, NULL},
	{sizeof(MTFace), "MTFace", 1, "UVTex", layerCopy_tface, NULL,
	 layerInterp_tface, layerSwap_tface, layerDefault_tface},
	/* 4 MCol structs per face */
	{sizeof(MCol)*4, "MCol", 4, "Col", NULL, NULL, layerInterp_mcol,
	 layerSwap_mcol, layerDefault_mcol},
	{sizeof(int), "", 0, NULL, NULL, NULL, NULL, NULL, NULL},
	/* 3 floats per normal vector */
	{sizeof(float)*3, "", 0, NULL, NULL, NULL, NULL, NULL, NULL},
	{sizeof(int), "", 0, NULL, NULL, NULL, NULL, NULL, NULL},
	{sizeof(MFloatProperty), "MFloatProperty",1,"Float",NULL,NULL,NULL,NULL},
	{sizeof(MIntProperty), "MIntProperty",1,"Int",NULL,NULL,NULL,NULL},
	{sizeof(MStringProperty), "MStringProperty",1,"String",NULL,NULL,NULL,NULL},
};

const char *LAYERTYPENAMES[CD_NUMTYPES] = {
	"CDMVert", "CDMSticky", "CDMDeformVert", "CDMEdge", "CDMFace", "CDMTFace",
	"CDMCol", "CDOrigIndex", "CDNormal", "CDFlags","CDMFloatProperty","CDMIntProperty","CDMStringProperty"};

const CustomDataMask CD_MASK_BAREMESH =
	CD_MASK_MVERT | CD_MASK_MEDGE | CD_MASK_MFACE;
const CustomDataMask CD_MASK_MESH =
	CD_MASK_MVERT | CD_MASK_MEDGE | CD_MASK_MFACE |
	CD_MASK_MSTICKY | CD_MASK_MDEFORMVERT | CD_MASK_MTFACE | CD_MASK_MCOL |
	CD_MASK_PROP_FLT | CD_MASK_PROP_INT | CD_MASK_PROP_STR;
const CustomDataMask CD_MASK_EDITMESH =
	CD_MASK_MSTICKY | CD_MASK_MDEFORMVERT | CD_MASK_MTFACE |
	CD_MASK_MCOL|CD_MASK_PROP_FLT | CD_MASK_PROP_INT | CD_MASK_PROP_STR;
const CustomDataMask CD_MASK_DERIVEDMESH =
	CD_MASK_MSTICKY | CD_MASK_MDEFORMVERT | CD_MASK_MTFACE |
	CD_MASK_MCOL | CD_MASK_ORIGINDEX;

static const LayerTypeInfo *layerType_getInfo(int type)
{
	if(type < 0 || type >= CD_NUMTYPES) return NULL;

	return &LAYERTYPEINFO[type];
}

static const char *layerType_getName(int type)
{
	if(type < 0 || type >= CD_NUMTYPES) return NULL;

	return LAYERTYPENAMES[type];
}

/********************* CustomData functions *********************/
static void customData_update_offsets(CustomData *data);

static CustomDataLayer *customData_add_layer__internal(CustomData *data,
	int type, int alloctype, void *layerdata, int totelem, const char *name);

void CustomData_merge(const struct CustomData *source, struct CustomData *dest,
                      CustomDataMask mask, int alloctype, int totelem)
{
	const LayerTypeInfo *typeInfo;
	CustomDataLayer *layer, *newlayer;
	int i, type, number = 0, lasttype = -1, lastactive = 0, lastrender = 0;

	for(i = 0; i < source->totlayer; ++i) {
		layer = &source->layers[i];
		typeInfo = layerType_getInfo(layer->type);

		type = layer->type;

		if (type != lasttype) {
			number = 0;
			lastactive = layer->active;
			lastrender = layer->active_rnd;
			lasttype = type;
		}
		else
			number++;

		if(layer->flag & CD_FLAG_NOCOPY) continue;
		else if(!(mask & (1 << type))) continue;
		else if(number < CustomData_number_of_layers(dest, type)) continue;

		if((alloctype == CD_ASSIGN) && (layer->flag & CD_FLAG_NOFREE))
			newlayer = customData_add_layer__internal(dest, type, CD_REFERENCE,
				layer->data, totelem, layer->name);
		else
			newlayer = customData_add_layer__internal(dest, type, alloctype,
				layer->data, totelem, layer->name);
		
		if(newlayer) {
			newlayer->active = lastactive;
			newlayer->active_rnd = lastrender;
		}
	}
}

void CustomData_copy(const struct CustomData *source, struct CustomData *dest,
                     CustomDataMask mask, int alloctype, int totelem)
{
	memset(dest, 0, sizeof(*dest));

	CustomData_merge(source, dest, mask, alloctype, totelem);
}

static void customData_free_layer__internal(CustomDataLayer *layer, int totelem)
{
	const LayerTypeInfo *typeInfo;

	if(!(layer->flag & CD_FLAG_NOFREE) && layer->data) {
		typeInfo = layerType_getInfo(layer->type);

		if(typeInfo->free)
			typeInfo->free(layer->data, totelem, typeInfo->size);

		if(layer->data)
			MEM_freeN(layer->data);
	}
}

void CustomData_free(CustomData *data, int totelem)
{
	int i;

	for(i = 0; i < data->totlayer; ++i)
		customData_free_layer__internal(&data->layers[i], totelem);

	if(data->layers)
		MEM_freeN(data->layers);
	
	memset(data, 0, sizeof(*data));
}

static void customData_update_offsets(CustomData *data)
{
	const LayerTypeInfo *typeInfo;
	int i, offset = 0;

	for(i = 0; i < data->totlayer; ++i) {
		typeInfo = layerType_getInfo(data->layers[i].type);

		data->layers[i].offset = offset;
		offset += typeInfo->size;
	}

	data->totsize = offset;
}

int CustomData_get_layer_index(const CustomData *data, int type)
{
	int i; 

	for(i=0; i < data->totlayer; ++i)
		if(data->layers[i].type == type)
			return i;

	return -1;
}

int CustomData_get_named_layer_index(const CustomData *data, int type, char *name)
{
	int i;

	for(i=0; i < data->totlayer; ++i)
		if(data->layers[i].type == type && strcmp(data->layers[i].name, name)==0)
			return i;

	return -1;
}

int CustomData_get_active_layer_index(const CustomData *data, int type)
{
	int i;

	for(i=0; i < data->totlayer; ++i)
		if(data->layers[i].type == type)
			return i + data->layers[i].active;

	return -1;
}

int CustomData_get_render_layer_index(const CustomData *data, int type)
{
	int i;

	for(i=0; i < data->totlayer; ++i)
		if(data->layers[i].type == type)
			return i + data->layers[i].active_rnd;

	return -1;
}

void CustomData_set_layer_active(CustomData *data, int type, int n)
{
	int i;

	for(i=0; i < data->totlayer; ++i)
		if(data->layers[i].type == type)
			data->layers[i].active = n;
}

void CustomData_set_layer_render(CustomData *data, int type, int n)
{
	int i;

	for(i=0; i < data->totlayer; ++i)
		if(data->layers[i].type == type)
			data->layers[i].active_rnd = n;
}


void CustomData_set_layer_flag(struct CustomData *data, int type, int flag)
{
	int i;

	for(i=0; i < data->totlayer; ++i)
		if(data->layers[i].type == type)
			data->layers[i].flag |= flag;
}

static int customData_resize(CustomData *data, int amount)
{
	CustomDataLayer *tmp = MEM_callocN(sizeof(*tmp)*(data->maxlayer + amount),
                                       "CustomData->layers");
	if(!tmp) return 0;

	data->maxlayer += amount;
	if (data->layers) {
		memcpy(tmp, data->layers, sizeof(*tmp) * data->totlayer);
		MEM_freeN(data->layers);
	}
	data->layers = tmp;

	return 1;
}

static CustomDataLayer *customData_add_layer__internal(CustomData *data,
	int type, int alloctype, void *layerdata, int totelem, const char *name)
{
	const LayerTypeInfo *typeInfo= layerType_getInfo(type);
	int size = typeInfo->size * totelem, flag = 0, index = data->totlayer;
	void *newlayerdata;

	if (!typeInfo->defaultname && CustomData_has_layer(data, type))
		return &data->layers[CustomData_get_layer_index(data, type)];

	if((alloctype == CD_ASSIGN) || (alloctype == CD_REFERENCE)) {
		newlayerdata = layerdata;
	}
	else {
		newlayerdata = MEM_callocN(size, layerType_getName(type));
		if(!newlayerdata)
			return NULL;
	}

	if (alloctype == CD_DUPLICATE) {
		if(typeInfo->copy)
			typeInfo->copy(layerdata, newlayerdata, totelem);
		else
			memcpy(newlayerdata, layerdata, size);
	}
	else if (alloctype == CD_DEFAULT) {
		if(typeInfo->set_default)
			typeInfo->set_default((char*)newlayerdata, totelem);
	}
	else if (alloctype == CD_REFERENCE)
		flag |= CD_FLAG_NOFREE;

	if(index >= data->maxlayer) {
		if(!customData_resize(data, CUSTOMDATA_GROW)) {
			if(newlayerdata != layerdata)
				MEM_freeN(newlayerdata);
			return NULL;
		}
	}
	
	data->totlayer++;

	/* keep layers ordered by type */
	for( ; index > 0 && data->layers[index - 1].type > type; --index)
		data->layers[index] = data->layers[index - 1];

	data->layers[index].type = type;
	data->layers[index].flag = flag;
	data->layers[index].data = newlayerdata;

	if(name) {
		strcpy(data->layers[index].name, name);
		CustomData_set_layer_unique_name(data, index);
	}
	else
		data->layers[index].name[0] = '\0';

	if(index > 0 && data->layers[index-1].type == type) {
		data->layers[index].active = data->layers[index-1].active;
		data->layers[index].active_rnd = data->layers[index-1].active_rnd;
	} else {
		data->layers[index].active = 0;
		data->layers[index].active_rnd = 0;
	}
	
	customData_update_offsets(data);

	return &data->layers[index];
}

void *CustomData_add_layer(CustomData *data, int type, int alloctype,
                           void *layerdata, int totelem)
{
	CustomDataLayer *layer;
	const LayerTypeInfo *typeInfo= layerType_getInfo(type);
	
	layer = customData_add_layer__internal(data, type, alloctype, layerdata,
	                                       totelem, typeInfo->defaultname);

	if(layer)
		return layer->data;

	return NULL;
}

/*same as above but accepts a name*/
void *CustomData_add_layer_named(CustomData *data, int type, int alloctype,
                           void *layerdata, int totelem, char *name)
{
	CustomDataLayer *layer;
	
	layer = customData_add_layer__internal(data, type, alloctype, layerdata,
	                                       totelem, name);

	if(layer)
		return layer->data;

	return NULL;
}


int CustomData_free_layer(CustomData *data, int type, int totelem, int index)
{
	int i;
	CustomDataLayer *layer;
	
	if (index < 0) return 0;

	layer = &data->layers[index];

	customData_free_layer__internal(&data->layers[index], totelem);

	for (i=index+1; i < data->totlayer; ++i)
		data->layers[i-1] = data->layers[i];

	data->totlayer--;

	/* if layer was last of type in array, set new active layer */
	if ((index >= data->totlayer) || (data->layers[index].type != type)) {
		i = CustomData_get_layer_index(data, type);
		
		if (i >= 0)
			for (; i < data->totlayer && data->layers[i].type == type; i++) {
				data->layers[i].active--;
				data->layers[i].active_rnd--;
			}
	}

	if (data->totlayer <= data->maxlayer-CUSTOMDATA_GROW)
		customData_resize(data, -CUSTOMDATA_GROW);

	customData_update_offsets(data);

	return 1;
}

int CustomData_free_layer_active(CustomData *data, int type, int totelem)
{
	int index = 0;
	index = CustomData_get_active_layer_index(data, type);
	if (index < 0) return 0;
	return CustomData_free_layer(data, type, totelem, index);
}


void CustomData_free_layers(CustomData *data, int type, int totelem)
{
	while (CustomData_has_layer(data, type))
		CustomData_free_layer_active(data, type, totelem);
}

int CustomData_has_layer(const CustomData *data, int type)
{
	return (CustomData_get_layer_index(data, type) != -1);
}

int CustomData_number_of_layers(const CustomData *data, int type)
{
	int i, number = 0;

	for(i = 0; i < data->totlayer; i++)
		if(data->layers[i].type == type)
			number++;
	
	return number;
}

void *CustomData_duplicate_referenced_layer(struct CustomData *data, int type)
{
	CustomDataLayer *layer;
	int layer_index;

	/* get the layer index of the first layer of type */
	layer_index = CustomData_get_active_layer_index(data, type);
	if(layer_index < 0) return NULL;

	layer = &data->layers[layer_index];

	if (layer->flag & CD_FLAG_NOFREE) {
		layer->data = MEM_dupallocN(layer->data);
		layer->flag &= ~CD_FLAG_NOFREE;
	}

	return layer->data;
}

void *CustomData_duplicate_referenced_layer_named(struct CustomData *data,
                                                  int type, char *name)
{
	CustomDataLayer *layer;
	int layer_index;

	/* get the layer index of the desired layer */
	layer_index = CustomData_get_named_layer_index(data, type, name);
	if(layer_index < 0) return NULL;

	layer = &data->layers[layer_index];

	if (layer->flag & CD_FLAG_NOFREE) {
		layer->data = MEM_dupallocN(layer->data);
		layer->flag &= ~CD_FLAG_NOFREE;
	}

	return layer->data;
}

void CustomData_free_temporary(CustomData *data, int totelem)
{
	CustomDataLayer *layer;
	int i, j;

	for(i = 0, j = 0; i < data->totlayer; ++i) {
		layer = &data->layers[i];

		if (i != j)
			data->layers[j] = data->layers[i];

		if ((layer->flag & CD_FLAG_TEMPORARY) == CD_FLAG_TEMPORARY)
			customData_free_layer__internal(layer, totelem);
		else
			j++;
	}

	data->totlayer = j;

	if(data->totlayer <= data->maxlayer-CUSTOMDATA_GROW)
		customData_resize(data, -CUSTOMDATA_GROW);

	customData_update_offsets(data);
}

void CustomData_set_only_copy(const struct CustomData *data,
                              CustomDataMask mask)
{
	int i;

	for(i = 0; i < data->totlayer; ++i)
		if(!(mask & (1 << data->layers[i].type)))
			data->layers[i].flag |= CD_FLAG_NOCOPY;
}

void CustomData_copy_data(const CustomData *source, CustomData *dest,
                          int source_index, int dest_index, int count)
{
	const LayerTypeInfo *typeInfo;
	int src_i, dest_i;
	int src_offset;
	int dest_offset;

	/* copies a layer at a time */
	dest_i = 0;
	for(src_i = 0; src_i < source->totlayer; ++src_i) {

		/* find the first dest layer with type >= the source type
		 * (this should work because layers are ordered by type)
		 */
		while(dest_i < dest->totlayer
		      && dest->layers[dest_i].type < source->layers[src_i].type)
			++dest_i;

		/* if there are no more dest layers, we're done */
		if(dest_i >= dest->totlayer) return;

		/* if we found a matching layer, copy the data */
		if(dest->layers[dest_i].type == source->layers[src_i].type) {
			char *src_data = source->layers[src_i].data;
			char *dest_data = dest->layers[dest_i].data;

			typeInfo = layerType_getInfo(source->layers[src_i].type);

			src_offset = source_index * typeInfo->size;
			dest_offset = dest_index * typeInfo->size;

			if(typeInfo->copy)
				typeInfo->copy(src_data + src_offset,
				                dest_data + dest_offset,
				                count);
			else
				memcpy(dest_data + dest_offset,
				       src_data + src_offset,
				       count * typeInfo->size);

			/* if there are multiple source & dest layers of the same type,
			 * we don't want to copy all source layers to the same dest, so
			 * increment dest_i
			 */
			++dest_i;
		}
	}
}

void CustomData_free_elem(CustomData *data, int index, int count)
{
	int i;
	const LayerTypeInfo *typeInfo;

	for(i = 0; i < data->totlayer; ++i) {
		if(!(data->layers[i].flag & CD_FLAG_NOFREE)) {
			typeInfo = layerType_getInfo(data->layers[i].type);

			if(typeInfo->free) {
				int offset = typeInfo->size * index;

				typeInfo->free((char *)data->layers[i].data + offset,
				               count, typeInfo->size);
			}
		}
	}
}

#define SOURCE_BUF_SIZE 100

void CustomData_interp(const CustomData *source, CustomData *dest,
                       int *src_indices, float *weights, float *sub_weights,
                       int count, int dest_index)
{
	int src_i, dest_i;
	int dest_offset;
	int j;
	void *source_buf[SOURCE_BUF_SIZE];
	void **sources = source_buf;

	/* slow fallback in case we're interpolating a ridiculous number of
	 * elements
	 */
	if(count > SOURCE_BUF_SIZE)
		sources = MEM_callocN(sizeof(*sources) * count,
		                      "CustomData_interp sources");

	/* interpolates a layer at a time */
	dest_i = 0;
	for(src_i = 0; src_i < source->totlayer; ++src_i) {
		const LayerTypeInfo *typeInfo= layerType_getInfo(source->layers[src_i].type);
		if(!typeInfo->interp) continue;

		/* find the first dest layer with type >= the source type
		 * (this should work because layers are ordered by type)
		 */
		while(dest_i < dest->totlayer
		      && dest->layers[dest_i].type < source->layers[src_i].type)
			++dest_i;

		/* if there are no more dest layers, we're done */
		if(dest_i >= dest->totlayer) return;

		/* if we found a matching layer, copy the data */
		if(dest->layers[dest_i].type == source->layers[src_i].type) {
			void *src_data = source->layers[src_i].data;

			for(j = 0; j < count; ++j)
				sources[j] = (char *)src_data
							 + typeInfo->size * src_indices[j];

			dest_offset = dest_index * typeInfo->size;

			typeInfo->interp(sources, weights, sub_weights, count,
						   (char *)dest->layers[dest_i].data + dest_offset);

			/* if there are multiple source & dest layers of the same type,
			 * we don't want to copy all source layers to the same dest, so
			 * increment dest_i
			 */
			++dest_i;
		}
	}

	if(count > SOURCE_BUF_SIZE) MEM_freeN(sources);
}

void CustomData_swap(struct CustomData *data, int index, int *corner_indices)
{
	const LayerTypeInfo *typeInfo;
	int i;

	for(i = 0; i < data->totlayer; ++i) {
		typeInfo = layerType_getInfo(data->layers[i].type);

		if(typeInfo->swap) {
			int offset = typeInfo->size * index;

			typeInfo->swap((char *)data->layers[i].data + offset, corner_indices);
		}
	}
}

void *CustomData_get(const CustomData *data, int index, int type)
{
	int offset;
	int layer_index;
	
	/* get the layer index of the active layer of type */
	layer_index = CustomData_get_active_layer_index(data, type);
	if(layer_index < 0) return NULL;

	/* get the offset of the desired element */
	offset = layerType_getInfo(type)->size * index;

	return (char *)data->layers[layer_index].data + offset;
}

void *CustomData_get_layer(const CustomData *data, int type)
{
	/* get the layer index of the active layer of type */
	int layer_index = CustomData_get_active_layer_index(data, type);
	if(layer_index < 0) return NULL;

	return data->layers[layer_index].data;
}

void *CustomData_get_layer_n(const CustomData *data, int type, int n)
{
	/* get the layer index of the active layer of type */
	int layer_index = CustomData_get_layer_index(data, type);
	if(layer_index < 0) return NULL;

	return data->layers[layer_index+n].data;
}

void *CustomData_get_layer_named(const struct CustomData *data, int type,
                                 char *name)
{
	int layer_index = CustomData_get_named_layer_index(data, type, name);
	if(layer_index < 0) return NULL;

	return data->layers[layer_index].data;
}

void *CustomData_set_layer(const CustomData *data, int type, void *ptr)
{
	/* get the layer index of the first layer of type */
	int layer_index = CustomData_get_active_layer_index(data, type);

	if(layer_index < 0) return NULL;

	data->layers[layer_index].data = ptr;

	return ptr;
}

void *CustomData_set_layer_n(const struct CustomData *data, int type, int n, void *ptr)
{
	/* get the layer index of the first layer of type */
	int layer_index = CustomData_get_layer_index(data, type);
	if(layer_index < 0) return NULL;

	data->layers[layer_index+n].data = ptr;

	return ptr;
}

void CustomData_set(const CustomData *data, int index, int type, void *source)
{
	void *dest = CustomData_get(data, index, type);
	const LayerTypeInfo *typeInfo = layerType_getInfo(type);

	if(!dest) return;

	if(typeInfo->copy)
		typeInfo->copy(source, dest, 1);
	else
		memcpy(dest, source, typeInfo->size);
}

/* EditMesh functions */

void CustomData_em_free_block(CustomData *data, void **block)
{
    const LayerTypeInfo *typeInfo;
    int i;

	if(!*block) return;

    for(i = 0; i < data->totlayer; ++i) {
        if(!(data->layers[i].flag & CD_FLAG_NOFREE)) {
            typeInfo = layerType_getInfo(data->layers[i].type);

            if(typeInfo->free) {
				int offset = data->layers[i].offset;
                typeInfo->free((char*)*block + offset, 1, typeInfo->size);
			}
        }
    }

	MEM_freeN(*block);
	*block = NULL;
}

static void CustomData_em_alloc_block(CustomData *data, void **block)
{
	/* TODO: optimize free/alloc */

	if (*block)
		CustomData_em_free_block(data, block);

	if (data->totsize > 0)
		*block = MEM_callocN(data->totsize, "CustomData EM block");
	else
		*block = NULL;
}

void CustomData_em_copy_data(const CustomData *source, CustomData *dest,
                            void *src_block, void **dest_block)
{
	const LayerTypeInfo *typeInfo;
	int dest_i, src_i;

	if (!*dest_block)
		CustomData_em_alloc_block(dest, dest_block);
	
	/* copies a layer at a time */
	dest_i = 0;
	for(src_i = 0; src_i < source->totlayer; ++src_i) {

		/* find the first dest layer with type >= the source type
		 * (this should work because layers are ordered by type)
		 */
		while(dest_i < dest->totlayer
		      && dest->layers[dest_i].type < source->layers[src_i].type)
			++dest_i;

		/* if there are no more dest layers, we're done */
		if(dest_i >= dest->totlayer) return;

		/* if we found a matching layer, copy the data */
		if(dest->layers[dest_i].type == source->layers[src_i].type) {
			char *src_data = (char*)src_block + source->layers[src_i].offset;
			char *dest_data = (char*)*dest_block + dest->layers[dest_i].offset;

			typeInfo = layerType_getInfo(source->layers[src_i].type);

			if(typeInfo->copy)
				typeInfo->copy(src_data, dest_data, 1);
			else
				memcpy(dest_data, src_data, typeInfo->size);

			/* if there are multiple source & dest layers of the same type,
			 * we don't want to copy all source layers to the same dest, so
			 * increment dest_i
			 */
			++dest_i;
		}
	}
}

void *CustomData_em_get(const CustomData *data, void *block, int type)
{
	int layer_index;
	
	/* get the layer index of the first layer of type */
	layer_index = CustomData_get_active_layer_index(data, type);
	if(layer_index < 0) return NULL;

	return (char *)block + data->layers[layer_index].offset;
}

void *CustomData_em_get_n(const CustomData *data, void *block, int type, int n)
{
	int layer_index;
	
	/* get the layer index of the first layer of type */
	layer_index = CustomData_get_active_layer_index(data, type);
	if(layer_index < 0) return NULL;

	return (char *)block + data->layers[layer_index+n].offset;
}

void CustomData_em_set(CustomData *data, void *block, int type, void *source)
{
	void *dest = CustomData_em_get(data, block, type);
	const LayerTypeInfo *typeInfo = layerType_getInfo(type);

	if(!dest) return;

	if(typeInfo->copy)
		typeInfo->copy(source, dest, 1);
	else
		memcpy(dest, source, typeInfo->size);
}

void CustomData_em_set_n(CustomData *data, void *block, int type, int n, void *source)
{
	void *dest = CustomData_em_get_n(data, block, type, n);
	const LayerTypeInfo *typeInfo = layerType_getInfo(type);

	if(!dest) return;

	if(typeInfo->copy)
		typeInfo->copy(source, dest, 1);
	else
		memcpy(dest, source, typeInfo->size);
}

void CustomData_em_interp(CustomData *data, void **src_blocks, float *weights,
                          float *sub_weights, int count, void *dest_block)
{
	int i, j;
	void *source_buf[SOURCE_BUF_SIZE];
	void **sources = source_buf;

	/* slow fallback in case we're interpolating a ridiculous number of
	 * elements
	 */
	if(count > SOURCE_BUF_SIZE)
		sources = MEM_callocN(sizeof(*sources) * count,
		                      "CustomData_interp sources");

	/* interpolates a layer at a time */
	for(i = 0; i < data->totlayer; ++i) {
		CustomDataLayer *layer = &data->layers[i];
		const LayerTypeInfo *typeInfo = layerType_getInfo(layer->type);

		if(typeInfo->interp) {
			for(j = 0; j < count; ++j)
				sources[j] = (char *)src_blocks[j] + layer->offset;

			typeInfo->interp(sources, weights, sub_weights, count,
			                  (char *)dest_block + layer->offset);
		}
	}

	if(count > SOURCE_BUF_SIZE) MEM_freeN(sources);
}

void CustomData_em_set_default(CustomData *data, void **block)
{
	const LayerTypeInfo *typeInfo;
	int i;

	if (!*block)
		CustomData_em_alloc_block(data, block);

	for(i = 0; i < data->totlayer; ++i) {
		int offset = data->layers[i].offset;

		typeInfo = layerType_getInfo(data->layers[i].type);

		if(typeInfo->set_default)
			typeInfo->set_default((char*)*block + offset, 1);
	}
}

void CustomData_to_em_block(const CustomData *source, CustomData *dest,
                            int src_index, void **dest_block)
{
	const LayerTypeInfo *typeInfo;
	int dest_i, src_i, src_offset;

	if (!*dest_block)
		CustomData_em_alloc_block(dest, dest_block);
	
	/* copies a layer at a time */
	dest_i = 0;
	for(src_i = 0; src_i < source->totlayer; ++src_i) {

		/* find the first dest layer with type >= the source type
		 * (this should work because layers are ordered by type)
		 */
		while(dest_i < dest->totlayer
		      && dest->layers[dest_i].type < source->layers[src_i].type)
			++dest_i;

		/* if there are no more dest layers, we're done */
		if(dest_i >= dest->totlayer) return;

		/* if we found a matching layer, copy the data */
		if(dest->layers[dest_i].type == source->layers[src_i].type) {
			int offset = dest->layers[dest_i].offset;
			char *src_data = source->layers[src_i].data;
			char *dest_data = (char*)*dest_block + offset;

			typeInfo = layerType_getInfo(dest->layers[dest_i].type);
			src_offset = src_index * typeInfo->size;

			if(typeInfo->copy)
				typeInfo->copy(src_data + src_offset, dest_data, 1);
			else
				memcpy(dest_data, src_data + src_offset, typeInfo->size);

			/* if there are multiple source & dest layers of the same type,
			 * we don't want to copy all source layers to the same dest, so
			 * increment dest_i
			 */
			++dest_i;
		}
	}
}

void CustomData_from_em_block(const CustomData *source, CustomData *dest,
                              void *src_block, int dest_index)
{
	const LayerTypeInfo *typeInfo;
	int dest_i, src_i, dest_offset;

	/* copies a layer at a time */
	dest_i = 0;
	for(src_i = 0; src_i < source->totlayer; ++src_i) {

		/* find the first dest layer with type >= the source type
		 * (this should work because layers are ordered by type)
		 */
		while(dest_i < dest->totlayer
		      && dest->layers[dest_i].type < source->layers[src_i].type)
			++dest_i;

		/* if there are no more dest layers, we're done */
		if(dest_i >= dest->totlayer) return;

		/* if we found a matching layer, copy the data */
		if(dest->layers[dest_i].type == source->layers[src_i].type) {
			int offset = source->layers[src_i].offset;
			char *src_data = (char*)src_block + offset;
			char *dest_data = dest->layers[dest_i].data;

			typeInfo = layerType_getInfo(dest->layers[dest_i].type);
			dest_offset = dest_index * typeInfo->size;

			if(typeInfo->copy)
				typeInfo->copy(src_data, dest_data + dest_offset, 1);
			else
				memcpy(dest_data + dest_offset, src_data, typeInfo->size);

			/* if there are multiple source & dest layers of the same type,
			 * we don't want to copy all source layers to the same dest, so
			 * increment dest_i
			 */
			++dest_i;
		}
	}

}

void CustomData_file_write_info(int type, char **structname, int *structnum)
{
	const LayerTypeInfo *typeInfo = layerType_getInfo(type);

	*structname = typeInfo->structname;
	*structnum = typeInfo->structnum;
}

int CustomData_sizeof(int type)
{
	const LayerTypeInfo *typeInfo = layerType_getInfo(type);

	return typeInfo->size;
}

const char *CustomData_layertype_name(int type)
{
	return layerType_getName(type);
}

static int  CustomData_is_property_layer(int type)
{
	if((type == CD_PROP_FLT) || (type == CD_PROP_INT) || (type == CD_PROP_STR))
		return 1;
	return 0;
}

void CustomData_set_layer_unique_name(CustomData *data, int index)
{
	char tempname[64];
	int number, i, type;
	char *dot, *name;
	CustomDataLayer *layer, *nlayer= &data->layers[index];
	const LayerTypeInfo *typeInfo= layerType_getInfo(nlayer->type);

	if (!typeInfo->defaultname)
		return;

	type = nlayer->type;
	name = nlayer->name;

	if (name[0] == '\0')
		BLI_strncpy(nlayer->name, typeInfo->defaultname, sizeof(nlayer->name));
	
	/* see if there is a duplicate */
	for(i=0; i<data->totlayer; i++) {
		layer = &data->layers[i];
		
		if(CustomData_is_property_layer(type)){
			if(i!=index && CustomData_is_property_layer(layer->type) && 
				strcmp(layer->name, name)==0)
					break;	
		
		}
		else{
			if(i!=index && layer->type==type && strcmp(layer->name, name)==0)
				break;
		}
	}

	if(i == data->totlayer)
		return;

	/* strip off the suffix */
	dot = strchr(nlayer->name, '.');
	if(dot) *dot=0;
	
	for(number=1; number <=999; number++) {
		sprintf(tempname, "%s.%03d", nlayer->name, number);

		for(i=0; i<data->totlayer; i++) {
			layer = &data->layers[i];
			
			if(CustomData_is_property_layer(type)){
				if(i!=index && CustomData_is_property_layer(layer->type) && 
					strcmp(layer->name, tempname)==0)

				break;
			}
			else{
				if(i!=index && layer->type==type && strcmp(layer->name, tempname)==0)
					break;
			}
		}

		if(i == data->totlayer) {
			BLI_strncpy(nlayer->name, tempname, sizeof(nlayer->name));
			return;
		}
	}	
}

int CustomData_verify_versions(struct CustomData *data, int index)
{
	const LayerTypeInfo *typeInfo;
	CustomDataLayer *layer = &data->layers[index];
	int i, keeplayer = 1;

	if (layer->type >= CD_NUMTYPES) {
		keeplayer = 0; /* unknown layer type from future version */
	}
	else {
		typeInfo = layerType_getInfo(layer->type);

		if (!typeInfo->defaultname && (index > 0) &&
			data->layers[index-1].type == layer->type)
			keeplayer = 0; /* multiple layers of which we only support one */
	}

	if (!keeplayer) {
	    for (i=index+1; i < data->totlayer; ++i)
    	    data->layers[i-1] = data->layers[i];
		data->totlayer--;
	}

	return keeplayer;
}

