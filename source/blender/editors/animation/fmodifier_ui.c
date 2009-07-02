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
 * Contributor(s): Blender Foundation, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* User-Interface Stuff for F-Modifiers:
 * This file defines the (C-Coded) templates + editing callbacks needed 
 * by the interface stuff or F-Modifiers, as used by F-Curves in the Graph Editor,
 * and NLA-Strips in the NLA Editor.
 */
 
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_nla.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_anim_api.h"
#include "ED_keyframing.h"
#include "ED_screen.h"
#include "ED_types.h"
#include "ED_util.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

// XXX! --------------------------------
/* temporary definition for limits of float number buttons (FLT_MAX tends to infinity with old system) */
#define UI_FLT_MAX 	10000.0f

/* ********************************************** */

#define B_REDR 					1
#define B_FMODIFIER_REDRAW		20

/* macro for use here to draw background box and set height */
// XXX for now, roundbox has it's callback func set to NULL to not intercept events
#define DRAW_BACKDROP(height) \
	{ \
		uiDefBut(block, ROUNDBOX, B_REDR, "", -3, *yco-height, width+3, height-1, NULL, 5.0, 0.0, 12.0, (float)rb_col, ""); \
	}

/* callback to verify modifier data */
static void validate_fmodifier_cb (bContext *C, void *fcm_v, void *dummy)
{
	FModifier *fcm= (FModifier *)fcm_v;
	FModifierTypeInfo *fmi= fmodifier_get_typeinfo(fcm);
	
	/* call the verify callback on the modifier if applicable */
	if (fmi && fmi->verify_data)
		fmi->verify_data(fcm);
}

/* callback to set the active modifier */
static void activate_fmodifier_cb (bContext *C, void *fmods_v, void *fcm_v)
{
	ListBase *modifiers = (ListBase *)fmods_v;
	FModifier *fcm= (FModifier *)fcm_v;
	
	/* call API function to set the active modifier for active modifier-stack */
	set_active_fmodifier(modifiers, fcm);
}

/* callback to remove the given modifier  */
static void delete_fmodifier_cb (bContext *C, void *fmods_v, void *fcm_v)
{
	ListBase *modifiers = (ListBase *)fmods_v;
	FModifier *fcm= (FModifier *)fcm_v;
	
	/* remove the given F-Modifier from the active modifier-stack */
	remove_fmodifier(modifiers, fcm);
}

/* --------------- */
	
/* draw settings for generator modifier */
static void draw_modifier__generator(uiBlock *block, FModifier *fcm, int *yco, short *height, short width, short active, int rb_col)
{
	FMod_Generator *data= (FMod_Generator *)fcm->data;
	char gen_mode[]="Generator Type%t|Expanded Polynomial%x0|Factorised Polynomial%x1";
	int cy= *yco - 30;
	uiBut *but;
	
	/* set the height */
	(*height) = 90;
	switch (data->mode) {
		case FCM_GENERATOR_POLYNOMIAL: /* polynomial expression */
			(*height) += 20*(data->poly_order+1) + 20;
			break;
		case FCM_GENERATOR_POLYNOMIAL_FACTORISED: /* factorised polynomial */
			(*height) += 20 * data->poly_order + 15;
			break;
	}
	
	/* basic settings (backdrop + mode selector + some padding) */
	DRAW_BACKDROP((*height));
	uiBlockBeginAlign(block);
		but= uiDefButI(block, MENU, B_FMODIFIER_REDRAW, gen_mode, 10,cy,width-30,19, &data->mode, 0, 0, 0, 0, "Selects type of generator algorithm.");
		uiButSetFunc(but, validate_fmodifier_cb, fcm, NULL);
		cy -= 20;
		
		uiDefButBitI(block, TOG, FCM_GENERATOR_ADDITIVE, B_FMODIFIER_REDRAW, "Additive", 10,cy,width-30,19, &data->flag, 0, 0, 0, 0, "Values generated by this modifier are applied on top of the existing values instead of overwriting them");
		cy -= 35;
	uiBlockEndAlign(block);
	
	/* now add settings for individual modes */
	switch (data->mode) {
		case FCM_GENERATOR_POLYNOMIAL: /* polynomial expression */
		{
			float *cp = NULL;
			char xval[32];
			unsigned int i;
			
			/* draw polynomial order selector */
			but= uiDefButI(block, NUM, B_FMODIFIER_REDRAW, "Poly Order: ", 10,cy,width-30,19, &data->poly_order, 1, 100, 0, 0, "'Order' of the Polynomial - for a polynomial with n terms, 'order' is n-1");
			uiButSetFunc(but, validate_fmodifier_cb, fcm, NULL);
			cy -= 35;
			
			/* draw controls for each coefficient and a + sign at end of row */
			uiDefBut(block, LABEL, 1, "y = ", 0, cy, 50, 20, NULL, 0.0, 0.0, 0, 0, "");
			
			cp= data->coefficients;
			for (i=0; (i < data->arraysize) && (cp); i++, cp++) {
				/* coefficient */
				uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "", 50, cy, 150, 20, cp, -UI_FLT_MAX, UI_FLT_MAX, 10, 3, "Coefficient for polynomial");
				
				/* 'x' param (and '+' if necessary) */
				if (i == 0)
					strcpy(xval, "");
				else if (i == 1)
					strcpy(xval, "x");
				else
					sprintf(xval, "x^%d", i);
				uiDefBut(block, LABEL, 1, xval, 200, cy, 50, 20, NULL, 0.0, 0.0, 0, 0, "Power of x");
				
				if ( (i != (data->arraysize - 1)) || ((i==0) && data->arraysize==2) )
					uiDefBut(block, LABEL, 1, "+", 250, cy, 30, 20, NULL, 0.0, 0.0, 0, 0, "");
				
				cy -= 20;
			}
		}
			break;
		
		case FCM_GENERATOR_POLYNOMIAL_FACTORISED: /* factorised polynomial expression */
		{
			float *cp = NULL;
			unsigned int i;
			
			/* draw polynomial order selector */
			but= uiDefButI(block, NUM, B_FMODIFIER_REDRAW, "Poly Order: ", 10,cy,width-30,19, &data->poly_order, 1, 100, 0, 0, "'Order' of the Polynomial - for a polynomial with n terms, 'order' is n-1");
			uiButSetFunc(but, validate_fmodifier_cb, fcm, NULL);
			cy -= 35;
			
			/* draw controls for each pair of coefficients */
			uiDefBut(block, LABEL, 1, "y = ", 0, cy, 50, 20, NULL, 0.0, 0.0, 0, 0, "");
			
			cp= data->coefficients;
			for (i=0; (i < data->poly_order) && (cp); i++, cp+=2) {
				/* opening bracket */
				uiDefBut(block, LABEL, 1, "(", 40, cy, 50, 20, NULL, 0.0, 0.0, 0, 0, "");
				
				/* coefficients */
				uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "", 50, cy, 100, 20, cp, -UI_FLT_MAX, UI_FLT_MAX, 10, 3, "Coefficient of x");
				
				uiDefBut(block, LABEL, 1, "x + ", 150, cy, 30, 20, NULL, 0.0, 0.0, 0, 0, "");
				
				uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "", 180, cy, 100, 20, cp+1, -UI_FLT_MAX, UI_FLT_MAX, 10, 3, "Second coefficient");
				
				/* closing bracket and '+' sign */
				if ( (i != (data->poly_order - 1)) || ((i==0) && data->poly_order==2) )
					uiDefBut(block, LABEL, 1, ") ?", 280, cy, 30, 20, NULL, 0.0, 0.0, 0, 0, "");
				else
					uiDefBut(block, LABEL, 1, ")", 280, cy, 30, 20, NULL, 0.0, 0.0, 0, 0, "");
				
				cy -= 20;
			}
		}
			break;
	}
}

/* --------------- */

/* draw settings for noise modifier */
static void draw_modifier__fn_generator(uiBlock *block, FModifier *fcm, int *yco, short *height, short width, short active, int rb_col)
{
	FMod_FunctionGenerator *data= (FMod_FunctionGenerator *)fcm->data;
	int cy= (*yco - 30), cy1= (*yco - 50), cy2= (*yco - 70);
	char fn_type[]="Built-In Function%t|Sin%x0|Cos%x1|Tan%x2|Square Root%x3|Natural Log%x4|Normalised Sin%x5";
	
	/* set the height */
	(*height) = 80;
	
	/* basic settings (backdrop + some padding) */
	DRAW_BACKDROP((*height));
	
	uiDefButI(block, MENU, B_FMODIFIER_REDRAW, fn_type,
			  3, cy, 300, 20, &data->type, 0, 0, 0, 0, "Type of function used to generate values");
	
	
	uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "Amplitude:", 
			  3, cy1, 150, 20, &data->amplitude, 0.000001, 10000.0, 0.01, 3, "Scale factor determining the maximum/minimum values.");
	uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "Value Offset:", 
			  3, cy2, 150, 20, &data->value_offset, 0.0, 10000.0, 0.01, 3, "Constant factor to offset values by.");
	
	uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "Phase Multiplier:", 
			  155, cy1, 150, 20, &data->phase_multiplier, 0.0, 100000.0, 0.1, 3, "Scale factor determining the 'speed' of the function.");
	uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "Phase Offset:", 
			  155, cy2, 150, 20, &data->phase_offset, 0.0, 100000.0, 0.1, 3, "Constant factor to offset time by for function.");

}

/* --------------- */

/* draw settings for cycles modifier */
static void draw_modifier__cycles(uiBlock *block, FModifier *fcm, int *yco, short *height, short width, short active, int rb_col)
{
	FMod_Cycles *data= (FMod_Cycles *)fcm->data;
	char cyc_mode[]="Cycling Mode%t|No Cycles%x0|Repeat Motion%x1|Repeat with Offset%x2|Repeat Mirrored%x3";
	int cy= (*yco - 30), cy1= (*yco - 50), cy2= (*yco - 70);
	
	/* set the height */
	(*height) = 80;
	
	/* basic settings (backdrop + some padding) */
	DRAW_BACKDROP((*height));
	
	/* 'before' range */
	uiDefBut(block, LABEL, 1, "Before:", 4, cy, 80, 20, NULL, 0.0, 0.0, 0, 0, "Settings for cycling before first keyframe");
	uiBlockBeginAlign(block);
		uiDefButS(block, MENU, B_FMODIFIER_REDRAW, cyc_mode, 3,cy1,150,20, &data->before_mode, 0, 0, 0, 0, "Cycling mode to use before first keyframe");
		uiDefButS(block, NUM, B_FMODIFIER_REDRAW, "Max Cycles:", 3, cy2, 150, 20, &data->before_cycles, 0, 10000, 10, 3, "Maximum number of cycles to allow (0 = infinite)");
	uiBlockEndAlign(block);
	
	/* 'after' range */
	uiDefBut(block, LABEL, 1, "After:", 155, cy, 80, 20, NULL, 0.0, 0.0, 0, 0, "Settings for cycling after last keyframe");
	uiBlockBeginAlign(block);
		uiDefButS(block, MENU, B_FMODIFIER_REDRAW, cyc_mode, 157,cy1,150,20, &data->after_mode, 0, 0, 0, 0, "Cycling mode to use after first keyframe");
		uiDefButS(block, NUM, B_FMODIFIER_REDRAW, "Max Cycles:", 157, cy2, 150, 20, &data->after_cycles, 0, 10000, 10, 3, "Maximum number of cycles to allow (0 = infinite)");
	uiBlockEndAlign(block);
}

/* --------------- */

/* draw settings for noise modifier */
static void draw_modifier__noise(uiBlock *block, FModifier *fcm, int *yco, short *height, short width, short active, int rb_col)
{
	FMod_Noise *data= (FMod_Noise *)fcm->data;
	int cy= (*yco - 30), cy1= (*yco - 50), cy2= (*yco - 70);
	char blend_mode[]="Modification %t|Replace %x0|Add %x1|Subtract %x2|Multiply %x3";
	
	/* set the height */
	(*height) = 80;
	
	/* basic settings (backdrop + some padding) */
	DRAW_BACKDROP((*height));
	
	uiDefButS(block, MENU, B_FMODIFIER_REDRAW, blend_mode,
			  3, cy, 150, 20, &data->modification, 0, 0, 0, 0, "Method of combining the results of this modifier and other modifiers.");
	
	uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "Size:", 
			  3, cy1, 150, 20, &data->size, 0.000001, 10000.0, 0.01, 3, "");
	uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "Strength:", 
			  3, cy2, 150, 20, &data->strength, 0.0, 10000.0, 0.01, 3, "");
	
	uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "Phase:", 
			  155, cy1, 150, 20, &data->phase, 0.0, 100000.0, 0.1, 3, "");
	uiDefButS(block, NUM, B_FMODIFIER_REDRAW, "Depth:", 
			  155, cy2, 150, 20, &data->depth, 0, 128, 1, 3, "");

}

/* --------------- */

#define BINARYSEARCH_FRAMEEQ_THRESH	0.0001

/* Binary search algorithm for finding where to insert Envelope Data Point.
 * Returns the index to insert at (data already at that index will be offset if replace is 0)
 */
static int binarysearch_fcm_envelopedata_index (FCM_EnvelopeData array[], float frame, int arraylen, short *exists)
{
	int start=0, end=arraylen;
	int loopbreaker= 0, maxloop= arraylen * 2;
	
	/* initialise exists-flag first */
	*exists= 0;
	
	/* sneaky optimisations (don't go through searching process if...):
	 *	- keyframe to be added is to be added out of current bounds
	 *	- keyframe to be added would replace one of the existing ones on bounds
	 */
	if ((arraylen <= 0) || (array == NULL)) {
		printf("Warning: binarysearch_fcm_envelopedata_index() encountered invalid array \n");
		return 0;
	}
	else {
		/* check whether to add before/after/on */
		float framenum;
		
		/* 'First' Point (when only one point, this case is used) */
		framenum= array[0].time;
		if (IS_EQT(frame, framenum, BINARYSEARCH_FRAMEEQ_THRESH)) {
			*exists = 1;
			return 0;
		}
		else if (frame < framenum)
			return 0;
			
		/* 'Last' Point */
		framenum= array[(arraylen-1)].time;
		if (IS_EQT(frame, framenum, BINARYSEARCH_FRAMEEQ_THRESH)) {
			*exists= 1;
			return (arraylen - 1);
		}
		else if (frame > framenum)
			return arraylen;
	}
	
	
	/* most of the time, this loop is just to find where to put it
	 * 	- 'loopbreaker' is just here to prevent infinite loops 
	 */
	for (loopbreaker=0; (start <= end) && (loopbreaker < maxloop); loopbreaker++) {
		/* compute and get midpoint */
		int mid = start + ((end - start) / 2);	/* we calculate the midpoint this way to avoid int overflows... */
		float midfra= array[mid].time;
		
		/* check if exactly equal to midpoint */
		if (IS_EQT(frame, midfra, BINARYSEARCH_FRAMEEQ_THRESH)) {
			*exists = 1;
			return mid;
		}
		
		/* repeat in upper/lower half */
		if (frame > midfra)
			start= mid + 1;
		else if (frame < midfra)
			end= mid - 1;
	}
	
	/* print error if loop-limit exceeded */
	if (loopbreaker == (maxloop-1)) {
		printf("Error: binarysearch_fcm_envelopedata_index() was taking too long \n");
		
		// include debug info 
		printf("\tround = %d: start = %d, end = %d, arraylen = %d \n", loopbreaker, start, end, arraylen);
	}
	
	/* not found, so return where to place it */
	return start;
}

/* callback to add new envelope data point */
// TODO: should we have a separate file for things like this?
static void fmod_envelope_addpoint_cb (bContext *C, void *fcm_dv, void *dummy)
{
	Scene *scene= CTX_data_scene(C);
	FMod_Envelope *env= (FMod_Envelope *)fcm_dv;
	FCM_EnvelopeData *fedn;
	FCM_EnvelopeData fed;
	
	/* init template data */
	fed.min= -1.0f;
	fed.max= 1.0f;
	fed.time= (float)scene->r.cfra; // XXX make this int for ease of use?
	fed.f1= fed.f2= 0;
	
	/* check that no data exists for the current frame... */
	if (env->data) {
		short exists = -1;
		int i= binarysearch_fcm_envelopedata_index(env->data, (float)(scene->r.cfra), env->totvert, &exists);
		
		/* binarysearch_...() will set exists by default to 0, so if it is non-zero, that means that the point exists already */
		if (exists)
			return;
			
		/* add new */
		fedn= MEM_callocN((env->totvert+1)*sizeof(FCM_EnvelopeData), "FCM_EnvelopeData");
		
		/* add the points that should occur before the point to be pasted */
		if (i > 0)
			memcpy(fedn, env->data, i*sizeof(FCM_EnvelopeData));
		
		/* add point to paste at index i */
		*(fedn + i)= fed;
		
		/* add the points that occur after the point to be pasted */
		if (i < env->totvert) 
			memcpy(fedn+i+1, env->data+i, (env->totvert-i)*sizeof(FCM_EnvelopeData));
		
		/* replace (+ free) old with new */
		MEM_freeN(env->data);
		env->data= fedn;
		
		env->totvert++;
	}
	else {
		env->data= MEM_callocN(sizeof(FCM_EnvelopeData), "FCM_EnvelopeData");
		*(env->data)= fed;
		
		env->totvert= 1;
	}
}

/* callback to remove envelope data point */
// TODO: should we have a separate file for things like this?
static void fmod_envelope_deletepoint_cb (bContext *C, void *fcm_dv, void *ind_v)
{
	FMod_Envelope *env= (FMod_Envelope *)fcm_dv;
	FCM_EnvelopeData *fedn;
	int index= GET_INT_FROM_POINTER(ind_v);
	
	/* check that no data exists for the current frame... */
	if (env->totvert > 1) {
		/* allocate a new smaller array */
		fedn= MEM_callocN(sizeof(FCM_EnvelopeData)*(env->totvert-1), "FCM_EnvelopeData");
		
		memcpy(fedn, &env->data, sizeof(FCM_EnvelopeData)*(index));
		memcpy(&fedn[index], &env->data[index+1], sizeof(FCM_EnvelopeData)*(env->totvert-index-1));
		
		/* free old array, and set the new */
		MEM_freeN(env->data);
		env->data= fedn;
		env->totvert--;
	}
	else {
		/* just free array, since the only vert was deleted */
		if (env->data) 
			MEM_freeN(env->data);
		env->totvert= 0;
	}
}

/* draw settings for envelope modifier */
static void draw_modifier__envelope(uiBlock *block, FModifier *fcm, int *yco, short *height, short width, short active, int rb_col)
{
	FMod_Envelope *env= (FMod_Envelope *)fcm->data;
	FCM_EnvelopeData *fed;
	uiBut *but;
	int cy= (*yco - 28);
	int i;
	
	/* set the height:
	 *	- basic settings + variable height from envelope controls
	 */
	(*height) = 115 + (35 * env->totvert);
	
	/* basic settings (backdrop + general settings + some padding) */
	DRAW_BACKDROP((*height));
	
	/* General Settings */
	uiDefBut(block, LABEL, 1, "Envelope:", 10, cy, 100, 20, NULL, 0.0, 0.0, 0, 0, "Settings for cycling before first keyframe");
	cy -= 20;
	
	uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "Reference Val:", 10, cy, 300, 20, &env->midval, -UI_FLT_MAX, UI_FLT_MAX, 10, 3, "");
		cy -= 20;
		
		uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "Min:", 10, cy, 150, 20, &env->min, -UI_FLT_MAX, env->max, 10, 3, "Minimum value (relative to Reference Value) that is used as the 'normal' minimum value");
		uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "Max:", 160, cy, 150, 20, &env->max, env->min, UI_FLT_MAX, 10, 3, "Maximum value (relative to Reference Value) that is used as the 'normal' maximum value");
		cy -= 35;
	uiBlockEndAlign(block);
	
	
	/* Points header */
	uiDefBut(block, LABEL, 1, "Control Points:", 10, cy, 150, 20, NULL, 0.0, 0.0, 0, 0, "");
	
	but= uiDefBut(block, BUT, B_FMODIFIER_REDRAW, "Add Point", 160,cy,150,19, NULL, 0, 0, 0, 0, "Adds a new control-point to the envelope on the current frame");
	uiButSetFunc(but, fmod_envelope_addpoint_cb, env, NULL);
	cy -= 35;
	
	/* Points List */
	for (i=0, fed=env->data; i < env->totvert; i++, fed++) {
		uiBlockBeginAlign(block);
			but=uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "Fra:", 2, cy, 90, 20, &fed->time, -UI_FLT_MAX, UI_FLT_MAX, 10, 1, "Frame that envelope point occurs");
			uiButSetFunc(but, validate_fmodifier_cb, fcm, NULL);
			
			uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "Min:", 92, cy, 100, 20, &fed->min, -UI_FLT_MAX, UI_FLT_MAX, 10, 2, "Minimum bound of envelope at this point");
			uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "Max:", 192, cy, 100, 20, &fed->max, -UI_FLT_MAX, UI_FLT_MAX, 10, 2, "Maximum bound of envelope at this point");
			
			but= uiDefIconBut(block, BUT, B_FMODIFIER_REDRAW, ICON_X, 292, cy, 18, 20, NULL, 0.0, 0.0, 0.0, 0.0, "Delete envelope control point");
			uiButSetFunc(but, fmod_envelope_deletepoint_cb, env, SET_INT_IN_POINTER(i));
		uiBlockBeginAlign(block);
		cy -= 25;
	}
}

/* --------------- */

/* draw settings for limits modifier */
static void draw_modifier__limits(uiBlock *block, FModifier *fcm, int *yco, short *height, short width, short active, int rb_col)
{
	FMod_Limits *data= (FMod_Limits *)fcm->data;
	const int togButWidth = 50;
	const int textButWidth = ((width/2)-togButWidth);
	
	/* set the height */
	(*height) = 60;
	
	/* basic settings (backdrop + some padding) */
	DRAW_BACKDROP((*height));
	
	/* Draw Pairs of LimitToggle+LimitValue */
	uiBlockBeginAlign(block); 
		uiDefButBitI(block, TOGBUT, FCM_LIMIT_XMIN, B_FMODIFIER_REDRAW, "xMin", 5, *yco-30, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum x value"); 
		uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "", 5+togButWidth, *yco-30, (textButWidth-5), 18, &data->rect.xmin, -UI_FLT_MAX, UI_FLT_MAX, 0.1,0.5,"Lowest x value to allow"); 
	uiBlockEndAlign(block); 
	
	uiBlockBeginAlign(block); 
		uiDefButBitI(block, TOGBUT, FCM_LIMIT_XMAX, B_FMODIFIER_REDRAW, "XMax", 5+(width-(textButWidth-5)-togButWidth), *yco-30, 50, 18, &data->flag, 0, 24, 0, 0, "Use maximum x value"); 
		uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "", 5+(width-textButWidth-5), *yco-30, (textButWidth-5), 18, &data->rect.xmax, -UI_FLT_MAX, UI_FLT_MAX, 0.1,0.5,"Highest x value to allow"); 
	uiBlockEndAlign(block); 
	
	uiBlockBeginAlign(block); 
		uiDefButBitI(block, TOGBUT, FCM_LIMIT_YMIN, B_FMODIFIER_REDRAW, "yMin", 5, *yco-52, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum y value"); 
		uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "", 5+togButWidth, *yco-52, (textButWidth-5), 18, &data->rect.ymin, -UI_FLT_MAX, UI_FLT_MAX, 0.1,0.5,"Lowest y value to allow"); 
	uiBlockEndAlign(block);
	
	uiBlockBeginAlign(block); 
		uiDefButBitI(block, TOGBUT, FCM_LIMIT_YMAX, B_FMODIFIER_REDRAW, "YMax", 5+(width-(textButWidth-5)-togButWidth), *yco-52, 50, 18, &data->flag, 0, 24, 0, 0, "Use maximum y value"); 
		uiDefButF(block, NUM, B_FMODIFIER_REDRAW, "", 5+(width-textButWidth-5), *yco-52, (textButWidth-5), 18, &data->rect.ymax, -UI_FLT_MAX, UI_FLT_MAX, 0.1,0.5,"Highest y value to allow"); 
	uiBlockEndAlign(block); 
}

/* --------------- */

// FIXME: remove dependency for F-Curve
void ANIM_uiTemplate_fmodifier_draw (uiBlock *block, ListBase *modifiers, FModifier *fcm, int *yco)
{
	FModifierTypeInfo *fmi= fmodifier_get_typeinfo(fcm);
	uiBut *but;
	short active= (fcm->flag & FMODIFIER_FLAG_ACTIVE);
	short width= 314;
	short height = 0; 
	int rb_col;
	
	/* draw header */
	{
		uiBlockSetEmboss(block, UI_EMBOSSN);
		
		/* rounded header */
		rb_col= (active)?-20:20;
		but= uiDefBut(block, ROUNDBOX, B_REDR, "", 0, *yco-2, width, 24, NULL, 5.0, 0.0, 15.0, (float)(rb_col-20), ""); 
		
		/* expand */
		uiDefIconButBitS(block, ICONTOG, FMODIFIER_FLAG_EXPANDED, B_REDR, ICON_TRIA_RIGHT,	5, *yco-1, 20, 20, &fcm->flag, 0.0, 0.0, 0, 0, "Modifier is expanded.");
		
		/* checkbox for 'active' status (for now) */
		but= uiDefIconButBitS(block, ICONTOG, FMODIFIER_FLAG_ACTIVE, B_REDR, ICON_RADIOBUT_OFF,	25, *yco-1, 20, 20, &fcm->flag, 0.0, 0.0, 0, 0, "Modifier is active one.");
		uiButSetFunc(but, activate_fmodifier_cb, modifiers, fcm);
		
		/* name */
		if (fmi)
			uiDefBut(block, LABEL, 1, fmi->name,	10+40, *yco, 150, 20, NULL, 0.0, 0.0, 0, 0, "F-Curve Modifier Type. Click to make modifier active one.");
		else
			uiDefBut(block, LABEL, 1, "<Unknown Modifier>",	10+40, *yco, 150, 20, NULL, 0.0, 0.0, 0, 0, "F-Curve Modifier Type. Click to make modifier active one.");
		
		/* 'mute' button */
		uiDefIconButBitS(block, ICONTOG, FMODIFIER_FLAG_MUTED, B_REDR, ICON_MUTE_IPO_OFF,	10+(width-60), *yco-1, 20, 20, &fcm->flag, 0.0, 0.0, 0, 0, "Modifier is temporarily muted (not evaluated).");
		
		/* delete button */
		but= uiDefIconBut(block, BUT, B_REDR, ICON_X, 10+(width-30), *yco, 19, 19, NULL, 0.0, 0.0, 0.0, 0.0, "Delete F-Curve Modifier.");
		uiButSetFunc(but, delete_fmodifier_cb, modifiers, fcm);
		
		uiBlockSetEmboss(block, UI_EMBOSS);
	}
	
	/* when modifier is expanded, draw settings */
	if (fcm->flag & FMODIFIER_FLAG_EXPANDED) {
		/* draw settings for individual modifiers */
		switch (fcm->type) {
			case FMODIFIER_TYPE_GENERATOR: /* Generator */
				draw_modifier__generator(block, fcm, yco, &height, width, active, rb_col);
				break;
				
			case FMODIFIER_TYPE_FN_GENERATOR: /* Built-In Function Generator */
				draw_modifier__fn_generator(block, fcm, yco, &height, width, active, rb_col);
				break;
				
			case FMODIFIER_TYPE_CYCLES: /* Cycles */
				draw_modifier__cycles(block, fcm, yco, &height, width, active, rb_col);
				break;
				
			case FMODIFIER_TYPE_ENVELOPE: /* Envelope */
				draw_modifier__envelope(block, fcm, yco, &height, width, active, rb_col);
				break;
				
			case FMODIFIER_TYPE_LIMITS: /* Limits */
				draw_modifier__limits(block, fcm, yco, &height, width, active, rb_col);
				break;
			
			case FMODIFIER_TYPE_NOISE: /* Noise */
				draw_modifier__noise(block, fcm, yco, &height, width, active, rb_col);
				break;
			
			default: /* unknown type */
				height= 96;
				//DRAW_BACKDROP(height); // XXX buggy...
				break;
		}
	}
	
	/* adjust height for new to start */
	(*yco) -= (height + 27); 
}

/* ********************************************** */

 