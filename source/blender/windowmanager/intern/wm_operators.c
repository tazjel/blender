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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <float.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <string.h>

#include "DNA_ID.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "BLI_blenlib.h"
#include "BLI_dynstr.h" /*for WM_operator_pystring */

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_library.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_scene.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_glutil.h" /* for paint cursor */

#include "IMB_imbuf_types.h"

#include "ED_screen.h"
#include "ED_util.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "wm.h"
#include "wm_draw.h"
#include "wm_event_system.h"
#include "wm_subwindow.h"
#include "wm_window.h"



static ListBase global_ops= {NULL, NULL};

/* ************ operator API, exported ********** */

wmOperatorType *WM_operatortype_find(const char *idname)
{
	wmOperatorType *ot;
	
	for(ot= global_ops.first; ot; ot= ot->next) {
		if(strncmp(ot->idname, idname, OP_MAX_TYPENAME)==0)
		   return ot;
	}
	printf("search for unknown operator %s\n", idname);
	return NULL;
}

wmOperatorType *WM_operatortype_first(void)
{
	return global_ops.first;
}

/* all ops in 1 list (for time being... needs evaluation later) */
void WM_operatortype_append(void (*opfunc)(wmOperatorType*))
{
	wmOperatorType *ot;
	
	ot= MEM_callocN(sizeof(wmOperatorType), "operatortype");
	ot->srna= RNA_def_struct(&BLENDER_RNA, "", "OperatorProperties");
	opfunc(ot);
	RNA_def_struct_ui_text(ot->srna, ot->name, ot->description ? ot->description:""); // XXX All ops should have a description but for now allow them not to.
	RNA_def_struct_identifier(ot->srna, ot->idname);
	BLI_addtail(&global_ops, ot);
}

void WM_operatortype_append_ptr(void (*opfunc)(wmOperatorType*, void*), void *userdata)
{
	wmOperatorType *ot;

	ot= MEM_callocN(sizeof(wmOperatorType), "operatortype");
	ot->srna= RNA_def_struct(&BLENDER_RNA, "", "OperatorProperties");
	opfunc(ot, userdata);
	RNA_def_struct_ui_text(ot->srna, ot->name, "DOC_BROKEN"); /* TODO - add a discription to wmOperatorType? */
	RNA_def_struct_identifier(ot->srna, ot->idname);
	BLI_addtail(&global_ops, ot);
}

int WM_operatortype_remove(const char *idname)
{
	wmOperatorType *ot = WM_operatortype_find(idname);

	if (ot==NULL)
		return 0;
	
	BLI_remlink(&global_ops, ot);
	RNA_struct_free(&BLENDER_RNA, ot->srna);
	MEM_freeN(ot);

	return 1;
}

/* print a string representation of the operator, with the args that it runs 
 * so python can run it again */
char *WM_operator_pystring(wmOperator *op)
{
	const char *arg_name= NULL;

	PropertyRNA *prop, *iterprop;

	/* for building the string */
	DynStr *dynstr= BLI_dynstr_new();
	char *cstring, *buf;
	int first_iter=1;

	BLI_dynstr_appendf(dynstr, "%s(", op->idname);

	iterprop= RNA_struct_iterator_property(op->ptr->type);

	RNA_PROP_BEGIN(op->ptr, propptr, iterprop) {
		prop= propptr.data;
		arg_name= RNA_property_identifier(prop);

		if (strcmp(arg_name, "rna_type")==0) continue;

		buf= RNA_property_as_string(op->ptr, prop);
		
		BLI_dynstr_appendf(dynstr, first_iter?"%s=%s":", %s=%s", arg_name, buf);

		MEM_freeN(buf);
		first_iter = 0;
	}
	RNA_PROP_END;

	BLI_dynstr_append(dynstr, ")");

	cstring = BLI_dynstr_get_cstring(dynstr);
	BLI_dynstr_free(dynstr);
	return cstring;
}

void WM_operator_properties_create(PointerRNA *ptr, const char *opstring)
{
	wmOperatorType *ot= WM_operatortype_find(opstring);

	if(ot)
		RNA_pointer_create(NULL, ot->srna, NULL, ptr);
	else
		RNA_pointer_create(NULL, &RNA_OperatorProperties, NULL, ptr);
}

void WM_operator_properties_free(PointerRNA *ptr)
{
	IDProperty *properties= ptr->data;

	if(properties) {
		IDP_FreeProperty(properties);
		MEM_freeN(properties);
	}
}

/* ************ default op callbacks, exported *********** */

/* invoke callback, uses enum property named "type" */
/* only weak thing is the fixed property name... */
int WM_menu_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	PropertyRNA *prop= RNA_struct_find_property(op->ptr, "type");
	uiPopupMenu *pup;
	uiLayout *layout;

	if(prop==NULL) {
		printf("WM_menu_invoke: %s has no \"type\" enum property\n", op->type->idname);
	}
	else if (RNA_property_type(prop) != PROP_ENUM) {
		printf("WM_menu_invoke: %s \"type\" is not an enum property\n", op->type->idname);
	}
	else {
		pup= uiPupMenuBegin(C, op->type->name, 0);
		layout= uiPupMenuLayout(pup);
		uiItemsEnumO(layout, op->type->idname, "type");
		uiPupMenuEnd(C, pup);
	}

	return OPERATOR_CANCELLED;
}

/* op->invoke */
int WM_operator_confirm(bContext *C, wmOperator *op, wmEvent *event)
{
	uiPopupMenu *pup;
	uiLayout *layout;

	pup= uiPupMenuBegin(C, "OK?", ICON_QUESTION);
	layout= uiPupMenuLayout(pup);
	uiItemO(layout, NULL, 0, op->type->idname);
	uiPupMenuEnd(C, pup);
	
	return OPERATOR_CANCELLED;
}

/* op->invoke, opens fileselect if filename property not set, otherwise executes */
int WM_operator_filesel(bContext *C, wmOperator *op, wmEvent *event)
{
	if (RNA_property_is_set(op->ptr, "filename")) {
		return WM_operator_call(C, op);
	} 
	else {
		WM_event_add_fileselect(C, op);
		return OPERATOR_RUNNING_MODAL;
	}
}

/* op->poll */
int WM_operator_winactive(bContext *C)
{
	if(CTX_wm_window(C)==NULL) return 0;
	return 1;
}

/* op->invoke */
static void redo_cb(bContext *C, void *arg_op, void *arg2)
{
	wmOperator *lastop= arg_op;
	
	if(lastop) {
		ED_undo_pop(C);
		WM_operator_repeat(C, lastop);
	}
}

static uiBlock *wm_block_create_redo(bContext *C, ARegion *ar, void *arg_op)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	wmOperator *op= arg_op;
	PointerRNA ptr;
	uiBlock *block;
	uiLayout *layout;
	uiStyle *style= U.uistyles.first;
	
	block= uiBeginBlock(C, ar, "redo_popup", UI_EMBOSS);
	uiBlockClearFlag(block, UI_BLOCK_LOOP);
	uiBlockSetFlag(block, UI_BLOCK_KEEP_OPEN|UI_BLOCK_RET_1);
	uiBlockSetFunc(block, redo_cb, arg_op, NULL);

	if(!op->properties) {
		IDPropertyTemplate val = {0};
		op->properties= IDP_New(IDP_GROUP, val, "wmOperatorProperties");
	}

	RNA_pointer_create(&wm->id, op->type->srna, op->properties, &ptr);
	layout= uiBlockLayout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0, 300, 20, style);
	uiDefAutoButsRNA(C, layout, &ptr, 2);

	uiPopupBoundsBlock(block, 4.0f, 0, 0);
	uiEndBlock(C, block);

	return block;
}

int WM_operator_redo(bContext *C, wmOperator *op, wmEvent *event)
{
	int retval= OPERATOR_CANCELLED;
	
	if(op->type->exec)
		retval= op->type->exec(C, op);

	if(retval != OPERATOR_CANCELLED)
		uiPupBlock(C, wm_block_create_redo, op);

	return retval;
}

int WM_operator_redo_popup(bContext *C, wmOperator *op)
{
	uiPupBlock(C, wm_block_create_redo, op);

	return OPERATOR_CANCELLED;
}

/* ***************** Debug menu ************************* */

static uiBlock *wm_block_create_menu(bContext *C, ARegion *ar, void *arg_op)
{
	wmOperator *op= arg_op;
	uiBlock *block;
	uiLayout *layout;
	uiStyle *style= U.uistyles.first;
	
	block= uiBeginBlock(C, ar, "_popup", UI_EMBOSS);
	uiBlockClearFlag(block, UI_BLOCK_LOOP);
	uiBlockSetFlag(block, UI_BLOCK_KEEP_OPEN|UI_BLOCK_RET_1);
	
	layout= uiBlockLayout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0, 300, 20, style);
	uiDefAutoButsRNA(C, layout, op->ptr, 2);
	
	uiPopupBoundsBlock(block, 4.0f, 0, 0);
	uiEndBlock(C, block);
	
	return block;
}

static int wm_debug_menu_exec(bContext *C, wmOperator *op)
{
	G.rt= RNA_int_get(op->ptr, "debugval");
	ED_screen_refresh(CTX_wm_manager(C), CTX_wm_window(C));
	WM_event_add_notifier(C, NC_WINDOW, NULL);
	
	return OPERATOR_FINISHED;	
}

static int wm_debug_menu_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	
	RNA_int_set(op->ptr, "debugval", G.rt);
	
	/* pass on operator, so return modal */
	uiPupBlockOperator(C, wm_block_create_menu, op, WM_OP_EXEC_DEFAULT);
	
	return OPERATOR_RUNNING_MODAL;
}

static void WM_OT_debug_menu(wmOperatorType *ot)
{
	ot->name= "Debug Menu";
	ot->idname= "WM_OT_debug_menu";
	
	ot->invoke= wm_debug_menu_invoke;
	ot->exec= wm_debug_menu_exec;
	ot->poll= WM_operator_winactive;
	
	RNA_def_int(ot->srna, "debugval", 0, -10000, 10000, "Debug Value", "", INT_MIN, INT_MAX);
}

/* ***************** Search menu ************************* */
static void operator_call_cb(struct bContext *C, void *arg1, void *arg2)
{
	wmOperatorType *ot= arg2;
	
	if(ot)
		WM_operator_name_call(C, ot->idname, WM_OP_INVOKE_DEFAULT, NULL);
}

static void operator_search_cb(const struct bContext *C, void *arg, char *str, uiSearchItems *items)
{
	wmOperatorType *ot = WM_operatortype_first();
	
	for(; ot; ot= ot->next) {
		
		if(BLI_strcasestr(ot->name, str)) {
			if(ot->poll==NULL || ot->poll((bContext *)C)) {
				char name[256];
				int len= strlen(ot->name);
				
				/* display name for menu, can hold hotkey */
				BLI_strncpy(name, ot->name, 256);
				
				/* check for hotkey */
				if(len < 256-6) {
					if(WM_key_event_operator_string(C, ot->idname, WM_OP_EXEC_DEFAULT, NULL, &name[len+1], 256-len-1))
						name[len]= '|';
				}
				
				if(0==uiSearchItemAdd(items, name, ot, 0))
					break;
			}
		}
	}
}

static uiBlock *wm_block_search_menu(bContext *C, ARegion *ar, void *arg_op)
{
	static char search[256]= "";
	wmEvent event;
	wmWindow *win= CTX_wm_window(C);
	uiBlock *block;
	uiBut *but;
	
	block= uiBeginBlock(C, ar, "_popup", UI_EMBOSS);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_RET_1);
	
	but= uiDefSearchBut(block, search, 0, ICON_VIEWZOOM, 256, 10, 10, 180, 19, "");
	uiButSetSearchFunc(but, operator_search_cb, NULL, operator_call_cb, NULL);
	
	/* fake button, it holds space for search items */
	uiDefBut(block, LABEL, 0, "", 10, 10 - uiSearchBoxhHeight(), 180, uiSearchBoxhHeight(), NULL, 0, 0, 0, 0, NULL);
	
	uiPopupBoundsBlock(block, 6.0f, 0, -20); /* move it downwards, mouse over button */
	uiEndBlock(C, block);
	
	event= *(win->eventstate);	/* XXX huh huh? make api call */
	event.type= EVT_BUT_OPEN;
	event.val= KM_PRESS;
	event.customdata= but;
	event.customdatafree= FALSE;
	wm_event_add(win, &event);
	
	return block;
}

static int wm_search_menu_exec(bContext *C, wmOperator *op)
{
	
	return OPERATOR_FINISHED;	
}

static int wm_search_menu_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	
	uiPupBlock(C, wm_block_search_menu, op);
	
	return OPERATOR_CANCELLED;
}

static void WM_OT_search_menu(wmOperatorType *ot)
{
	ot->name= "Search Menu";
	ot->idname= "WM_OT_search_menu";
	
	ot->invoke= wm_search_menu_invoke;
	ot->exec= wm_search_menu_exec;
	ot->poll= WM_operator_winactive;
}


/* ************ window / screen operator definitions ************** */

static void WM_OT_window_duplicate(wmOperatorType *ot)
{
	ot->name= "Duplicate Window";
	ot->idname= "WM_OT_window_duplicate";
	
	ot->invoke= WM_operator_confirm;
	ot->exec= wm_window_duplicate_op;
	ot->poll= WM_operator_winactive;
}

static void WM_OT_save_homefile(wmOperatorType *ot)
{
	ot->name= "Save User Settings";
	ot->idname= "WM_OT_save_homefile";
	
	ot->invoke= WM_operator_confirm;
	ot->exec= WM_write_homefile;
	ot->poll= WM_operator_winactive;
}

static void WM_OT_read_homefile(wmOperatorType *ot)
{
	ot->name= "Reload Start-Up File";
	ot->idname= "WM_OT_read_homefile";
	
	ot->invoke= WM_operator_confirm;
	ot->exec= WM_read_homefile;
	ot->poll= WM_operator_winactive;
	
	RNA_def_boolean(ot->srna, "factory", 0, "Factory Settings", "");
}


/* ********* recent file *********** */

static int recentfile_exec(bContext *C, wmOperator *op)
{
	int event= RNA_int_get(op->ptr, "nr");

	// XXX wm in context is not set correctly after WM_read_file -> crash
	// do it before for now, but is this correct with multiple windows?

	if(event>0) {
		if (G.sce[0] && (event==1)) {
			WM_event_add_notifier(C, NC_WINDOW, NULL);
			WM_read_file(C, G.sce, op->reports);
		}
		else {
			struct RecentFile *recent = BLI_findlink(&(G.recent_files), event-2);
			if(recent) {
				WM_event_add_notifier(C, NC_WINDOW, NULL);
				WM_read_file(C, recent->filename, op->reports);
			}
		}
	}
	return 0;
}

static int wm_recentfile_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	struct RecentFile *recent;
	uiPopupMenu *pup;
	uiLayout *layout;
	int i, ofs= 0;

	pup= uiPupMenuBegin(C, "Open Recent", 0);
	layout= uiPupMenuLayout(pup);

	if(G.sce[0]) {
		uiItemIntO(layout, G.sce, 0, op->type->idname, "nr", 1);
		ofs = 1;
	}
	
	for(recent = G.recent_files.first, i=0; (i<U.recent_files) && (recent); recent = recent->next, i++)
		if(strcmp(recent->filename, G.sce))
			uiItemIntO(layout, recent->filename, 0, op->type->idname, "nr", i+ofs+1);

	uiPupMenuEnd(C, pup);
	
	return OPERATOR_CANCELLED;
}

static void WM_OT_open_recentfile(wmOperatorType *ot)
{
	ot->name= "Open Recent File";
	ot->idname= "WM_OT_open_recentfile";
	
	ot->invoke= wm_recentfile_invoke;
	ot->exec= recentfile_exec;
	ot->poll= WM_operator_winactive;
	
	RNA_def_property(ot->srna, "nr", PROP_INT, PROP_UNSIGNED);
}

/* ********* main file *********** */

static void untitled(char *name)
{
	if (G.save_over == 0 && strlen(name) < FILE_MAX-16) {
		char *c= BLI_last_slash(name);
		
		if (c)
			strcpy(&c[1], "untitled.blend");
		else
			strcpy(name, "untitled.blend");
	}
}


static int wm_open_mainfile_invoke(bContext *C, wmOperator *op, wmEvent *event)
{

	RNA_string_set(op->ptr, "filename", G.sce);
	WM_event_add_fileselect(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int wm_open_mainfile_exec(bContext *C, wmOperator *op)
{
	char filename[FILE_MAX];
	RNA_string_get(op->ptr, "filename", filename);
	
	// XXX wm in context is not set correctly after WM_read_file -> crash
	// do it before for now, but is this correct with multiple windows?
	WM_event_add_notifier(C, NC_WINDOW, NULL);

	WM_read_file(C, filename, op->reports);
	
	return 0;
}

static void WM_OT_open_mainfile(wmOperatorType *ot)
{
	ot->name= "Open Blender File";
	ot->idname= "WM_OT_open_mainfile";
	
	ot->invoke= wm_open_mainfile_invoke;
	ot->exec= wm_open_mainfile_exec;
	ot->poll= WM_operator_winactive;
	
	ot->flag= 0;
	
	RNA_def_property(ot->srna, "filename", PROP_STRING, PROP_FILEPATH);

}

static int wm_save_as_mainfile_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	char name[FILE_MAX];
	
	BLI_strncpy(name, G.sce, FILE_MAX);
	untitled(name);
	RNA_string_set(op->ptr, "filename", name);
	
	WM_event_add_fileselect(C, op);

	return OPERATOR_RUNNING_MODAL;
}

/* function used for WM_OT_save_mainfile too */
static int wm_save_as_mainfile_exec(bContext *C, wmOperator *op)
{
	char filename[FILE_MAX];
	
	if(RNA_property_is_set(op->ptr, "filename"))
		RNA_string_get(op->ptr, "filename", filename);
	else {
		BLI_strncpy(filename, G.sce, FILE_MAX);
		untitled(filename);
	}
	WM_write_file(C, filename, op->reports);
	
	WM_event_add_notifier(C, NC_WM|ND_FILESAVE, NULL);

	return 0;
}

static void WM_OT_save_as_mainfile(wmOperatorType *ot)
{
	ot->name= "Save As Blender File";
	ot->idname= "WM_OT_save_as_mainfile";
	
	ot->invoke= wm_save_as_mainfile_invoke;
	ot->exec= wm_save_as_mainfile_exec;
	ot->poll= WM_operator_winactive;
	
	ot->flag= 0;
	
	RNA_def_property(ot->srna, "filename", PROP_STRING, PROP_FILEPATH);

}

/* *************** Save file directly ******** */

static int wm_save_mainfile_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	char name[FILE_MAX];
	
	BLI_strncpy(name, G.sce, FILE_MAX);
	untitled(name);
	RNA_string_set(op->ptr, "filename", name);
	uiPupMenuSaveOver(C, op, name);

	return OPERATOR_RUNNING_MODAL;
}

static void WM_OT_save_mainfile(wmOperatorType *ot)
{
	ot->name= "Save Blender File";
	ot->idname= "WM_OT_save_mainfile";
	
	ot->invoke= wm_save_mainfile_invoke;
	ot->exec= wm_save_as_mainfile_exec;
	ot->poll= WM_operator_winactive;
	
	ot->flag= 0;
	
	RNA_def_property(ot->srna, "filename", PROP_STRING, PROP_FILEPATH);
	
}


/* *********************** */

static void WM_OT_window_fullscreen_toggle(wmOperatorType *ot)
{
    ot->name= "Toggle Fullscreen";
    ot->idname= "WM_OT_window_fullscreen_toggle";

    ot->invoke= WM_operator_confirm;
    ot->exec= wm_window_fullscreen_toggle_op;
    ot->poll= WM_operator_winactive;
}

static int wm_exit_blender_op(bContext *C, wmOperator *op)
{
	WM_operator_free(op);
	
	WM_exit(C);	
	
	return OPERATOR_FINISHED;
}

static void WM_OT_exit_blender(wmOperatorType *ot)
{
	ot->name= "Exit Blender";
	ot->idname= "WM_OT_exit_blender";

	ot->invoke= WM_operator_confirm;
	ot->exec= wm_exit_blender_op;
	ot->poll= WM_operator_winactive;
}

/* ************ default paint cursors, draw always around cursor *********** */
/*
 - returns handler to free 
 - poll(bContext): returns 1 if draw should happen
 - draw(bContext): drawing callback for paint cursor
*/

void *WM_paint_cursor_activate(wmWindowManager *wm, int (*poll)(bContext *C),
			       void (*draw)(bContext *C, int, int, void *customdata), void *customdata)
{
	wmPaintCursor *pc= MEM_callocN(sizeof(wmPaintCursor), "paint cursor");
	
	BLI_addtail(&wm->paintcursors, pc);
	
	pc->customdata = customdata;
	pc->poll= poll;
	pc->draw= draw;
	
	return pc;
}

void WM_paint_cursor_end(wmWindowManager *wm, void *handle)
{
	wmPaintCursor *pc;
	
	for(pc= wm->paintcursors.first; pc; pc= pc->next) {
		if(pc == (wmPaintCursor *)handle) {
			BLI_remlink(&wm->paintcursors, pc);
			MEM_freeN(pc);
			return;
		}
	}
}

/* ************ window gesture operator-callback definitions ************** */
/*
 * These are default callbacks for use in operators requiring gesture input
 */

/* **************** Border gesture *************** */

/* Border gesture has two types:
   1) WM_GESTURE_CROSS_RECT: starts a cross, on mouse click it changes to border 
   2) WM_GESTURE_RECT: starts immediate as a border, on mouse click or release it ends

   It stores 4 values (xmin, xmax, ymin, ymax) and event it ended with (event_type)
*/

static int border_apply(bContext *C, wmOperator *op, int event_type)
{
	wmGesture *gesture= op->customdata;
	rcti *rect= gesture->customdata;
	
	if(rect->xmin > rect->xmax)
		SWAP(int, rect->xmin, rect->xmax);
	if(rect->ymin > rect->ymax)
		SWAP(int, rect->ymin, rect->ymax);
	
	if(rect->xmin==rect->xmax || rect->ymin==rect->ymax)
		return 0;
		
	/* operator arguments and storage. */
	RNA_int_set(op->ptr, "xmin", rect->xmin);
	RNA_int_set(op->ptr, "ymin", rect->ymin);
	RNA_int_set(op->ptr, "xmax", rect->xmax);
	RNA_int_set(op->ptr, "ymax", rect->ymax);
	if( RNA_struct_find_property(op->ptr, "event_type") )
		RNA_int_set(op->ptr, "event_type", event_type);
	
	op->type->exec(C, op);
	
	return 1;
}

static void wm_gesture_end(bContext *C, wmOperator *op)
{
	wmGesture *gesture= op->customdata;
	
	WM_gesture_end(C, gesture);	/* frees gesture itself, and unregisters from window */
	op->customdata= NULL;

	ED_area_tag_redraw(CTX_wm_area(C));
	
	if( RNA_struct_find_property(op->ptr, "cursor") )
		WM_cursor_restore(CTX_wm_window(C));
}

int WM_border_select_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	op->customdata= WM_gesture_new(C, event, WM_GESTURE_CROSS_RECT);

	/* add modal handler */
	WM_event_add_modal_handler(C, &CTX_wm_window(C)->handlers, op);
	
	wm_gesture_tag_redraw(C);

	return OPERATOR_RUNNING_MODAL;
}

int WM_border_select_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	wmGesture *gesture= op->customdata;
	rcti *rect= gesture->customdata;
	int sx, sy;
	
	switch(event->type) {
		case MOUSEMOVE:
			
			wm_subwindow_getorigin(CTX_wm_window(C), gesture->swinid, &sx, &sy);
			
			if(gesture->type==WM_GESTURE_CROSS_RECT && gesture->mode==0) {
				rect->xmin= rect->xmax= event->x - sx;
				rect->ymin= rect->ymax= event->y - sy;
			}
			else {
				rect->xmax= event->x - sx;
				rect->ymax= event->y - sy;
			}
			
			wm_gesture_tag_redraw(C);

			break;
			
		case LEFTMOUSE:
		case MIDDLEMOUSE:
		case RIGHTMOUSE:
			if(event->val==1) {
				if(gesture->type==WM_GESTURE_CROSS_RECT && gesture->mode==0) {
					gesture->mode= 1;
					wm_gesture_tag_redraw(C);
				}
			}
			else {
				if(border_apply(C, op, event->type)) {
					wm_gesture_end(C, op);
					return OPERATOR_FINISHED;
				}
				wm_gesture_end(C, op);
				return OPERATOR_CANCELLED;
			}
			break;
		case ESCKEY:
			wm_gesture_end(C, op);
			return OPERATOR_CANCELLED;
	}
	return OPERATOR_RUNNING_MODAL;
}

/* **************** circle gesture *************** */
/* works now only for selection or modal paint stuff, calls exec while hold mouse, exit on release */

int WM_gesture_circle_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	op->customdata= WM_gesture_new(C, event, WM_GESTURE_CIRCLE);
	
	/* add modal handler */
	WM_event_add_modal_handler(C, &CTX_wm_window(C)->handlers, op);
	
	wm_gesture_tag_redraw(C);
	
	return OPERATOR_RUNNING_MODAL;
}

static void gesture_circle_apply(bContext *C, wmOperator *op)
{
	wmGesture *gesture= op->customdata;
	rcti *rect= gesture->customdata;
	
	/* operator arguments and storage. */
	RNA_int_set(op->ptr, "x", rect->xmin);
	RNA_int_set(op->ptr, "y", rect->ymin);
	RNA_int_set(op->ptr, "radius", rect->xmax);
	
	if(op->type->exec)
		op->type->exec(C, op);
}

int WM_gesture_circle_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	wmGesture *gesture= op->customdata;
	rcti *rect= gesture->customdata;
	int sx, sy;
	
	switch(event->type) {
		case MOUSEMOVE:
			
			wm_subwindow_getorigin(CTX_wm_window(C), gesture->swinid, &sx, &sy);
			
			rect->xmin= event->x - sx;
			rect->ymin= event->y - sy;
			
			wm_gesture_tag_redraw(C);
			
			if(gesture->mode)
				gesture_circle_apply(C, op);

			break;
		case WHEELUPMOUSE:
			rect->xmax += 2 + rect->xmax/10;
			wm_gesture_tag_redraw(C);
			break;
		case WHEELDOWNMOUSE:
			rect->xmax -= 2 + rect->xmax/10;
			if(rect->xmax < 1) rect->xmax= 1;
			wm_gesture_tag_redraw(C);
			break;
		case LEFTMOUSE:
		case MIDDLEMOUSE:
		case RIGHTMOUSE:
			if(event->val==0) {	/* key release */
				wm_gesture_end(C, op);
				return OPERATOR_FINISHED;
			}
			else {
				if( RNA_struct_find_property(op->ptr, "event_type") )
					RNA_int_set(op->ptr, "event_type", event->type);
				
				/* apply first click */
				gesture_circle_apply(C, op);
				gesture->mode= 1;
			}
			break;
		case ESCKEY:
			wm_gesture_end(C, op);
			return OPERATOR_CANCELLED;
	}
	return OPERATOR_RUNNING_MODAL;
}

#if 0
/* template to copy from */
void WM_OT_circle_gesture(wmOperatorType *ot)
{
	ot->name= "Circle Gesture";
	ot->idname= "WM_OT_circle_gesture";
	
	ot->invoke= WM_gesture_circle_invoke;
	ot->modal= WM_gesture_circle_modal;
	
	ot->poll= WM_operator_winactive;
	
	RNA_def_property(ot->srna, "x", PROP_INT, PROP_NONE);
	RNA_def_property(ot->srna, "y", PROP_INT, PROP_NONE);
	RNA_def_property(ot->srna, "radius", PROP_INT, PROP_NONE);

}
#endif

/* **************** Tweak gesture *************** */

static void tweak_gesture_modal(bContext *C, wmEvent *event)
{
	wmWindow *window= CTX_wm_window(C);
	wmGesture *gesture= window->tweak;
	rcti *rect= gesture->customdata;
	int sx, sy, val;
	
	switch(event->type) {
		case MOUSEMOVE:
			
			wm_subwindow_getorigin(window, gesture->swinid, &sx, &sy);
			
			rect->xmax= event->x - sx;
			rect->ymax= event->y - sy;
			
			if((val= wm_gesture_evaluate(C, gesture))) {
				wmEvent event;

				event= *(window->eventstate);
				if(gesture->event_type==LEFTMOUSE)
					event.type= EVT_TWEAK_L;
				else if(gesture->event_type==RIGHTMOUSE)
					event.type= EVT_TWEAK_R;
				else
					event.type= EVT_TWEAK_M;
				event.val= val;
				/* mouse coords! */
				wm_event_add(window, &event);
				
				WM_gesture_end(C, gesture);	/* frees gesture itself, and unregisters from window */
				window->tweak= NULL;
			}
			
			break;
			
		case LEFTMOUSE:
		case RIGHTMOUSE:
		case MIDDLEMOUSE:
			if(gesture->event_type==event->type) {
				WM_gesture_end(C, gesture);
				window->tweak= NULL;
				
				/* when tweak fails we should give the other keymap entries a chance */
				event->val= KM_RELEASE;
			}
			break;
		default:
			WM_gesture_end(C, gesture);
			window->tweak= NULL;
	}
}

/* standard tweak, called after window handlers passed on event */
void wm_tweakevent_test(bContext *C, wmEvent *event, int action)
{
	wmWindow *win= CTX_wm_window(C);
	
	if(win->tweak==NULL) {
		if(CTX_wm_region(C)) {
			if(event->val) { // pressed
				if( ELEM3(event->type, LEFTMOUSE, MIDDLEMOUSE, RIGHTMOUSE) )
					win->tweak= WM_gesture_new(C, event, WM_GESTURE_TWEAK);
			}
		}
	}
	else {
		if(action==WM_HANDLER_BREAK) {
			WM_gesture_end(C, win->tweak);
			win->tweak= NULL;
		}
		else
			tweak_gesture_modal(C, event);
	}
}

/* *********************** lasso gesture ****************** */

int WM_gesture_lasso_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	op->customdata= WM_gesture_new(C, event, WM_GESTURE_LASSO);
	
	/* add modal handler */
	WM_event_add_modal_handler(C, &CTX_wm_window(C)->handlers, op);
	
	wm_gesture_tag_redraw(C);
	
	if( RNA_struct_find_property(op->ptr, "cursor") )
		WM_cursor_modal(CTX_wm_window(C), RNA_int_get(op->ptr, "cursor"));
	
	return OPERATOR_RUNNING_MODAL;
}

int WM_gesture_lines_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	op->customdata= WM_gesture_new(C, event, WM_GESTURE_LINES);
	
	/* add modal handler */
	WM_event_add_modal_handler(C, &CTX_wm_window(C)->handlers, op);
	
	wm_gesture_tag_redraw(C);
	
	if( RNA_struct_find_property(op->ptr, "cursor") )
		WM_cursor_modal(CTX_wm_window(C), RNA_int_get(op->ptr, "cursor"));
	
	return OPERATOR_RUNNING_MODAL;
}


static void gesture_lasso_apply(bContext *C, wmOperator *op, int event_type)
{
	wmGesture *gesture= op->customdata;
	PointerRNA itemptr;
	float loc[2];
	int i;
	short *lasso= gesture->customdata;
	
	/* operator storage as path. */

	for(i=0; i<gesture->points; i++, lasso+=2) {
		loc[0]= lasso[0];
		loc[1]= lasso[1];
		RNA_collection_add(op->ptr, "path", &itemptr);
		RNA_float_set_array(&itemptr, "loc", loc);
	}
	
	wm_gesture_end(C, op);
		
	if(op->type->exec)
		op->type->exec(C, op);
	
}

int WM_gesture_lasso_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	wmGesture *gesture= op->customdata;
	int sx, sy;
	
	switch(event->type) {
		case MOUSEMOVE:
			
			wm_gesture_tag_redraw(C);
			
			wm_subwindow_getorigin(CTX_wm_window(C), gesture->swinid, &sx, &sy);
			if(gesture->points < WM_LASSO_MAX_POINTS) {
				short *lasso= gesture->customdata;
				lasso += 2 * gesture->points;
				lasso[0] = event->x - sx;
				lasso[1] = event->y - sy;
				gesture->points++;
			}
			else {
				gesture_lasso_apply(C, op, event->type);
				return OPERATOR_FINISHED;
			}
			break;
			
		case LEFTMOUSE:
		case MIDDLEMOUSE:
		case RIGHTMOUSE:
			if(event->val==0) {	/* key release */
				gesture_lasso_apply(C, op, event->type);
				return OPERATOR_FINISHED;
			}
			break;
		case ESCKEY:
			wm_gesture_end(C, op);
			return OPERATOR_CANCELLED;
	}
	return OPERATOR_RUNNING_MODAL;
}

int WM_gesture_lines_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	return WM_gesture_lasso_modal(C, op, event);
}

#if 0
/* template to copy from */

static int gesture_lasso_exec(bContext *C, wmOperator *op)
{
	RNA_BEGIN(op->ptr, itemptr, "path") {
		float loc[2];
		
		RNA_float_get_array(&itemptr, "loc", loc);
		printf("Location: %f %f\n", loc[0], loc[1]);
	}
	RNA_END;
	
	return OPERATOR_FINISHED;
}

void WM_OT_lasso_gesture(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	ot->name= "Lasso Gesture";
	ot->idname= "WM_OT_lasso_gesture";
	
	ot->invoke= WM_gesture_lasso_invoke;
	ot->modal= WM_gesture_lasso_modal;
	ot->exec= gesture_lasso_exec;
	
	ot->poll= WM_operator_winactive;
	
	prop= RNA_def_property(ot->srna, "path", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_runtime(prop, &RNA_OperatorMousePath);
}
#endif

/* *********************** radial control ****************** */

const int WM_RADIAL_CONTROL_DISPLAY_SIZE = 200;

typedef struct wmRadialControl {
	int mode;
	float initial_value, value, max_value;
	int initial_mouse[2];
	void *cursor;
	GLuint tex;
} wmRadialControl;

static void wm_radial_control_paint(bContext *C, int x, int y, void *customdata)
{
	wmRadialControl *rc = (wmRadialControl*)customdata;
	ARegion *ar = CTX_wm_region(C);
	float r1=0.0f, r2=0.0f, r3=0.0f, angle=0.0f;

	/* Keep cursor in the original place */
	x = rc->initial_mouse[0] - ar->winrct.xmin;
	y = rc->initial_mouse[1] - ar->winrct.ymin;

	glPushMatrix();
	
	glTranslatef((float)x, (float)y, 0.0f);

	if(rc->mode == WM_RADIALCONTROL_SIZE) {
		r1= rc->value;
		r2= rc->initial_value;
		r3= r1;
	} else if(rc->mode == WM_RADIALCONTROL_STRENGTH) {
		r1= (1 - rc->value) * WM_RADIAL_CONTROL_DISPLAY_SIZE;
		r2= WM_RADIAL_CONTROL_DISPLAY_SIZE;
		r3= WM_RADIAL_CONTROL_DISPLAY_SIZE;
	} else if(rc->mode == WM_RADIALCONTROL_ANGLE) {
		r1= r2= WM_RADIAL_CONTROL_DISPLAY_SIZE;
		r3= WM_RADIAL_CONTROL_DISPLAY_SIZE;
		angle = rc->value;
	}

	glColor4ub(255, 255, 255, 128);
	glEnable( GL_LINE_SMOOTH );
	glEnable(GL_BLEND);

	if(rc->mode == WM_RADIALCONTROL_ANGLE)
		fdrawline(0, 0, WM_RADIAL_CONTROL_DISPLAY_SIZE, 0);

	if(rc->tex) {
		const float str = rc->mode == WM_RADIALCONTROL_STRENGTH ? (rc->value + 0.5) : 1;

		if(rc->mode == WM_RADIALCONTROL_ANGLE) {
			glRotatef(angle, 0, 0, 1);
			fdrawline(0, 0, WM_RADIAL_CONTROL_DISPLAY_SIZE, 0);
		}

		glBindTexture(GL_TEXTURE_2D, rc->tex);

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glEnable(GL_TEXTURE_2D);
		glBegin(GL_QUADS);
		glColor4f(0,0,0, str);
		glTexCoord2f(0,0);
		glVertex2f(-r3, -r3);
		glTexCoord2f(1,0);
		glVertex2f(r3, -r3);
		glTexCoord2f(1,1);
		glVertex2f(r3, r3);
		glTexCoord2f(0,1);
		glVertex2f(-r3, r3);
		glEnd();
		glDisable(GL_TEXTURE_2D);
	}

	glColor4ub(255, 255, 255, 128);	
	glutil_draw_lined_arc(0.0, M_PI*2.0, r1, 40);
	glutil_draw_lined_arc(0.0, M_PI*2.0, r2, 40);
	glDisable(GL_BLEND);
	glDisable( GL_LINE_SMOOTH );
	
	glPopMatrix();
}

int WM_radial_control_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	wmRadialControl *rc = (wmRadialControl*)op->customdata;
	int mode, initial_mouse[2], delta[2];
	float dist;
	double new_value = RNA_float_get(op->ptr, "new_value");
	int ret = OPERATOR_RUNNING_MODAL;

	mode = RNA_int_get(op->ptr, "mode");
	RNA_int_get_array(op->ptr, "initial_mouse", initial_mouse);

	switch(event->type) {
	case MOUSEMOVE:
		delta[0]= initial_mouse[0] - event->x;
		delta[1]= initial_mouse[1] - event->y;
		dist= sqrt(delta[0]*delta[0]+delta[1]*delta[1]);

		if(mode == WM_RADIALCONTROL_SIZE)
			new_value = dist;
		else if(mode == WM_RADIALCONTROL_STRENGTH) {
			new_value = 1 - dist / WM_RADIAL_CONTROL_DISPLAY_SIZE;
		} else if(mode == WM_RADIALCONTROL_ANGLE)
			new_value = ((int)(atan2(delta[1], delta[0]) * (180.0 / M_PI)) + 180);
		
		if(event->ctrl) {
			if(mode == WM_RADIALCONTROL_STRENGTH)
				new_value = ((int)(new_value * 100) / 10*10) / 100.0f;
			else
				new_value = ((int)new_value + 5) / 10*10;
		}
		
		break;
	case ESCKEY:
	case RIGHTMOUSE:
		ret = OPERATOR_CANCELLED;
		break;
	case LEFTMOUSE:
	case PADENTER:
		op->type->exec(C, op);
		ret = OPERATOR_FINISHED;
		break;
	}

	/* Clamp */
	if(new_value > rc->max_value)
		new_value = rc->max_value;
	else if(new_value < 0)
		new_value = 0;

	/* Update paint data */
	rc->value = new_value;

	RNA_float_set(op->ptr, "new_value", new_value);

	if(ret != OPERATOR_RUNNING_MODAL) {
		WM_paint_cursor_end(CTX_wm_manager(C), rc->cursor);
		MEM_freeN(rc);
	}
	
	ED_region_tag_redraw(CTX_wm_region(C));

	return ret;
}

/* Expects the operator customdata to be an ImBuf (or NULL) */
int WM_radial_control_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	wmRadialControl *rc = MEM_callocN(sizeof(wmRadialControl), "radial control");
	int mode = RNA_int_get(op->ptr, "mode");
	float initial_value = RNA_float_get(op->ptr, "initial_value");
	int mouse[2] = {event->x, event->y};

	if(mode == WM_RADIALCONTROL_SIZE) {
		rc->max_value = 200;
		mouse[0]-= initial_value;
	}
	else if(mode == WM_RADIALCONTROL_STRENGTH) {
		rc->max_value = 1;
		mouse[0]-= WM_RADIAL_CONTROL_DISPLAY_SIZE * (1 - initial_value);
	}
	else if(mode == WM_RADIALCONTROL_ANGLE) {
		rc->max_value = 360;
		mouse[0]-= WM_RADIAL_CONTROL_DISPLAY_SIZE * cos(initial_value);
		mouse[1]-= WM_RADIAL_CONTROL_DISPLAY_SIZE * sin(initial_value);
		initial_value *= 180.0f/M_PI;
	}

	if(op->customdata) {
		ImBuf *im = (ImBuf*)op->customdata;
		/* Build GL texture */
		glGenTextures(1, &rc->tex);
		glBindTexture(GL_TEXTURE_2D, rc->tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, im->x, im->y, 0, GL_ALPHA, GL_FLOAT, im->rect_float);
		MEM_freeN(im->rect_float);
		MEM_freeN(im);
	}

	RNA_int_set_array(op->ptr, "initial_mouse", mouse);
	RNA_float_set(op->ptr, "new_value", initial_value);
		
	op->customdata = rc;
	rc->mode = mode;
	rc->initial_value = initial_value;
	rc->initial_mouse[0] = mouse[0];
	rc->initial_mouse[1] = mouse[1];
	rc->cursor = WM_paint_cursor_activate(CTX_wm_manager(C), op->type->poll,
					      wm_radial_control_paint, op->customdata);

	/* add modal handler */
	WM_event_add_modal_handler(C, &CTX_wm_window(C)->handlers, op);
	
	WM_radial_control_modal(C, op, event);
	
	return OPERATOR_RUNNING_MODAL;
}

/* Gets a descriptive string of the operation */
void WM_radial_control_string(wmOperator *op, char str[], int maxlen)
{
	int mode = RNA_int_get(op->ptr, "mode");
	float v = RNA_float_get(op->ptr, "new_value");

	if(mode == WM_RADIALCONTROL_SIZE)
		sprintf(str, "Size: %d", (int)v);
	else if(mode == WM_RADIALCONTROL_STRENGTH)
		sprintf(str, "Strength: %d", (int)v);
	else if(mode == WM_RADIALCONTROL_ANGLE)
		sprintf(str, "Angle: %d", (int)(v * 180.0f/M_PI));
}

/** Important: this doesn't define an actual operator, it
    just sets up the common parts of the radial control op. **/
void WM_OT_radial_control_partial(wmOperatorType *ot)
{
	static EnumPropertyItem prop_mode_items[] = {
		{WM_RADIALCONTROL_SIZE, "SIZE", 0, "Size", ""},
		{WM_RADIALCONTROL_STRENGTH, "STRENGTH", 0, "Strength", ""},
		{WM_RADIALCONTROL_ANGLE, "ANGLE", 0, "Angle", ""},
		{0, NULL, 0, NULL, NULL}};

	/* Should be set in custom invoke() */
	RNA_def_float(ot->srna, "initial_value", 0, 0, FLT_MAX, "Initial Value", "", 0, FLT_MAX);

	/* Set internally, should be used in custom exec() to get final value */
	RNA_def_float(ot->srna, "new_value", 0, 0, FLT_MAX, "New Value", "", 0, FLT_MAX);

	/* Should be set before calling operator */
	RNA_def_enum(ot->srna, "mode", prop_mode_items, 0, "Mode", "");

	/* Internal */
	RNA_def_int_vector(ot->srna, "initial_mouse", 2, NULL, INT_MIN, INT_MAX, "initial_mouse", "", INT_MIN, INT_MAX);
}

/* ************************** timer for testing ***************** */

/* uses no type defines, fully local testing function anyway... ;) */

static int ten_timer_exec(bContext *C, wmOperator *op)
{
	ARegion *ar= CTX_wm_region(C);
	double stime= PIL_check_seconds_timer();
	int type = RNA_int_get(op->ptr, "type");
	int a, time;
	char tmpstr[128];
	
	WM_cursor_wait(1);
	
	for(a=0; a<10; a++) {
		if (type==0) {
			ED_region_do_draw(C, ar);
		} 
		else if (type==1) {
			wmWindow *win= CTX_wm_window(C);
			
			ED_region_tag_redraw(ar);
			wm_draw_update(C);
			
			CTX_wm_window_set(C, win);	/* XXX context manipulation warning! */
		}
		else if (type==2) {
			wmWindow *win= CTX_wm_window(C);
			ScrArea *sa;
			
			for(sa= CTX_wm_screen(C)->areabase.first; sa; sa= sa->next)
				ED_area_tag_redraw(sa);
			wm_draw_update(C);
			
			CTX_wm_window_set(C, win);	/* XXX context manipulation warning! */
		}
		else if (type==3) {
			Scene *scene= CTX_data_scene(C);
			
			if(a & 1) scene->r.cfra--;
			else scene->r.cfra++;
			scene_update_for_newframe(scene, scene->lay);
		}
		else {
			ED_undo_pop(C);
			ED_undo_redo(C);
		}
	}
	
	time= (int) ((PIL_check_seconds_timer()-stime)*1000);
	
	if(type==0) sprintf(tmpstr, "10 x Draw Region: %d ms", time);
	if(type==1) sprintf(tmpstr, "10 x Draw Region and Swap: %d ms", time);
	if(type==2) sprintf(tmpstr, "10 x Draw Window and Swap: %d ms", time);
	if(type==3) sprintf(tmpstr, "Anim Step: %d ms", time);
	if(type==4) sprintf(tmpstr, "10 x Undo/Redo: %d ms", time);
	
	WM_cursor_wait(0);
	
	uiPupMenuNotice(C, tmpstr);
	
	return OPERATOR_FINISHED;
}

static void WM_OT_ten_timer(wmOperatorType *ot)
{
	static EnumPropertyItem prop_type_items[] = {
	{0, "DRAW", 0, "Draw Region", ""},
	{1, "DRAWSWAP", 0, "Draw Region + Swap", ""},
	{2, "DRAWWINSWAP", 0, "Draw Window + Swap", ""},
	{3, "ANIMSTEP", 0, "Anim Step", ""},
	{4, "UNDO", 0, "Undo/Redo", ""},
	{0, NULL, 0, NULL, NULL}};
	
	ot->name= "Ten Timer";
	ot->idname= "WM_OT_ten_timer";
	
	ot->invoke= WM_menu_invoke;
	ot->exec= ten_timer_exec;
	ot->poll= WM_operator_winactive;
	
	RNA_def_enum(ot->srna, "type", prop_type_items, 0, "Type", "");

}



/* ******************************************************* */
 
/* called on initialize WM_exit() */
void wm_operatortype_free(void)
{
	BLI_freelistN(&global_ops);
}

/* called on initialize WM_init() */
void wm_operatortype_init(void)
{
	WM_operatortype_append(WM_OT_window_duplicate);
	WM_operatortype_append(WM_OT_read_homefile);
	WM_operatortype_append(WM_OT_save_homefile);
	WM_operatortype_append(WM_OT_window_fullscreen_toggle);
	WM_operatortype_append(WM_OT_exit_blender);
	WM_operatortype_append(WM_OT_open_recentfile);
	WM_operatortype_append(WM_OT_open_mainfile);
	WM_operatortype_append(WM_OT_jobs_timer);
	WM_operatortype_append(WM_OT_save_as_mainfile);
	WM_operatortype_append(WM_OT_save_mainfile);
	WM_operatortype_append(WM_OT_ten_timer);
	WM_operatortype_append(WM_OT_debug_menu);
	WM_operatortype_append(WM_OT_search_menu);
}

/* default keymap for windows and screens, only call once per WM */
void wm_window_keymap(wmWindowManager *wm)
{
	ListBase *keymap= WM_keymap_listbase(wm, "Window", 0, 0);
	
	/* items to make WM work */
	WM_keymap_verify_item(keymap, "WM_OT_jobs_timer", TIMERJOBS, KM_ANY, KM_ANY, 0);
	
	/* note, this doesn't replace existing keymap items */
	WM_keymap_verify_item(keymap, "WM_OT_window_duplicate", WKEY, KM_PRESS, KM_CTRL|KM_ALT, 0);
	WM_keymap_verify_item(keymap, "WM_OT_read_homefile", XKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_verify_item(keymap, "WM_OT_save_homefile", UKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_verify_item(keymap, "WM_OT_open_recentfile", OKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_verify_item(keymap, "WM_OT_open_mainfile", F1KEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "WM_OT_save_mainfile", WKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_verify_item(keymap, "WM_OT_save_as_mainfile", F2KEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "WM_OT_window_fullscreen_toggle", F11KEY, KM_PRESS, 0, 0);
	WM_keymap_verify_item(keymap, "WM_OT_exit_blender", QKEY, KM_PRESS, KM_CTRL, 0);

	/* debug/testing */
	WM_keymap_verify_item(keymap, "WM_OT_ten_timer", TKEY, KM_PRESS, KM_ALT|KM_CTRL, 0);
	WM_keymap_verify_item(keymap, "WM_OT_debug_menu", DKEY, KM_PRESS, KM_ALT|KM_CTRL, 0);
	WM_keymap_verify_item(keymap, "WM_OT_search_menu", FKEY, KM_PRESS, KM_ALT|KM_CTRL, 0);
	
}

