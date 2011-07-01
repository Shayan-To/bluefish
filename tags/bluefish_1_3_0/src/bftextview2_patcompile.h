/* Bluefish HTML Editor
 * bftextview2_patcompile.h
 *
 * Copyright (C) 2008 Olivier Sessink
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/* for the design docs see bftextview2.h */
#ifndef _BFTEXTVIEW2_PATCOMPILE_H_
#define _BFTEXTVIEW2_PATCOMPILE_H_

#include "bftextview2.h"
void bftextview2_scantable_rematch_highlights(Tscantable *st, const gchar *lang);
gint16 new_context(Tscantable *st, const gchar *lang, gchar *symbols, const gchar *contexthighlight, gboolean autocomplete_case_insens);
void match_set_nextcontext(Tscantable *st, guint16 matchnum, guint16 nextcontext);
void match_autocomplete_reference(Tscantable *st,guint16 matchnum, gboolean autocomplete,gchar *keyword,guint16 context,gchar *append_to_ac,gchar *reference);
void compile_existing_match(Tscantable *st,guint16 matchnum, gint16 context);
guint16 add_keyword_to_scanning_table(Tscantable *st, gchar *pattern, const gchar *lang, const gchar *selfhighlight, const gchar *blockhighlight
				, gboolean is_regex,gboolean case_insens, gint16 context, gint16 nextcontext
				, gboolean starts_block, gboolean ends_block, guint blockstartpattern
				, gboolean add_to_ac, gchar *append_to_ac, gchar *reference);
void print_DFA(Tscantable *st, char start, char end);
void print_scantable_stats(Tscantable *st);
Tscantable *scantable_new(guint size_table, guint size_matches, guint size_contexts);
Tscantable *bftextview2_scantable_new(GtkTextBuffer *buffer);

#endif