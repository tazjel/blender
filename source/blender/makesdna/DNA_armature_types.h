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
 * Contributor(s): Full recode, Ton Roosendaal, Crete 2005
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef DNA_ARMATURE_TYPES_H
#define DNA_ARMATURE_TYPES_H

#include "DNA_listBase.h"
#include "DNA_ID.h"

/* this system works on different transformation space levels;

1) Bone Space;		with each Bone having own (0,0,0) origin
2) Armature Space;  the rest position, in Object space, Bones Spaces are applied hierarchical
3) Pose Space;		the animation position, in Object space
4) World Space;		Object matrix applied to Pose or Armature space

*/

typedef struct Bone {
	struct Bone		*next, *prev;	/*	Next/prev elements within this list	*/
	struct Bone		*parent;		/*	Parent (ik parent if appropriate flag is set		*/
	ListBase		childbase;		/*	Children	*/
	char			name[32];		/*  Name of the bone - must be unique within the armature */

	float			roll;   /*  roll is input for editmode, length calculated */
	float			head[3];		
	float			tail[3];		/*	head/tail and roll in Bone Space	*/
	float			bone_mat[3][3]; /*  rotation derived from head/tail/roll */
	
	int				flag;
	
	float			arm_head[3];		
	float			arm_tail[3];	/*	head/tail and roll in Armature Space (rest pos) */
	float			arm_mat[4][4];  /*  matrix: (bonemat(b)+head(b))*arm_mat(b-1), rest pos*/
	
	float			dist, weight;			/*  dist, weight: for non-deformgroup deforms */
	float			xwidth, length, zwidth;	/*  width: for block bones. keep in this order, transform! */
	float			ease1, ease2;			/*  length of bezier handles */
	float			rad_head, rad_tail;	/* radius for head/tail sphere, defining deform as well */
	
	float			size[3];		/*  patch for upward compat, UNUSED! */
	short			boneclass;
	short			segments;		/*  for B-bones */
}Bone;

typedef struct bArmature {
	ID			id;
	ListBase	bonebase;
	ListBase	chainbase;
	int			flag;
	int			drawtype;			
	int			deformflag;
	int			res3;			
}bArmature;

/* armature->flag */
/* dont use bit 7, was saved in files to disable stuff */

/* armature->flag */
#define		ARM_RESTPOS		0x0001
			/* XRAY is here only for backwards converting */
#define		ARM_DRAWXRAY	0x0002
#define		ARM_DRAWAXES	0x0004
#define		ARM_DRAWNAMES	0x0008
#define		ARM_POSEMODE	0x0010
#define		ARM_EDITMODE	0x0020
#define		ARM_DELAYDEFORM 0x0040
#define		ARM_DONT_USE    0x0080
#define		ARM_MIRROR_EDIT	0x0100

/* armature->drawtype */
#define		ARM_OCTA		0
#define		ARM_LINE		1
#define		ARM_B_BONE		2
#define		ARM_ENVELOPE	3

/* armature->deformflag */
#define		ARM_DEF_VGROUP		1
#define		ARM_DEF_ENVELOPE	2


/* bone->flag */
#define		BONE_SELECTED	1
#define		BONE_ROOTSEL	2
#define		BONE_TIPSEL		4
			/* Used instead of BONE_SELECTED during transform */
#define		BONE_TRANSFORM  8
#define		BONE_IK_TOPARENT 16
			/* 32 used to be quatrot, was always set in files, do not reuse unless you clear it always */
			/* hidden Bones when drawing Posechannels */
#define		BONE_HIDDEN_P		64
			/* For detecting cyclic dependancies */
#define		BONE_DONE		128
			/* active is on mouse clicks only */
#define		BONE_ACTIVE		256
			/* No parent rotation or scale */
#define		BONE_HINGE		512
			/* hidden Bones when drawing Armature Editmode */
#define		BONE_HIDDEN_A		1024
			/* multiplies vgroup with envelope */
#define		BONE_MULT_VG_ENV	2048

/* bone->flag  bits */
#define		BONE_IK_TOPARENTBIT		4
#define		BONE_HIDDENBIT			6


enum {
		BONE_SKINNABLE  =       0,
		BONE_UNSKINNABLE
};

#endif
