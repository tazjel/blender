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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/brush.c
 *  \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_rand.h"

#include "BKE_brush.h"
#include "BKE_colortools.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_paint.h"
#include "BKE_texture.h"
#include "BKE_icons.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "RE_render_ext.h" /* externtex */
#include "RE_shader_ext.h"

static void brush_defaults(Brush *brush)
{
	brush->blend = 0;
	brush->flag = 0;

	brush->ob_mode = OB_MODE_ALL_PAINT;

	/* BRUSH SCULPT TOOL SETTINGS */
	brush->weight = 1.0f; /* weight of brush 0 - 1.0 */
	brush->size = 35; /* radius of the brush in pixels */
	brush->alpha = 0.5f; /* brush strength/intensity probably variable should be renamed? */
	brush->autosmooth_factor = 0.0f;
	brush->crease_pinch_factor = 0.5f;
	brush->sculpt_plane = SCULPT_DISP_DIR_AREA;
	brush->plane_offset = 0.0f; /* how far above or below the plane that is found by averaging the faces */
	brush->plane_trim = 0.5f;
	brush->clone.alpha = 0.5f;
	brush->normal_weight = 0.0f;
	brush->flag |= BRUSH_ALPHA_PRESSURE;

	/* BRUSH PAINT TOOL SETTINGS */
	brush->rgb[0] = 1.0f; /* default rgb color of the brush when painting - white */
	brush->rgb[1] = 1.0f;
	brush->rgb[2] = 1.0f;

	/* BRUSH STROKE SETTINGS */
	brush->flag |= (BRUSH_SPACE | BRUSH_SPACE_ATTEN);
	brush->spacing = 10; /* how far each brush dot should be spaced as a percentage of brush diameter */

	brush->smooth_stroke_radius = 75;
	brush->smooth_stroke_factor = 0.9f;

	brush->rate = 0.1f; /* time delay between dots of paint or sculpting when doing airbrush mode */

	brush->jitter = 0.0f;

	/* BRUSH TEXTURE SETTINGS */
	default_mtex(&brush->mtex);

	brush->texture_sample_bias = 0; /* value to added to texture samples */
	brush->texture_overlay_alpha = 33;

	/* brush appearance  */

	brush->add_col[0] = 1.00; /* add mode color is light red */
	brush->add_col[1] = 0.39;
	brush->add_col[2] = 0.39;

	brush->sub_col[0] = 0.39; /* subtract mode color is light blue */
	brush->sub_col[1] = 0.39;
	brush->sub_col[2] = 1.00;
}

/* Datablock add/copy/free/make_local */

Brush *BKE_brush_add(Main *bmain, const char *name)
{
	Brush *brush;

	brush = BKE_libblock_alloc(&bmain->brush, ID_BR, name);

	/* enable fake user by default */
	brush->id.flag |= LIB_FAKEUSER;

	brush_defaults(brush);

	brush->sculpt_tool = SCULPT_TOOL_DRAW; /* sculpting defaults to the draw tool for new brushes */

	/* the default alpha falloff curve */
	BKE_brush_curve_preset(brush, CURVE_PRESET_SMOOTH);

	return brush;
}

Brush *BKE_brush_copy(Brush *brush)
{
	Brush *brushn;
	
	brushn = BKE_libblock_copy(&brush->id);

	if (brush->mtex.tex)
		id_us_plus((ID *)brush->mtex.tex);

	if (brush->icon_imbuf)
		brushn->icon_imbuf = IMB_dupImBuf(brush->icon_imbuf);

	brushn->preview = NULL;

	brushn->curve = curvemapping_copy(brush->curve);

	/* enable fake user by default */
	if (!(brushn->id.flag & LIB_FAKEUSER)) {
		brushn->id.flag |= LIB_FAKEUSER;
		brushn->id.us++;
	}
	
	return brushn;
}

/* not brush itself */
void BKE_brush_free(Brush *brush)
{
	if (brush->mtex.tex)
		brush->mtex.tex->id.us--;

	if (brush->icon_imbuf)
		IMB_freeImBuf(brush->icon_imbuf);

	BKE_previewimg_free(&(brush->preview));

	curvemapping_free(brush->curve);
}

static void extern_local_brush(Brush *brush)
{
	id_lib_extern((ID *)brush->mtex.tex);
	id_lib_extern((ID *)brush->clone.image);
}

void BKE_brush_make_local(Brush *brush)
{

	/* - only lib users: do nothing
	 * - only local users: set flag
	 * - mixed: make copy
	 */

	Main *bmain = G.main;
	Scene *scene;
	int is_local = FALSE, is_lib = FALSE;

	if (brush->id.lib == NULL) return;

	if (brush->clone.image) {
		/* special case: ima always local immediately. Clone image should only
		 * have one user anyway. */
		id_clear_lib_data(bmain, &brush->clone.image->id);
		extern_local_brush(brush);
	}

	for (scene = bmain->scene.first; scene && ELEM(0, is_lib, is_local); scene = scene->id.next) {
		if (paint_brush(&scene->toolsettings->imapaint.paint) == brush) {
			if (scene->id.lib) is_lib = TRUE;
			else is_local = TRUE;
		}
	}

	if (is_local && is_lib == FALSE) {
		id_clear_lib_data(bmain, &brush->id);
		extern_local_brush(brush);

		/* enable fake user by default */
		if (!(brush->id.flag & LIB_FAKEUSER)) {
			brush->id.flag |= LIB_FAKEUSER;
			brush->id.us++;
		}
	}
	else if (is_local && is_lib) {
		Brush *brush_new = BKE_brush_copy(brush);
		brush_new->id.us = 1; /* only keep fake user */
		brush_new->id.flag |= LIB_FAKEUSER;

		/* Remap paths of new ID using old library as base. */
		BKE_id_lib_local_paths(bmain, brush->id.lib, &brush_new->id);
		
		for (scene = bmain->scene.first; scene; scene = scene->id.next) {
			if (paint_brush(&scene->toolsettings->imapaint.paint) == brush) {
				if (scene->id.lib == NULL) {
					paint_brush_set(&scene->toolsettings->imapaint.paint, brush_new);
				}
			}
		}
	}
}

void BKE_brush_debug_print_state(Brush *br)
{
	/* create a fake brush and set it to the defaults */
	Brush def = {{NULL}};
	brush_defaults(&def);
	
#define BR_TEST(field, t)					\
	if (br->field != def.field)				\
		printf("br->" #field " = %" #t ";\n", br->field)

#define BR_TEST_FLAG(_f)							\
	if ((br->flag & _f) && !(def.flag & _f))		\
		printf("br->flag |= " #_f ";\n");			\
	else if (!(br->flag & _f) && (def.flag & _f))	\
		printf("br->flag &= ~" #_f ";\n")
	

	/* print out any non-default brush state */
	BR_TEST(normal_weight, f);

	BR_TEST(blend, d);
	BR_TEST(size, d);

	/* br->flag */
	BR_TEST_FLAG(BRUSH_AIRBRUSH);
	BR_TEST_FLAG(BRUSH_TORUS);
	BR_TEST_FLAG(BRUSH_ALPHA_PRESSURE);
	BR_TEST_FLAG(BRUSH_SIZE_PRESSURE);
	BR_TEST_FLAG(BRUSH_JITTER_PRESSURE);
	BR_TEST_FLAG(BRUSH_SPACING_PRESSURE);
	BR_TEST_FLAG(BRUSH_RAKE);
	BR_TEST_FLAG(BRUSH_ANCHORED);
	BR_TEST_FLAG(BRUSH_DIR_IN);
	BR_TEST_FLAG(BRUSH_SPACE);
	BR_TEST_FLAG(BRUSH_SMOOTH_STROKE);
	BR_TEST_FLAG(BRUSH_PERSISTENT);
	BR_TEST_FLAG(BRUSH_ACCUMULATE);
	BR_TEST_FLAG(BRUSH_LOCK_ALPHA);
	BR_TEST_FLAG(BRUSH_ORIGINAL_NORMAL);
	BR_TEST_FLAG(BRUSH_OFFSET_PRESSURE);
	BR_TEST_FLAG(BRUSH_SPACE_ATTEN);
	BR_TEST_FLAG(BRUSH_ADAPTIVE_SPACE);
	BR_TEST_FLAG(BRUSH_LOCK_SIZE);
	BR_TEST_FLAG(BRUSH_TEXTURE_OVERLAY);
	BR_TEST_FLAG(BRUSH_EDGE_TO_EDGE);
	BR_TEST_FLAG(BRUSH_RESTORE_MESH);
	BR_TEST_FLAG(BRUSH_INVERSE_SMOOTH_PRESSURE);
	BR_TEST_FLAG(BRUSH_RANDOM_ROTATION);
	BR_TEST_FLAG(BRUSH_PLANE_TRIM);
	BR_TEST_FLAG(BRUSH_FRONTFACE);
	BR_TEST_FLAG(BRUSH_CUSTOM_ICON);

	BR_TEST(jitter, f);
	BR_TEST(spacing, d);
	BR_TEST(smooth_stroke_radius, d);
	BR_TEST(smooth_stroke_factor, f);
	BR_TEST(rate, f);

	BR_TEST(alpha, f);

	BR_TEST(sculpt_plane, d);

	BR_TEST(plane_offset, f);

	BR_TEST(autosmooth_factor, f);

	BR_TEST(crease_pinch_factor, f);

	BR_TEST(plane_trim, f);

	BR_TEST(texture_sample_bias, f);
	BR_TEST(texture_overlay_alpha, d);

	BR_TEST(add_col[0], f);
	BR_TEST(add_col[1], f);
	BR_TEST(add_col[2], f);
	BR_TEST(sub_col[0], f);
	BR_TEST(sub_col[1], f);
	BR_TEST(sub_col[2], f);

	printf("\n");
	
#undef BR_TEST
#undef BR_TEST_FLAG
}

void BKE_brush_sculpt_reset(Brush *br)
{
	/* enable this to see any non-default
	 * settings used by a brush: */
	// BKE_brush_debug_print_state(br);

	brush_defaults(br);
	BKE_brush_curve_preset(br, CURVE_PRESET_SMOOTH);

	switch (br->sculpt_tool) {
		case SCULPT_TOOL_CLAY:
			br->flag |= BRUSH_FRONTFACE;
			break;
		case SCULPT_TOOL_CREASE:
			br->flag |= BRUSH_DIR_IN;
			br->alpha = 0.25;
			break;
		case SCULPT_TOOL_FILL:
			br->add_col[1] = 1;
			br->sub_col[0] = 0.25;
			br->sub_col[1] = 1;
			break;
		case SCULPT_TOOL_FLATTEN:
			br->add_col[1] = 1;
			br->sub_col[0] = 0.25;
			br->sub_col[1] = 1;
			break;
		case SCULPT_TOOL_INFLATE:
			br->add_col[0] = 0.750000;
			br->add_col[1] = 0.750000;
			br->add_col[2] = 0.750000;
			br->sub_col[0] = 0.250000;
			br->sub_col[1] = 0.250000;
			br->sub_col[2] = 0.250000;
			break;
		case SCULPT_TOOL_NUDGE:
			br->add_col[0] = 0.250000;
			br->add_col[1] = 1.000000;
			br->add_col[2] = 0.250000;
			break;
		case SCULPT_TOOL_PINCH:
			br->add_col[0] = 0.750000;
			br->add_col[1] = 0.750000;
			br->add_col[2] = 0.750000;
			br->sub_col[0] = 0.250000;
			br->sub_col[1] = 0.250000;
			br->sub_col[2] = 0.250000;
			break;
		case SCULPT_TOOL_SCRAPE:
			br->add_col[1] = 1.000000;
			br->sub_col[0] = 0.250000;
			br->sub_col[1] = 1.000000;
			break;
		case SCULPT_TOOL_ROTATE:
			br->alpha = 1.0;
			break;
		case SCULPT_TOOL_SMOOTH:
			br->flag &= ~BRUSH_SPACE_ATTEN;
			br->spacing = 5;
			br->add_col[0] = 0.750000;
			br->add_col[1] = 0.750000;
			br->add_col[2] = 0.750000;
			break;
		case SCULPT_TOOL_GRAB:
		case SCULPT_TOOL_SNAKE_HOOK:
		case SCULPT_TOOL_THUMB:
			br->size = 75;
			br->flag &= ~BRUSH_ALPHA_PRESSURE;
			br->flag &= ~BRUSH_SPACE;
			br->flag &= ~BRUSH_SPACE_ATTEN;
			br->add_col[0] = 0.250000;
			br->add_col[1] = 1.000000;
			br->add_col[2] = 0.250000;
			break;
		default:
			break;
	}
}

/* Library Operations */
void BKE_brush_curve_preset(Brush *b, /*CurveMappingPreset*/ int preset)
{
	CurveMap *cm = NULL;

	if (!b->curve)
		b->curve = curvemapping_add(1, 0, 0, 1, 1);

	cm = b->curve->cm;
	cm->flag &= ~CUMA_EXTEND_EXTRAPOLATE;

	b->curve->preset = preset;
	curvemap_reset(cm, &b->curve->clipr, b->curve->preset, CURVEMAP_SLOPE_NEGATIVE);
	curvemapping_changed(b->curve, FALSE);
}

int BKE_brush_texture_set_nr(Brush *brush, int nr)
{
	ID *idtest, *id = NULL;

	id = (ID *)brush->mtex.tex;

	idtest = (ID *)BLI_findlink(&G.main->tex, nr - 1);
	if (idtest == NULL) { /* new tex */
		if (id) idtest = (ID *)BKE_texture_copy((Tex *)id);
		else idtest = (ID *)add_texture(G.main, "Tex");
		idtest->us--;
	}
	if (idtest != id) {
		BKE_brush_texture_delete(brush);

		brush->mtex.tex = (Tex *)idtest;
		id_us_plus(idtest);

		return 1;
	}

	return 0;
}

int BKE_brush_texture_delete(Brush *brush)
{
	if (brush->mtex.tex)
		brush->mtex.tex->id.us--;

	return 1;
}

int BKE_brush_clone_image_set_nr(Brush *brush, int nr)
{
	if (brush && nr > 0) {
		Image *ima = (Image *)BLI_findlink(&G.main->image, nr - 1);

		if (ima) {
			BKE_brush_clone_image_delete(brush);
			brush->clone.image = ima;
			id_us_plus(&ima->id);
			brush->clone.offset[0] = brush->clone.offset[1] = 0.0f;

			return 1;
		}
	}

	return 0;
}

int BKE_brush_clone_image_delete(Brush *brush)
{
	if (brush && brush->clone.image) {
		brush->clone.image->id.us--;
		brush->clone.image = NULL;
		return 1;
	}

	return 0;
}

/* Generic texture sampler for 3D painting systems. point has to be either in
 * region space mouse coordinates, or 3d world coordinates for 3D mapping */
float BKE_brush_sample_tex_3D(const Scene *scene, Brush *br,
                              const float point[3],
                              float rgba[4], const int thread,
                              struct ImagePool *pool)
{
	UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
	MTex *mtex = &br->mtex;
	float intensity = 1.0;
	bool hasrgb = false;

	if (!mtex->tex) {
		intensity = 1;
	}
	else if (mtex->brush_map_mode == MTEX_MAP_MODE_3D) {
		/* Get strength by feeding the vertex
		 * location directly into a texture */
		hasrgb = externtex(mtex, point, &intensity,
		                   rgba, rgba + 1, rgba + 2, rgba + 3, thread, pool);
	}
	else {
		float rotation = -mtex->rot;
		float point_2d[2] = {point[0], point[1]};
		float x = 0.0f, y = 0.0f; /* Quite warnings */
		float radius = 1.0f; /* Quite warnings */
		float co[3];

		if (mtex->brush_map_mode == MTEX_MAP_MODE_VIEW) {
			/* keep coordinates relative to mouse */

			rotation += ups->brush_rotation;

			point_2d[0] -= ups->tex_mouse[0];
			point_2d[1] -= ups->tex_mouse[1];

			/* use pressure adjusted size for fixed mode */
			radius = ups->pixel_radius;

			x = point_2d[0];
			y = point_2d[1];
		}
		else if (mtex->brush_map_mode == MTEX_MAP_MODE_TILED) {
			/* leave the coordinates relative to the screen */

			/* use unadjusted size for tiled mode */
			radius = BKE_brush_size_get(scene, br);

			x = point_2d[0];
			y = point_2d[1];
		}

		x /= radius;
		y /= radius;

		/* it is probably worth optimizing for those cases where
		 * the texture is not rotated by skipping the calls to
		 * atan2, sqrtf, sin, and cos. */
		if (rotation > 0.001f || rotation < -0.001f) {
			const float angle    = atan2f(y, x) + rotation;
			const float flen     = sqrtf(x * x + y * y);

			x = flen * cosf(angle);
			y = flen * sinf(angle);
		}

		x *= br->mtex.size[0];
		y *= br->mtex.size[1];

		co[0] = x + br->mtex.ofs[0];
		co[1] = y + br->mtex.ofs[1];
		co[2] = 0.0f;

		hasrgb = externtex(mtex, co, &intensity,
		                   rgba, rgba + 1, rgba + 2, rgba + 3, thread, pool);
	}

	intensity += br->texture_sample_bias;

	if (!hasrgb) {
		rgba[0] = intensity;
		rgba[1] = intensity;
		rgba[2] = intensity;
		rgba[3] = 1.0f;
	}

	return intensity;
}


/* Brush Sampling for 2D brushes. when we unify the brush systems this will be necessarily a separate function */
float BKE_brush_sample_tex_2D(const Scene *scene, Brush *brush, const float xy[2], float rgba[4], struct ImagePool *pool)
{
	MTex *mtex = &brush->mtex;

	if (mtex && mtex->tex) {
		float co[3], tin, tr, tg, tb, ta;
		int hasrgb;
		const int radius = BKE_brush_size_get(scene, brush);

		co[0] = xy[0] / radius;
		co[1] = xy[1] / radius;
		co[2] = 0.0f;

		hasrgb = externtex(mtex, co, &tin, &tr, &tg, &tb, &ta, 0, pool);

		if (hasrgb) {
			rgba[0] = tr;
			rgba[1] = tg;
			rgba[2] = tb;
			rgba[3] = ta;
		}
		else {
			rgba[0] = tin;
			rgba[1] = tin;
			rgba[2] = tin;
			rgba[3] = 1.0f;
		}
		return tin;
	}
	else {
		rgba[0] = rgba[1] = rgba[2] = rgba[3] = 1.0f;
		return 1.0;
	}
}

/* TODO, use define for 'texfall' arg
 * NOTE: only used for 2d brushes currently! */
void BKE_brush_imbuf_new(const Scene *scene, Brush *brush, short flt, short texfall, int bufsize, ImBuf **outbuf, int use_color_correction)
{
	ImBuf *ibuf;
	float xy[2], rgba[4], *dstf;
	int x, y, rowbytes, xoff, yoff, imbflag;
	const int radius = BKE_brush_size_get(scene, brush);
	unsigned char *dst, crgb[3];
	const float alpha = BKE_brush_alpha_get(scene, brush);
	float brush_rgb[3];

	imbflag = (flt) ? IB_rectfloat : IB_rect;
	xoff = -bufsize / 2.0f + 0.5f;
	yoff = -bufsize / 2.0f + 0.5f;
	rowbytes = bufsize * 4;

	if (*outbuf)
		ibuf = *outbuf;
	else
		ibuf = IMB_allocImBuf(bufsize, bufsize, 32, imbflag);

	if (flt) {
		copy_v3_v3(brush_rgb, brush->rgb);
		if (use_color_correction) {
			srgb_to_linearrgb_v3_v3(brush_rgb, brush_rgb);
		}

		for (y = 0; y < ibuf->y; y++) {
			dstf = ibuf->rect_float + y * rowbytes;

			for (x = 0; x < ibuf->x; x++, dstf += 4) {
				xy[0] = x + xoff;
				xy[1] = y + yoff;

				if (texfall == 0) {
					copy_v3_v3(dstf, brush_rgb);
					dstf[3] = alpha * BKE_brush_curve_strength_clamp(brush, len_v2(xy), radius);
				}
				else if (texfall == 1) {
					BKE_brush_sample_tex_2D(scene, brush, xy, dstf, 0);
				}
				else if (texfall == 2) {
					BKE_brush_sample_tex_2D(scene, brush, xy, rgba, 0);
					mul_v3_v3v3(dstf, rgba, brush_rgb);
					dstf[3] = rgba[3] * alpha * BKE_brush_curve_strength_clamp(brush, len_v2(xy), radius);
				}
				else {
					BKE_brush_sample_tex_2D(scene, brush, xy, rgba, 0);
					copy_v3_v3(dstf, brush_rgb);
					dstf[3] = rgba[3] * alpha * BKE_brush_curve_strength_clamp(brush, len_v2(xy), radius);
				}
			}
		}
	}
	else {
		float alpha_f; /* final float alpha to convert to char */
		rgb_float_to_uchar(crgb, brush->rgb);

		for (y = 0; y < ibuf->y; y++) {
			dst = (unsigned char *)ibuf->rect + y * rowbytes;

			for (x = 0; x < ibuf->x; x++, dst += 4) {
				xy[0] = x + xoff;
				xy[1] = y + yoff;

				if (texfall == 0) {
					alpha_f = alpha * BKE_brush_curve_strength(brush, len_v2(xy), radius);

					dst[0] = crgb[0];
					dst[1] = crgb[1];
					dst[2] = crgb[2];
					dst[3] = FTOCHAR(alpha_f);
				}
				else if (texfall == 1) {
					BKE_brush_sample_tex_2D(scene, brush, xy, rgba, 0);
					rgba_float_to_uchar(dst, rgba);
				}
				else if (texfall == 2) {
					BKE_brush_sample_tex_2D(scene, brush, xy, rgba, 0);
					mul_v3_v3(rgba, brush->rgb);
					alpha_f = rgba[3] * alpha * BKE_brush_curve_strength_clamp(brush, len_v2(xy), radius);

					rgb_float_to_uchar(dst, rgba);

					dst[3] = FTOCHAR(alpha_f);
				}
				else {
					BKE_brush_sample_tex_2D(scene, brush, xy, rgba, 0);
					alpha_f = rgba[3] * alpha * BKE_brush_curve_strength_clamp(brush, len_v2(xy), radius);

					dst[0] = crgb[0];
					dst[1] = crgb[1];
					dst[2] = crgb[2];
					dst[3] = FTOCHAR(alpha_f);
				}
			}
		}
	}

	*outbuf = ibuf;
}

/* Unified Size and Strength */

/* XXX: be careful about setting size and unprojected radius
 * because they depend on one another
 * these functions do not set the other corresponding value
 * this can lead to odd behavior if size and unprojected
 * radius become inconsistent.
 * the biggest problem is that it isn't possible to change
 * unprojected radius because a view context is not
 * available.  my ussual solution to this is to use the
 * ratio of change of the size to change the unprojected
 * radius.  Not completely convinced that is correct.
 * In any case, a better solution is needed to prevent
 * inconsistency. */

void BKE_brush_size_set(Scene *scene, Brush *brush, int size)
{
	UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
	
	size = (int)((float)size / U.pixelsize);
	
	if (ups->flag & UNIFIED_PAINT_SIZE)
		ups->size = size;
	else
		brush->size = size;
}

int BKE_brush_size_get(const Scene *scene, Brush *brush)
{
	UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
	int size = (ups->flag & UNIFIED_PAINT_SIZE) ? ups->size : brush->size;
	
	return (int)((float)size * U.pixelsize);
}

int BKE_brush_use_locked_size(const Scene *scene, Brush *brush)
{
	const short us_flag = scene->toolsettings->unified_paint_settings.flag;

	return (us_flag & UNIFIED_PAINT_SIZE) ?
	       (us_flag & UNIFIED_PAINT_BRUSH_LOCK_SIZE) :
	       (brush->flag & BRUSH_LOCK_SIZE);
}

int BKE_brush_use_size_pressure(const Scene *scene, Brush *brush)
{
	const short us_flag = scene->toolsettings->unified_paint_settings.flag;

	return (us_flag & UNIFIED_PAINT_SIZE) ?
	       (us_flag & UNIFIED_PAINT_BRUSH_SIZE_PRESSURE) :
	       (brush->flag & BRUSH_SIZE_PRESSURE);
}

int BKE_brush_use_alpha_pressure(const Scene *scene, Brush *brush)
{
	const short us_flag = scene->toolsettings->unified_paint_settings.flag;

	return (us_flag & UNIFIED_PAINT_ALPHA) ?
	       (us_flag & UNIFIED_PAINT_BRUSH_ALPHA_PRESSURE) :
	       (brush->flag & BRUSH_ALPHA_PRESSURE);
}

void BKE_brush_unprojected_radius_set(Scene *scene, Brush *brush, float unprojected_radius)
{
	UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;

	if (ups->flag & UNIFIED_PAINT_SIZE)
		ups->unprojected_radius = unprojected_radius;
	else
		brush->unprojected_radius = unprojected_radius;
}

float BKE_brush_unprojected_radius_get(const Scene *scene, Brush *brush)
{
	UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;

	return (ups->flag & UNIFIED_PAINT_SIZE) ?
	       ups->unprojected_radius :
	       brush->unprojected_radius;
}

void BKE_brush_alpha_set(Scene *scene, Brush *brush, float alpha)
{
	UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;

	if (ups->flag & UNIFIED_PAINT_ALPHA)
		ups->alpha = alpha;
	else
		brush->alpha = alpha;
}

float BKE_brush_alpha_get(const Scene *scene, Brush *brush)
{
	UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;

	return (ups->flag & UNIFIED_PAINT_ALPHA) ? ups->alpha : brush->alpha;
}

float BKE_brush_weight_get(const Scene *scene, Brush *brush)
{
	UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;

	return (ups->flag & UNIFIED_PAINT_WEIGHT) ? ups->weight : brush->weight;
}

void BKE_brush_weight_set(const Scene *scene, Brush *brush, float value)
{
	UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;

	if (ups->flag & UNIFIED_PAINT_WEIGHT)
		ups->weight = value;
	else
		brush->weight = value;
}

/* scale unprojected radius to reflect a change in the brush's 2D size */
void BKE_brush_scale_unprojected_radius(float *unprojected_radius,
                                        int new_brush_size,
                                        int old_brush_size)
{
	float scale = new_brush_size;
	/* avoid division by zero */
	if (old_brush_size != 0)
		scale /= (float)old_brush_size;
	(*unprojected_radius) *= scale;
}

/* scale brush size to reflect a change in the brush's unprojected radius */
void BKE_brush_scale_size(int *r_brush_size,
                          float new_unprojected_radius,
                          float old_unprojected_radius)
{
	float scale = new_unprojected_radius;
	/* avoid division by zero */
	if (old_unprojected_radius != 0)
		scale /= new_unprojected_radius;
	(*r_brush_size) = (int)((float)(*r_brush_size) * scale);
}

void BKE_brush_jitter_pos(const Scene *scene, Brush *brush, const float pos[2], float jitterpos[2])
{
	int use_jitter = (brush->flag & BRUSH_ABSOLUTE_JITTER) ?
		(brush->jitter_absolute != 0) : (brush->jitter != 0);

	/* jitter-ed brush gives weird and unpredictable result for this
	 * kinds of stroke, so manually disable jitter usage (sergey) */
	use_jitter &= (brush->flag & (BRUSH_RESTORE_MESH | BRUSH_ANCHORED)) == 0;

	if (use_jitter) {
		float rand_pos[2];
		float spread;
		int diameter;

		do {
			rand_pos[0] = BLI_frand() - 0.5f;
			rand_pos[1] = BLI_frand() - 0.5f;
		} while (len_v2(rand_pos) > 0.5f);


		if (brush->flag & BRUSH_ABSOLUTE_JITTER) {
			diameter = 2 * brush->jitter_absolute;
			spread = 1.0;
		}
		else {
			diameter = 2 * BKE_brush_size_get(scene, brush);
			spread = brush->jitter;
		}
		/* find random position within a circle of diameter 1 */
		jitterpos[0] = pos[0] + 2 * rand_pos[0] * diameter * spread;
		jitterpos[1] = pos[1] + 2 * rand_pos[1] * diameter * spread;
	}
	else {
		copy_v2_v2(jitterpos, pos);
	}
}

/* Uses the brush curve control to find a strength value between 0 and 1 */
float BKE_brush_curve_strength_clamp(Brush *br, float p, const float len)
{
	if (p >= len) return 0;
	else p = p / len;

	curvemapping_initialize(br->curve);
	p = curvemapping_evaluateF(br->curve, 0, p);

	if (p < 0.0f) p = 0.0f;
	else if (p > 1.0f) p = 1.0f;
	return p;
}
/* same as above but can return negative values if the curve enables
 * used for sculpt only */
float BKE_brush_curve_strength(Brush *br, float p, const float len)
{
	if (p >= len)
		p = 1.0f;
	else
		p = p / len;

	curvemapping_initialize(br->curve);
	return curvemapping_evaluateF(br->curve, 0, p);
}

/* TODO: should probably be unified with BrushPainter stuff? */
unsigned int *BKE_brush_gen_texture_cache(Brush *br, int half_side)
{
	unsigned int *texcache = NULL;
	MTex *mtex = &br->mtex;
	TexResult texres = {0};
	int hasrgb, ix, iy;
	int side = half_side * 2;

	if (mtex->tex) {
		float x, y, step = 2.0 / side, co[3];

		texcache = MEM_callocN(sizeof(int) * side * side, "Brush texture cache");

		/*do normalized cannonical view coords for texture*/
		for (y = -1.0, iy = 0; iy < side; iy++, y += step) {
			for (x = -1.0, ix = 0; ix < side; ix++, x += step) {
				co[0] = x;
				co[1] = y;
				co[2] = 0.0f;

				/* This is copied from displace modifier code */
				hasrgb = multitex_ext(mtex->tex, co, NULL, NULL, 0, &texres, NULL);

				/* if the texture gave an RGB value, we assume it didn't give a valid
				 * intensity, so calculate one (formula from do_material_tex).
				 * if the texture didn't give an RGB value, copy the intensity across
				 */
				if (hasrgb & TEX_RGB)
					texres.tin = rgb_to_grayscale(&texres.tr);

				((char *)texcache)[(iy * side + ix) * 4] =
				((char *)texcache)[(iy * side + ix) * 4 + 1] =
				((char *)texcache)[(iy * side + ix) * 4 + 2] =
				((char *)texcache)[(iy * side + ix) * 4 + 3] = (char)(texres.tin * 255.0f);
			}
		}
	}

	return texcache;
}


/**** Radial Control ****/
struct ImBuf *BKE_brush_gen_radial_control_imbuf(Brush *br)
{
	ImBuf *im = MEM_callocN(sizeof(ImBuf), "radial control texture");
	unsigned int *texcache;
	int side = 128;
	int half = side / 2;
	int i, j;

	texcache = BKE_brush_gen_texture_cache(br, half);
	im->rect_float = MEM_callocN(sizeof(float) * side * side, "radial control rect");
	im->x = im->y = side;

	for (i = 0; i < side; ++i) {
		for (j = 0; j < side; ++j) {
			float magn = sqrtf(powf(i - half, 2) + powf(j - half, 2));
			im->rect_float[i * side + j] = BKE_brush_curve_strength_clamp(br, magn, half);
		}
	}

	/* Modulate curve with texture */
	if (texcache) {
		for (i = 0; i < side; ++i) {
			for (j = 0; j < side; ++j) {
				const int col = texcache[i * side + j];
				im->rect_float[i * side + j] *= (((char *)&col)[0] + ((char *)&col)[1] + ((char *)&col)[2]) / 3.0f / 255.0f;
			}
		}

		MEM_freeN(texcache);
	}

	return im;
}
