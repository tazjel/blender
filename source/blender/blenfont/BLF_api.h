/**
 * $Id:
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BLF_API_H
#define BLF_API_H

struct rctf;

int BLF_init(int points, int dpi);
void BLF_exit(void);

int BLF_load(char *name);
int BLF_load_mem(char *name, unsigned char *mem, int mem_size);

/*
 * Set/Get the current font.
 */
void BLF_set(int fontid);
int BLF_get(void);

void BLF_aspect(float aspect);
void BLF_position(float x, float y, float z);
void BLF_size(int size, int dpi);

/* Draw the string using the default font, size and dpi. */
void BLF_draw_default(float x, float y, float z, char *str);

/* Draw the string using the current font. */
void BLF_draw(char *str);

/*
 * This function return the bounding box of the string
 * and are not multiplied by the aspect.
 */
void BLF_boundbox(char *str, struct rctf *box);

/*
 * The next both function return the width and height
 * of the string, using the current font and both value 
 * are multiplied by the aspect of the font.
 */
float BLF_width(char *str);
float BLF_height(char *str);

/*
 * and this two function return the width and height
 * of the string, using the default font and both value
 * are multiplied by the aspect of the font.
 */
float BLF_width_default(char *str);
float BLF_height_default(char *str);

/*
 * By default, rotation and clipping are disable and
 * have to be enable/disable using BLF_enable/disable.
 */
void BLF_rotation(float angle);
void BLF_clipping(float xmin, float ymin, float xmax, float ymax);
void BLF_blur(int size);


void BLF_enable(int option);
void BLF_disable(int option);

/*
 * Search the path directory to the locale files, this try all
 * the case for Linux, Win and Mac.
 */
void BLF_lang_init(void);

/* Set the current locale. */
void BLF_lang_set(const char *);

/* Set the current encoding name. */
void BLF_lang_encoding_name(const char *str);

/* Add a path to the font dir paths. */
void BLF_dir_add(const char *path);

/* Remove a path from the font dir paths. */
void BLF_dir_rem(const char *path);

/* Return an array with all the font dir (this can be used for filesel) */
char **BLF_dir_get(int *ndir);

/* Free the data return by BLF_dir_get. */
void BLF_dir_free(char **dirs, int count);

/* font->flags. */
#define BLF_ROTATION (1<<0)
#define BLF_CLIPPING (1<<1)

/* font->mode. */
#define BLF_MODE_TEXTURE 0
#define BLF_MODE_BITMAP 1

#endif /* BLF_API_H */
