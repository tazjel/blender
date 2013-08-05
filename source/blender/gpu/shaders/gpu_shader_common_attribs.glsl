/* begin known attributes */

attribute vec4 b_Vertex;

#if !defined(USE_MATERIAL_COLOR)
attribute vec4 b_Color;
#endif

#if defined(USE_LIGHTING)
attribute vec3 b_Normal;
#endif

#if defined(USE_TEXTURE)

#if GPU_MAX_COMMON_TEXCOORDS > 0
attribute vec4 b_MultiTexCoord0;
#endif

#if GPU_MAX_COMMON_TEXCOORDS > 1
attribute vec4 b_MultiTexCoord1;
#endif

#if GPU_MAX_COMMON_TEXCOORDS > 2
attribute vec4 b_MultiTexCoord2;
#endif

#if GPU_MAX_COMMON_TEXCOORDS > 3
attribute vec4 b_MultiTexCoord3;
#endif

#if GPU_MAX_COMMON_TEXCOORDS > 4
attribute vec4 b_MultiTexCoord4;
#endif

#if GPU_MAX_COMMON_TEXCOORDS > 5
attribute vec4 b_MultiTexCoord5;
#endif

#if GPU_MAX_COMMON_TEXCOORDS > 6
attribute vec4 b_MultiTexCoord6;
#endif

#if GPU_MAX_COMMON_TEXCOORDS > 7
attribute vec4 b_MultiTexCoord7;
#endif

#endif

/* end known attributes */

