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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef ED_SEQUENCER_INTERN_H
#define ED_SEQUENCER_INTERN_H

#include "RNA_access.h"
#include "DNA_sequence_types.h"

/* internal exports only */

struct Sequence;
struct bContext;
struct rctf;
struct SpaceSeq;
struct ARegion;
struct Scene;

/* sequencer_header.c */
void sequencer_header_buttons(const struct bContext *C, struct ARegion *ar);

/* sequencer_draw.c */
void drawseqspace(const struct bContext *C, struct ARegion *ar);
void seq_reset_imageofs(struct SpaceSeq *sseq);

/* sequencer_edit.c */
struct View2D;
int check_single_seq(struct Sequence *seq);
int seq_tx_get_final_left(struct Sequence *seq, int metaclip);
int seq_tx_get_final_right(struct Sequence *seq, int metaclip);
void seq_rectf(struct Sequence *seq, struct rctf *rectf);
void boundbox_seq(struct Scene *scene, struct rctf *rect);
struct Sequence *get_last_seq(struct Scene *scene);
struct Sequence *find_nearest_seq(struct Scene *scene, struct View2D *v2d, int *hand, short mval[2]);
struct Sequence *find_neighboring_sequence(struct Scene *scene, struct Sequence *test, int lr, int sel);
void deselect_all_seq(struct Scene *scene);
void recurs_sel_seq(struct Sequence *seqm);
int event_to_efftype(int event);
void set_last_seq(struct Scene *scene, struct Sequence *seq);
int seq_effect_find_selected(struct Scene *scene, struct Sequence *activeseq, int type, struct Sequence **selseq1, struct Sequence **selseq2, struct Sequence **selseq3, char **error_str);
struct Sequence *alloc_sequence(struct ListBase *lb, int cfra, int machine);

/* externs */
extern EnumPropertyItem sequencer_prop_effect_types[];
extern EnumPropertyItem prop_side_types[];

/* operators */
struct wmOperatorType;
struct wmWindowManager;
void SEQUENCER_OT_cut(struct wmOperatorType *ot);
void SEQUENCER_OT_mute(struct wmOperatorType *ot);
void SEQUENCER_OT_unmute(struct wmOperatorType *ot);
void SEQUENCER_OT_lock(struct wmOperatorType *ot);
void SEQUENCER_OT_unlock(struct wmOperatorType *ot);
void SEQUENCER_OT_reload(struct wmOperatorType *ot);
void SEQUENCER_OT_refresh_all(struct wmOperatorType *ot);
void SEQUENCER_OT_add_duplicate(struct wmOperatorType *ot);
void SEQUENCER_OT_delete(struct wmOperatorType *ot);
void SEQUENCER_OT_separate_images(struct wmOperatorType *ot);
void SEQUENCER_OT_meta_toggle(struct wmOperatorType *ot);
void SEQUENCER_OT_meta_make(struct wmOperatorType *ot);
void SEQUENCER_OT_meta_separate(struct wmOperatorType *ot);

void SEQUENCER_OT_view_all(struct wmOperatorType *ot);
void SEQUENCER_OT_view_selected(struct wmOperatorType *ot);

/* sequencer_select.c */
void SEQUENCER_OT_deselect_all(struct wmOperatorType *ot);
void SEQUENCER_OT_select(struct wmOperatorType *ot);
void SEQUENCER_OT_select_more(struct wmOperatorType *ot);
void SEQUENCER_OT_select_less(struct wmOperatorType *ot);
void SEQUENCER_OT_select_linked(struct wmOperatorType *ot);
void SEQUENCER_OT_select_pick_linked(struct wmOperatorType *ot);
void SEQUENCER_OT_select_handles(struct wmOperatorType *ot);
void SEQUENCER_OT_select_active_side(struct wmOperatorType *ot);
void SEQUENCER_OT_borderselect(struct wmOperatorType *ot);
void SEQUENCER_OT_select_invert(struct wmOperatorType *ot);


/* sequencer_select.c */
void SEQUENCER_OT_add_scene_strip(struct wmOperatorType *ot);
void SEQUENCER_OT_add_movie_strip(struct wmOperatorType *ot);
void SEQUENCER_OT_add_sound_strip(struct wmOperatorType *ot);
void SEQUENCER_OT_add_image_strip(struct wmOperatorType *ot);
void SEQUENCER_OT_add_effect_strip(struct wmOperatorType *ot);

/* RNA enums, just to be more readable */
enum {
	SEQ_SIDE_NONE=0,
    SEQ_SIDE_LEFT,
    SEQ_SIDE_RIGHT,
	SEQ_SIDE_BOTH,
};
enum {
    SEQ_CUT_SOFT,
    SEQ_CUT_HARD,
};
enum {
    SEQ_SELECTED,
    SEQ_UNSELECTED,
};

/* defines used internally */
#define SEQ_ALLSEL	(SELECT+SEQ_LEFTSEL+SEQ_RIGHTSEL)
#define SEQ_DESEL	~SEQ_ALLSEL
#define SCE_MARKERS 0 // XXX - dummy

/* sequencer_ops.c */
void sequencer_operatortypes(void);
void sequencer_keymap(struct wmWindowManager *wm);

/* sequencer_scope.c */
struct ImBuf *make_waveform_view_from_ibuf(struct ImBuf * ibuf);
struct ImBuf *make_sep_waveform_view_from_ibuf(struct ImBuf * ibuf);
struct ImBuf *make_vectorscope_view_from_ibuf(struct ImBuf * ibuf);
struct ImBuf *make_zebra_view_from_ibuf(struct ImBuf * ibuf, float perc);
struct ImBuf *make_histogram_view_from_ibuf(struct ImBuf * ibuf);

#endif /* ED_SEQUENCER_INTERN_H */

