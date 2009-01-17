/* Testing code for new animation system in 2.5 
 * Copyright 2009, Joshua Leung
 */

#ifndef DNA_ANIM_TYPES_H
#define DNA_ANIM_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_ID.h"
#include "DNA_listBase.h"
#include "DNA_curve_types.h"

/* ************************************************ */
/* F-Curve DataTypes */

/* Modifiers -------------------------------------- */

/* F-Curve Modifiers (fcm) 
 *
 * These alter the way F-Curves behave, by altering the value that is returned
 * when evaluating the curve's data at some time (t). 
 */
typedef struct FModifier {
	struct FModifier *next, *prev;
	
	void *data;			/* pointer to modifier data */
	
	char name[64];		/* user-defined description for the modifier */
	short type;			/* type of f-curve modifier */
	short flag;			/* settings for the modifier */
	
	float influence;	/* the amount that the modifier should influence the value */
} FModifier;

/* Types of F-Curve modifier 
 * WARNING: order here is important!
 */
enum {
	FMODIFIER_TYPE_NULL = 0,
	FMODIFIER_TYPE_GENERATOR,
	FMODIFIER_TYPE_ENVELOPE,
	FMODIFIER_TYPE_CYCLES,
	FMODIFIER_TYPE_NOISE,		/* unimplemented - generate variations using some basic noise generator... */
	FMODIFIER_TYPE_FILTER,		/* unimplemented - for applying: fft, high/low pass filters, etc. */
	FMODIFIER_TYPE_PYTHON,		
	
	/* NOTE: all new modifiers must be added above this line */
	FMODIFIER_NUM_TYPES
} eFModifier_Types;

/* F-Curve Modifier Settings */
enum {
		/* modifier is not able to be evaluated for some reason, and should be skipped (internal) */
	FMODIFIER_FLAG_DISABLED		= (1<<0),
		/* modifier's data is expanded (in UI) */
	FMODIFIER_FLAG_EXPANDED		= (1<<1),
		/* modifier is active one (in UI) for editing purposes */
	FMODIFIER_FLAG_ACTIVE		= (1<<2),
} eFModifier_Flags; 

/* --- */

/* generator modifier data */
typedef struct FMod_Generator {
		/* generator based on PyExpression */
	char expression[256];		/* python expression to use as generator */
	
		/* simple polynomial generator (y = C[0]*(x^(n)) + C[1]*(x^(n-1)) + ... C[n])  */
	float *poly_coefficients;	/* array of the coefficients for the polynomial (poly_order + 1 items long) */
	unsigned int poly_order;	/* order of the polynomial (i.e. 1 for linear, 2 for quadratic) */
	
		/* settings */
	short flag;					/* settings */
	short mode;					/* which 'generator' to use */
} FMod_Generator;

/* generator modes */
enum {
	FCM_GENERATOR_POLYNOMIAL	= 0,
	FCM_GENERATOR_EXPRESSION,
} eFMod_Generator_Modes;


/* envelope modifier - envelope data */
typedef struct FCM_EnvelopeData {
	float min, max;				/* min/max values for envelope at this point (absolute values)  */
	float time;					/* time for that this sample-point occurs */
	
	short f1;					/* settings for 'min' control point */
	short f2;					/* settings for 'max' control point */
} FCM_EnvelopeData;

/* envelope-like adjustment to values (for fade in/out) */
typedef struct FMod_Envelope {
	FCM_EnvelopeData *data;		/* data-points defining envelope to apply (array)  */
	int totvert;				/* number of envelope points */
	
	float midval;				/* value that envelope's influence is centered around / based on */
	float min, max;				/* distances from 'middle-value' for 1:1 envelope influence */
} FMod_Envelope;


/* cycling/repetition modifier data */
// TODO: we can only do complete cycles...
typedef struct FMod_Cycles {
	short 	before_mode;		/* extrapolation mode to use before first keyframe */
	short 	after_mode;			/* extrapolation mode to use after last keyframe */
	short 	before_cycles;		/* number of 'cycles' before first keyframe to do */
	short 	after_cycles;		/* number of 'cycles' after last keyframe to do */
} FMod_Cycles;

/* cycling modes */
enum {
	FCM_EXTRAPOLATE_NONE = 0,			/* don't do anything */
	FCM_EXTRAPOLATE_CYCLIC,				/* repeat keyframe range as-is */
	FCM_EXTRAPOLATE_CYCLIC_OFFSET,		/* repeat keyframe range, but with offset based on gradient between values */
} eFMod_Cycling_Modes;


/* Python-script modifier data */
typedef struct FMod_Python {
	struct Text *script;		/* text buffer containing script to execute */
	IDProperty *prop;			/* ID-properties to provide 'custom' settings */
} FMod_Python;

/* Drivers -------------------------------------- */

/* Channel Driver (i.e. Drivers / Expressions) (driver)
 *
 * Channel Drivers are part of the dependency system, and are executed in addition to 
 * normal user-defined animation. They take the animation result of some channel(s), and
 * use that (optionally combined with its own F-Curve for modification of results) to define
 * the value of some setting semi-procedurally.
 *
 * Drivers are stored as part of F-Curve data, so that the F-Curve's RNA-path settings (for storing
 * what setting the driver will affect). The order in which they are stored defines the order that they're
 * evaluated in. This order is set by the Depsgraph's sorting stuff. 
 */
typedef struct ChannelDriver {
		/* primary target */
	ID 	*id;			/* ID-block which owns the target */
	char *rna_path;		/* target channel to use as driver value */
	int array_index;	/* if applicable, the index of the RNA-array item to use as driver */
	
		/* value cache (placed here for alignment reasons) */
	float curval;		/* result of previous evaluation, for subtraction from result under certain circumstances */
	
		/* secondary target (for rotational difference) */
	ID 	*id2;			/* ID-block which owns the second target */
	char *rna_path2;	/* second target channel to use as driver value */
	int array_index2;	/* if applicable, the index of the RNA-array item to use as driver */
		
		/* general settings (placed here for alignment reasons) */
	int type;			/* type of driver */
	int flag;			/* settings of driver */
	
	float influence;	/* influence of driver on result */ // XXX to be implemented... this is like the constraint influence setting
	
		/* settings for Python Drivers (PyDrivers) */
	char expression[256]; /* python expression to execute (may call functions defined in an accessory file) */
} ChannelDriver;

/* driver type */
enum {
		/* channel drives channel */
	DRIVER_TYPE_CHANNEL	= 0,
		/* py-expression used as driver */
	DRIVER_TYPE_PYTHON,
		/* rotational difference (must use rotation channels only) */
	DRIVER_TYPE_ROTDIFF,
} eDriver_Types;

/* driver flags */
enum {
		/* driver has invalid settings (internal flag)  */
	DRIVER_FLAG_INVALID		= (1<<0),
		/* driver was disabled temporarily, so shouldn't be evaluated (set by user) */
	DRIVER_FLAG_DISABLED 	= (1<<1),
		/* driver needs recalculation (set by depsgraph) */
	DRIVER_FLAG_RECALC		= (1<<2),
		/* driver does replace value, but overrides (for layering of animation over driver) */
		// TODO: is this necessary?
	DRIVER_FLAG_LAYERING	= (1<<3),
} eDriver_Flags;

/* F-Curves -------------------------------------- */

/* FPoint (fpt)
 *
 * This is the bare-minimum data required storing motion samples. Should be more efficient
 * than using BPoints, which contain a lot of other unnecessary data...
 */
typedef struct FPoint {
	float vec[2];		/* time + value */
	int flag;			/* selection info */
	int pad;
} FPoint;

/* 'Function-Curve' - defines values over time for a given setting (fcu) */
typedef struct FCurve {
	struct FCurve *next, *prev;
	
		/* driver settings */
	ChannelDriver *driver;	/* only valid for drivers (i.e. stored in AnimData not Actions) */
		/* evaluation settings */
	ListBase modifiers;		/* FCurve Modifiers */
		
		/* motion data */
	BezTriple *bezt;		/* user-editable keyframes (array) */
	FPoint *fpt;			/* 'baked/imported' motion samples (array) */
	int totvert;			/* total number of points which define the curve (i.e. size of arrays in FPoints) */
	
		/* value cache + settings */
	float curval;			/* value stored from last time curve was evaluated */
	short flag;				/* user-editable settings for this curve */
	short extend;			/* value-extending mode for this curve (does not cover  */
	
		/* RNA - data link */
	int array_index;		/* if applicable, the index of the RNA-array item to get */
	char *rna_path;			/* RNA-path to resolve data-access */
} FCurve;


/* user-editable flags/settings */
enum {
		/* curve/keyframes are visible in editor */
	FCURVE_VISIBLE		= (1<<0),
		/* curve is selected for editing  */
	FCURVE_SELECTED 	= (1<<1),
		/* curve is active one */
	FCURVE_ACTIVE		= (1<<2),
		/* keyframes (beztriples) cannot be edited */
	FCURVE_PROTECTED	= (1<<3),
		/* fcurve will not be evaluated for the next round */
	FCURVE_MUTED		= (1<<4),
		/* fcurve uses 'auto-handles', which stay horizontal... */
	FCURVE_AUTO_HANDLES	= (1<<5),
	
		/* skip evaluation, as RNA-path cannot be resolved (similar to muting, but cannot be set by user) */
	FCURVE_DISABLED			= (1<<10),
		/* curve can only have whole-number values (int or boolean types) */
	FCURVE_INT_VALUES		= (1<<11),
} eFCurve_Flags;

/* extrapolation modes (only simple value 'extending') */
enum {
	FCURVE_EXTRAPOLATE_CONSTANT	= 0,	/* just extend min/max keyframe value  */
	FCURVE_EXTRAPOLATE_LINEAR,			/* just extend gradient of segment between first segment keyframes */
} eFCurve_Extend;

/* interpolation modes (used only for BezTriple->ipo) */
// TODO: move this to curve-types file?
enum {
	BEZT_IPO_CONST = 0,	/* constant interpolation */
	BEZT_IPO_LIN,		/* linear interpolation */
	BEZT_IPO_BEZ,		/* bezier interpolation */
} eBezTriple_Interpolation;

/* ************************************************ */
/* 'Action' Datatypes */

/* Groups -------------------------------------- */

#if 0

/* Action-Channel Group (agrp)

 * These are stored as a list per-Action, and are only used to 
 * group that Action's channels in an Animation Editor. 
 *
 * Even though all FCurves live in a big list per Action, each group they are in also
 * holds references to the achans within that list which belong to it. Care must be taken to
 * ensure that action-groups never end up being the sole 'owner' of a channel.
 * 
 * This is also exploited for bone-groups. Bone-Groups are stored per bPose, and are used 
 * primarily to colour bones in the 3d-view. There are other benefits too, but those are mostly related
 * to Action-Groups.
 */
typedef struct bActionGroup {
	struct ActionGroup *next, *prev;
	
	ListBase channels;			/* Note: this must not be touched by standard listbase functions which would clear links to other channels */
	
	int flag;					/* settings for this action-group */
	int customCol;				/* index of custom color set to use when used for bones (0=default - used for all old files, -1=custom set) */				
	char name[64];				/* name of the group */
	
	ThemeWireColor cs;			/* color set to use when customCol == -1 */
} bActionGroup;

/* Action Group flags */
enum {
	AGRP_SELECTED 	= (1<<0),
	AGRP_ACTIVE 	= (1<<1),
	AGRP_PROTECTED 	= (1<<2),
	AGRP_EXPANDED 	= (1<<3),
	
	AGRP_TEMP		= (1<<30),
	AGRP_MOVED 		= (1<<31)
} eActionGroup_Flag;

#endif

/* Actions -------------------------------------- */

/* Action - reusable F-Curve 'bag'  (act) 
 *
 * This contains F-Curves that may affect settings from more than one ID blocktype and/or 
 * datablock (i.e. sub-data linked/used directly to the ID block that the animation data is linked to), 
 * but with the restriction that the other unrelated data (i.e. data that is not directly used or linked to
 * by the source ID block).
 *
 * It serves as a 'unit' of reusable animation information (i.e. keyframes/motion data), that 
 * affects a group of related settings (as defined by the user). 
 */
// XXX use this as drop-in replacement at some point...
typedef struct nAction {
	ID 	id;				/* ID-serialisation for relinking */
	
	ListBase curves;	/* function-curves (FCurve) */
	ListBase chanbase;	/* legacy data - Action Channels (bActionChannel) in pre-2.5 animation system */
	ListBase groups;	/* groups of function-curves (bActionGroup) */
	ListBase markers;	/* markers local to the Action (used to provide Pose-Libraries) */
	
	int flag;			/* settings for this action */
	int active_marker;	/* index of the active marker */
} nAction;


/* Flags for the action */
enum {
		/* flags for displaying in UI */
	ACT_EXPANDED 	= (1<<0),
	ACT_SELECTED	= (1<<1),
	
		/* flags for evaluation/editing */
	ACT_MUTED		= (1<<9),
	ACT_PROTECTED	= (1<<10),
} eAction_Flags;

/* ************************************************ */
/* Animation Reuse - i.e. users of Actions */

/* Retargetting ----------------------------------- */

/* Retargetting Pair
 *
 * Defines what parts of the paths should be remapped from 'abc' to 'xyz'.
 * TODO:
 *	- Regrex (possibly provided through PY, though having our own module might be faster)
 *	  would be important to have at some point. Current replacements are just simple
 *	  string matches...
 */
typedef struct AnimMapPair {
	char from[128];		/* part of path to bed replaced */
	char to[128];		/* part of path to replace with */
} AnimMapPair;

/* Retargetting Information for Actions 
 *
 * This should only be used if it is strictly necessary (i.e. user will need to explictly 
 * add this when they find that some channels do not match, or motion is not going to right 
 * places). When executing an action, this will be checked to see if it provides any useful
 * remaps for the given paths.
 *
 * NOTE: we currently don't store this in the Action itself, as that causes too many problems.
 */
// FIXME: will this be too clumsy or slow? If we're using RNA paths anyway, we'll have to accept
// such consequences...
typedef struct AnimMapper {
	struct AnimMapper *next, *prev;
	
	nAction *target;		/* target action */
	ListBase mappings;		/* remapping table (bAnimMapPair) */
} AnimMapper;

/* ************************************************ */
/* NLA - Non-Linear Animation */
// TODO: the concepts here still need to be refined to solve any unresolved items

/* NLA Modifiers ---------------------------------- */

/* These differ from F-Curve modifiers, as although F-Curve modifiers also operate on a 
 * per-channel basis too (in general), they are part of the animation data itself, which
 * means that their effects are inherited by all of their users. In order to counteract this,
 * the modifiers here should be used to provide variation to pre-created motions only. 
 */

/* NLA Strips ------------------------------------- */

/* NLA Strip (strip)
 *
 * A NLA Strip is a container for the reuse of Action data, defining parameters
 * to control the remapping of the Action data to some destination. Actions being
 * referenced by NLA-Strips SHOULD-NOT be editable, unless they were created in such
 * a way that results in very little mapping distortion (i.e. for layered animation only,
 * opposed to prebuilt 'blocks' which are quickly dumped into the NLA for crappymatic machima-type
 * stuff)
 */
typedef struct NlaStrip {
	struct NlaStrip *next, *prev;
	
	nAction *act;				/* Action that is referenced by this strip */
	AnimMapper *remap;			/* Remapping info this strip (for tweaking correspondance of action with context) */
	
	ListBase modifiers;			/* NLA Modifiers */	
	
	ListBase fcurves;			/* F-Curves for controlling this strip's influence and timing */
	float influence;			/* Influence of strip */
	float act_time;				/* Current 'time' within action being used */
	
	float start, end;			/* extents of the strip */
	float actstart, actend;		/* range of the action to use */
	
	float	repeat;				/* The number of times to repeat the action range (only when no F-Curves) */
	float	scale;				/* The amount the action range is scaled by (only when no F-Curves) */
	
	float blendin, blendout;	/* strip blending length (only used when there are no F-Curves) */	
	int blendmode;				/* strip blending mode */	
	
	int flag;					/* settings */
	
		// umm... old unused cruft? 
	int stride_axis;			/* axis for stridebone stuff - 0=x, 1=y, 2=z */
	int pad;
	
	float	actoffs;			/* Offset within action, for cycles and striding (only set for ACT_USESTRIDE) */
	float	stridelen;			/* The stridelength (considered when flag & ACT_USESTRIDE) */
	
	char	stridechannel[32];	/* Instead of stridelen, it uses an action channel */
	char	offs_bone[32];		/* if repeat, use this bone/channel for defining offset */
} NlaStrip;

/* NLA Strip Blending Mode */
enum {
	NLASTRIPMODE_BLEND = 0,
	NLASTRIPMODE_ADD,
	NLASTRIPMODE_SUBTRACT,
} eActStrip_Mode;

/* NLA Strip Settings */
// TODO: check on which of these are still useful...
enum {
	NLASTRIP_SELECT			= (1<<0),
	NLASTRIP_USESTRIDE		= (1<<1),
	NLASTRIP_BLENDTONEXT	= (1<<2),	/* Not implemented. Is not used anywhere */
	NLASTRIP_HOLDLASTFRAME	= (1<<3),
	NLASTRIP_ACTIVE			= (1<<4),
	NLASTRIP_LOCK_ACTION	= (1<<5),
	NLASTRIP_MUTE			= (1<<6),
	NLASTRIP_REVERSE		= (1<<7),	/* This has yet to be implemented. To indicate that a strip should be played backwards */
	NLASTRIP_CYCLIC_USEX	= (1<<8),
	NLASTRIP_CYCLIC_USEY	= (1<<9),
	NLASTRIP_CYCLIC_USEZ	= (1<<10),
	NLASTRIP_AUTO_BLENDS	= (1<<11),
	NLASTRIP_TWEAK			= (1<<12),	/* This strip is a tweaking strip (only set if owner track is a tweak track) */
} eActionStrip_Flag;

/* NLA Tracks ------------------------------------- */

/* NLA Track (nlt)
 *
 * A track groups a bunch of 'strips', which should form a continous set of 
 * motion, on top of which other such groups can be layered. This should allow
 * for animators to work in a non-destructive manner, layering tweaks, etc. over
 * 'rough' blocks of their work.
 */
typedef struct NlaTrack {
	struct NlaTrack *next, *prev;
	
	ListBase strips;		/* bActionStrips in this track */
	
	int flag;				/* settings for this track */
	int index;				/* index of the track in the stack (NOTE: not really useful, but we need a pad var anyways!) */
	
	char info[64];			/* short user-description of this track */
} NlaTrack;

/* settings for track */
enum {
		/* track is the one that settings can be modified on (doesn't indicate 
		 * that it's for 'tweaking' though) 
		 */
	NLATRACK_ACTIVE		= (1<<0),
		/* track is selected in UI for relevant editing operations */
	NLATRACK_SELECTED	= (1<<1),
		/* track is not evaluated */
	NLATRACK_MUTED		= (1<<2),
		/* track is the only one evaluated (must be used in conjunction with adt->flag) */
	NLATRACK_SOLO		= (1<<3),
		/* track's settings (and strips) cannot be edited (to guard against unwanted changes) */
	NLATRACK_PROTECTED	= (1<<4),
		/* strip is the 'last' one that should be evaluated, as the active action 
		 * is being used to tweak the animation of the strips up to here 
		 */
	NLATRACK_TWEAK		= (1<<5),
} eNlaTrack_Flag;

/* ************************************************ */
/* Animation Data */

/* AnimOverride ------------------------------------- */

/* Animation Override (aor) 
 *
 * This is used to as temporary storage of values which have been changed by the user, but not
 * yet keyframed (thus, would get overwritten by the animation system before the user had a chance
 * to see the changes that were made). 
 *
 * It is probably not needed for overriding keyframed values in most cases, as those will only get evaluated
 * on frame-change now. That situation may change in future.
 */
typedef struct AnimOverride {
	struct AnimOverride *next, *prev;
	
	char *rna_path;			/* RNA-path to use to resolve data-access */
	int array_index;		/* if applicable, the index of the RNA-array item to get */
	
	float value;			/* value to override setting with */
} AnimOverride;

/* AnimData ------------------------------------- */

/* Animation data for some ID block (adt)
 * 
 * This block of data is used to provide all of the necessary animation data for a datablock.
 * Currently, this data will not be reusable, as there shouldn't be any need to do so.
 * 
 * This information should be made available for most if not all ID-blocks, which should 
 * enable all of its settings to be animatable locally. Animation from 'higher-up' ID-AnimData
 * blocks may override local settings.
 *
 * This datablock should be placed immediately after the ID block where it is used, so that
 * the code which retrieves this data can do so in an easier manner. See blenkernel/internal/anim_sys.c for details.
 */
typedef struct AnimData {	
		/* active action - acts as the 'tweaking track' for the NLA */
	nAction 	*action;		
		/* remapping-info for active action - should only be used if needed 
		 * (for 'foreign' actions that aren't working correctly) 
		 */
	AnimMapper	*remap;			
	
		/* nla-tracks */
	ListBase 	nla_tracks;
	
	/* 'drivers' for this ID-block's settings - FCurves, but are completely 
	 * separate from those for animation data 
	 */
	ListBase	drivers;	/* standard user-created Drivers/Expressions (used as part of a rig) */
	ListBase 	overrides;	/* temp storage (AnimOverride) of values for settings that are animated (but the value hasn't been keyframed) */
	
		/* settings for animation evaluation */
	int flag;			/* user-defined settings */
	int recalc;			/* depsgraph recalculation flags */		
} AnimData;

/* Animation Data settings (mostly for NLA) */
enum {
		/* only evaluate a single track in the NLA */
	ADT_NLA_SOLO_TRACK		= (1<<0),
		/* don't use NLA */
	ADT_NLA_EVAL_OFF		= (1<<1),
		/* don't execute drivers */
	ADT_DRIVERS_DISABLED	= (1<<2),
} eAnimData_Flag;

/* Animation Data recalculation settings (to be set by depsgraph) */
enum {
	ADT_RECALC_DRIVERS		= (1<<0),
	ADT_RECALC_ANIM			= (1<<1),
	ADT_RECALC_ALL			= (ADT_RECALC_DRIVERS|ADT_RECALC_ANIM),
} eAnimData_Recalc;

/* Base Struct for Anim ------------------------------------- */

/* Used for BKE_animdata_from_id() 
 * All ID-datablocks which have their own 'local' AnimData
 * should have the same arrangement in their structs.
 */
typedef struct IdAdtTemplate {
	ID id;
	AnimData *adt;
} IdAdtTemplate;

/* ************************************************ */

#ifdef __cplusplus
};
#endif

#endif /* DNA_ANIM_TYPES_H */
