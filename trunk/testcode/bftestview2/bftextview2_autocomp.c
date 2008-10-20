#include <string.h>
#include <gdk/gdkkeysyms.h>
#include "bftextview2_scanner.h"
#include "bftextview2_autocomp.h"


typedef struct {
	Tcontext *context;
	gchar *prefix;
	gchar *newprefix;
	GtkWidget *win;
	GtkListStore *store;
	GtkTreeView *tree;
	GtkWidget *reflabel;
} Tacwin;

#define ACWIN(p) ((Tacwin *)(p))

static void acwin_cleanup(BluefishTextView * btv) {
	if (btv->autocomp) {
		g_free(ACWIN(btv->autocomp)->prefix);
		gtk_widget_destroy(ACWIN(btv->autocomp)->win);
		g_free(ACWIN(btv->autocomp));
		btv->autocomp = NULL;
	}
}

static gboolean acwin_move_selection(BluefishTextView *btv, gint keyval) {
	GtkTreeSelection *selection;
	GtkTreeIter it;
	GtkTreeModel *model;
	GtkTreePath *path;

	selection = gtk_tree_view_get_selection(ACWIN(btv->autocomp)->tree);
	if (gtk_tree_selection_get_selected(selection,&model,&it)) {
		gint i,rows=12, *indices=NULL;
		path = gtk_tree_model_get_path(model, &it);
		indices = gtk_tree_path_get_indices(path);
		switch (keyval) {
			case GDK_Up:	/* move the selection one up */
				gtk_tree_path_prev(path);
			break;
			case GDK_Down:
				gtk_tree_path_next(path);
			break;
			case GDK_Page_Down:
				i = MIN(gtk_tree_model_iter_n_children(model,NULL)-1,indices[0]+rows);
				gtk_tree_path_free(path);
				path = gtk_tree_path_new_from_indices(i,-1);
			break;
			case GDK_Page_Up:
				i = MAX(indices[0]-rows,0);
				gtk_tree_path_free(path);
				path = gtk_tree_path_new_from_indices(i,-1);
			break;
			case GDK_Home:
				gtk_tree_path_free(path);
				path = gtk_tree_path_new_first();
			break;
			case GDK_End:
				gtk_tree_path_free(path);
				i = gtk_tree_model_iter_n_children(model,NULL);
				path = gtk_tree_path_new_from_indices(i-1,-1);
			break;
			default:
				return FALSE;
			break;	
		}
		if (gtk_tree_model_get_iter(model, &it, path))
		{
			gtk_tree_selection_select_iter(selection, &it);
			gtk_tree_view_scroll_to_cell(ACWIN(btv->autocomp)->tree, path, NULL, FALSE, 0, 0);
		}
		gtk_tree_path_free(path);
		return TRUE;
	} else {
		/* set selection */
		
	}
	return FALSE;
}

gboolean acwin_check_keypress(BluefishTextView *btv, GdkEventKey *event)
{
	DBG_AUTOCOMP("got keyval %c\n",event->keyval);
	switch (event->keyval) {
	case GDK_Return: {
		GtkTreeSelection *selection;
		GtkTreeIter it;
		GtkTreeModel *model;
		selection = gtk_tree_view_get_selection(ACWIN(btv->autocomp)->tree);
		if (selection && gtk_tree_selection_get_selected(selection,&model,&it)) {
			gchar *string;
			gtk_tree_model_get(model,&it,1,&string,-1);
			DBG_AUTOCOMP("got string %s\n",string);
			gtk_text_buffer_insert_at_cursor(gtk_text_view_get_buffer(GTK_TEXT_VIEW(btv)),string+strlen(ACWIN(btv->autocomp)->prefix),-1);
			g_free(string);
		}
		acwin_cleanup(btv);
		return TRUE;
	} break;
	case GDK_Tab:
		if (ACWIN(btv->autocomp)->newprefix && strlen(ACWIN(btv->autocomp)->newprefix) > strlen(ACWIN(btv->autocomp)->prefix)) {
			gtk_text_buffer_insert_at_cursor(gtk_text_view_get_buffer(GTK_TEXT_VIEW(btv)),ACWIN(btv->autocomp)->newprefix+strlen(ACWIN(btv->autocomp)->prefix),-1);
		}
		return TRUE;
	break;
	case GDK_Escape:
		acwin_cleanup(btv);
		return TRUE;
	break;
	case GDK_Up:
	case GDK_Down:
	case GDK_Page_Down:
	case GDK_Page_Up:
	case GDK_Home:
	case GDK_End:
		if (acwin_move_selection(btv, event->keyval))
			return TRUE;
	break;
	}

	return FALSE;
}

static void acw_selection_changed_lcb(GtkTreeSelection* selection,Tacwin *acw) {
	GtkTreeModel *model;
	GtkTreeIter iter;
	if (!acw->context->reference)
		return;
	
	if (gtk_tree_selection_get_selected(selection,&model,&iter)) {
		gchar *key;
		gtk_tree_model_get(model,&iter,1,&key,-1);
		if (key) {
			gchar *string = g_hash_table_lookup(acw->context->reference,key);
			if (string) {
				DBG_AUTOCOMP("show %s\n",string);
				gtk_label_set_markup(GTK_LABEL(acw->reflabel),string);
				gtk_widget_show(acw->reflabel);
				gtk_widget_set_size_request(acw->win, 300, 200);
				return;
			}
		}
	}
	gtk_widget_hide(acw->reflabel);
	gtk_widget_set_size_request(acw->win, 150, 200);
}

static Tacwin *acwin_create(BluefishTextView *btv, guint16 context) {
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	GtkWidget *scroll, *vbar, *hbox;
	Tacwin * acw;
	GtkTreeSelection* selection;
	
	acw = g_new0(Tacwin,1);
	acw->context = &g_array_index(btv->bflang->st->contexts,Tcontext, context);
	acw->win = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_widget_set_app_paintable(acw->win, TRUE);
	gtk_window_set_resizable(GTK_WINDOW(acw->win), FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(acw->win), 1);
	gtk_window_set_decorated(GTK_WINDOW(acw->win),FALSE);
	gtk_window_set_type_hint(GTK_WINDOW(acw->win),GDK_WINDOW_TYPE_HINT_POPUP_MENU);

	acw->store = gtk_list_store_new(2,G_TYPE_STRING,G_TYPE_STRING);
	acw->tree = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(acw->store)));
	g_object_unref(acw->store);
	
	gtk_tree_view_set_headers_visible(acw->tree, FALSE);
	scroll = gtk_scrolled_window_new(NULL, NULL);
	vbar = gtk_scrolled_window_get_vscrollbar(GTK_SCROLLED_WINDOW(scroll));
	/*gtk_widget_set_size_request(vbar,10,-1);*/
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("", cell, "markup", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(acw->tree), column);
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(acw->tree),FALSE);
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(acw->tree));
	g_signal_connect(G_OBJECT(selection),"changed",G_CALLBACK(acw_selection_changed_lcb),acw);
	
/*	gtk_tree_view_set_search_column(GTK_TREE_VIEW(acw->tree),1);
	gtk_tree_view_set_search_equal_func(GTK_TREE_VIEW(acw->tree),acwin_tree_search_lcb,prefix,NULL);*/
	
	/*g_signal_connect_swapped(GTK_WINDOW(acw->win),"expose-event",G_CALLBACK(ac_paint),acw->win);*/
	/*gtk_window_set_position (GTK_WINDOW(acw->win), GTK_WIN_POS_MOUSE);*/
	gtk_container_add(GTK_CONTAINER(scroll), GTK_WIDGET(acw->tree));

	hbox = gtk_hbox_new(FALSE,0);
	gtk_box_pack_start(GTK_BOX(hbox),scroll,TRUE,TRUE,0);
	acw->reflabel = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(acw->reflabel),TRUE);
	gtk_box_pack_start(GTK_BOX(hbox),acw->reflabel,TRUE,TRUE,0);
	gtk_container_add(GTK_CONTAINER(acw->win), hbox);
	gtk_widget_set_size_request(acw->reflabel,150,-1);
	gtk_widget_show_all(scroll);
	gtk_widget_show(hbox);
	/*gtk_widget_set_size_request(GTK_WIDGET(acw->tree),100,200);*/
	/*gtk_widget_set_size_request(acw->win, 150, 200);*/
	/*g_signal_connect(G_OBJECT(acw->win),"key-release-event",G_CALLBACK(acwin_key_release_lcb),acw);*/

	return acw;
}

static void acwin_position_at_cursor(BluefishTextView *btv) {
	GtkTextIter it;
	GdkRectangle rect;
	GdkScreen *screen;
	gint x,y;
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(btv));
	screen = gtk_widget_get_screen(GTK_WIDGET(btv));
	
	gtk_text_buffer_get_iter_at_mark(buffer,&it,gtk_text_buffer_get_insert(buffer));
	gtk_text_view_get_iter_location(GTK_TEXT_VIEW(btv),&it,&rect);
	gtk_text_view_buffer_to_window_coords(GTK_TEXT_VIEW(btv), GTK_TEXT_WINDOW_TEXT, rect.x, rect.y,&rect.x, &rect.y);
	gdk_window_get_origin(gtk_text_view_get_window(GTK_TEXT_VIEW(btv),GTK_TEXT_WINDOW_TEXT),&x,&y);
	
	gtk_window_move(GTK_WINDOW(ACWIN(btv->autocomp)->win),rect.x+x ,rect.y+y);
}

/* not only fills the tree, but calculates and sets the required width as well */
static void acwin_fill_tree(Tacwin *acw, GList *items) {
	GList *tmplist,*list;
	gchar *longest=NULL;
	guint numitems=0,longestlen=1;
	
	list = tmplist = g_list_sort(g_list_copy(items), (GCompareFunc) g_strcmp0);
	while (tmplist)	{
		GtkTreeIter it;
		gchar *tmp;
		guint len;
		gtk_list_store_append(acw->store,&it);
		tmp = g_markup_escape_text(tmplist->data,-1);
		len = strlen(tmplist->data);
		if (len > longestlen) {
			g_free(longest);
			longest = tmp;
			longestlen = len;
		}
		gtk_list_store_set(acw->store,&it,0,tmp,1,tmplist->data,-1);
		if (tmp!=longest)
			g_free(tmp);
		numitems++;
		tmplist = g_list_next(tmplist);
	}
	g_list_free(list);
	if (longest) {
		gint len,rowh,h,w;
		PangoLayout *panlay = gtk_widget_create_pango_layout(GTK_WIDGET(acw->tree), NULL);
		pango_layout_set_markup(panlay,longest,-1);
		pango_layout_get_pixel_size(panlay, &len, &rowh);
		h = MIN(MAX((numitems+1)*rowh+8,150),350);
		g_print("numitems=%d, rowh=%d, new height=%d\n",numitems,rowh,h);
		w = len+20;
		gtk_widget_set_size_request(GTK_WIDGET(acw->tree),w,h); /* ac_window */
		g_free(longest);
	}
}

static void print_ac_items(GCompletion *gc) {
	DBG_AUTOCOMP("autocompletion has %d items:",g_list_length(g_list_first(gc->items)));
	GList *tmplist = g_list_first(gc->items);
	while (tmplist) {
		DBG_AUTOCOMP(" %s",(char *)tmplist->data);
		tmplist = g_list_next(tmplist);
	}
	DBG_AUTOCOMP("\n");
}

void autocomp_run(BluefishTextView *btv, gboolean user_requested) {
	GtkTextIter cursorpos,iter;
	GtkTextBuffer *buffer;
	gint contextnum;
	gunichar uc;
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(btv));

	if (!btv->bflang || !btv->bflang->st)
		return;

	gtk_text_buffer_get_iter_at_mark(buffer,&cursorpos,gtk_text_buffer_get_insert(buffer));
	
	iter = cursorpos;
	gtk_text_iter_set_line_offset(&iter,0);


	scan_for_autocomp_prefix(btv,&iter,&cursorpos,&contextnum);
	DBG_AUTOCOMP("got possible match start at %d in context %d, cursor is at %d\n",gtk_text_iter_get_offset(&iter),contextnum,gtk_text_iter_get_offset(&cursorpos));
	/* see if next character is end or symbol */
	uc = gtk_text_iter_get_char(&cursorpos);
	if (uc < NUMSCANCHARS) {
		guint16 identstate = g_array_index(btv->bflang->st->contexts, Tcontext, contextnum).identstate;
		if (g_array_index(btv->bflang->st->table, Ttablerow, identstate).row[uc] == identstate) {
			/* current character is not a symbol! */
			DBG_AUTOCOMP("current character %d is not a symbol\n",uc);
			return;
		}
	} else {
		return;
	}


	if ((user_requested || !gtk_text_iter_equal(&iter,&cursorpos)) && g_array_index(btv->bflang->st->contexts,Tcontext, contextnum).ac != NULL) {
		/* we have a prefix or it is user requested, and we have a context with autocompletion */
		gchar *newprefix, *prefix;
		GList *items;
		
		print_ac_items(g_array_index(btv->bflang->st->contexts,Tcontext, contextnum).ac);
		
		prefix = gtk_text_buffer_get_text(buffer,&iter,&cursorpos,TRUE);
		items = g_completion_complete(g_array_index(btv->bflang->st->contexts,Tcontext, contextnum).ac,prefix,&newprefix);
		DBG_AUTOCOMP("got %d autocompletion items for prefix %s in context %d, newprefix=%s\n",g_list_length(items),prefix,contextnum,newprefix);
		
		if (items!=NULL && (items->next != NULL || strcmp(items->data,prefix)!=0) ) {
					/* do not popup if there are 0 items, and also not if there is 1 item which equals the prefix */
			GtkTreeSelection *selection;
			GtkTreeIter it;
			/* create the GUI */
			if (!btv->autocomp) {
				btv->autocomp = acwin_create(btv, contextnum);
			} else {
				g_free(ACWIN(btv->autocomp)->prefix);
				g_free(ACWIN(btv->autocomp)->newprefix);
				gtk_list_store_clear(ACWIN(btv->autocomp)->store);
			}
			ACWIN(btv->autocomp)->prefix = g_strdup(prefix);
			ACWIN(btv->autocomp)->newprefix = g_strdup(newprefix);
			acwin_fill_tree(ACWIN(btv->autocomp), items);
			acwin_position_at_cursor(btv);
			gtk_widget_show(ACWIN(btv->autocomp)->win);
			gtk_tree_model_get_iter_first(GTK_TREE_MODEL(ACWIN(btv->autocomp)->store), &it);
			selection = gtk_tree_view_get_selection(ACWIN(btv->autocomp)->tree);
			gtk_tree_selection_select_iter(selection, &it);
		} else {
			acwin_cleanup(btv);
		}
		g_free(newprefix);
		g_free(prefix);
	} else {
		DBG_AUTOCOMP("no autocompletion data for context %d, or no prefix\n",contextnum);
		acwin_cleanup(btv);
	}
}
