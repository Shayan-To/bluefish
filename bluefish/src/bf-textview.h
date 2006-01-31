/* Bluefish HTML Editor
 * bf-textview.h - Bluefish text widget
 *
 * Copyright (C) 2005 Oskar Świda swida@aragorn.pb.bialystok.pl
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


#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#ifndef __BF_TEXTVIEW_H__
#define __BF_TEXTVIEW_H__



#define BFTV_UTF8_RANGE	128

/**
*	BfLangBlock:
*	@id: block identifier
*	@begin: string which begins this block
*	@end: string which ends this block
*	@group: block group
*	@case_sensitive: are begin and end strings case sensitive
*	@scanned: should we scan text inside the block
*	@foldable: can we fold this block
*	@markup: should we scan markup tags inside block
*
* Represents language block definition read from configuration file. 
*/
typedef struct {
	guchar *id, *begin, *end;
	guchar *group;
	gboolean case_sensitive;
	gboolean scanned;
	gboolean foldable;
	gboolean markup;
	gboolean regexp;
	/*< private > */
	gint tabid;
	gshort spec_type;			/* 0 - normal block, 1- tag_begin */
	GtkTextTag *tag;
	gint min_state,max_state;
} BfLangBlock;

/**
*	BfLangToken:
*	@text: token text
*	@name: optional token name - this is for tokens defined as regular expressions
*	@group: token group
*	@context: pointer to #BfLangBlock structure representing token context (if token context is different than NULL, 
*		this token will be recognized only inside that block).
*	@regexp: is this token regular expression
*
* Represents language token definition read from configuration file. 
*/
typedef struct {
	guchar *text;
	guchar *name;
	guchar *group;
	BfLangBlock *context;
	gboolean regexp;
	/*< private > */
	gint tabid;
	gshort spec_type;			/* 0 - normal block, 1- tag_end, 2 - attr, 3 - attr2, 4 - tag_begin_end in tag context */
	GtkTextTag *tag;
	gint min_state,max_state;
} BfLangToken;

/**
*	BfLangConfig:
*
* Represents language configuration file. 
*/

typedef struct {
	/*< private > */
	guchar *name, *extensions, *description;
	gboolean case_sensitive;
	gboolean scan_tags;
	gboolean scan_blocks;
	gboolean restricted_tags_only;
	gboolean indent_blocks;
	gunichar as_triggers[BFTV_UTF8_RANGE];

	GHashTable *tokens;
	GHashTable *blocks;
	GHashTable *groups;
	GHashTable *blocks_id;
	GArray *dfa, *tag_dfa;
	GArray *line_indent;
	gshort **scan_table;
	gshort **tag_scan_table;
	gint tabnum, tag_tabnum;
	gint counter;
	gchar escapes[BFTV_UTF8_RANGE];
	GHashTable *restricted_tags;
	gint tokennum;				/* = BFTV_TOKEN_IDS + 1; */
	gint blocknum;				/* = BFTV_BLOCK_IDS + 1; */
	BfLangBlock *iblock;
	GtkTextTag *tag_begin;
	GtkTextTag *tag_end;
	GtkTextTag *attr_name;
	GtkTextTag *attr_val;
	BfLangBlock **token_allowed_states; /* this table decribes which states are allowed in what context */
} BfLangConfig;

typedef struct {
	GHashTable *languages;		/* table of recognized languages - structures BfLangConfig */
} BfLangManager;

BfLangManager *bf_lang_mgr_new();
BfLangConfig *bf_lang_mgr_load_config(BfLangManager * mgr, const gchar * filename);
BfLangConfig *bf_lang_mgr_get_config(BfLangManager * mgr, const gchar * filename);

typedef struct {
	/*< private > */
	BfLangToken *def;
	GtkTextIter itstart, itend;
} TBfToken;

typedef struct {
	/*< private > */
	BfLangBlock *def;
	gchar *tagname; /* optionally for tags */
	GtkTextIter b_start, b_end;
	GtkTextIter e_start, e_end;
	GtkTextMark *mark_begin, *mark_end;
	gboolean single_line;
	gint b_len, e_len;
} TBfBlock;

/* typedef struct {
	
	gchar *name;
	GtkTextIter b_start, b_end;
	GtkTextIter e_start, e_end;
} TBfTag;


typedef struct {
	
	gchar *name, *value;
	GtkTextIter name_start, name_end;
	GtkTextIter value_start, value_end;
} TBfTagAttr;*/



enum {
	BFTV_STATE_NORMAL,
	BFTV_STATE_TOKEN,
	BFTV_STATE_DONT_SCAN,
	BFTV_STATE_BLOCK
};

typedef struct {
	/*< private > */
	TBfBlock *current_block;
	BfLangBlock *current_context;
	gint state;
	gint mins,maxs;
	GQueue block_stack;
	GQueue block_stack2; /* separate stack for tag blocks */
	GQueue tag_stack;
} TBfScanner;



#define BF_TYPE_TEXTVIEW	(bf_textview_get_type())
#define BF_TEXTVIEW(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), bf_textview_get_type(), BfTextView)
#define BF_TEXTVIEW_CONST(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), bf_textview_get_type(), BfTextView const)
#define BF_TEXTVIEW_CLASS(klass)	G_TYPE_CHECK_CLASS_CAST((klass), bf_textview_get_type(), BfTextViewClass)
#define BF_IS_TEXTVIEW(obj)	G_TYPE_CHECK_INSTANCE_TYPE((obj), bf_textview_get_type ())
#define BF_TEXTVIEW_GET_CLASS(obj)	G_TYPE_INSTANCE_GET_CLASS((obj), bf_textview_get_type(), BfTextViewClass)


typedef struct {
	/*< private > */
	gchar *name;
	GdkPixbuf *pixmap;
} BfTextViewSymbol;

/*
 * Main object structure
 */

#define BFTV_HL_MODE_ALL					1
#define BFTV_HL_MODE_VISIBLE			2

typedef struct {
	GtkTextView __parent__;
	gchar bkg_color[8], fg_color[8];
	gboolean show_lines;
	gboolean show_blocks;
	gboolean show_symbols;
	gboolean highlight;			/* TRUE if highlighting should be performed */
	gboolean mark_tokens;		/* TRUE if tokens should be marked also */
	gboolean match_blocks;		/* TRUE if matching block begin/end should be shown */
	gboolean show_current_line;	/* TRUE if current line should be shown */
	gboolean tag_autoclose;           /* TRUE if tags should be automatically closed */
	gint hl_mode;				/* highlighting mode */
	/*< private > */
	gint lw_size_lines, lw_size_blocks, lw_size_sym;
	GHashTable *symbols;
	GHashTable *symbol_lines;
	BfLangConfig *lang;
	TBfScanner scanner;
	GtkTextTag *folded_tag, *block_tag, *fold_header_tag;
	GtkTextTag *block_match_tag;
	GHashTable *group_tags, *token_tags, *block_tags;
	GtkWidget *fold_menu;
/*    GHashTable *token_styles,*block_styles,*tag_styles,*group_styles;*/
	gboolean need_rescan; /* this is set to true if the buffer is changed, but the
						 widget is not visible to the user, the scanning is then postponed until 
						 the widget is visible to the user */
	gulong insert_signal_id; /* needed for blocking */						 
	GHashTable *fbal_cache; /* first block at line - cache */
	GHashTable *lbal_cache; /* last block at line - cache */
	gboolean delete_rescan; /* indicates if rescan is done from delete operation */
	GtkTextMark *last_matched_block; /* last matched block */
	gboolean paste_operation; /* indicates if we perform paste */
} BfTextView;

/*
 * Class definition
 */

typedef struct {
	GtkTextViewClass __parent__;
} BfTextViewClass;

 /* Public methods  */
GType bf_textview_get_type(void);

void bf_textview_set_language_ptr(BfTextView * self, BfLangConfig * cfg);

void bf_textview_scan(BfTextView * self);
void bf_textview_scan_area(BfTextView * self, GtkTextIter * start, GtkTextIter * end);
void bf_textview_scan_visible(BfTextView * self);

GtkWidget *bf_textview_new(void);
GtkWidget *bf_textview_new_with_buffer(GtkTextBuffer * buffer);

void bf_textview_show_lines(BfTextView * self, gboolean show);
void bf_textview_show_symbols(BfTextView * self, gboolean show);
gboolean bf_textview_add_symbol(BfTextView * self, gchar * name, GdkPixbuf * pix);
void bf_textview_remove_symbol(BfTextView * self, gchar * name);
void bf_textview_set_symbol(BfTextView * self, gchar * name, gint line, gboolean set);
void bf_textview_show_blocks(BfTextView * self, gboolean show);

void bf_textview_set_highlight(BfTextView * self, gboolean on);
void bf_textview_set_match_blocks(BfTextView * self, gboolean on);
void bf_textview_set_mark_tokens(BfTextView * self, gboolean on);


void bf_textview_fold_blocks(BfTextView * self, gboolean fold);
void bf_textview_fold_blocks_area(BfTextView * self, GtkTextIter * start, GtkTextIter * end,
								  gboolean fold);
void bf_textview_fold_blocks_lines(BfTextView * self, gint start, gint end, gboolean fold);

#define BF_GNB_CHAR					0
#define BF_GNB_LINE					1
#define BF_GNB_DOCUMENT	2
#define BF_GNB_HERE					3

GtkTextMark *bf_textview_get_nearest_block(BfTextView * self, GtkTextIter * iter, gboolean backward,
										   gint mode, gboolean not_single);
GtkTextMark *bf_textview_get_nearest_block_of_type(BfTextView * self, BfLangBlock * block,
												   GtkTextIter * iter, gboolean backward, gint mode,
												   gboolean not_single);


void bf_textview_set_bg_color(BfTextView * self, gchar * color);
void bf_textview_set_fg_color(BfTextView * self, gchar * color);
void bf_textview_recolor(BfTextView *view, gchar *fg_color, gchar *bg_color );

/* these functions return the names of groups, blocks and tokens that are defined 
for a certain language config. They are used in the preferences panel to build
the GUI to set textstyles */
GList *bf_lang_get_groups(BfLangConfig * cfg);
GList *bf_lang_get_blocks_for_group(BfLangConfig * cfg, guchar * group);
GList *bf_lang_get_tokens_for_group(BfLangConfig * cfg, guchar * group);
gboolean bf_lang_needs_tags(BfLangConfig * cfg);
void bf_lang_mgr_retag(void); 
#endif
