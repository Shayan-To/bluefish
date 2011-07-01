/* Bluefish HTML Editor
 * autocomp.c - autocompletion system
 *
 * Copyright (C) 2006  Oskar Świda oskar.swida@gmail.com
 * additional extensions Copyright 2007 Olivier Sessink
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

#include "bluefish.h"
#include "autocomp.h"
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/hash.h>
#include <libxml/xmlschemas.h>
#include <libxml/schemasInternals.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

static gint ac_paint(GtkWidget *win)
{
	if (!win) return FALSE;
	gtk_paint_flat_box (win->style, win->window,
	                    GTK_STATE_NORMAL, GTK_SHADOW_ETCHED_IN,
	                    NULL, win, "acomp",
	                    0, 0, -1, -1);
	gtk_paint_shadow (win->style, win->window,
	                  GTK_STATE_NORMAL, GTK_SHADOW_ETCHED_IN,
	                  NULL, win, "acomp",
	                  0, 0, -1, -1);
	return FALSE;
}

static gboolean ac_press(GtkWidget *widget,GdkEventKey *event,gpointer user_data)
{
	GtkDialog *dlg = GTK_DIALOG(user_data);
	switch ( event->keyval )
	{
		case GDK_Return:
			gtk_dialog_response(dlg,GTK_RESPONSE_OK);
			return TRUE;
		case GDK_Escape:
			gtk_dialog_response(dlg,GTK_RESPONSE_CANCEL);
			return TRUE;
	}
	return FALSE;
}

/* the search function now asks the user to input the complete string, but it is more logical 
for the user to continue typing after the prefix that has been typed in the document already..
so this function should know about the prefix in the document and use it..

for example, I enter mysql_ in my text and press ctrl-space, the autocompletion popup 
becomes visible, and I continue typing query --> I want mysql_query to be selected!! */
gboolean ac_tree_search (GtkTreeModel *model,gint column,const gchar *key,
                         GtkTreeIter *iter,gpointer data)
{
	gboolean retval = TRUE;
	gchar *txt = NULL;
	gchar *tmp;

	gtk_tree_model_get(model,iter,column,&txt,-1);
	tmp = g_strconcat((gchar*)data, key, NULL);
	DEBUG_MSG("ac_tree_search, test %s in %s\n",tmp,txt);
	if (g_str_has_prefix(txt,tmp))
		retval= FALSE;

	g_free(txt);
	g_free(tmp);
	return retval;
}

typedef struct {
	GtkWidget *win;
	GtkListStore *store;
	GtkTreeView *tree;
	gchar *prefix;
} Tacwin;

/* Creates ac window */
static Tacwin *ac_create_window(const gpointer prefix) {
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	GtkWidget *scroll, *vbar;

	PangoFontDescription *fontdesc;
	GtkRcStyle *rc;

	GtkTreeModel *sortmodel;

	Tacwin * acw;
	
	acw = g_new(Tacwin,1);
	acw->prefix = g_strdup(prefix);
	
	acw->store = gtk_list_store_new(2,G_TYPE_STRING,G_TYPE_STRING);
	sortmodel = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL(acw->store));
	acw->tree = GTK_TREE_VIEW(gtk_tree_view_new_with_model(sortmodel));
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sortmodel),1, GTK_SORT_ASCENDING);
	
	gtk_tree_view_set_headers_visible(acw->tree, FALSE);
	scroll = gtk_scrolled_window_new(NULL, NULL);
#if GTK_CHECK_VERSION(2,8,0)
	vbar = gtk_scrolled_window_get_vscrollbar(GTK_SCROLLED_WINDOW(scroll));
#else
	vbar = GTK_SCROLLED_WINDOW(scroll)->vscrollbar;
#endif
	gtk_widget_set_size_request(vbar,10,-1);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("", cell, "markup", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(acw->tree), column);
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(acw->tree),TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(acw->tree),1);
	gtk_tree_view_set_search_equal_func(GTK_TREE_VIEW(acw->tree),ac_tree_search,prefix,NULL);
	acw->win = gtk_dialog_new();
	gtk_widget_set_app_paintable (acw->win, TRUE);
	gtk_window_set_resizable (GTK_WINDOW(acw->win), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (acw->win), 0);
	g_signal_connect_swapped(GTK_WINDOW(acw->win),"expose-event",G_CALLBACK(ac_paint),acw->win);
	gtk_window_set_position (GTK_WINDOW(acw->win), GTK_WIN_POS_MOUSE);
	gtk_container_add(GTK_CONTAINER(scroll), GTK_WIDGET(acw->tree));
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(acw->win)->vbox),scroll,TRUE,TRUE,0);
	gtk_widget_show_all(scroll);
	gtk_window_set_decorated(GTK_WINDOW(acw->win),FALSE);
	gtk_dialog_set_has_separator (GTK_DIALOG(acw->win),FALSE);
	g_signal_connect(G_OBJECT(acw->win),"key-release-event",G_CALLBACK(ac_press),acw->win);

	/* or we should use the same font of the document, or we use the gtk font. this hardcoded
	font is (for me) too small */
/*	fontdesc = pango_font_description_from_string("Sans 8");
	gtk_widget_modify_font(GTK_WIDGET(scroll), fontdesc);
	gtk_widget_modify_font(GTK_WIDGET(tree), fontdesc);

	pango_font_description_free (fontdesc);*/
	gtk_widget_hide(GTK_DIALOG(acw->win)->action_area);
	
	/* hardcoded colors might not match with the users theme */
/*	rc = gtk_widget_get_modifier_style(GTK_WIDGET(tree));
	rc->color_flags[3] = GTK_RC_BG | GTK_RC_BASE;
	rc->color_flags[0] = GTK_RC_BG | GTK_RC_BASE;
	gdk_color_parse("#F7F3D2",&rc->bg[0]);
	gdk_color_parse("#F7F3D2",&rc->base[0]);
	gdk_color_parse("#E6DDB8",&rc->bg[3]);
	gdk_color_parse("#E6DDB8",&rc->base[3]);
	gtk_widget_modify_style(GTK_WIDGET(tree),rc);*/
	return acw;
}

/* called once during initialisation (in bluefish.c) */
Tautocomp *ac_init()
{
	Tautocomp *ret = g_new0(Tautocomp,1);
	ret->lang_lists = g_hash_table_new(g_str_hash,g_str_equal);
	ret->dtd_lists = g_hash_table_new(g_str_hash,g_str_equal);
	return ret;
}

/* 
 Runs autocomp window with given list and prefix (string from document at cursor position)
 Returns: only string which has been inserted into document
 PERFORMS INSERTION! 
*/
static gchar *ac_run(Tautocomp *ac, GList *strings, gchar *prefix_in, GtkTextView *view, gboolean empty_allowed,gchar *append) 
{
	GtkTextIter it,it3;
	GdkScreen *screen;
	GtkTextBuffer *buf = gtk_text_view_get_buffer(view);
	GdkRectangle rect;
	gint x,y,w,h,len=0;
	GtkTreeIter it2;
	GList *lst,*items;
	gchar *tofree;
	GtkTreeSelection *selection;
	PangoLayout *l;
	GtkTreeModel *model;
	GtkTreePath *path;
	gchar **arr;
	
	Tacwin *acw;
	GCompletion *gc;
	gchar *prefix = g_strdup(prefix_in);
	gint prefixlen = strlen(prefix);

	if ( !view || !ac || (!empty_allowed && strcmp(prefix,"")==0) ) return NULL;

	/* first see if there is only a single autocomplete option -> insert that */
	
	gc = g_completion_new(NULL);	
	
	if ( strcmp(prefix,"")==0 ) 	{
		items = strings;
	} else {
		
		gchar *newprefix=NULL;
		
		g_completion_add_items(gc,strings);
		items = g_completion_complete(gc,prefix,&newprefix);
		if (newprefix) {
			gint newprefixlen =strlen(newprefix);
			DEBUG_MSG("ac_run, newprefix=%s\n",newprefix); 
			if (newprefixlen > prefixlen) {
				gtk_text_buffer_insert_at_cursor(buf,newprefix+prefixlen,-1);
				g_free(prefix);
				if (items && items->next==NULL) {  /* this is short for: items has 1 entry */
					/* one entry means that all possible text is already inserted by 'newprefix' , so just cleanup and return */
					g_completion_free(gc);
					return NULL;
				}
				prefix = newprefix;
				prefixlen = newprefixlen;
			} else {
				g_free(newprefix);
			}
		}
	}

	DEBUG_MSG("ac_run, have %d items for prefix %s\n",g_list_length(items),prefix);	
	
	acw = ac_create_window(prefix);
	
	screen = gtk_widget_get_screen(GTK_WIDGET(view));
	gtk_text_buffer_get_iter_at_mark(buf,&it,gtk_text_buffer_get_insert(buf));
	gtk_text_view_get_iter_location(view,&it,&rect);
	gtk_text_view_buffer_to_window_coords(view, GTK_TEXT_WINDOW_TEXT, rect.x, rect.y,&rect.x, &rect.y);
	gdk_window_get_origin(gtk_text_view_get_window(view,GTK_TEXT_WINDOW_TEXT),&x,&y);
	l = gtk_widget_create_pango_layout(GTK_WIDGET(acw->tree), NULL);
	it3 = it;
	gtk_text_iter_set_line(&it3,gtk_text_iter_get_line(&it));

	
	w=40;
	/*items = g_list_sort(items,(GCompareFunc)g_strcasecmp);*/
	if (items)
	{
		gtk_list_store_clear(acw->store);
		lst = g_list_first(items);
		while (lst)	{
			gtk_list_store_append(acw->store,&it2);
			arr = g_strsplit((gchar*)lst->data,"$",-1);
			if (arr[1]) {
				if (arr[1][0]=='r')
					tofree = g_strdup_printf("<span foreground=\"#E21111\">%s</span>",arr[0]);
				else
					tofree = g_strdup_printf("<span foreground=\"#000000\" >%s</span>",arr[0]);
				gtk_list_store_set(acw->store,&it2,0,tofree,1,arr[0],-1);
			} else {
				DEBUG_MSG("ac_run, appending %s to liststore\n",arr[0]);
				tofree = g_strdup_printf("<span foreground=\"#000000\" >%s</span>",arr[0]);
				gtk_list_store_set(acw->store,&it2,0,tofree,1,arr[0],-1);
			}
			pango_layout_set_markup(l,tofree,-1);
			pango_layout_get_pixel_size(l, &len, &h);
			if ( len+20 > w ) w = len+20;
			if ( tofree ) g_free(tofree);
			g_strfreev(arr);
			lst = g_list_next(lst);
		}
		/*w = MAX(len*w+15,40);*/
		h=MIN((g_list_length(items)+1)*h+8,200);
		gtk_widget_set_size_request(GTK_WIDGET(acw->tree),w,h); /* ac_window */
		x += rect.x;
		y = y + rect.y + rect.height;
		if ( x + w > gdk_screen_get_width(screen))
			x = gdk_screen_get_width(screen) - x - w;
		if ( y + h > gdk_screen_get_height(screen))
			y = gdk_screen_get_height(screen) - y - h;
			
		
		gtk_window_move(GTK_WINDOW(acw->win),x ,y);
		gtk_tree_model_get_iter_first(GTK_TREE_MODEL(acw->store),&it2);
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(acw->store),&it2);
		gtk_tree_view_set_cursor(acw->tree,path,NULL,FALSE);
		gtk_tree_path_free(path);
		if ( gtk_dialog_run(GTK_DIALOG(acw->win)) ==GTK_RESPONSE_OK )
		{
			selection = gtk_tree_view_get_selection(acw->tree);
			if ( selection && gtk_tree_selection_get_selected(selection,&model,&it2))
			{
				gtk_tree_model_get(model,&it2,1,&tofree,-1);
				DEBUG_MSG("ac_run, inserting tofree='%s', append='%s'\n",tofree,append);
				tofree = tofree+strlen(prefix);
				if ( strcmp(tofree,"")!=0 )
				{
					gtk_text_buffer_insert_at_cursor(buf,tofree,-1);
					if (append) 
						gtk_text_buffer_insert_at_cursor(buf,append,-1);
				}
			}
		}
	}
	g_object_unref(acw->store);
	gtk_widget_destroy(acw->win);
	g_free(acw);
	g_completion_free(gc);	
	return tofree;
}

void ac_run_lang(Tautocomp *ac, gchar *prefix, gchar *name, GtkTextView *view,gchar *append)
{
	gpointer ptr = g_hash_table_lookup(ac->lang_lists, name);
	if ( ptr )
		ac_run(ac, (GList*)ptr, prefix, view, FALSE,append);
}

void ac_run_schema(Tautocomp *ac, gchar *prefix, GList *schemas, Tdtd_list *internal, GtkTextView *view,gchar *append)
{
	gpointer ptr = NULL;
	GList *lst=NULL,*lst2;
	lst2 = schemas;

	while ( lst2 )
	{
		ptr = g_hash_table_lookup(ac->dtd_lists, (gchar*)lst2->data);
		if ( ptr )
		{
			Tdtd_list *l = (Tdtd_list*)ptr;
			lst = g_list_concat(lst, l->elements);
		}
		lst2 = g_list_next(lst2);
	}
	if ( internal )
		lst = g_list_concat(lst,internal->elements);
	if ( lst )
		ac_run(ac, lst, prefix, view, TRUE,append);

}

void ac_run_tag_attributes(Tautocomp *ac, gchar *tag, gchar *prefix, GList *schemas, Tdtd_list *internal, GtkTextView *view,gchar *append)
{
	gpointer ptr = NULL;
	GList *lst=NULL,*lst2;

	lst2 = schemas;

	while ( lst2 )
	{
		ptr = g_hash_table_lookup(ac->dtd_lists, (gchar*)lst2->data);
		if ( ptr )
		{
			Tdtd_list *l = (Tdtd_list*)ptr;
			lst = g_hash_table_lookup(l->ea,tag);
			if ( lst ) break;
		}
		lst2 = g_list_next(lst2);
	}
	if (internal)
	{
		lst2 = g_hash_table_lookup(internal->ea,tag);
		if ( lst2 )
			lst = g_list_concat(lst,lst2);
	}
	if ( lst )
		ac_run(ac, lst, prefix, view, TRUE,append);


}

void ac_add_lang_list(Tautocomp *ac, gchar *name, GList *strings)
{
	g_hash_table_replace(ac->lang_lists, name, strings);
}

static void ac_scan_xml_hash(void *payload, void *data, xmlChar *name)
{
	Tdtd_list *d = (Tdtd_list*)data;
	xmlElementPtr el = (xmlElementPtr)payload;
	xmlAttributePtr attr = el->attributes, ptr=NULL;
	gchar *nn = g_strdup((gchar *)name);
	GList *alist = NULL;

	ptr = attr;
	while ( ptr )
	{
		gchar *str =NULL;
		gchar *req=NULL;
		if (ptr->def == XML_ATTRIBUTE_REQUIRED)
			req="$r";
		else
			req="";
		if ( ptr->prefix )
			str = g_string_chunk_insert_const(d->attributes,g_strstrip(g_strdup_printf("%s:%s%s",ptr->prefix,
			                                  ptr->name,req)));
		else
			str = g_string_chunk_insert_const(d->attributes,g_strstrip(g_strdup_printf("%s%s",
			                                  ptr->name,req)));
		alist = g_list_append(alist, str);
		ptr = ptr->nexth;
	}
	d->elements = g_list_append(d->elements,nn);
	g_hash_table_insert(d->ea,nn,alist);
}

/*
 Parse XML chunk and analyze schema
 Returns schema name
*/
gchar *ac_add_dtd_list(Tautocomp *ac, gchar *chunk,gboolean internal,Tdtd_list **intptr)
{
	int ret;
	gchar *name=NULL;
	xmlParserCtxtPtr ctxt;
	xmlDtdPtr dtd=NULL; /* the resulting document tree */

	ctxt = xmlCreatePushParserCtxt(NULL, NULL,  NULL, 0, "memory");
	ret = xmlParseChunk(ctxt, chunk, g_utf8_strlen(chunk,-1), 0);
	if ( ret!= 0 ) return NULL;

	if ( ctxt->hasExternalSubset)
	{
		name = g_strdup((gchar *)ctxt->extSubURI);
		if (  g_hash_table_lookup(ac->dtd_lists,name)==NULL )
		{
			if (ctxt->hasExternalSubset) {
				if (g_str_has_prefix((gchar*)ctxt->extSubURI,"http://")) {
					if (main_v->props.load_network_dtd)	
						dtd = xmlParseDTD(ctxt->extSubSystem,ctxt->extSubURI);
				}
				else
					dtd = xmlParseDTD(ctxt->extSubSystem,ctxt->extSubURI);				
			}	
		}
	}
	else
	{
		if ( ctxt->myDoc )
			dtd = xmlGetIntSubset(ctxt->myDoc);
	}

	if ( dtd )
	{
		if ( dtd->elements )
		{
			Tdtd_list *data = g_new0(Tdtd_list,1);
			data->elements = NULL;
			data->attributes = g_string_chunk_new(1);
			data->ea = g_hash_table_new(g_str_hash,g_str_equal);
			xmlHashScan((xmlHashTablePtr)(dtd->elements),ac_scan_xml_hash,data);
			if ( !internal )
				g_hash_table_insert(ac->dtd_lists,name, data);
			else
				*intptr = data;
		}
		xmlFreeDtd(dtd);
	}
	else {
		g_free(name);
		name=NULL;
	}
	xmlCleanupParser();
	xmlFreeParserCtxt(ctxt);
	return name;
}

static void ac_xmlschema_walk(xmlNodePtr node, Tdtd_list *data)
{
	if (xmlStrcmp(node->name, (const xmlChar *) "element") == 0)
	{
		gchar *nn = g_strdup((gchar *)xmlGetProp(node, (const xmlChar *) "name"));
		gchar *type = NULL;
		gchar **arr = NULL;
		data->elements = g_list_append(data->elements,nn);
		type = (gchar *)xmlGetProp(node, (const xmlChar *) "type");
		arr = g_strsplit(type,":",-1);
		xmlFree(type);
		if ( arr[1] )
			g_hash_table_insert(data->element_types,nn,g_strstrip(g_strdup(arr[1])));
		else
			g_hash_table_insert(data->element_types,nn,g_strstrip(g_strdup(arr[0])));
		g_strfreev(arr);
	}
	else if (xmlStrcmp(node->name, (const xmlChar *) "attribute") == 0)
	{
		gchar *pname = (gchar *)xmlGetProp(node->parent, (const xmlChar *) "name");
		gchar *use = (gchar *)xmlGetProp(node, (const xmlChar *) "use");
		gchar *aname = NULL;
		GList *lst = NULL;
		if (xmlStrcmp(node->parent->name, (const xmlChar *) "complexType") == 0)
		{
			lst = g_hash_table_lookup(data->type_attrs,pname);
			aname = (gchar *)xmlGetProp(node, (const xmlChar *) "name");
			if (use && strcmp(use, "required") == 0)
				lst = g_list_append(lst,g_strdup_printf("%s$r",aname));
			else
				lst = g_list_append(lst,g_strdup(aname));
			xmlFree(aname);
			g_hash_table_replace(data->type_attrs,g_strdup(pname),lst);
		}
		xmlFree(pname);
		if (use) xmlFree(use);
	}
	if (node->next)
		ac_xmlschema_walk(node->next,data);
	if (node->children)
		ac_xmlschema_walk(node->children,data);
}


gchar *ac_add_xmlschema_list(Tautocomp *ac, gchar *uri)
{
	/* schema implementation is incomplete */
	xmlSchemaParserCtxtPtr ctx = xmlSchemaNewParserCtxt(uri);
	gchar *name = g_strdup(uri);
	xmlSchemaPtr schema =NULL;
	if ( g_hash_table_lookup(ac->dtd_lists,name)==NULL )
	{
		schema =   xmlSchemaParse(ctx);
		if ( schema )
		{
			GList *lst;
			gpointer aux=NULL;
			gchar *type = NULL;
			Tdtd_list *data = g_new0(Tdtd_list,1);
			data->elements = NULL;
			data->attributes = g_string_chunk_new(1);
			data->ea = g_hash_table_new(g_str_hash,g_str_equal);
			data->type_attrs = g_hash_table_new(g_str_hash,g_str_equal);
			data->element_types = g_hash_table_new(g_str_hash,g_str_equal);
			ac_xmlschema_walk(schema->doc->children,data);
			lst = data->elements;
			while ( lst )
			{
				type = g_hash_table_lookup(data->element_types,(gchar*)lst->data);
				if ( type )
				{
					aux = g_hash_table_lookup(data->type_attrs,type);
					if ( aux )
						g_hash_table_insert(data->ea,(gchar*)lst->data,aux);
				}
				lst = g_list_next(lst);
			}
			g_hash_table_insert(ac->dtd_lists,name, data);
			xmlSchemaFree(schema);
		}
		else
		{
			g_free(name);
			name=NULL;
		}
	}
	else
	{
		g_free(name);
		name = NULL;
	}
	return name;
}


static gboolean  ac_key_choice_press  (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	GtkDialog *dlg = GTK_DIALOG(user_data);
	if (!g_unichar_isalnum((gunichar)event->keyval)
	    && event->keyval!=GDK_Escape
	    && !g_unichar_isspace((gunichar)event->keyval))
		return FALSE;
	switch (event->keyval)
	{
		case GDK_Escape:gtk_dialog_response(dlg,GTK_RESPONSE_CANCEL);break;
		default:
			if ( !(event->state & GDK_SHIFT_MASK)
			    && !(event->state & GDK_CONTROL_MASK)
			    && !(event->state & GDK_MOD1_MASK))
				gtk_dialog_response(dlg,GTK_RESPONSE_CANCEL);
			else
			{
				g_object_set_data(G_OBJECT(dlg),"keyname",gtk_accelerator_name(event->keyval,event->state));
				gtk_dialog_response(dlg,GTK_RESPONSE_OK);
			}
		break;
	}
	return FALSE;
}

/* Shows a dialog which allows to select accelerator key for autocompletion */
gchar *ac_key_choice()
{
	GtkWidget *dialog1;
	GtkWidget *dialog_vbox1;
	GtkWidget *vbox1;
	GtkWidget *label1;
	GtkWidget *label2;
	GtkWidget *dialog_action_area1;
	
	dialog1 = gtk_dialog_new ();
	gtk_window_set_title (GTK_WINDOW (dialog1), "Autocompletion accelerator");
	gtk_window_set_position (GTK_WINDOW (dialog1), GTK_WIN_POS_CENTER);
	gtk_window_set_modal (GTK_WINDOW (dialog1), TRUE);
	/*gtk_window_set_decorated (GTK_WINDOW (dialog1), FALSE);*/
	gtk_window_set_type_hint (GTK_WINDOW (dialog1), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog1), FALSE);
	
	dialog_vbox1 = GTK_DIALOG (dialog1)->vbox;
	gtk_widget_show (dialog_vbox1);
	
	vbox1 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox1);
	gtk_box_pack_start (GTK_BOX (dialog_vbox1), vbox1, FALSE, FALSE, 0);
	
	label1 = gtk_label_new ("<b>Keystroke choice </b>");
	gtk_widget_show (label1);
	gtk_box_pack_start (GTK_BOX (vbox1), label1, FALSE, FALSE, 0);
	gtk_label_set_use_markup (GTK_LABEL (label1), TRUE);
	gtk_label_set_justify (GTK_LABEL (label1), GTK_JUSTIFY_CENTER);
	gtk_misc_set_padding (GTK_MISC (label1), 2, 2);
	
	label2 = gtk_label_new ("\nPress requested key combination.\nPlease use Ctrl, Shift, Alt key with any other key.\n<i>Esc to cancel.</i>");
	gtk_widget_show (label2);
	gtk_box_pack_start (GTK_BOX (vbox1), label2, FALSE, FALSE, 0);
	gtk_label_set_use_markup (GTK_LABEL (label2), TRUE);
	gtk_label_set_justify (GTK_LABEL (label2), GTK_JUSTIFY_CENTER);
	gtk_misc_set_padding (GTK_MISC (label2), 2, 2);
	
	dialog_action_area1 = GTK_DIALOG (dialog1)->action_area;
	gtk_widget_show (dialog_action_area1);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area1), GTK_BUTTONBOX_END);
	g_signal_connect(G_OBJECT(dialog1),"key-press-event",G_CALLBACK(ac_key_choice_press),dialog1);
	if ( gtk_dialog_run(GTK_DIALOG(dialog1)) == GTK_RESPONSE_OK )
	{
		gpointer ptr = g_object_get_data(G_OBJECT(dialog1),"keyname");
		gtk_widget_destroy(dialog1);
		if (ptr) return (gchar*)ptr;
		else return NULL;
	}
	gtk_widget_destroy(dialog1);
	return NULL;
} 
