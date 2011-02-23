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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BPY_UTIL_H
#define BPY_UTIL_H

#if PY_VERSION_HEX <  0x03010000
#error "Python versions below 3.1 are not supported anymore, you'll need to update your python."
#endif

#include "RNA_types.h" /* for EnumPropertyItem only */

struct EnumPropertyItem;
struct ReportList;

char *BPy_enum_as_string(struct EnumPropertyItem *item);

#define BLANK_PYTHON_TYPE {PyVarObject_HEAD_INIT(NULL, 0) NULL}

/* error reporting */
short BPy_reports_to_error(struct ReportList *reports, const short clear);
short BPy_errors_to_report(struct ReportList *reports);

/* TODO - find a better solution! */
struct bContext *BPy_GetContext(void);
void BPy_SetContext(struct bContext *C);

extern void bpy_context_set(struct bContext *C, PyGILState_STATE *gilstate);
extern void bpy_context_clear(struct bContext *C, PyGILState_STATE *gilstate);
#endif
