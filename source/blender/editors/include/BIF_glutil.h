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
 * Contributor(s): Blender Foundation 2002-2008
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file BIF_glutil.h
 *  \ingroup editorui
 */

#ifndef __BIF_GLUTIL_H__
#define __BIF_GLUTIL_H__

struct rcti;
struct rctf;

//void fdrawbezier(float vec[4][3]);

struct ImBuf;
struct bContext;
struct ColorManagedViewSettings;
struct ColorManagedDisplaySettings;

void sdrawtri(short x1, short y1, short x2, short y2);
void sdrawtrifill(short x1, short y1, short x2, short y2);

void sdrawXORline(int x0, int y0, int x1, int y1);
void sdrawXORline4(int nr, int x0, int y0, int x1, int y1);

void fdrawXORellipse(float xofs, float yofs, float hw, float hh);
void fdrawXORcirc(float xofs, float yofs, float rad);

void fdrawcheckerboard(float x1, float y1, float x2, float y2);

/* OpenGL stipple defines */
/* OpenGL stipple defines */
extern const unsigned char stipple_halftone[128];
extern const unsigned char stipple_quarttone[128];
extern const unsigned char stipple_diag_stripes_pos[128];
extern const unsigned char stipple_diag_stripes_neg[128];


/**
 * Routines an integer value as obtained by glGetIntegerv.
 * The param must cause only one value to be gotten from GL.
 */
int glaGetOneInteger(int param);

/**
 * Routines a float value as obtained by glGetIntegerv.
 * The param must cause only one value to be gotten from GL.
 */
float glaGetOneFloat(int param);

/**
 * Functions like a limited glDrawPixels, except ensures that
 * the image is displayed onscreen even if the \a x and \a y
 * coordinates for would be clipped. The routine respects the
 * glPixelZoom values, pixel unpacking parameters are _not_
 * respected.
 *
 * \attention This routine makes many assumptions: the rect data
 * is expected to be in RGBA unsigned byte format, the coordinate
 * (GLA_PIXEL_OFS, GLA_PIXEL_OFS) is assumed to be within the view frustum,
 * and the modelview and projection matrices are assumed to define a
 * 1-to-1 mapping to screen space.
 * \attention Furthermore, in the case of zoomed or unpixel aligned
 * images extending outside the view frustum, but still within the
 * window, some portion of the image may be visible left and/or
 * below of the given \a x and \a y coordinates. It is recommended
 * to use the glScissor functionality if images are to be drawn
 * with an inset view matrix.
 */
void glaDrawPixelsSafe(float x, float y, int img_w, int img_h, int row_w, int format, int type, void *rect);

/**
 * glaDrawPixelsTex - Functions like a limited glDrawPixels, but actually draws the
 * image using textures, which can be tremendously faster on low-end
 * cards, and also avoids problems with the raster position being
 * clipped when offscreen. The routine respects the glPixelZoom values,
 * pixel unpacking parameters are _not_ respected.
 *
 * \attention This routine makes many assumptions: the rect data
 * is expected to be in RGBA byte or float format, and the
 * modelview and projection matrices are assumed to define a
 * 1-to-1 mapping to screen space.
 */

void glaDrawPixelsTex(float x, float y, int img_w, int img_h, int format, int type, int zoomfilter, void *rect);

/**
 * glaDrawPixelsAuto - Switches between texture or pixel drawing using UserDef.
 * only RGBA
 * needs glaDefine2DArea to be set.
 */
void glaDrawPixelsAuto(float x, float y, int img_w, int img_h, int format, int type, int zoomfilter, void *rect);


void glaDrawPixelsTexScaled(float x, float y, int img_w, int img_h, int format, int type, int zoomfilter, const void *rect, float scaleX, float scaleY);

/* 2D Drawing Assistance */

/** Define a 2D area (viewport, scissor, matrices) for OpenGL rendering.
 *
 * glwDefine2DArea and glaBegin2DDraw set up an OpenGL state appropriate
 * for drawing using both vertice (Vertex, etc) and raster (RasterPos, Rect)
 * commands. All coordinates should be at integer positions. There is little
 * to no reason to use glVertex2f etc. functions during 2D rendering, and
 * thus no reason to +-0.5 the coordinates or perform other silly
 * tricks.
 *
 * \param screen_rect The screen rectangle to be defined for 2D drawing.
 */
void glaDefine2DArea(struct rcti *screen_rect);

typedef struct gla2DDrawInfo gla2DDrawInfo;

/* UNUSED */
#if 0

gla2DDrawInfo  *glaBegin2DDraw(struct rcti *screen_rect, struct rctf *world_rect);
void gla2DDrawTranslatePt(gla2DDrawInfo *di, float wo_x, float wo_y, int *sc_x_r, int *sc_y_r);
void gla2DDrawTranslatePtv(gla2DDrawInfo *di, float world[2], int screen_r[2]);

void glaEnd2DDraw(gla2DDrawInfo *di);

/** Adjust the transformation mapping of a 2d area */
void gla2DGetMap(gla2DDrawInfo *di, struct rctf *rect);
void gla2DSetMap(gla2DDrawInfo *di, struct rctf *rect);
#endif

void set_inverted_drawing(int enable);
void setlinestyle(int nr);

/* own working polygon offset */
void bglPolygonOffset(float viewdist, float dist);

/* For caching opengl matrices (gluProject/gluUnProject) */
typedef struct bglMats {
	float modelview[16];
	float projection[16];
	int viewport[4];
} bglMats;
void bgl_get_mats(bglMats *mats);

#define GLA_PIXEL_OFS 0.375f

/* **** Color management helper functions for GLSL display/transform ***** */

/* Draw imbuf on a screen, preferably using GLSL display transform */
void glaDrawImBuf_glsl(struct ImBuf *ibuf, float x, float y, int zoomfilter,
                       struct ColorManagedViewSettings *view_settings,
                       struct ColorManagedDisplaySettings *display_settings);

/* Draw imbuf on a screen, preferably using GLSL display transform */
void glaDrawImBuf_glsl_ctx(const struct bContext *C, struct ImBuf *ibuf, float x, float y, int zoomfilter);

/* Transform buffer from role to scene linear space using GLSL OCIO conversion */
int glaBufferTransformFromRole_glsl(float *buffer, int width, int height, int role);

#endif /* __BIF_GLUTIL_H__ */

