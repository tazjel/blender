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
 */

#ifndef __SVM_TYPES_H__
#define __SVM_TYPES_H__

CCL_NAMESPACE_BEGIN

/* Stack */

/* SVM stack has a fixed size */
#define SVM_STACK_SIZE 255
/* SVM stack offsets with this value indicate that it's not on the stack */
#define SVM_STACK_INVALID 255 

/* Nodes */

typedef enum NodeType {
	NODE_END = 0,
	NODE_CLOSURE_BSDF,
	NODE_CLOSURE_EMISSION,
	NODE_CLOSURE_BACKGROUND,
	NODE_CLOSURE_SET_WEIGHT,
	NODE_CLOSURE_WEIGHT,
	NODE_MIX_CLOSURE,
	NODE_JUMP,
	NODE_TEX_IMAGE,
	NODE_TEX_IMAGE_BOX,
	NODE_TEX_SKY,
	NODE_GEOMETRY,
	NODE_GEOMETRY_DUPLI,
	NODE_LIGHT_PATH,
	NODE_VALUE_F,
	NODE_VALUE_V,
	NODE_MIX,
	NODE_ATTR,
	NODE_CONVERT,
	NODE_FRESNEL,
	NODE_EMISSION_WEIGHT,
	NODE_TEX_GRADIENT,
	NODE_TEX_VORONOI,
	NODE_TEX_MUSGRAVE,
	NODE_TEX_WAVE,
	NODE_TEX_MAGIC,
	NODE_TEX_NOISE,
	NODE_SHADER_JUMP,
	NODE_SET_DISPLACEMENT,
	NODE_GEOMETRY_BUMP_DX,
	NODE_GEOMETRY_BUMP_DY,
	NODE_SET_BUMP,
	NODE_MATH,
	NODE_VECTOR_MATH,
	NODE_MAPPING,
	NODE_TEX_COORD,
	NODE_TEX_COORD_BUMP_DX,
	NODE_TEX_COORD_BUMP_DY,
	NODE_ADD_CLOSURE,
	NODE_EMISSION_SET_WEIGHT_TOTAL,
	NODE_ATTR_BUMP_DX,
	NODE_ATTR_BUMP_DY,
	NODE_TEX_ENVIRONMENT,
	NODE_CLOSURE_HOLDOUT,
	NODE_LAYER_WEIGHT,
	NODE_CLOSURE_VOLUME,
	NODE_SEPARATE_RGB,
	NODE_COMBINE_RGB,
	NODE_HSV,
	NODE_CAMERA,
	NODE_INVERT,
	NODE_NORMAL,
	NODE_GAMMA,
	NODE_TEX_CHECKER,
	NODE_BRIGHTCONTRAST,
	NODE_RGB_RAMP,
	NODE_RGB_CURVES,
	NODE_MIN_MAX,
	NODE_LIGHT_FALLOFF,
	NODE_OBJECT_INFO,
	NODE_PARTICLE_INFO,
	NODE_TEX_BRICK,
	NODE_CLOSURE_SET_NORMAL,
} NodeType;

typedef enum NodeAttributeType {
	NODE_ATTR_FLOAT = 0,
	NODE_ATTR_FLOAT3
} NodeAttributeType;

typedef enum NodeGeometry {
	NODE_GEOM_P = 0,
	NODE_GEOM_N,
	NODE_GEOM_T,
	NODE_GEOM_I,
	NODE_GEOM_Ng,
	NODE_GEOM_uv
} NodeGeometry;

typedef enum NodeObjectInfo {
	NODE_INFO_OB_LOCATION,
	NODE_INFO_OB_INDEX,
	NODE_INFO_MAT_INDEX,
	NODE_INFO_OB_RANDOM
} NodeObjectInfo;

typedef enum NodeParticleInfo {
	NODE_INFO_PAR_INDEX,
	NODE_INFO_PAR_AGE,
	NODE_INFO_PAR_LIFETIME,
	NODE_INFO_PAR_LOCATION,
	NODE_INFO_PAR_ROTATION,
	NODE_INFO_PAR_SIZE,
	NODE_INFO_PAR_VELOCITY,
	NODE_INFO_PAR_ANGULAR_VELOCITY
} NodeParticleInfo;

typedef enum NodeLightPath {
	NODE_LP_camera = 0,
	NODE_LP_shadow,
	NODE_LP_diffuse,
	NODE_LP_glossy,
	NODE_LP_singular,
	NODE_LP_reflection,
	NODE_LP_transmission,
	NODE_LP_backfacing,
	NODE_LP_ray_length
} NodeLightPath;

typedef enum NodeLightFalloff {
	NODE_LIGHT_FALLOFF_QUADRATIC,
	NODE_LIGHT_FALLOFF_LINEAR,
	NODE_LIGHT_FALLOFF_CONSTANT
} NodeLightFalloff;

typedef enum NodeTexCoord {
	NODE_TEXCO_NORMAL,
	NODE_TEXCO_OBJECT,
	NODE_TEXCO_CAMERA,
	NODE_TEXCO_WINDOW,
	NODE_TEXCO_REFLECTION,
	NODE_TEXCO_DUPLI_GENERATED,
	NODE_TEXCO_DUPLI_UV
} NodeTexCoord;

typedef enum NodeMix {
	NODE_MIX_BLEND = 0,
	NODE_MIX_ADD,
	NODE_MIX_MUL,
	NODE_MIX_SUB,
	NODE_MIX_SCREEN,
	NODE_MIX_DIV,
	NODE_MIX_DIFF,
	NODE_MIX_DARK,
	NODE_MIX_LIGHT,
	NODE_MIX_OVERLAY,
	NODE_MIX_DODGE,
	NODE_MIX_BURN,
	NODE_MIX_HUE,
	NODE_MIX_SAT,
	NODE_MIX_VAL,
	NODE_MIX_COLOR,
	NODE_MIX_SOFT,
	NODE_MIX_LINEAR,
	NODE_MIX_CLAMP /* used for the clamp UI option */
} NodeMix;

typedef enum NodeMath {
	NODE_MATH_ADD,
	NODE_MATH_SUBTRACT,
	NODE_MATH_MULTIPLY,
	NODE_MATH_DIVIDE,
	NODE_MATH_SINE,
	NODE_MATH_COSINE,
	NODE_MATH_TANGENT,
	NODE_MATH_ARCSINE,
	NODE_MATH_ARCCOSINE,
	NODE_MATH_ARCTANGENT,
	NODE_MATH_POWER,
	NODE_MATH_LOGARITHM,
	NODE_MATH_MINIMUM,
	NODE_MATH_MAXIMUM,
	NODE_MATH_ROUND,
	NODE_MATH_LESS_THAN,
	NODE_MATH_GREATER_THAN,
	NODE_MATH_CLAMP /* used for the clamp UI option */
} NodeMath;

typedef enum NodeVectorMath {
	NODE_VECTOR_MATH_ADD,
	NODE_VECTOR_MATH_SUBTRACT,
	NODE_VECTOR_MATH_AVERAGE,
	NODE_VECTOR_MATH_DOT_PRODUCT,
	NODE_VECTOR_MATH_CROSS_PRODUCT,
	NODE_VECTOR_MATH_NORMALIZE
} NodeVectorMath;

typedef enum NodeConvert {
	NODE_CONVERT_FV,
	NODE_CONVERT_CF,
	NODE_CONVERT_VF
} NodeConvert;

typedef enum NodeDistanceMetric {
	NODE_VORONOI_DISTANCE_SQUARED,
	NODE_VORONOI_ACTUAL_DISTANCE,
	NODE_VORONOI_MANHATTAN,
	NODE_VORONOI_CHEBYCHEV,
	NODE_VORONOI_MINKOVSKY_H,
	NODE_VORONOI_MINKOVSKY_4,
	NODE_VORONOI_MINKOVSKY
} NodeDistanceMetric;

typedef enum NodeNoiseBasis {
	NODE_NOISE_PERLIN,
	NODE_NOISE_VORONOI_F1,
	NODE_NOISE_VORONOI_F2,
	NODE_NOISE_VORONOI_F3,
	NODE_NOISE_VORONOI_F4,
	NODE_NOISE_VORONOI_F2_F1,
	NODE_NOISE_VORONOI_CRACKLE,
	NODE_NOISE_CELL_NOISE
} NodeNoiseBasis;

typedef enum NodeWaveBasis {
	NODE_WAVE_SINE,
	NODE_WAVE_SAW,
	NODE_WAVE_TRI
} NodeWaveBasis;

typedef enum NodeMusgraveType {
	NODE_MUSGRAVE_MULTIFRACTAL,
	NODE_MUSGRAVE_FBM,
	NODE_MUSGRAVE_HYBRID_MULTIFRACTAL,
	NODE_MUSGRAVE_RIDGED_MULTIFRACTAL,
	NODE_MUSGRAVE_HETERO_TERRAIN
} NodeMusgraveType;

typedef enum NodeWaveType {
	NODE_WAVE_BANDS,
	NODE_WAVE_RINGS
} NodeWaveType;

typedef enum NodeGradientType {
	NODE_BLEND_LINEAR,
	NODE_BLEND_QUADRATIC,
	NODE_BLEND_EASING,
	NODE_BLEND_DIAGONAL,
	NODE_BLEND_RADIAL,
	NODE_BLEND_QUADRATIC_SPHERE,
	NODE_BLEND_SPHERICAL
} NodeGradientType;

typedef enum NodeVoronoiColoring {
	NODE_VORONOI_INTENSITY,
	NODE_VORONOI_CELLS
} NodeVoronoiColoring;

typedef enum NodeBlendWeightType {
	NODE_LAYER_WEIGHT_FRESNEL,
	NODE_LAYER_WEIGHT_FACING
} NodeBlendWeightType;

typedef enum ShaderType {
	SHADER_TYPE_SURFACE,
	SHADER_TYPE_VOLUME,
	SHADER_TYPE_DISPLACEMENT
} ShaderType;

/* Closure */

typedef enum ClosureType {
	CLOSURE_BSDF_ID,

	CLOSURE_BSDF_DIFFUSE_ID,
	CLOSURE_BSDF_OREN_NAYAR_ID,

	CLOSURE_BSDF_GLOSSY_ID,
	CLOSURE_BSDF_REFLECTION_ID,
	CLOSURE_BSDF_MICROFACET_GGX_ID,
	CLOSURE_BSDF_MICROFACET_BECKMANN_ID,
	CLOSURE_BSDF_WARD_ID,
	CLOSURE_BSDF_ASHIKHMIN_VELVET_ID,
	CLOSURE_BSDF_WESTIN_SHEEN_ID,

	CLOSURE_BSDF_TRANSMISSION_ID,
	CLOSURE_BSDF_TRANSLUCENT_ID,
	CLOSURE_BSDF_REFRACTION_ID,
	CLOSURE_BSDF_WESTIN_BACKSCATTER_ID,
	CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID,
	CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID,
	CLOSURE_BSDF_GLASS_ID,

	CLOSURE_BSDF_TRANSPARENT_ID,

	CLOSURE_BSSRDF_CUBIC_ID,
	CLOSURE_EMISSION_ID,
	CLOSURE_DEBUG_ID,
	CLOSURE_BACKGROUND_ID,
	CLOSURE_HOLDOUT_ID,
	CLOSURE_SUBSURFACE_ID,

	CLOSURE_VOLUME_ID,
	CLOSURE_VOLUME_TRANSPARENT_ID,
	CLOSURE_VOLUME_ISOTROPIC_ID,

	NBUILTIN_CLOSURES
} ClosureType;

/* watch this, being lazy with memory usage */
#define CLOSURE_IS_BSDF(type) (type <= CLOSURE_BSDF_TRANSPARENT_ID)
#define CLOSURE_IS_BSDF_DIFFUSE(type) (type >= CLOSURE_BSDF_DIFFUSE_ID && type <= CLOSURE_BSDF_OREN_NAYAR_ID)
#define CLOSURE_IS_BSDF_GLOSSY(type) (type >= CLOSURE_BSDF_GLOSSY_ID && type <= CLOSURE_BSDF_WESTIN_SHEEN_ID)
#define CLOSURE_IS_BSDF_TRANSMISSION(type) (type >= CLOSURE_BSDF_TRANSMISSION_ID && type <= CLOSURE_BSDF_GLASS_ID)
#define CLOSURE_IS_VOLUME(type) (type >= CLOSURE_VOLUME_ID && type <= CLOSURE_VOLUME_ISOTROPIC_ID)
#define CLOSURE_IS_EMISSION(type) (type == CLOSURE_EMISSION_ID)
#define CLOSURE_IS_HOLDOUT(type) (type == CLOSURE_HOLDOUT_ID)
#define CLOSURE_IS_BACKGROUND(type) (type == CLOSURE_BACKGROUND_ID)

CCL_NAMESPACE_END

#endif /*  __SVM_TYPES_H__ */

