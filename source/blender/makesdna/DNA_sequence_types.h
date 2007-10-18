/**
 * blenlib/DNA_sequence_types.h (mar-2001 nzc)
 *
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
#ifndef DNA_SEQUENCE_TYPES_H
#define DNA_SEQUENCE_TYPES_H

#include "DNA_listBase.h"

/* needed for sound support */
#include "DNA_sound_types.h"

struct Ipo;
struct Scene;

/* strlens; 80= FILE_MAXFILE, 160= FILE_MAXDIR */

typedef struct StripElem {
	char name[80];
	struct ImBuf *ibuf;
	struct StripElem *se1, *se2, *se3;
	short ok;
	short pad;
	int nr;
} StripElem;

typedef struct Strip {
	struct Strip *next, *prev;
	int rt, len, us, done;
	StripElem *stripdata;
	char dir[160];
	int orx, ory;
} Strip;


typedef struct PluginSeq {
	char name[256];
	void *handle;

	char *pname;

	int vars, version;

	void *varstr;
	float *cfra;

	float data[32];

	void *instance_private_data;
	void **current_private_data;

	void (*doit)(void);

	void (*callback)(void);
} PluginSeq;

/* The sequence structure is the basic struct used by any strip. each of the strips uses a different sequence structure.*/
/* WATCH IT: first part identical to ID (for use in ipo's) */

typedef struct Sequence {

	struct Sequence *next, *prev;
	struct Sequence *newseq; /* should not be saved in DNA - only used for recursive duplicate and recently select more/less */
	char name[24]; /* name, not set by default and dosnt need to be unique as with ID's */

	short flag, type;	/*flags bitmap (see below) and the type of sequence*/
	int len; /* the length of the contense of this strip - before handles are applied */
	int start, startofs, endofs;
	int startstill, endstill;
	int machine, depth; /*machine - the strip channel, depth - the depth in the sequence when dealing with metastrips */
	int startdisp, enddisp;	/*starting and ending points in the sequence*/
	float mul, handsize;
					/* is sfra needed anymore? - it looks like its only used in one place */
	int sfra;		/* starting frame according to the timeline of the scene. */

	Strip *strip;
	StripElem *curelem;	/* reference the current frame - value from give_stripelem */

	struct Ipo *ipo;
	struct Scene *scene;
	struct anim *anim;
	float facf0, facf1;

	PluginSeq *plugin;

	/* pointers for effects: */
	struct Sequence *seq1, *seq2, *seq3;

	ListBase seqbase;	/* list of strips for metastrips */

	struct bSound *sound;	/* the linked "bSound" object */
        struct hdaudio *hdaudio; /* external hdaudio object */
	float level, pan;	/* level in dB (0=full), pan -1..1 */
	int curpos;		/* last sample position in audio_fill() */
	float strobe;

	void *effectdata;	/* Struct pointer for effect settings */

	int anim_preseek;
	int pad;
} Sequence;

typedef struct MetaStack {
	struct MetaStack *next, *prev;
	ListBase *oldbasep;
	Sequence *parseq;
} MetaStack;

typedef struct Editing {
	ListBase *seqbasep;
	ListBase seqbase;
	ListBase metastack;
	short flag;
	short pad;
	int rt;
} Editing;

/* ************* Effect Variable Structs ********* */
typedef struct WipeVars {
	float edgeWidth,angle;
	short forward, wipetype;
} WipeVars;

typedef struct GlowVars {	
	float fMini;	/*	Minimum intensity to trigger a glow */
	float fClamp;
	float fBoost;	/*	Amount to multiply glow intensity */
    float dDist;	/*	Radius of glow blurring */
	int	dQuality;
	int	bNoComp;	/*	SHOW/HIDE glow buffer */
} GlowVars;

typedef struct TransformVars {
	float ScalexIni;
	float ScaleyIni;
	float ScalexFin;
	float ScaleyFin;
	float xIni;
	float xFin;
	float yIni;
	float yFin;
	float rotIni;
	float rotFin;
	int percent;
	int interpolation;
} TransformVars;

typedef struct SolidColorVars {
	float col[3];
	float pad;
} SolidColorVars;

typedef struct SpeedControlVars {
	float * frameMap;
	float globalSpeed;
	int flags;
	int length;
	int pad;
} SpeedControlVars;

/* SpeedControlVars->flags */
#define SEQ_SPEED_INTEGRATE      1
#define SEQ_SPEED_BLEND          2
#define SEQ_SPEED_COMPRESS_IPO_Y 4

/* ***************** SEQUENCE ****************** */

/* seq->flag */
#define SEQ_LEFTSEL				2
#define SEQ_RIGHTSEL			4
#define SEQ_OVERLAP				8
#define SEQ_FILTERY				16
#define SEQ_MUTE				32
#define SEQ_MAKE_PREMUL			64
#define SEQ_REVERSE_FRAMES		128
#define SEQ_IPO_FRAME_LOCKED	256
#define SEQ_EFFECT_NOT_LOADED	512
#define SEQ_FLAG_DELETE			1024
#define SEQ_FLIPX				2048
#define SEQ_FLIPY				4096

/* seq->type WATCH IT: SEQ_EFFECT BIT is used to determine if this is an effect strip!!! */
#define SEQ_IMAGE		0
#define SEQ_META		1
#define SEQ_SCENE		2
#define SEQ_MOVIE		3
#define SEQ_RAM_SOUND		4
#define SEQ_HD_SOUND            5
#define SEQ_MOVIE_AND_HD_SOUND  6 /* helper for add_sequence */

#define SEQ_EFFECT		8
#define SEQ_CROSS		8
#define SEQ_ADD			9
#define SEQ_SUB			10
#define SEQ_ALPHAOVER	11
#define SEQ_ALPHAUNDER	12
#define SEQ_GAMCROSS	13
#define SEQ_MUL			14
#define SEQ_OVERDROP	15
#define SEQ_PLUGIN		24
#define SEQ_WIPE		25
#define SEQ_GLOW		26
#define SEQ_TRANSFORM		27
#define SEQ_COLOR               28
#define SEQ_SPEED               29


#endif

