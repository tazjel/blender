#ifndef __BL_MATERIAL_H__
#define __BL_MATERIAL_H__

#include "STR_String.h"
#include "MT_Point2.h"

// --
struct MTex;
struct Material;
struct Image;
struct TFace;
struct MTex;
struct Material;
struct EnvMap;
// --

/** max units
	this will default to users available units
	to build with more available, just increment this value
	although the more you add the slower the search time will be.
	we will go for three, which should be enough
*/
#define MAXTEX			3//match in RAS_TexVert

// different mapping modes
class BL_Mapping
{
public:
	int mapping;
	float scale[3];
	float offsets[3];
	int projplane[3];
	STR_String objconame;
};

// base material struct
class BL_Material
{
private:
	unsigned int rgb[4];
	MT_Point2 uv[4];
public:
	// -----------------------------------
	BL_Material();

	int IdMode;
	unsigned int ras_mode;

	STR_String texname[MAXTEX];
	unsigned int flag[MAXTEX];
	int tile,tilexrep[MAXTEX],tileyrep[MAXTEX];
	STR_String matname;
	STR_String mtexname[MAXTEX];

	float matcolor[4];
	float speccolor[3];
	short transp, pad;

	float hard, spec_f;
	float alpha, emit, color_blend[MAXTEX], ref;
	float amb;

	int blend_mode[MAXTEX];

	int	 mode;
	int num_enabled;
	
	int material_index;

	BL_Mapping	mapping[MAXTEX];
	STR_String	imageId[MAXTEX];


	Material*			material;
	TFace*				tface;
	Image*				img[MAXTEX];
	EnvMap*				cubemap[MAXTEX];
	
	void SetConversionRGB(unsigned int *rgb);
	void GetConversionRGB(unsigned int *rgb);

	void SetConversionUV(MT_Point2 *uv);
	void GetConversionUV(MT_Point2 *uv);

};

// BL_Material::IdMode
enum BL_IdMode {
	DEFAULT_BLENDER=-1,
	TEXFACE,
	ONETEX,
	TWOTEX,
	GREATERTHAN2
};

// BL_Material::blend_mode[index]
enum BL_BlendMode
{
	BLEND_MIX=1,
	BLEND_ADD,
	BLEND_SUB,
	BLEND_MUL,
	BLEND_SCR
};

// -------------------------------------
// BL_Material::flag[index]
enum BL_flag
{
	MIPMAP=1,		// set to use mipmaps
	CALCALPHA=2,	// additive
	USEALPHA=4,		// use actual alpha channel
	TEXALPHA=8,		// use alpha combiner functions
	TEXNEG=16,		// negate blending
	HASIPO=32
};

// BL_Material::ras_mode
enum BL_ras_mode
{
	POLY_VIS=1,
	COLLIDER=2,
	ZSORT=4,
	TRANSP=8,
	TRIANGLE=16,
	USE_LIGHT=32,
	WIRE=64
};

// -------------------------------------
// BL_Material::mapping[index]::mapping
enum BL_MappingFlag
{
	USEREFL=1,
	USEENV=2,
	USEOBJ=4
};

// BL_Material::BL_Mapping::projplane
enum BL_MappingProj
{
	PROJN=0,
	PROJX,
	PROJY,
	PROJZ
};

// ------------------------------------
//extern void initBL_Material(BL_Material* mat);
extern MTex* getImageFromMaterial(Material *mat, int index);
extern int  getNumTexChannels( Material *mat );
// ------------------------------------

#endif


