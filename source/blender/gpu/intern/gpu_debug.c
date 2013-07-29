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
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_debug.c
 *  \ingroup gpu
 */

#ifdef DEBUG

#include <stdio.h>
#include <string.h>
#include "intern/gpu_glew.h"

/* debugging aid */
static void gpu_get_print(const char *name, GLenum type)
{
	float value[32];
	int a;
	
	memset(value, 0, sizeof(value));
	glGetFloatv(type, value);

	printf("%s: ", name);
	for (a = 0; a < 32; a++)
		printf("%.2f ", value[a]);
	printf("\n");
}

void GPU_state_print(void)
{
#if defined(WITH_GL_PROFILE_COMPAT)
	gpu_get_print("GL_ACCUM_ALPHA_BITS", GL_ACCUM_ALPHA_BITS);
	gpu_get_print("GL_ACCUM_BLUE_BITS", GL_ACCUM_BLUE_BITS);
	gpu_get_print("GL_ACCUM_CLEAR_VALUE", GL_ACCUM_CLEAR_VALUE);
	gpu_get_print("GL_ACCUM_GREEN_BITS", GL_ACCUM_GREEN_BITS);
	gpu_get_print("GL_ACCUM_RED_BITS", GL_ACCUM_RED_BITS);
	gpu_get_print("GL_ACTIVE_TEXTURE", GL_ACTIVE_TEXTURE);
	gpu_get_print("GL_ALIASED_POINT_SIZE_RANGE", GL_ALIASED_POINT_SIZE_RANGE);
	gpu_get_print("GL_ALIASED_LINE_WIDTH_RANGE", GL_ALIASED_LINE_WIDTH_RANGE);
	gpu_get_print("GL_ALPHA_BIAS", GL_ALPHA_BIAS);
	gpu_get_print("GL_ALPHA_BITS", GL_ALPHA_BITS);
	gpu_get_print("GL_ALPHA_SCALE", GL_ALPHA_SCALE);
	gpu_get_print("GL_ALPHA_TEST", GL_ALPHA_TEST);
	gpu_get_print("GL_ALPHA_TEST_FUNC", GL_ALPHA_TEST_FUNC);
	gpu_get_print("GL_ALPHA_TEST_REF", GL_ALPHA_TEST_REF);
	gpu_get_print("GL_ARRAY_BUFFER_BINDING", GL_ARRAY_BUFFER_BINDING);
	gpu_get_print("GL_ATTRIB_STACK_DEPTH", GL_ATTRIB_STACK_DEPTH);
	gpu_get_print("GL_AUTO_NORMAL", GL_AUTO_NORMAL);
	gpu_get_print("GL_AUX_BUFFERS", GL_AUX_BUFFERS);
	gpu_get_print("GL_BLEND", GL_BLEND);
	gpu_get_print("GL_BLEND_COLOR", GL_BLEND_COLOR);
	gpu_get_print("GL_BLEND_DST_ALPHA", GL_BLEND_DST_ALPHA);
	gpu_get_print("GL_BLEND_DST_RGB", GL_BLEND_DST_RGB);
	gpu_get_print("GL_BLEND_EQUATION_RGB", GL_BLEND_EQUATION_RGB);
	gpu_get_print("GL_BLEND_EQUATION_ALPHA", GL_BLEND_EQUATION_ALPHA);
	gpu_get_print("GL_BLEND_SRC_ALPHA", GL_BLEND_SRC_ALPHA);
	gpu_get_print("GL_BLEND_SRC_RGB", GL_BLEND_SRC_RGB);
	gpu_get_print("GL_BLUE_BIAS", GL_BLUE_BIAS);
	gpu_get_print("GL_BLUE_BITS", GL_BLUE_BITS);
	gpu_get_print("GL_BLUE_SCALE", GL_BLUE_SCALE);
	gpu_get_print("GL_CLIENT_ACTIVE_TEXTURE", GL_CLIENT_ACTIVE_TEXTURE);
	gpu_get_print("GL_CLIENT_ATTRIB_STACK_DEPTH", GL_CLIENT_ATTRIB_STACK_DEPTH);
	gpu_get_print("GL_CLIP_PLANE0", GL_CLIP_PLANE0);
	gpu_get_print("GL_COLOR_ARRAY", GL_COLOR_ARRAY);
	gpu_get_print("GL_COLOR_ARRAY_BUFFER_BINDING", GL_COLOR_ARRAY_BUFFER_BINDING);
	gpu_get_print("GL_COLOR_ARRAY_SIZE", GL_COLOR_ARRAY_SIZE);
	gpu_get_print("GL_COLOR_ARRAY_STRIDE", GL_COLOR_ARRAY_STRIDE);
	gpu_get_print("GL_COLOR_ARRAY_TYPE", GL_COLOR_ARRAY_TYPE);
	gpu_get_print("GL_COLOR_CLEAR_VALUE", GL_COLOR_CLEAR_VALUE);
	gpu_get_print("GL_COLOR_LOGIC_OP", GL_COLOR_LOGIC_OP);
	gpu_get_print("GL_COLOR_MATERIAL", GL_COLOR_MATERIAL);
	gpu_get_print("GL_COLOR_MATERIAL_FACE", GL_COLOR_MATERIAL_FACE);
	gpu_get_print("GL_COLOR_MATERIAL_PARAMETER", GL_COLOR_MATERIAL_PARAMETER);
	gpu_get_print("GL_COLOR_MATRIX", GL_COLOR_MATRIX);
	gpu_get_print("GL_COLOR_MATRIX_STACK_DEPTH", GL_COLOR_MATRIX_STACK_DEPTH);
	gpu_get_print("GL_COLOR_SUM", GL_COLOR_SUM);
	gpu_get_print("GL_COLOR_TABLE", GL_COLOR_TABLE);
	gpu_get_print("GL_COLOR_WRITEMASK", GL_COLOR_WRITEMASK);
	gpu_get_print("GL_COMPRESSED_TEXTURE_FORMATS", GL_COMPRESSED_TEXTURE_FORMATS);
	gpu_get_print("GL_CONVOLUTION_1D", GL_CONVOLUTION_1D);
	gpu_get_print("GL_CONVOLUTION_2D", GL_CONVOLUTION_2D);
	gpu_get_print("GL_CULL_FACE", GL_CULL_FACE);
	gpu_get_print("GL_CULL_FACE_MODE", GL_CULL_FACE_MODE);
	gpu_get_print("GL_CURRENT_COLOR", GL_CURRENT_COLOR);
	gpu_get_print("GL_CURRENT_FOG_COORD", GL_CURRENT_FOG_COORD);
	gpu_get_print("GL_CURRENT_INDEX", GL_CURRENT_INDEX);
	gpu_get_print("GL_CURRENT_NORMAL", GL_CURRENT_NORMAL);
	gpu_get_print("GL_CURRENT_PROGRAM", GL_CURRENT_PROGRAM);
	gpu_get_print("GL_CURRENT_RASTER_COLOR", GL_CURRENT_RASTER_COLOR);
	gpu_get_print("GL_CURRENT_RASTER_DISTANCE", GL_CURRENT_RASTER_DISTANCE);
	gpu_get_print("GL_CURRENT_RASTER_INDEX", GL_CURRENT_RASTER_INDEX);
	gpu_get_print("GL_CURRENT_RASTER_POSITION", GL_CURRENT_RASTER_POSITION);
	gpu_get_print("GL_CURRENT_RASTER_POSITION_VALID", GL_CURRENT_RASTER_POSITION_VALID);
	gpu_get_print("GL_CURRENT_RASTER_SECONDARY_COLOR", GL_CURRENT_RASTER_SECONDARY_COLOR);
	gpu_get_print("GL_CURRENT_RASTER_TEXTURE_COORDS", GL_CURRENT_RASTER_TEXTURE_COORDS);
	gpu_get_print("GL_CURRENT_SECONDARY_COLOR", GL_CURRENT_SECONDARY_COLOR);
	gpu_get_print("GL_CURRENT_TEXTURE_COORDS", GL_CURRENT_TEXTURE_COORDS);
	gpu_get_print("GL_DEPTH_BIAS", GL_DEPTH_BIAS);
	gpu_get_print("GL_DEPTH_BITS", GL_DEPTH_BITS);
	gpu_get_print("GL_DEPTH_CLEAR_VALUE", GL_DEPTH_CLEAR_VALUE);
	gpu_get_print("GL_DEPTH_FUNC", GL_DEPTH_FUNC);
	gpu_get_print("GL_DEPTH_RANGE", GL_DEPTH_RANGE);
	gpu_get_print("GL_DEPTH_SCALE", GL_DEPTH_SCALE);
	gpu_get_print("GL_DEPTH_TEST", GL_DEPTH_TEST);
	gpu_get_print("GL_DEPTH_WRITEMASK", GL_DEPTH_WRITEMASK);
	gpu_get_print("GL_DITHER", GL_DITHER);
	gpu_get_print("GL_DOUBLEBUFFER", GL_DOUBLEBUFFER);
	gpu_get_print("GL_DRAW_BUFFER", GL_DRAW_BUFFER);
	gpu_get_print("GL_DRAW_BUFFER0", GL_DRAW_BUFFER0);
	gpu_get_print("GL_EDGE_FLAG", GL_EDGE_FLAG);
	gpu_get_print("GL_EDGE_FLAG_ARRAY", GL_EDGE_FLAG_ARRAY);
	gpu_get_print("GL_EDGE_FLAG_ARRAY_BUFFER_BINDING", GL_EDGE_FLAG_ARRAY_BUFFER_BINDING);
	gpu_get_print("GL_EDGE_FLAG_ARRAY_STRIDE", GL_EDGE_FLAG_ARRAY_STRIDE);
	gpu_get_print("GL_ELEMENT_ARRAY_BUFFER_BINDING", GL_ELEMENT_ARRAY_BUFFER_BINDING);
	gpu_get_print("GL_FEEDBACK_BUFFER_SIZE", GL_FEEDBACK_BUFFER_SIZE);
	gpu_get_print("GL_FEEDBACK_BUFFER_TYPE", GL_FEEDBACK_BUFFER_TYPE);
	gpu_get_print("GL_FOG", GL_FOG);
	gpu_get_print("GL_FOG_COORD_ARRAY", GL_FOG_COORD_ARRAY);
	gpu_get_print("GL_FOG_COORD_ARRAY_BUFFER_BINDING", GL_FOG_COORD_ARRAY_BUFFER_BINDING);
	gpu_get_print("GL_FOG_COORD_ARRAY_STRIDE", GL_FOG_COORD_ARRAY_STRIDE);
	gpu_get_print("GL_FOG_COORD_ARRAY_TYPE", GL_FOG_COORD_ARRAY_TYPE);
	gpu_get_print("GL_FOG_COORD_SRC", GL_FOG_COORD_SRC);
	gpu_get_print("GL_FOG_COLOR", GL_FOG_COLOR);
	gpu_get_print("GL_FOG_DENSITY", GL_FOG_DENSITY);
	gpu_get_print("GL_FOG_END", GL_FOG_END);
	gpu_get_print("GL_FOG_HINT", GL_FOG_HINT);
	gpu_get_print("GL_FOG_INDEX", GL_FOG_INDEX);
	gpu_get_print("GL_FOG_MODE", GL_FOG_MODE);
	gpu_get_print("GL_FOG_START", GL_FOG_START);
	gpu_get_print("GL_FRAGMENT_SHADER_DERIVATIVE_HINT", GL_FRAGMENT_SHADER_DERIVATIVE_HINT);
	gpu_get_print("GL_FRONT_FACE", GL_FRONT_FACE);
	gpu_get_print("GL_GENERATE_MIPMAP_HINT", GL_GENERATE_MIPMAP_HINT);
	gpu_get_print("GL_GREEN_BIAS", GL_GREEN_BIAS);
	gpu_get_print("GL_GREEN_BITS", GL_GREEN_BITS);
	gpu_get_print("GL_GREEN_SCALE", GL_GREEN_SCALE);
	gpu_get_print("GL_HISTOGRAM", GL_HISTOGRAM);
	gpu_get_print("GL_INDEX_ARRAY", GL_INDEX_ARRAY);
	gpu_get_print("GL_INDEX_ARRAY_BUFFER_BINDING", GL_INDEX_ARRAY_BUFFER_BINDING);
	gpu_get_print("GL_INDEX_ARRAY_STRIDE", GL_INDEX_ARRAY_STRIDE);
	gpu_get_print("GL_INDEX_ARRAY_TYPE", GL_INDEX_ARRAY_TYPE);
	gpu_get_print("GL_INDEX_BITS", GL_INDEX_BITS);
	gpu_get_print("GL_INDEX_CLEAR_VALUE", GL_INDEX_CLEAR_VALUE);
	gpu_get_print("GL_INDEX_LOGIC_OP", GL_INDEX_LOGIC_OP);
	gpu_get_print("GL_INDEX_MODE", GL_INDEX_MODE);
	gpu_get_print("GL_INDEX_OFFSET", GL_INDEX_OFFSET);
	gpu_get_print("GL_INDEX_SHIFT", GL_INDEX_SHIFT);
	gpu_get_print("GL_INDEX_WRITEMASK", GL_INDEX_WRITEMASK);
	gpu_get_print("GL_LIGHT0", GL_LIGHT0);
	gpu_get_print("GL_LIGHTING", GL_LIGHTING);
	gpu_get_print("GL_LIGHT_MODEL_AMBIENT", GL_LIGHT_MODEL_AMBIENT);
	gpu_get_print("GL_LIGHT_MODEL_COLOR_CONTROL", GL_LIGHT_MODEL_COLOR_CONTROL);
	gpu_get_print("GL_LIGHT_MODEL_LOCAL_VIEWER", GL_LIGHT_MODEL_LOCAL_VIEWER);
	gpu_get_print("GL_LIGHT_MODEL_TWO_SIDE", GL_LIGHT_MODEL_TWO_SIDE);
	gpu_get_print("GL_LINE_SMOOTH", GL_LINE_SMOOTH);
	gpu_get_print("GL_LINE_SMOOTH_HINT", GL_LINE_SMOOTH_HINT);
	gpu_get_print("GL_LINE_STIPPLE", GL_LINE_STIPPLE);
	gpu_get_print("GL_LINE_STIPPLE_PATTERN", GL_LINE_STIPPLE_PATTERN);
	gpu_get_print("GL_LINE_STIPPLE_REPEAT", GL_LINE_STIPPLE_REPEAT);
	gpu_get_print("GL_LINE_WIDTH", GL_LINE_WIDTH);
	gpu_get_print("GL_LINE_WIDTH_GRANULARITY", GL_LINE_WIDTH_GRANULARITY);
	gpu_get_print("GL_LINE_WIDTH_RANGE", GL_LINE_WIDTH_RANGE);
	gpu_get_print("GL_LIST_BASE", GL_LIST_BASE);
	gpu_get_print("GL_LIST_INDEX", GL_LIST_INDEX);
	gpu_get_print("GL_LIST_MODE", GL_LIST_MODE);
	gpu_get_print("GL_LOGIC_OP_MODE", GL_LOGIC_OP_MODE);
	gpu_get_print("GL_MAP1_COLOR_4", GL_MAP1_COLOR_4);
	gpu_get_print("GL_MAP1_GRID_DOMAIN", GL_MAP1_GRID_DOMAIN);
	gpu_get_print("GL_MAP1_GRID_SEGMENTS", GL_MAP1_GRID_SEGMENTS);
	gpu_get_print("GL_MAP1_INDEX", GL_MAP1_INDEX);
	gpu_get_print("GL_MAP1_NORMAL", GL_MAP1_NORMAL);
	gpu_get_print("GL_MAP1_TEXTURE_COORD_1", GL_MAP1_TEXTURE_COORD_1);
	gpu_get_print("GL_MAP1_TEXTURE_COORD_2", GL_MAP1_TEXTURE_COORD_2);
	gpu_get_print("GL_MAP1_TEXTURE_COORD_3", GL_MAP1_TEXTURE_COORD_3);
	gpu_get_print("GL_MAP1_TEXTURE_COORD_4", GL_MAP1_TEXTURE_COORD_4);
	gpu_get_print("GL_MAP1_VERTEX_3", GL_MAP1_VERTEX_3);
	gpu_get_print("GL_MAP1_VERTEX_4", GL_MAP1_VERTEX_4);
	gpu_get_print("GL_MAP2_COLOR_4", GL_MAP2_COLOR_4);
	gpu_get_print("GL_MAP2_GRID_DOMAIN", GL_MAP2_GRID_DOMAIN);
	gpu_get_print("GL_MAP2_GRID_SEGMENTS", GL_MAP2_GRID_SEGMENTS);
	gpu_get_print("GL_MAP2_INDEX", GL_MAP2_INDEX);
	gpu_get_print("GL_MAP2_NORMAL", GL_MAP2_NORMAL);
	gpu_get_print("GL_MAP2_TEXTURE_COORD_1", GL_MAP2_TEXTURE_COORD_1);
	gpu_get_print("GL_MAP2_TEXTURE_COORD_2", GL_MAP2_TEXTURE_COORD_2);
	gpu_get_print("GL_MAP2_TEXTURE_COORD_3", GL_MAP2_TEXTURE_COORD_3);
	gpu_get_print("GL_MAP2_TEXTURE_COORD_4", GL_MAP2_TEXTURE_COORD_4);
	gpu_get_print("GL_MAP2_VERTEX_3", GL_MAP2_VERTEX_3);
	gpu_get_print("GL_MAP2_VERTEX_4", GL_MAP2_VERTEX_4);
	gpu_get_print("GL_MAP_COLOR", GL_MAP_COLOR);
	gpu_get_print("GL_MAP_STENCIL", GL_MAP_STENCIL);
	gpu_get_print("GL_MATRIX_MODE", GL_MATRIX_MODE);
	gpu_get_print("GL_MAX_3D_TEXTURE_SIZE", GL_MAX_3D_TEXTURE_SIZE);
	gpu_get_print("GL_MAX_CLIENT_ATTRIB_STACK_DEPTH", GL_MAX_CLIENT_ATTRIB_STACK_DEPTH);
	gpu_get_print("GL_MAX_ATTRIB_STACK_DEPTH", GL_MAX_ATTRIB_STACK_DEPTH);
	gpu_get_print("GL_MAX_CLIP_PLANES", GL_MAX_CLIP_PLANES);
	gpu_get_print("GL_MAX_COLOR_MATRIX_STACK_DEPTH", GL_MAX_COLOR_MATRIX_STACK_DEPTH);
	gpu_get_print("GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS", GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
	gpu_get_print("GL_MAX_CUBE_MAP_TEXTURE_SIZE", GL_MAX_CUBE_MAP_TEXTURE_SIZE);
	gpu_get_print("GL_MAX_DRAW_BUFFERS", GL_MAX_DRAW_BUFFERS);
	gpu_get_print("GL_MAX_ELEMENTS_INDICES", GL_MAX_ELEMENTS_INDICES);
	gpu_get_print("GL_MAX_ELEMENTS_VERTICES", GL_MAX_ELEMENTS_VERTICES);
	gpu_get_print("GL_MAX_EVAL_ORDER", GL_MAX_EVAL_ORDER);
	gpu_get_print("GL_MAX_FRAGMENT_UNIFORM_COMPONENTS", GL_MAX_FRAGMENT_UNIFORM_COMPONENTS);
	gpu_get_print("GL_MAX_LIGHTS", GL_MAX_LIGHTS);
	gpu_get_print("GL_MAX_LIST_NESTING", GL_MAX_LIST_NESTING);
	gpu_get_print("GL_MAX_MODELVIEW_STACK_DEPTH", GL_MAX_MODELVIEW_STACK_DEPTH);
	gpu_get_print("GL_MAX_NAME_STACK_DEPTH", GL_MAX_NAME_STACK_DEPTH);
	gpu_get_print("GL_MAX_PIXEL_MAP_TABLE", GL_MAX_PIXEL_MAP_TABLE);
	gpu_get_print("GL_MAX_PROJECTION_STACK_DEPTH", GL_MAX_PROJECTION_STACK_DEPTH);
	gpu_get_print("GL_MAX_TEXTURE_COORDS", GL_MAX_TEXTURE_COORDS);
	gpu_get_print("GL_MAX_TEXTURE_IMAGE_UNITS", GL_MAX_TEXTURE_IMAGE_UNITS);
	gpu_get_print("GL_MAX_TEXTURE_LOD_BIAS", GL_MAX_TEXTURE_LOD_BIAS);
	gpu_get_print("GL_MAX_TEXTURE_SIZE", GL_MAX_TEXTURE_SIZE);
	gpu_get_print("GL_MAX_TEXTURE_STACK_DEPTH", GL_MAX_TEXTURE_STACK_DEPTH);
	gpu_get_print("GL_MAX_TEXTURE_UNITS", GL_MAX_TEXTURE_UNITS);
	gpu_get_print("GL_MAX_VARYING_FLOATS", GL_MAX_VARYING_FLOATS);
	gpu_get_print("GL_MAX_VERTEX_ATTRIBS", GL_MAX_VERTEX_ATTRIBS);
	gpu_get_print("GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS", GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS);
	gpu_get_print("GL_MAX_VERTEX_UNIFORM_COMPONENTS", GL_MAX_VERTEX_UNIFORM_COMPONENTS);
	gpu_get_print("GL_MAX_VIEWPORT_DIMS", GL_MAX_VIEWPORT_DIMS);
	gpu_get_print("GL_MINMAX", GL_MINMAX);
	gpu_get_print("GL_MODELVIEW_MATRIX", GL_MODELVIEW_MATRIX);
	gpu_get_print("GL_MODELVIEW_STACK_DEPTH", GL_MODELVIEW_STACK_DEPTH);
	gpu_get_print("GL_NAME_STACK_DEPTH", GL_NAME_STACK_DEPTH);
	gpu_get_print("GL_NORMAL_ARRAY", GL_NORMAL_ARRAY);
	gpu_get_print("GL_NORMAL_ARRAY_BUFFER_BINDING", GL_NORMAL_ARRAY_BUFFER_BINDING);
	gpu_get_print("GL_NORMAL_ARRAY_STRIDE", GL_NORMAL_ARRAY_STRIDE);
	gpu_get_print("GL_NORMAL_ARRAY_TYPE", GL_NORMAL_ARRAY_TYPE);
	gpu_get_print("GL_NORMALIZE", GL_NORMALIZE);
	gpu_get_print("GL_NUM_COMPRESSED_TEXTURE_FORMATS", GL_NUM_COMPRESSED_TEXTURE_FORMATS);
	gpu_get_print("GL_PACK_ALIGNMENT", GL_PACK_ALIGNMENT);
	gpu_get_print("GL_PACK_IMAGE_HEIGHT", GL_PACK_IMAGE_HEIGHT);
	gpu_get_print("GL_PACK_LSB_FIRST", GL_PACK_LSB_FIRST);
	gpu_get_print("GL_PACK_ROW_LENGTH", GL_PACK_ROW_LENGTH);
	gpu_get_print("GL_PACK_SKIP_IMAGES", GL_PACK_SKIP_IMAGES);
	gpu_get_print("GL_PACK_SKIP_PIXELS", GL_PACK_SKIP_PIXELS);
	gpu_get_print("GL_PACK_SKIP_ROWS", GL_PACK_SKIP_ROWS);
	gpu_get_print("GL_PACK_SWAP_BYTES", GL_PACK_SWAP_BYTES);
	gpu_get_print("GL_PERSPECTIVE_CORRECTION_HINT", GL_PERSPECTIVE_CORRECTION_HINT);
	gpu_get_print("GL_PIXEL_MAP_A_TO_A_SIZE", GL_PIXEL_MAP_A_TO_A_SIZE);
	gpu_get_print("GL_PIXEL_MAP_B_TO_B_SIZE", GL_PIXEL_MAP_B_TO_B_SIZE);
	gpu_get_print("GL_PIXEL_MAP_G_TO_G_SIZE", GL_PIXEL_MAP_G_TO_G_SIZE);
	gpu_get_print("GL_PIXEL_MAP_I_TO_A_SIZE", GL_PIXEL_MAP_I_TO_A_SIZE);
	gpu_get_print("GL_PIXEL_MAP_I_TO_B_SIZE", GL_PIXEL_MAP_I_TO_B_SIZE);
	gpu_get_print("GL_PIXEL_MAP_I_TO_G_SIZE", GL_PIXEL_MAP_I_TO_G_SIZE);
	gpu_get_print("GL_PIXEL_MAP_I_TO_I_SIZE", GL_PIXEL_MAP_I_TO_I_SIZE);
	gpu_get_print("GL_PIXEL_MAP_I_TO_R_SIZE", GL_PIXEL_MAP_I_TO_R_SIZE);
	gpu_get_print("GL_PIXEL_MAP_R_TO_R_SIZE", GL_PIXEL_MAP_R_TO_R_SIZE);
	gpu_get_print("GL_PIXEL_MAP_S_TO_S_SIZE", GL_PIXEL_MAP_S_TO_S_SIZE);
	gpu_get_print("GL_PIXEL_PACK_BUFFER_BINDING", GL_PIXEL_PACK_BUFFER_BINDING);
	gpu_get_print("GL_PIXEL_UNPACK_BUFFER_BINDING", GL_PIXEL_UNPACK_BUFFER_BINDING);
	gpu_get_print("GL_POINT_DISTANCE_ATTENUATION", GL_POINT_DISTANCE_ATTENUATION);
	gpu_get_print("GL_POINT_FADE_THRESHOLD_SIZE", GL_POINT_FADE_THRESHOLD_SIZE);
	gpu_get_print("GL_POINT_SIZE", GL_POINT_SIZE);
	gpu_get_print("GL_POINT_SIZE_GRANULARITY", GL_POINT_SIZE_GRANULARITY);
	gpu_get_print("GL_POINT_SIZE_MAX", GL_POINT_SIZE_MAX);
	gpu_get_print("GL_POINT_SIZE_MIN", GL_POINT_SIZE_MIN);
	gpu_get_print("GL_POINT_SIZE_RANGE", GL_POINT_SIZE_RANGE);
	gpu_get_print("GL_POINT_SMOOTH", GL_POINT_SMOOTH);
	gpu_get_print("GL_POINT_SMOOTH_HINT", GL_POINT_SMOOTH_HINT);
	gpu_get_print("GL_POINT_SPRITE", GL_POINT_SPRITE);
	gpu_get_print("GL_POLYGON_MODE", GL_POLYGON_MODE);
	gpu_get_print("GL_POLYGON_OFFSET_FACTOR", GL_POLYGON_OFFSET_FACTOR);
	gpu_get_print("GL_POLYGON_OFFSET_UNITS", GL_POLYGON_OFFSET_UNITS);
	gpu_get_print("GL_POLYGON_OFFSET_FILL", GL_POLYGON_OFFSET_FILL);
	gpu_get_print("GL_POLYGON_OFFSET_LINE", GL_POLYGON_OFFSET_LINE);
	gpu_get_print("GL_POLYGON_OFFSET_POINT", GL_POLYGON_OFFSET_POINT);
	gpu_get_print("GL_POLYGON_SMOOTH", GL_POLYGON_SMOOTH);
	gpu_get_print("GL_POLYGON_SMOOTH_HINT", GL_POLYGON_SMOOTH_HINT);
	gpu_get_print("GL_POLYGON_STIPPLE", GL_POLYGON_STIPPLE);
	gpu_get_print("GL_POST_COLOR_MATRIX_COLOR_TABLE", GL_POST_COLOR_MATRIX_COLOR_TABLE);
	gpu_get_print("GL_POST_COLOR_MATRIX_RED_BIAS", GL_POST_COLOR_MATRIX_RED_BIAS);
	gpu_get_print("GL_POST_COLOR_MATRIX_GREEN_BIAS", GL_POST_COLOR_MATRIX_GREEN_BIAS);
	gpu_get_print("GL_POST_COLOR_MATRIX_BLUE_BIAS", GL_POST_COLOR_MATRIX_BLUE_BIAS);
	gpu_get_print("GL_POST_COLOR_MATRIX_ALPHA_BIAS", GL_POST_COLOR_MATRIX_ALPHA_BIAS);
	gpu_get_print("GL_POST_COLOR_MATRIX_RED_SCALE", GL_POST_COLOR_MATRIX_RED_SCALE);
	gpu_get_print("GL_POST_COLOR_MATRIX_GREEN_SCALE", GL_POST_COLOR_MATRIX_GREEN_SCALE);
	gpu_get_print("GL_POST_COLOR_MATRIX_BLUE_SCALE", GL_POST_COLOR_MATRIX_BLUE_SCALE);
	gpu_get_print("GL_POST_COLOR_MATRIX_ALPHA_SCALE", GL_POST_COLOR_MATRIX_ALPHA_SCALE);
	gpu_get_print("GL_POST_CONVOLUTION_COLOR_TABLE", GL_POST_CONVOLUTION_COLOR_TABLE);
	gpu_get_print("GL_POST_CONVOLUTION_RED_BIAS", GL_POST_CONVOLUTION_RED_BIAS);
	gpu_get_print("GL_POST_CONVOLUTION_GREEN_BIAS", GL_POST_CONVOLUTION_GREEN_BIAS);
	gpu_get_print("GL_POST_CONVOLUTION_BLUE_BIAS", GL_POST_CONVOLUTION_BLUE_BIAS);
	gpu_get_print("GL_POST_CONVOLUTION_ALPHA_BIAS", GL_POST_CONVOLUTION_ALPHA_BIAS);
	gpu_get_print("GL_POST_CONVOLUTION_RED_SCALE", GL_POST_CONVOLUTION_RED_SCALE);
	gpu_get_print("GL_POST_CONVOLUTION_GREEN_SCALE", GL_POST_CONVOLUTION_GREEN_SCALE);
	gpu_get_print("GL_POST_CONVOLUTION_BLUE_SCALE", GL_POST_CONVOLUTION_BLUE_SCALE);
	gpu_get_print("GL_POST_CONVOLUTION_ALPHA_SCALE", GL_POST_CONVOLUTION_ALPHA_SCALE);
	gpu_get_print("GL_PROJECTION_MATRIX", GL_PROJECTION_MATRIX);
	gpu_get_print("GL_PROJECTION_STACK_DEPTH", GL_PROJECTION_STACK_DEPTH);
	gpu_get_print("GL_READ_BUFFER", GL_READ_BUFFER);
	gpu_get_print("GL_RED_BIAS", GL_RED_BIAS);
	gpu_get_print("GL_RED_BITS", GL_RED_BITS);
	gpu_get_print("GL_RED_SCALE", GL_RED_SCALE);
	gpu_get_print("GL_RENDER_MODE", GL_RENDER_MODE);
	gpu_get_print("GL_RESCALE_NORMAL", GL_RESCALE_NORMAL);
	gpu_get_print("GL_RGBA_MODE", GL_RGBA_MODE);
	gpu_get_print("GL_SAMPLE_BUFFERS", GL_SAMPLE_BUFFERS);
	gpu_get_print("GL_SAMPLE_COVERAGE_VALUE", GL_SAMPLE_COVERAGE_VALUE);
	gpu_get_print("GL_SAMPLE_COVERAGE_INVERT", GL_SAMPLE_COVERAGE_INVERT);
	gpu_get_print("GL_SAMPLES", GL_SAMPLES);
	gpu_get_print("GL_SCISSOR_BOX", GL_SCISSOR_BOX);
	gpu_get_print("GL_SCISSOR_TEST", GL_SCISSOR_TEST);
	gpu_get_print("GL_SECONDARY_COLOR_ARRAY", GL_SECONDARY_COLOR_ARRAY);
	gpu_get_print("GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING", GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING);
	gpu_get_print("GL_SECONDARY_COLOR_ARRAY_SIZE", GL_SECONDARY_COLOR_ARRAY_SIZE);
	gpu_get_print("GL_SECONDARY_COLOR_ARRAY_STRIDE", GL_SECONDARY_COLOR_ARRAY_STRIDE);
	gpu_get_print("GL_SECONDARY_COLOR_ARRAY_TYPE", GL_SECONDARY_COLOR_ARRAY_TYPE);
	gpu_get_print("GL_SELECTION_BUFFER_SIZE", GL_SELECTION_BUFFER_SIZE);
	gpu_get_print("GL_SEPARABLE_2D", GL_SEPARABLE_2D);
	gpu_get_print("GL_SHADE_MODEL", GL_SHADE_MODEL);
	gpu_get_print("GL_SMOOTH_LINE_WIDTH_RANGE", GL_SMOOTH_LINE_WIDTH_RANGE);
	gpu_get_print("GL_SMOOTH_LINE_WIDTH_GRANULARITY", GL_SMOOTH_LINE_WIDTH_GRANULARITY);
	gpu_get_print("GL_SMOOTH_POINT_SIZE_RANGE", GL_SMOOTH_POINT_SIZE_RANGE);
	gpu_get_print("GL_SMOOTH_POINT_SIZE_GRANULARITY", GL_SMOOTH_POINT_SIZE_GRANULARITY);
	gpu_get_print("GL_STENCIL_BACK_FAIL", GL_STENCIL_BACK_FAIL);
	gpu_get_print("GL_STENCIL_BACK_FUNC", GL_STENCIL_BACK_FUNC);
	gpu_get_print("GL_STENCIL_BACK_PASS_DEPTH_FAIL", GL_STENCIL_BACK_PASS_DEPTH_FAIL);
	gpu_get_print("GL_STENCIL_BACK_PASS_DEPTH_PASS", GL_STENCIL_BACK_PASS_DEPTH_PASS);
	gpu_get_print("GL_STENCIL_BACK_REF", GL_STENCIL_BACK_REF);
	gpu_get_print("GL_STENCIL_BACK_VALUE_MASK", GL_STENCIL_BACK_VALUE_MASK);
	gpu_get_print("GL_STENCIL_BACK_WRITEMASK", GL_STENCIL_BACK_WRITEMASK);
	gpu_get_print("GL_STENCIL_BITS", GL_STENCIL_BITS);
	gpu_get_print("GL_STENCIL_CLEAR_VALUE", GL_STENCIL_CLEAR_VALUE);
	gpu_get_print("GL_STENCIL_FAIL", GL_STENCIL_FAIL);
	gpu_get_print("GL_STENCIL_FUNC", GL_STENCIL_FUNC);
	gpu_get_print("GL_STENCIL_PASS_DEPTH_FAIL", GL_STENCIL_PASS_DEPTH_FAIL);
	gpu_get_print("GL_STENCIL_PASS_DEPTH_PASS", GL_STENCIL_PASS_DEPTH_PASS);
	gpu_get_print("GL_STENCIL_REF", GL_STENCIL_REF);
	gpu_get_print("GL_STENCIL_TEST", GL_STENCIL_TEST);
	gpu_get_print("GL_STENCIL_VALUE_MASK", GL_STENCIL_VALUE_MASK);
	gpu_get_print("GL_STENCIL_WRITEMASK", GL_STENCIL_WRITEMASK);
	gpu_get_print("GL_STEREO", GL_STEREO);
	gpu_get_print("GL_SUBPIXEL_BITS", GL_SUBPIXEL_BITS);
	gpu_get_print("GL_TEXTURE_1D", GL_TEXTURE_1D);
	gpu_get_print("GL_TEXTURE_BINDING_1D", GL_TEXTURE_BINDING_1D);
	gpu_get_print("GL_TEXTURE_2D", GL_TEXTURE_2D);
	gpu_get_print("GL_TEXTURE_BINDING_2D", GL_TEXTURE_BINDING_2D);
	gpu_get_print("GL_TEXTURE_3D", GL_TEXTURE_3D);
	gpu_get_print("GL_TEXTURE_BINDING_3D", GL_TEXTURE_BINDING_3D);
	gpu_get_print("GL_TEXTURE_BINDING_CUBE_MAP", GL_TEXTURE_BINDING_CUBE_MAP);
	gpu_get_print("GL_TEXTURE_COMPRESSION_HINT", GL_TEXTURE_COMPRESSION_HINT);
	gpu_get_print("GL_TEXTURE_COORD_ARRAY", GL_TEXTURE_COORD_ARRAY);
	gpu_get_print("GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING", GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING);
	gpu_get_print("GL_TEXTURE_COORD_ARRAY_SIZE", GL_TEXTURE_COORD_ARRAY_SIZE);
	gpu_get_print("GL_TEXTURE_COORD_ARRAY_STRIDE", GL_TEXTURE_COORD_ARRAY_STRIDE);
	gpu_get_print("GL_TEXTURE_COORD_ARRAY_TYPE", GL_TEXTURE_COORD_ARRAY_TYPE);
	gpu_get_print("GL_TEXTURE_CUBE_MAP", GL_TEXTURE_CUBE_MAP);
	gpu_get_print("GL_TEXTURE_GEN_Q", GL_TEXTURE_GEN_Q);
	gpu_get_print("GL_TEXTURE_GEN_R", GL_TEXTURE_GEN_R);
	gpu_get_print("GL_TEXTURE_GEN_S", GL_TEXTURE_GEN_S);
	gpu_get_print("GL_TEXTURE_GEN_T", GL_TEXTURE_GEN_T);
	gpu_get_print("GL_TEXTURE_MATRIX", GL_TEXTURE_MATRIX);
	gpu_get_print("GL_TEXTURE_STACK_DEPTH", GL_TEXTURE_STACK_DEPTH);
	gpu_get_print("GL_TRANSPOSE_COLOR_MATRIX", GL_TRANSPOSE_COLOR_MATRIX);
	gpu_get_print("GL_TRANSPOSE_MODELVIEW_MATRIX", GL_TRANSPOSE_MODELVIEW_MATRIX);
	gpu_get_print("GL_TRANSPOSE_PROJECTION_MATRIX", GL_TRANSPOSE_PROJECTION_MATRIX);
	gpu_get_print("GL_TRANSPOSE_TEXTURE_MATRIX", GL_TRANSPOSE_TEXTURE_MATRIX);
	gpu_get_print("GL_UNPACK_ALIGNMENT", GL_UNPACK_ALIGNMENT);
	gpu_get_print("GL_UNPACK_IMAGE_HEIGHT", GL_UNPACK_IMAGE_HEIGHT);
	gpu_get_print("GL_UNPACK_LSB_FIRST", GL_UNPACK_LSB_FIRST);
	gpu_get_print("GL_UNPACK_ROW_LENGTH", GL_UNPACK_ROW_LENGTH);
	gpu_get_print("GL_UNPACK_SKIP_IMAGES", GL_UNPACK_SKIP_IMAGES);
	gpu_get_print("GL_UNPACK_SKIP_PIXELS", GL_UNPACK_SKIP_PIXELS);
	gpu_get_print("GL_UNPACK_SKIP_ROWS", GL_UNPACK_SKIP_ROWS);
	gpu_get_print("GL_UNPACK_SWAP_BYTES", GL_UNPACK_SWAP_BYTES);
	gpu_get_print("GL_VERTEX_ARRAY", GL_VERTEX_ARRAY);
	gpu_get_print("GL_VERTEX_ARRAY_BUFFER_BINDING", GL_VERTEX_ARRAY_BUFFER_BINDING);
	gpu_get_print("GL_VERTEX_ARRAY_SIZE", GL_VERTEX_ARRAY_SIZE);
	gpu_get_print("GL_VERTEX_ARRAY_STRIDE", GL_VERTEX_ARRAY_STRIDE);
	gpu_get_print("GL_VERTEX_ARRAY_TYPE", GL_VERTEX_ARRAY_TYPE);
	gpu_get_print("GL_VERTEX_PROGRAM_POINT_SIZE", GL_VERTEX_PROGRAM_POINT_SIZE);
	gpu_get_print("GL_VERTEX_PROGRAM_TWO_SIDE", GL_VERTEX_PROGRAM_TWO_SIDE);
	gpu_get_print("GL_VIEWPORT", GL_VIEWPORT);
	gpu_get_print("GL_ZOOM_X", GL_ZOOM_X);
	gpu_get_print("GL_ZOOM_Y", GL_ZOOM_Y);
#endif
}

#endif