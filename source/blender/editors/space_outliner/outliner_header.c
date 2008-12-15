/**
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>

#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_global.h"
#include "BKE_screen.h"

#include "ED_screen.h"
#include "ED_util.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "outliner_intern.h"


/* ************************ header area region *********************** */

static void do_viewmenu(bContext *C, void *arg, int event)
{
	
}

static uiBlock *dummy_viewmenu(bContext *C, uiMenuBlockHandle *handle, void *arg_unused)
{
	ScrArea *curarea= C->area;
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(C, handle->region, "dummy_viewmenu", UI_EMBOSSP, UI_HELV);
	uiBlockSetButmFunc(block, do_viewmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Nothing yet", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}
	
	uiTextBoundsBlock(block, 50);
	uiEndBlock(C, block);
	
	return block;
}

static void do_outliner_buttons(bContext *C, void *arg, int event)
{
	switch(event) {
	}
}


void outliner_header_buttons(const bContext *C, ARegion *ar)
{
	ScrArea *sa= C->area;
	SpaceOops *soutliner= sa->spacedata.first;
	uiBlock *block;
	int xco, yco= 3;
	char *path;
	
	block= uiBeginBlock(C, ar, "header buttons", UI_EMBOSS, UI_HELV);
	uiBlockSetHandleFunc(block, do_outliner_buttons, NULL);
	
	xco= ED_area_header_standardbuttons(C, block, yco);
	
	if((sa->flag & HEADER_NO_PULLDOWN)==0) {
		int xmax;
		
		/* pull down menus */
		uiBlockSetEmboss(block, UI_EMBOSSP);
		
		xmax= GetButStringLength("View");
		uiDefPulldownBut(block, dummy_viewmenu, C->area, 
						 "View", xco, yco-2, xmax-3, 24, ""); 
		xco += xmax;
		
		/* header text */
		xco += XIC*2;
		
		path= (soutliner->rnapath)? soutliner->rnapath: "Main";
		xmax= GetButStringLength(path);
		uiDefBut(block, LABEL, 0, path, xco, yco-2, xmax-3, 24, 0, 0, 0, 0, 0, "Current RNA Path");
		xco += xmax;
		
		uiBlockSetEmboss(block, UI_EMBOSS);
	}
	
	/* always as last  */
	UI_view2d_totRect_set(&ar->v2d, xco+XIC+80, ar->v2d.tot.ymax-ar->v2d.tot.ymin);
	
	uiEndBlock(C, block);
	uiDrawBlock(block);
}


