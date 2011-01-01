/* Bluefish HTML Editor
 * filefilter.c - 
 *
 * Copyright (C) 2002,2003,2004,2005,2006,2007 Olivier Sessink
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

/* #define DEBUG */


#include <gtk/gtk.h>
#include <string.h>

#include "bluefish.h"
#include "filebrowser2.h"
#include "bf_lib.h"
#include "dialog_utils.h"
#include "document.h"
#include "file.h"
#include "filetype.h"
#include "file_dialogs.h"
#include "gtk_easy.h"        /* destroy_disposable_menu_cb() */
#include "stringlist.h"      /* count_array() */

typedef struct {
	gchar *pattern;
	GPatternSpec* patspec;
} Tpattern;

static Tpattern *new_pattern(gchar *name) {
	Tpattern *pat = g_new(Tpattern,1);			
	pat->pattern = g_strdup(name);
	pat->patspec = g_pattern_spec_new(name);
	return pat;
}


static GList *remove_pattern_from_list(GList *list, const gchar *pattern) {
	GList *tmplist = g_list_first(list);
	while (tmplist) {
		Tpattern *pat = (Tpattern *)tmplist->data;
		if (strcmp(pat->pattern, pattern)==0) {
			g_free(pat->pattern);
			g_pattern_spec_free(pat->patspec);
			g_free(pat);
			list = g_list_delete_link(list, tmplist);
			return list;
		}
		
		tmplist = g_list_next(tmplist);
	}
	return list;
}

static gboolean filename_match(Tfilter *filter, const gchar *string) {
	GList *tmplist;
	guint len = strlen(string);	
	const gchar *reversed = g_utf8_strreverse(string,-1);
	for (tmplist = g_list_first(filter->patterns) ; tmplist ; tmplist = g_list_next(tmplist)) {
		if (g_pattern_match(((Tpattern *)tmplist->data)->patspec,len,string,reversed))
			return TRUE;
	}
	return FALSE;
}

gboolean file_visible_in_filter(Tfilter *filter, const gchar *mime_type, const gchar *filename) {

	if (filter)	
		return (((mime_type && GPOINTER_TO_INT(g_hash_table_lookup(filter->filetypes,mime_type))) 
				|| (filter->patterns && filename && filename_match(filter, filename))) ? filter->mode : !filter->mode);
	return TRUE;
}


Tfilter *find_filter_by_name(const gchar *name) {
	GList *tmplist = g_list_first(main_v->filefilters);
	while(tmplist) {
		Tfilter *filter = (Tfilter *)tmplist->data;
		if (strcmp(filter->name, name)==0) {
			return filter;
		}
		tmplist = g_list_next(tmplist);
	}
	return NULL;
}

static GHashTable *hashtable_from_string(const gchar *mimetypes) {
	GHashTable *filetypes = g_hash_table_new_full(g_str_hash,g_str_equal,g_free,NULL);
	if (mimetypes){
		gchar **types = g_strsplit(mimetypes, ":", 127);
		gchar **type = types;
		while (*type) {
			g_hash_table_replace(filetypes, g_strdup(*type), GINT_TO_POINTER(1));
			type++;
		}
		g_strfreev(types);
	}
	return filetypes;
}

static GList *patternlist_from_string(const gchar *patterns) {
	if (patterns){
		GList *list=NULL;
		gchar **pats = g_strsplit(patterns, ":", 127);
		gchar **pattern = pats;
		while (*pattern) {
			/* do something */
			if (strlen(*pattern)>0) {
				list = g_list_append(list, new_pattern(*pattern));
			} 
			pattern++;
		}
		g_strfreev(pats);
		return list;
	}
	return NULL;
}

static Tfilter *new_filter(const gchar *name, const gchar *mode, const gchar *mimetypes, const gchar *patterns) {
	Tfilter *filter = g_new(Tfilter,1);
	filter->name = g_strdup(name);
	filter->refcount = 1;
	filter->mode = atoi(mode);
	filter->filetypes = hashtable_from_string(mimetypes);
	filter->patterns = patternlist_from_string(patterns);
	return filter;
}

static void filter_destroy(Tfilter *filter) {
	GList *tmplist;
	g_free(filter->name);
	g_hash_table_destroy(filter->filetypes);
	for (tmplist=g_list_first(filter->patterns);tmplist;tmplist=g_list_next(tmplist)) {
		Tpattern *pat = tmplist->data;
		g_pattern_spec_free(pat->patspec);
		g_free(pat->pattern);
	}
	g_list_free(filter->patterns);
	g_free(filter);
}

void filter_unref(Tfilter *filter) {
	filter->refcount++;
	if (filter->refcount <= 0) {
		filter_destroy(filter);
	}
}

void filter_ref(Tfilter *filter) {
	filter->refcount++;
}

void filter_delete(Tfilter *filter) {
	GList *tmplist;
	gchar **strarr;
	/* delete from config */
	tmplist = g_list_first(main_v->globses.filefilters);
	while (tmplist) {
		strarr = (gchar **) tmplist->data;
		if (strarr && strcmp(strarr[0],filter->name)==0) {
			/* config string found */
			main_v->globses.filefilters = g_list_remove(main_v->globses.filefilters, strarr);
			break;
		}
		tmplist = g_list_next(tmplist);
	}
	/* delete from current list of filters, but we need to 
	make sure no window is actually using this filter! */
	tmplist = g_list_first(main_v->bfwinlist);
	while (tmplist) {
		Tbfwin *bfwin = BFWIN(tmplist->data);
		/* test if the filter is named in the current session */
		if (bfwin->session->last_filefilter && strcmp(bfwin->session->last_filefilter,filter->name)==0) {
			g_free(bfwin->session->last_filefilter);
			bfwin->session->last_filefilter = NULL;
		}
		if (bfwin->fb2) {
			fb2_unset_filter(bfwin, filter);
		}
		tmplist = g_list_next(tmplist);
	}
	/* now really remove the filter */
	main_v->filefilters = g_list_remove(main_v->filefilters, filter);
	filter_unref(filter);
}

/* 
 * WARNING: these filter are also used in the filechooser dialog (file->open in the menu)
 */
void filters_rebuild(void) {
	GList *tmplist;
	/* free any existing filters */
	tmplist = g_list_first(main_v->filefilters);
	while (tmplist) {
		filter_unref(tmplist->data);
		tmplist = g_list_next(tmplist);
	}
	g_list_free(main_v->filefilters);
	main_v->filefilters = NULL;

	/* build a list of filters */
	main_v->filefilters = g_list_prepend(NULL, new_filter(_("All files"), "0", NULL, NULL));
	tmplist = g_list_first(main_v->globses.filefilters);
	while (tmplist) {
		gchar **strarr = (gchar **) tmplist->data;
		Tfilter *filter = new_filter(strarr[0], strarr[1], strarr[2], strarr[3]);
		main_v->filefilters = g_list_prepend(main_v->filefilters, filter);
		tmplist = g_list_next(tmplist);
	}
}

static void restore_filter_from_config(Tfilter *filter, const gchar *origname) {
	GList *tmplist;
	gchar **strarr=NULL;
	gint mode;
	
	if (!origname) return;
	
	tmplist = g_list_first(main_v->globses.filefilters);
	while (tmplist) {
		strarr = (gchar **) tmplist->data;
		if (strarr && strcmp(strarr[0],origname)==0) {
			/* config string found */
			break;
		}
		tmplist = g_list_next(tmplist);
	}
	if (!strarr) return;
	
	if (strcmp(filter->name,origname)!=0) {
		g_free(filter->name);
		filter->name = g_strdup(origname);
	}
	mode = atoi(strarr[1]);
	if (mode != filter->mode) {
		filter->mode = mode;
	}
	
	g_hash_table_destroy(filter->filetypes);
	filter->filetypes = hashtable_from_string(strarr[2]);
	filter->patterns =  patternlist_from_string(strarr[3]);
}

static void hashtable_to_string_lcb(gpointer key, gpointer value, gpointer data) {
	g_string_append((GString *)data, (gchar *)key);
	g_string_append_c((GString *)data,':');
}

static void apply_filter_to_config(Tfilter *filter, const gchar *origname) {
	GList *tmplist;
	gchar **strarr = NULL;
	GString *gstr;
	if (origname) {
		/* find the config string, if it existed before */
		tmplist = g_list_first(main_v->globses.filefilters);
		while (tmplist) {
			strarr = (gchar **) tmplist->data;
			if (strarr && strcmp(strarr[0],origname)==0) {
				/* config string found */
				break;
			}
			tmplist = g_list_next(tmplist);
		}
	}
	if (strarr == NULL) {
		DEBUG_MSG("apply_filter_to_config, prepending new entry in config list\n");
		/* no config string with this name, */
		strarr = (gchar **)g_new0(gpointer, 5);
		main_v->globses.filefilters = g_list_prepend(main_v->globses.filefilters, strarr);
	}
	if (origname == NULL) {
		main_v->filefilters = g_list_prepend(main_v->filefilters, filter);
	}
	/* the config string has three entries: the name, inverse filtering, filetypes */
	if (strarr[0]) g_free(strarr[0]);
	strarr[0] = g_strdup(filter->name);
	if (strarr[1]) g_free(strarr[1]);
	strarr[1] = g_strdup_printf("%d",filter->mode);
	gstr = g_string_new("");
	g_hash_table_foreach(filter->filetypes,hashtable_to_string_lcb,gstr);
	if (strarr[2]) g_free(strarr[2]);
	strarr[2] = g_string_free(gstr,FALSE);
	if (strarr[3]) g_free(strarr[3]);
	gstr = g_string_new("");
	{
		GList *tmplist;
		for (tmplist = g_list_first(filter->patterns) ; tmplist ; tmplist = g_list_next(tmplist)) {
			Tpattern *pat = tmplist->data;
			g_string_append(gstr, pat->pattern);
			g_string_append_c(gstr,':');
		}
		strarr[3] = g_string_free(gstr,FALSE);
	}
}

/*
the filefilter gui has one listmodel with all filetypes currently known in bluefish, 
and two filtermodels, in_model shows all types in the filter, out_model shows all other filetypes
and two treeviews.

*/

typedef struct {
	GtkWidget *win;
	GtkListStore *lstore;
	GtkTreeModel *in_model; /* shows all the types IN the filter */
	GtkTreeModel *out_model; /* shows all types OUTside the filter */
	GtkWidget *in_view;
	GtkWidget *out_view;
	GtkWidget *nameentry;
	GtkWidget *inversecheck;
	GtkWidget *patentry;
	
	Tfilter *curfilter;
	gchar *origname;
} Tfilefiltergui;

static void filefiltergui_destroy_lcb(GtkWidget *widget, Tfilefiltergui *ffg) {
	g_free(ffg->origname);
	g_object_unref(ffg->lstore);
	window_destroy(ffg->win);
	g_free(ffg);
	DEBUG_MSG("filefiltergui_destroy_lcb, done\n");	
}

static void filefiltergui_cancel_clicked(GtkWidget *widget, Tfilefiltergui *ffg) {
	restore_filter_from_config(ffg->curfilter, ffg->origname);
	filefiltergui_destroy_lcb(widget, ffg);
}

static void filefiltergui_ok_clicked(GtkWidget *widget, Tfilefiltergui *ffg) {
	g_free(ffg->curfilter->name);
	ffg->curfilter->name = gtk_editable_get_chars(GTK_EDITABLE(ffg->nameentry),0,-1);
	ffg->curfilter->mode = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ffg->inversecheck));
	DEBUG_MSG("filefiltergui_ok_clicked, filter '%s' has mode %d\n",ffg->curfilter->name,ffg->curfilter->mode);
	apply_filter_to_config(ffg->curfilter, ffg->origname);
	filefiltergui_destroy_lcb(widget, ffg);
}

static gboolean filefiltergui_infilter_visiblefunc(GtkTreeModel *model,GtkTreeIter *iter,gpointer data) {
	Tfilefiltergui *ffg = data;
	gboolean retval = FALSE;
	gboolean type;
	gchar *name=NULL;
	gtk_tree_model_get(model, iter, 0, &name, 2, &type, -1);
	if (name) {
		if (type == 0) {
			retval = (g_hash_table_lookup(ffg->curfilter->filetypes, name) != NULL);
		} else { /* type == 1*/
			GList *tmplist;
			for (tmplist = g_list_first(ffg->curfilter->patterns) ; tmplist ; tmplist = g_list_next(tmplist)) {
				Tpattern *pat = tmplist->data;
				if (strcmp(pat->pattern, name)==0) {
					retval = TRUE;
					break;
				}
			}
		}
		g_free(name);
	}
	return retval;
}
static gboolean filefiltergui_outfilter_visiblefunc(GtkTreeModel *model,GtkTreeIter *iter,gpointer data) {
	return !filefiltergui_infilter_visiblefunc(model,iter,data);
}

static void filefiltergui_add_filetypes(gpointer key,gpointer value,gpointer data) {
	Tfilefiltergui *ffg = data;
	
	if (strlen(key)>0 && g_hash_table_lookup(main_v->filetypetable, key) == NULL) {
		GtkTreeIter it;
		Tfiletype *ft = get_filetype_for_mime_type(key);
		gtk_list_store_prepend(ffg->lstore,&it);
		gtk_list_store_set(ffg->lstore,&it,0,ft->mime_type,1,ft->icon,2,0,-1);
	}
}

static void filefiltergui_2right_clicked(GtkWidget *widget, Tfilefiltergui *ffg) {
	GtkTreeIter iter;
	GtkTreeSelection *select;
	GtkTreeModel *model;

	/* get the selection */
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(ffg->out_view));
	if (gtk_tree_selection_get_selected(select, &model, &iter)) {
		gchar *name;
		gint type;
		gtk_tree_model_get(model, &iter, 0, &name, 2, &type, -1);
		/* add the selection to the filter */
		DEBUG_MSG("filefiltergui_2right_clicked, adding %s\n",name);
		if (type == 0) {
			g_hash_table_replace(ffg->curfilter->filetypes, name, GINT_TO_POINTER(1));
		} else {
			ffg->curfilter->patterns = g_list_append(ffg->curfilter->patterns, new_pattern(name));
		}

		DEBUG_MSG("filefiltergui_2right_clicked, refilter\n");
		/* refilter */
		gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(ffg->in_model));
		gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(ffg->out_model));
	} else {
		DEBUG_MSG("filefiltergui_2right_clicked, nothing selected\n");
	}
}

static void filefiltergui_2left_clicked(GtkWidget *widget, Tfilefiltergui *ffg) {
	GtkTreeIter iter;
	GtkTreeSelection *select;
	GtkTreeModel *model;

	/* get the selection */
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW (ffg->in_view));
	if (gtk_tree_selection_get_selected(select, &model, &iter)) {
		gchar *name;
		gboolean type;
		gtk_tree_model_get(model, &iter, 0, &name,2,&type, -1);
		/* add the selection to the filter */
		if (type == 0) {
			DEBUG_MSG("filefiltergui_2left_clicked, removing %s\n",name);
			g_hash_table_remove(ffg->curfilter->filetypes, name);
		} else {
			/* remove from the list of the filter */
			ffg->curfilter->patterns = remove_pattern_from_list(ffg->curfilter->patterns, name);
		}
		g_free (name);
		DEBUG_MSG("filefiltergui_2left_clicked, refilter\n");
		/* refilter */
		gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(ffg->in_model));
		gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(ffg->out_model));
	} else {
		DEBUG_MSG("filefiltergui_2left_clicked, nothing selected\n");
	}
}

static void filefiltergui_addpattern_clicked_lcb(GtkWidget *widget, Tfilefiltergui *ffg) {
	Tpattern *pat;
	GtkTreeIter it;
	pat = g_new(Tpattern,1);
	pat->pattern = g_strdup(gtk_entry_get_text(GTK_ENTRY(ffg->patentry)));
	pat->patspec = g_pattern_spec_new(pat->pattern);
	ffg->curfilter->patterns = g_list_append(ffg->curfilter->patterns, pat);
	gtk_list_store_prepend(ffg->lstore,&it);
	gtk_list_store_set(ffg->lstore,&it,0,pat->pattern,1,NULL,2,1,-1);
}

void filefilter_gui(Tfilter *filter) {
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GList *tmplist;
	GtkWidget *table,*hbox,*but,*vbox,*scrolwin;

	Tfilefiltergui *ffg = g_new0(Tfilefiltergui,1);
	ffg->curfilter = filter;
	if (filter) {
		ffg->origname = g_strdup(filter->name);
	}
	if (!filter) {
		ffg->curfilter = g_new0(Tfilter,1);
		ffg->curfilter->name = g_strdup("New filter");
		ffg->curfilter->filetypes = g_hash_table_new_full(g_str_hash,g_str_equal,g_free,NULL);
	}
	
	DEBUG_MSG("filefilter_gui, editing filter %p\n",ffg->curfilter); 
	ffg->win = window_full2(_("Edit filter"), GTK_WIN_POS_MOUSE, 10, G_CALLBACK(filefiltergui_destroy_lcb),ffg, TRUE, NULL);
	gtk_window_set_default_size(GTK_WINDOW(ffg->win),400,400);
	ffg->lstore = gtk_list_store_new(3, G_TYPE_STRING, GDK_TYPE_PIXBUF, G_TYPE_BOOLEAN);
	
	/* fill the list model from the currently known filetypes */
	tmplist = g_list_first(main_v->filetypelist);
	while (tmplist) {
		GtkTreeIter it;
		Tfiletype *ft = tmplist->data;
		if (strncmp(ft->mime_type,"x-directory",11)!=0) {
			gtk_list_store_prepend(ffg->lstore,&it);
			DEBUG_MSG("filefilter_gui, adding %s\n",ft->mime_type);
			gtk_list_store_set(ffg->lstore,&it,0,ft->mime_type,1,ft->icon,2,0, -1);
		}
		tmplist = g_list_next(tmplist);
	}
	/* make sure that all filetypes that exist in the current filter are shown */
	g_hash_table_foreach(ffg->curfilter->filetypes,filefiltergui_add_filetypes,ffg);
	/* add the patterns from the current filter */
	tmplist = g_list_first(ffg->curfilter->patterns);
	while (tmplist) {
		GtkTreeIter it;
		Tpattern *pat = (Tpattern *)tmplist->data;
		gtk_list_store_prepend(ffg->lstore,&it);
		gtk_list_store_set(ffg->lstore,&it,0,pat->pattern,1,NULL,2,1,-1);
		tmplist = g_list_next(tmplist);
	}
	
	ffg->in_model = gtk_tree_model_filter_new(GTK_TREE_MODEL(ffg->lstore), NULL);
	gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(ffg->in_model),filefiltergui_infilter_visiblefunc,ffg,NULL);
	ffg->out_model = gtk_tree_model_filter_new(GTK_TREE_MODEL(ffg->lstore), NULL);
	gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(ffg->out_model),filefiltergui_outfilter_visiblefunc,ffg,NULL);

	table = gtk_table_new(5,4,FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	
	ffg->nameentry = entry_with_text(ffg->curfilter->name, 50);
	gtk_table_attach(GTK_TABLE(table),ffg->nameentry,0,1,0,1,GTK_FILL,GTK_FILL,0,0);
	
	ffg->inversecheck = checkbut_with_value(_("Hide files that match the filter"), !ffg->curfilter->mode);
	gtk_table_attach(GTK_TABLE(table),ffg->inversecheck,0,1,1,2,GTK_FILL,GTK_FILL,0,0);
	
	ffg->patentry = entry_with_text("*.*", 20);
	gtk_table_attach(GTK_TABLE(table),ffg->patentry,2,3,1,2,GTK_FILL,GTK_FILL,0,0);
	but = gtk_button_new_with_label("Add pattern");
	g_signal_connect(but,"clicked",G_CALLBACK(filefiltergui_addpattern_clicked_lcb),ffg);
	gtk_table_attach(GTK_TABLE(table),but,3,4,1,2,GTK_FILL,GTK_FILL,0,0);
	
	ffg->in_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ffg->in_model));
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Mime type"),renderer,"text", 0,NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(ffg->in_view), column);
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(_("Icon"),renderer,"pixbuf", 1,NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(ffg->in_view), column);
	scrolwin = 	gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolwin),ffg->in_view);
	gtk_table_attach_defaults(GTK_TABLE(table),scrolwin,2,4,2,3);
	
	ffg->out_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ffg->out_model));
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Mime type"),renderer,"text", 0,NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(ffg->out_view), column);
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(_("Icon"),renderer,"pixbuf", 1,NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(ffg->out_view), column);
	scrolwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolwin),ffg->out_view);
	gtk_table_attach_defaults(GTK_TABLE(table),scrolwin,0,1,2,3);
	
	vbox = gtk_vbox_new(TRUE,5);
	but = gtk_button_new_with_label("->");
	g_signal_connect(but,"clicked",G_CALLBACK(filefiltergui_2right_clicked),ffg);
	gtk_box_pack_start(GTK_BOX(vbox),but,TRUE,TRUE,0);
	but = gtk_button_new_with_label("<-");
	g_signal_connect(but,"clicked",G_CALLBACK(filefiltergui_2left_clicked),ffg);
	gtk_box_pack_start(GTK_BOX(vbox),but,TRUE,TRUE,0);
	gtk_table_attach(GTK_TABLE(table),vbox,1,2,2,3,GTK_EXPAND|GTK_FILL,GTK_EXPAND,5,5);
	
	hbox = gtk_hbutton_box_new();
	gtk_hbutton_box_set_layout_default(GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbox), 12);
	but = bf_stock_cancel_button(G_CALLBACK(filefiltergui_cancel_clicked), ffg);
   gtk_box_pack_start(GTK_BOX(hbox),but, FALSE, FALSE, 0);	
	but = bf_stock_ok_button(G_CALLBACK(filefiltergui_ok_clicked), ffg);
	gtk_box_pack_start(GTK_BOX(hbox),but, FALSE, FALSE, 0);
	
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 3, 4, 5,GTK_FILL,GTK_FILL,0,0);

	gtk_container_add(GTK_CONTAINER(ffg->win), table);
	gtk_widget_show_all(ffg->win);	
}