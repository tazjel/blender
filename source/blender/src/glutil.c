

/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_vec_types.h"

#include "BKE_utildefines.h"

#include "BLI_arithb.h"
#include "BIF_gl.h"
#include "BIF_glutil.h"

	/* Invert line handling */
	
#define glToggle(mode, onoff)	(((onoff)?glEnable:glDisable)(mode))

static void set_inverted_drawing(int enable) 
{
	glLogicOp(enable?GL_INVERT:GL_COPY);

	/* Use GL_BLEND_EQUATION_EXT on sgi (if we have it),
	 * apparently GL_COLOR_LOGIC_OP doesn't work on O2?
	 * Is this an sgi bug or our bug?
	 */
#if defined(__sgi) && defined(GL_BLEND_EQUATION_EXT)
	glBlendEquationEXT(enable?GL_LOGIC_OP:GL_FUNC_ADD_EXT);
	glToggle(GL_BLEND, enable);
#else
	glToggle(GL_COLOR_LOGIC_OP, enable);
#endif

	glToggle(GL_DITHER, !enable);
}

void sdrawXORline(int x0, int y0, int x1, int y1)
{
	if(x0==x1 && y0==y1) return;

	set_inverted_drawing(1);
	
	glBegin(GL_LINES);
	glVertex2i(x0, y0);
	glVertex2i(x1, y1);
	glEnd();
	
	set_inverted_drawing(0);
}

void glutil_draw_front_xor_line(int x0, int y0, int x1, int y1)
{
	glReadBuffer(GL_FRONT);
	glDrawBuffer(GL_FRONT);
	sdrawXORline(x0, y0, x1, y1);
	glFlush();
	glReadBuffer(GL_BACK);
	glDrawBuffer(GL_BACK);
	
}

void sdrawXORline4(int nr, int x0, int y0, int x1, int y1)
{
	static short old[4][2][2];
	static char flags[4]= {0, 0, 0, 0};
	
		/* with builtin memory, max 4 lines */

	set_inverted_drawing(1);
		
	glBegin(GL_LINES);
	if(nr== -1) { /* flush */
		for (nr=0; nr<4; nr++) {
			if (flags[nr]) {
				glVertex2sv(old[nr][0]);
				glVertex2sv(old[nr][1]);
				flags[nr]= 0;
			}
		}
	} else {
		if(nr>=0 && nr<4) {
			if(flags[nr]) {
				glVertex2sv(old[nr][0]);
				glVertex2sv(old[nr][1]);
			}

			old[nr][0][0]= x0;
			old[nr][0][1]= y0;
			old[nr][1][0]= x1;
			old[nr][1][1]= y1;
			
			flags[nr]= 1;
		}
		
		glVertex2i(x0, y0);
		glVertex2i(x1, y1);
	}
	glEnd();
	
	set_inverted_drawing(0);
}

void fdrawXORcirc(float xofs, float yofs, float rad)
{
	set_inverted_drawing(1);

	glPushMatrix();
	glTranslatef(xofs, yofs, 0.0);
	glutil_draw_lined_arc(0.0, M_PI*2.0, rad, 20);
	glPopMatrix();

	set_inverted_drawing(0);
}

void glutil_draw_filled_arc(float start, float angle, float radius, int nsegments) {
	int i;
	
	glBegin(GL_TRIANGLE_FAN);
	glVertex2f(0.0, 0.0);
	for (i=0; i<nsegments; i++) {
		float t= (float) i/(nsegments-1);
		float cur= start + t*angle;
		
		glVertex2f(cos(cur)*radius, sin(cur)*radius);
	}
	glEnd();
}

void glutil_draw_lined_arc(float start, float angle, float radius, int nsegments) {
	int i;
	
	glBegin(GL_LINE_STRIP);
	for (i=0; i<nsegments; i++) {
		float t= (float) i/(nsegments-1);
		float cur= start + t*angle;
		
		glVertex2f(cos(cur)*radius, sin(cur)*radius);
	}
	glEnd();
}

int glaGetOneInteger(int param)
{
	GLint i;
	glGetIntegerv(param, &i);
	return i;
}

float glaGetOneFloat(int param)
{
	GLfloat v;
	glGetFloatv(param, &v);
	return v;
}

void glaRasterPosSafe2f(float x, float y, float known_good_x, float known_good_y)
{
	GLubyte dummy= 0;

		/* As long as known good coordinates are correct
		 * this is guarenteed to generate an ok raster
		 * position (ignoring potential (real) overflow
		 * issues).
		 */
	glRasterPos2f(known_good_x, known_good_y);

		/* Now shift the raster position to where we wanted
		 * it in the first place using the glBitmap trick.
		 */
	glBitmap(0, 0, 0, 0, x - known_good_x, y - known_good_y, &dummy);
}

static int get_cached_work_texture(int *w_r, int *h_r)
{
	static GLint texid= -1;
	static int tex_w= 256;
	static int tex_h= 256;

	if (texid==-1) {
		GLint ltexid= glaGetOneInteger(GL_TEXTURE_2D);
		unsigned char *tbuf;

		glGenTextures(1, &texid);

		glBindTexture(GL_TEXTURE_2D, texid);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		tbuf= MEM_callocN(tex_w*tex_h*4, "tbuf");
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex_w, tex_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, tbuf);
		MEM_freeN(tbuf);

		glBindTexture(GL_TEXTURE_2D, ltexid);
	}

	*w_r= tex_w;
	*h_r= tex_h;
	return texid;
}

void glaDrawPixelsTex(float x, float y, int img_w, int img_h, void *rect)
{
	unsigned char *uc_rect= (unsigned char*) rect;
	float xzoom= glaGetOneFloat(GL_ZOOM_X), yzoom= glaGetOneFloat(GL_ZOOM_Y);
	int ltexid= glaGetOneInteger(GL_TEXTURE_2D);
	int lrowlength= glaGetOneInteger(GL_UNPACK_ROW_LENGTH);
	int subpart_x, subpart_y, tex_w, tex_h;
	int texid= get_cached_work_texture(&tex_w, &tex_h);
	int nsubparts_x= (img_w+(tex_w-1))/tex_w;
	int nsubparts_y= (img_h+(tex_h-1))/tex_h;

	glPixelStorei(GL_UNPACK_ROW_LENGTH, img_w);
	glBindTexture(GL_TEXTURE_2D, texid);

	for (subpart_y=0; subpart_y<nsubparts_y; subpart_y++) {
		for (subpart_x=0; subpart_x<nsubparts_x; subpart_x++) {
			int subpart_w= (subpart_x==nsubparts_x-1)?(img_w-subpart_x*tex_w):tex_w;
			int subpart_h= (subpart_y==nsubparts_y-1)?(img_h-subpart_y*tex_h):tex_h;
			float rast_x= x+subpart_x*tex_w*xzoom;
			float rast_y= y+subpart_y*tex_h*yzoom;

			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, subpart_w, subpart_h, GL_RGBA, GL_UNSIGNED_BYTE, &uc_rect[(subpart_y*tex_w)*img_w*4 + (subpart_x*tex_w)*4]);

			glColor3ub(255, 255, 255);
			glEnable(GL_TEXTURE_2D);
			glBegin(GL_QUADS);
			glTexCoord2f(0, 0);
			glVertex2f(rast_x, rast_y);

			glTexCoord2f((float) subpart_w/tex_w, 0);
			glVertex2f(rast_x+subpart_w*xzoom, rast_y);

			glTexCoord2f((float) subpart_w/tex_w, (float) subpart_h/tex_h);
			glVertex2f(rast_x+subpart_w*xzoom, rast_y+subpart_h*yzoom);

			glTexCoord2f(0, (float) subpart_h/tex_h);
			glVertex2f(rast_x, rast_y+subpart_h*yzoom);
			glEnd();
			glDisable(GL_TEXTURE_2D);
		}
	}

	glBindTexture(GL_TEXTURE_2D, ltexid);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, lrowlength);
}

void glaDrawPixelsSafe(float x, float y, int img_w, int img_h, void *rect)
{
	unsigned char *uc_rect= (unsigned char*) rect;

	float xzoom= glaGetOneFloat(GL_ZOOM_X);
	float yzoom= glaGetOneFloat(GL_ZOOM_Y);
		
		/* The pixel space coordinate of the intersection of
		 * the [zoomed] image with the origin.
		 */
	float ix= -x/xzoom;
	float iy= -y/yzoom;
	
		/* The maximum pixel amounts the image can be cropped
		 * at the lower left without exceeding the origin.
		 */
	int off_x= floor(MAX2(ix, 0));
	int off_y= floor(MAX2(iy, 0));
		
		/* The zoomed space coordinate of the raster position 
		 * (starting at the lower left most unclipped pixel).
		 */
	float rast_x= x + off_x*xzoom;
	float rast_y= y + off_y*yzoom;

	GLfloat scissor[4];
	int draw_w, draw_h;

		/* Determine the smallest number of pixels we need to draw
		 * before the image would go off the upper right corner.
		 * 
		 * It may seem this is just an optimization but some graphics 
		 * cards (ATI) freak out if there is a large zoom factor and
		 * a large number of pixels off the screen (probably at some
		 * level the number of image pixels to draw is getting multiplied
		 * by the zoom and then clamped). Making sure we draw the
		 * fewest pixels possible keeps everyone mostly happy (still
		 * fails if we zoom in on one really huge pixel so that it
		 * covers the entire screen).
		 */
	glGetFloatv(GL_SCISSOR_BOX, scissor);
	draw_w = MIN2(img_w-off_x, ceil((scissor[2]-rast_x)/xzoom));
	draw_h = MIN2(img_h-off_y, ceil((scissor[3]-rast_y)/yzoom));

	if (draw_w>0 && draw_h>0) {
		int old_row_length = glaGetOneInteger(GL_UNPACK_ROW_LENGTH);

			/* Don't use safe RasterPos (slower) if we can avoid it. */
		if (rast_x>=0 && rast_y>=0) {
			glRasterPos2f(rast_x, rast_y);
		} else {
			glaRasterPosSafe2f(rast_x, rast_y, 0, 0);
		}

		glPixelStorei(GL_UNPACK_ROW_LENGTH, img_w);
		glDrawPixels(draw_w, draw_h, GL_RGBA, GL_UNSIGNED_BYTE, uc_rect + (off_y*img_w + off_x)*4);
		glPixelStorei(GL_UNPACK_ROW_LENGTH,  old_row_length);
	}
}

/* 2D Drawing Assistance */

void glaDefine2DArea(rcti *screen_rect)
{
	int sc_w= screen_rect->xmax - screen_rect->xmin;
	int sc_h= screen_rect->ymax - screen_rect->ymin;

	glViewport(screen_rect->xmin, screen_rect->ymin, sc_w, sc_h);
	glScissor(screen_rect->xmin, screen_rect->ymin, sc_w, sc_h);

		/* The 0.375 magic number is to shift the matrix so that
		 * both raster and vertex integer coordinates fall at pixel
		 * centers properly. For a longer discussion see the OpenGL
		 * Programming Guide, Appendix H, Correctness Tips.
		 */

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, sc_w, 0.0, sc_h, -1, 1);
	glTranslatef(0.375, 0.375, 0.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

struct gla2DDrawInfo {
	int orig_vp[4], orig_sc[4];
	float orig_projmat[16], orig_viewmat[16];

	rcti screen_rect;
	rctf world_rect;

	float wo_to_sc[2];
};

gla2DDrawInfo *glaBegin2DDraw(rcti *screen_rect, rctf *world_rect) 
{
	gla2DDrawInfo *di= MEM_mallocN(sizeof(*di), "gla2DDrawInfo");
	int sc_w, sc_h;
	float wo_w, wo_h;

	glGetIntegerv(GL_VIEWPORT, (GLint *)di->orig_vp);
	glGetIntegerv(GL_SCISSOR_BOX, (GLint *)di->orig_sc);
	glGetFloatv(GL_PROJECTION_MATRIX, (GLfloat *)di->orig_projmat);
	glGetFloatv(GL_MODELVIEW_MATRIX, (GLfloat *)di->orig_viewmat);

	di->screen_rect= *screen_rect;
	if (world_rect) {
		di->world_rect= *world_rect;
	} else {
		di->world_rect.xmin= di->screen_rect.xmin;
		di->world_rect.ymin= di->screen_rect.ymin;
		di->world_rect.xmax= di->screen_rect.xmax;
		di->world_rect.ymax= di->screen_rect.ymax;
	}

	sc_w= (di->screen_rect.xmax-di->screen_rect.xmin);
	sc_h= (di->screen_rect.ymax-di->screen_rect.ymin);
	wo_w= (di->world_rect.xmax-di->world_rect.xmin);
	wo_h= (di->world_rect.ymax-di->world_rect.ymin);

	di->wo_to_sc[0]= sc_w/wo_w;
	di->wo_to_sc[1]= sc_h/wo_h;

	glaDefine2DArea(&di->screen_rect);

	return di;
}

void gla2DDrawTranslatePt(gla2DDrawInfo *di, float wo_x, float wo_y, int *sc_x_r, int *sc_y_r)
{
	*sc_x_r= (wo_x - di->world_rect.xmin)*di->wo_to_sc[0];
	*sc_y_r= (wo_y - di->world_rect.ymin)*di->wo_to_sc[1];
}
void gla2DDrawTranslatePtv(gla2DDrawInfo *di, float world[2], int screen_r[2])
{
	screen_r[0]= (world[0] - di->world_rect.xmin)*di->wo_to_sc[0];
	screen_r[1]= (world[1] - di->world_rect.ymin)*di->wo_to_sc[1];
}

void glaEnd2DDraw(gla2DDrawInfo *di)
{
	glViewport(di->orig_vp[0], di->orig_vp[1], di->orig_vp[2], di->orig_vp[3]);
	glScissor(di->orig_vp[0], di->orig_vp[1], di->orig_vp[2], di->orig_vp[3]);
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(di->orig_projmat);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(di->orig_viewmat);

	MEM_freeN(di);
}

/* **************** glPoint hack ************************ */

static int curmode=0;
static int pointhack=0;
static GLubyte Squaredot[16] = { 0xff,0xff,0xff,0xff,
								 0xff,0xff,0xff,0xff,
								 0xff,0xff,0xff,0xff, 
								 0xff,0xff,0xff,0xff};

void bglBegin(int mode)
{
	curmode= mode;
	
	if(mode==GL_POINTS) {
		float value[4];
		glGetFloatv(GL_POINT_SIZE_RANGE, value);
		if(value[1]<2.0) {
			glGetFloatv(GL_POINT_SIZE, value);
			pointhack= floor(value[0]+0.5);
			if(pointhack>4) pointhack= 4;
		}
		else glBegin(mode);
	}
}


void bglVertex3fv(float *vec)
{
	switch(curmode) {
	case GL_POINTS:
		if(pointhack) {
			glRasterPos3fv(vec);
			glBitmap(pointhack, pointhack, (float)pointhack/2.0, (float)pointhack/2.0, 0.0, 0.0, Squaredot);
		}
		else glVertex3fv(vec);
		break;
	}
}

void bglVertex2fv(float *vec)
{
	switch(curmode) {
	case GL_POINTS:
		if(pointhack) {
			glRasterPos2fv(vec);
			glBitmap(pointhack, pointhack, (float)pointhack/2, pointhack/2, 0.0, 0.0, Squaredot);
		}
		else glVertex2fv(vec);
		break;
	}
}


void bglEnd(void)
{
	if(pointhack) pointhack= 0;
	else glEnd();
	
}

/* *************** glPolygonOffset hack ************* */

// both temporal, so here for now (ton)
#include "BKE_global.h"
#include "DNA_view3d_types.h"

/* dist is only for ortho now... */
void bglPolygonOffset(float dist) 
{
	static float winmat[16], offset=0.0;	
	
	if(dist!=0.0) {
		float offs;
		
		// glEnable(GL_POLYGON_OFFSET_FILL);
		// glPolygonOffset(-1.0, -1.0);

		/* hack below is to mimic polygon offset */
		glMatrixMode(GL_PROJECTION);
		glGetFloatv(GL_PROJECTION_MATRIX, (float *)winmat);
		
		/* dist is from camera to center point */
		
		if(winmat[15]>0.5) offs= 0.00001*dist*G.vd->dist;  // ortho tweaking
		else offs= 0.0005*dist;  // should be clipping value or so...
		
		winmat[14]-= offs;
		offset+= offs;
		
		glLoadMatrixf(winmat);
		glMatrixMode(GL_MODELVIEW);
	}
	else {

		glMatrixMode(GL_PROJECTION);
		winmat[14]+= offset;
		offset= 0.0;
		glLoadMatrixf(winmat);
		glMatrixMode(GL_MODELVIEW);
	}
}




