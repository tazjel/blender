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
#ifndef __BKE_SCREEN_H__
#define __BKE_SCREEN_H__

/** \file BKE_screen.h
 *  \ingroup bke
 *  \since March 2001
 *  \author nzc
 */

struct ARegion;
struct Header;
struct ListBase;
struct Menu;
struct Panel;
struct Scene;
struct ScrArea;
struct SpaceType;
struct View3D;
struct bContext;
struct bContextDataResult;
struct bScreen;
struct uiLayout;
struct uiMenuItem;
struct wmKeyConfig;
struct wmNotifier;
struct wmWindow;
struct wmWindowManager;

#include "RNA_types.h"

/* spacetype has everything stored to get an editor working, it gets initialized via 
 * ED_spacetypes_init() in editors/space_api/spacetypes.c   */
/* an editor in Blender is a combined ScrArea + SpaceType + SpaceData */

#define BKE_ST_MAXNAME  64

typedef struct SpaceType {
	struct SpaceType *next, *prev;
	
	char name[BKE_ST_MAXNAME];                  /* for menus */
	int spaceid;                                /* unique space identifier */
	int iconid;                                 /* icon lookup for menus */
	
	/* initial allocation, after this WM will call init() too */
	struct SpaceLink    *(*new)(const struct bContext *C);
	/* not free spacelink itself */
	void (*free)(struct SpaceLink *);
	
	/* init is to cope with file load, screen (size) changes, check handlers */
	void (*init)(struct wmWindowManager *, struct ScrArea *);
	/* Listeners can react to bContext changes */
	void (*listener)(struct ScrArea *, struct wmNotifier *);
	
	/* refresh context, called after filereads, ED_area_tag_refresh() */
	void (*refresh)(const struct bContext *, struct ScrArea *);
	
	/* after a spacedata copy, an init should result in exact same situation */
	struct SpaceLink    *(*duplicate)(struct SpaceLink *);

	/* register operator types on startup */
	void (*operatortypes)(void);
	/* add default items to WM keymap */
	void (*keymap)(struct wmKeyConfig *);
	/* on startup, define dropboxes for spacetype+regions */
	void (*dropboxes)(void);

	/* return context data */
	int (*context)(const struct bContext *, const char *, struct bContextDataResult *);

	/* region type definitions */
	ListBase regiontypes;
	
	/* tool shelf definitions */
	ListBase toolshelf;
		
	/* read and write... */
	
	/* default keymaps to add */
	int keymapflag;
	
} SpaceType;

/* region types are also defined using spacetypes_init, via a callback */

typedef struct ARegionType {
	struct ARegionType *next, *prev;
	
	int regionid;           /* unique identifier within this space, defines RGN_TYPE_xxxx */
	
	/* add handlers, stuff you only do once or on area/region type/size changes */
	void (*init)(struct wmWindowManager *, struct ARegion *);
	/* draw entirely, view changes should be handled here */
	void (*draw)(const struct bContext *, struct ARegion *);
	/* contextual changes should be handled here */
	void (*listener)(struct ARegion *, struct wmNotifier *);
	
	void (*free)(struct ARegion *);

	/* split region, copy data optionally */
	void        *(*duplicate)(void *);

	
	/* register operator types on startup */
	void (*operatortypes)(void);
	/* add own items to keymap */
	void (*keymap)(struct wmKeyConfig *);
	/* allows default cursor per region */
	void (*cursor)(struct wmWindow *, struct ScrArea *, struct ARegion *ar);

	/* return context data */
	int (*context)(const struct bContext *, const char *, struct bContextDataResult *);

	/* custom drawing callbacks */
	ListBase drawcalls;

	/* panels type definitions */
	ListBase paneltypes;

	/* header type definitions */
	ListBase headertypes;
	
	/* hardcoded constraints, smaller than these values region is not visible */
	int minsizex, minsizey;
	/* when new region opens (region prefsizex/y are zero then */
	int prefsizex, prefsizey;
	/* default keymaps to add */
	int keymapflag;
	/* return without drawing. lock is set by region definition, and copied to do_lock by render. can become flag */
	short do_lock, lock;
	/* call cursor function on each move event */
	short event_cursor;
	/* If it's enabled, don't clear the region before drawing panels (ED_region_panels) */
	char dont_clear;
} ARegionType;

/* panel types */

typedef struct PanelType {
	struct PanelType *next, *prev;
	
	char idname[BKE_ST_MAXNAME];            /* unique name */
	char label[BKE_ST_MAXNAME];             /* for panel header */
	char context[BKE_ST_MAXNAME];           /* for buttons window */
	int space_type;
	int region_type;

	int flag;

	/* verify if the panel should draw or not */
	int (*poll)(const struct bContext *, struct PanelType *);
	/* draw header (optional) */
	void (*draw_header)(const struct bContext *, struct Panel *);
	/* draw entirely, view changes should be handled here */
	void (*draw)(const struct bContext *, struct Panel *);

	/* RNA integration */
	ExtensionRNA ext;
} PanelType;

/* header types */

typedef struct HeaderType {
	struct HeaderType *next, *prev;

	char idname[BKE_ST_MAXNAME];        /* unique name */
	int space_type;

	/* draw entirely, view changes should be handled here */
	void (*draw)(const struct bContext *, struct Header *);

	/* RNA integration */
	ExtensionRNA ext;
} HeaderType;

typedef struct Header {
	struct HeaderType *type;    /* runtime */
	struct uiLayout *layout;    /* runtime for drawing */
} Header;


/* menu types */

typedef struct MenuType {
	struct MenuType *next, *prev;

	char idname[BKE_ST_MAXNAME];        /* unique name */
	char label[BKE_ST_MAXNAME];         /* for button text */
	char       *description;

	/* verify if the menu should draw or not */
	int (*poll)(const struct bContext *, struct MenuType *);
	/* draw entirely, view changes should be handled here */
	void (*draw)(const struct bContext *, struct Menu *);

	/* RNA integration */
	ExtensionRNA ext;
} MenuType;

typedef struct Menu {
	struct MenuType *type;      /* runtime */
	struct uiLayout *layout;    /* runtime for drawing */
} Menu;

/* spacetypes */
struct SpaceType *BKE_spacetype_from_id(int spaceid);
struct ARegionType *BKE_regiontype_from_id(struct SpaceType *st, int regionid);
const struct ListBase *BKE_spacetypes_list(void);
void BKE_spacetype_register(struct SpaceType *st);
void BKE_spacetypes_free(void); /* only for quitting blender */

/* spacedata */
void BKE_spacedata_freelist(ListBase *lb);
void BKE_spacedata_copylist(ListBase *lb1, ListBase *lb2);
void BKE_spacedata_draw_locks(int set);

/* area/regions */
struct ARegion *BKE_area_region_copy(struct SpaceType *st, struct ARegion *ar);
void            BKE_area_region_free(struct SpaceType *st, struct ARegion *ar);
void            BKE_screen_area_free(struct ScrArea *sa);

struct ARegion *BKE_area_find_region_type(struct ScrArea *sa, int type);
struct ScrArea *BKE_screen_find_big_area(struct bScreen *sc, const int spacetype, const short min);

void BKE_screen_view3d_sync(struct View3D *v3d, struct Scene *scene);
void BKE_screen_view3d_scene_sync(struct bScreen *sc);
void BKE_screen_view3d_main_sync(ListBase *screen_lb, struct Scene *scene);

/* zoom factor conversion */
float BKE_screen_view3d_zoom_to_fac(float camzoom);
float BKE_screen_view3d_zoom_from_fac(float zoomfac);

/* screen */
void BKE_screen_free(struct bScreen *sc); 
unsigned int BKE_screen_visible_layers(struct bScreen *screen, struct Scene *scene);

#endif

