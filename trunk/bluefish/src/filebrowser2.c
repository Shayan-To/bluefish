/* Bluefish HTML Editor
 * filebrowser2.c - the filebrowser v2
 *
 * Copyright (C) 2002,2003,2004,2005,2006,2007,2008 Olivier Sessink
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

#define DEBUG

/* ******* FILEBROWSER DESIGN ********
there is only one treestore left for all bluefish windows. This treestore has all files 
and all directories used in all bluefish windows. This treestore has a column for the pixmap
and the name (both shown to the user), but also the GnomeVFSURI for the full path (excluding the 
trailing slash for directories, see #1), and the type. This treestore is in main_v->fb2config. Each file 
or directory added to the treestore is also added to a hashtable in main_v->fb2config

the GUI uses two filtermodels on top of the treestore, and then two sortmodels, so they can 
show a specific part of the treestore (for example with a basedir other then the root, or with 
some filter applied).

#1) the hash for file:///home/ is different from file:///home so we should choose which hash is
to be stored in the hashtable. The method gnome_vfs_uri_get_parent() returns directories without
the trailing slash. So it is convenient to use directories without trailing slashes all the way.

alex: g_hash_table_new(gnome_vfs_uri_hash, gnome_vfs_uri_hequal) is what you're supposed to do
*/

#include <gtk/gtk.h>
#include <string.h>

#include "bluefish.h"
#include "bf_lib.h"
#include "dialog_utils.h"
#include "document.h"
#include "file.h"
#include "filetype.h"
#include "file_dialogs.h"
#include "gtk_easy.h"        /* destroy_disposable_menu_cb() */
#include "menu.h"            /* menu_translate() */
#include "project.h"
#include "stringlist.h"      /* count_array() */
#include "filefilter.h"

typedef struct {
  GtkTreeStore *filesystem_tstore; /* the directory tree */ 
  GHashTable *filesystem_itable; /* iter to known files and directories */
  GdkPixbuf *unknown_icon;
  GdkPixbuf *dir_icon;
  GList *uri_in_refresh; /* all uris currently in refresh are stored here, because 
                  two refreshes on the same uri should not happen */
} Tfilebrowser2config;
#define FB2CONFIG(var) ((Tfilebrowser2config *)(var))

typedef struct {
#ifdef HAVE_ATLEAST_GIO_2_16
  GCancellable* cancel;
  GFile *uri;
  GFile *p_uri;
  GFileEnumerator* gfe;
#else /* no HAVE_ATLEAST_GIO_2_16  */
  GnomeVFSAsyncHandle *handle;
  GnomeVFSURI *uri;
  GnomeVFSURI *p_uri;
#endif /* else HAVE_ATLEAST_GIO_2_16 */
  GtkTreeIter *parent;
} Turi_in_refresh;

enum {
  PIXMAP_COLUMN,
  FILENAME_COLUMN,
  URI_COLUMN,
  REFRESH_COLUMN,
  TYPE_COLUMN,
#ifdef HAVE_ATLEAST_GIO_2_16
  FILEINFO_COLUMN,
#endif /* HAVE_ATLEAST_GIO_2_16 */
  N_COLUMNS
};

enum {
  DIR_NAME_COLUMN,
  DIR_URI_COLUMN
};

enum {
  viewmode_tree,
  viewmode_dual,
  viewmode_flat
};

typedef struct {
  GtkWidget *dirmenu_v;
  gulong dirmenu_changed_signal;
  GtkTreeModel *dirmenu_m;
  GnomeVFSURI *currentdir;
  
  GtkWidget *vbox; /* toiplevel vbox for filebrowser widgets */
  GtkWidget *dir_v; /* treeview widget */
  GtkWidget *vpaned;/* added to the vbox when two panels are used */
  GtkWidget *dirscrolwin; /* added to the vbox when one panel is used */
  GtkWidget *file_v; /* treeview widget */
  
  gulong expand_signal;
  
  GnomeVFSURI *basedir;
  Tfilter *curfilter;

  GtkTreeModel *dir_tfilter;
  GtkTreeModel *dir_tsort;

  GtkTreeModel *file_lfilter;
  GtkTreeModel *file_lsort;

  gboolean last_popup_on_dir;
  gboolean filebrowser_show_hidden_files;
  gboolean filebrowser_show_backup_files;
  gint filebrowser_viewmode;
  Tbfwin *bfwin;
} Tfilebrowser2;

#define FILEBROWSER2(var) ((Tfilebrowser2 *)(var))
static void dirmenu_set_curdir(Tfilebrowser2 *fb2, GnomeVFSURI *newcurdir);
static void refilter_filelist(Tfilebrowser2 *fb2, GtkTreePath *newroot);
static GnomeVFSURI *fb2_uri_from_fspath(Tfilebrowser2 *fb2, GtkTreePath *fs_path);
static void fb2_set_basedir_backend(Tfilebrowser2 *fb2, GnomeVFSURI *uri);
static void fb2_set_viewmode_widgets(Tfilebrowser2 *fb2, gint viewmode);
/**************/

void DEBUG_URI(GnomeVFSURI *uri, gboolean newline) {
#ifdef HAVE_ATLEAST_GIO_2_16
  gchar *name = g_file_get_uri(uri);
#else /* no HAVE_ATLEAST_GIO_2_16  */
  gchar *name = gnome_vfs_uri_to_string(uri, GNOME_VFS_URI_HIDE_PASSWORD);
#endif /* else HAVE_ATLEAST_GIO_2_16 */
  DEBUG_MSG("%s", name);
  if (newline) {
    DEBUG_MSG("\n");
  }
  g_free(name);
}
static void DEBUG_DIRITER(GtkTreeIter *diriter) {
  gchar *name;
  GnomeVFSURI *uri;
  gtk_tree_model_get(GTK_TREE_MODEL(FB2CONFIG(main_v->fb2config)->filesystem_tstore), diriter, FILENAME_COLUMN, &name, URI_COLUMN, &uri, -1);
  DEBUG_MSG("DEBUG_DIRITER, iter(%p) has filename %s, uri ",diriter,name);
  DEBUG_URI(uri, TRUE);
  g_free(name);
}

static void DEBUG_TPATH(GtkTreeModel *model, GtkTreePath *path, gboolean newline) {
  gchar *tname, *filename;
  GtkTreeIter iter;
  tname = gtk_tree_path_to_string(path);
  DEBUG_MSG(tname);
  if (gtk_tree_model_get_iter(model,&iter,path)) {
    gtk_tree_model_get(model, &iter, FILENAME_COLUMN, &filename, -1);
  } else {
    filename = "NO VALID ITER";
  }
  DEBUG_MSG(" -> %s", filename);
  if (newline) {
    DEBUG_MSG("\n");
  }
  g_free(tname);
  g_free(filename);
}
/**************/
typedef struct { 
  Tbfwin *bfwin;
  GtkWidget *win;
  GtkWidget *doc_entry;
  GtkWidget *web_entry;
  GnomeVFSURI *documentroot;
  GnomeVFSURI *webroot;
} Tdocrootdialog;

static void drd_response_lcb(GtkDialog * dialog, gint response, Tdocrootdialog * drd) {
  if (response == GTK_RESPONSE_ACCEPT) {
    GnomeVFSURI *docroot, *webroot;
#ifdef HAVE_ATLEAST_GIO_2_16
    docroot = g_file_new_for_uri(gtk_entry_get_text(GTK_ENTRY(drd->doc_entry)));
    webroot = g_file_new_for_uri(gtk_entry_get_text(GTK_ENTRY(drd->web_entry)));
#else /* no HAVE_ATLEAST_GIO_2_16  */
    docroot = gnome_vfs_uri_new(gtk_entry_get_text(GTK_ENTRY(drd->doc_entry)));
    webroot = gnome_vfs_uri_new(gtk_entry_get_text(GTK_ENTRY(drd->web_entry)));
#endif /* else HAVE_ATLEAST_GIO_2_16 */
    if (docroot && webroot) {
      if (drd->bfwin->session->documentroot)
        g_free(drd->bfwin->session->documentroot);
#ifdef HAVE_ATLEAST_GIO_2_16
      drd->bfwin->session->documentroot = g_file_get_uri(docroot);
#else /* no HAVE_ATLEAST_GIO_2_16  */
      drd->bfwin->session->documentroot = gnome_vfs_uri_to_string(docroot,0);
#endif /* else HAVE_ATLEAST_GIO_2_16 */
      if (drd->bfwin->session->webroot)
        g_free(drd->bfwin->session->webroot);
#ifdef HAVE_ATLEAST_GIO_2_16
      drd->bfwin->session->webroot = g_file_get_uri(webroot);
#else /* no HAVE_ATLEAST_GIO_2_16  */
      drd->bfwin->session->webroot = gnome_vfs_uri_to_string(webroot,0);
#endif /* else HAVE_ATLEAST_GIO_2_16 */
      gnome_vfs_uri_unref(docroot);
      gnome_vfs_uri_unref(webroot);
    }
  }
  gtk_widget_destroy(drd->win);
  if (drd->webroot)
    gnome_vfs_uri_unref(drd->webroot);
  if (drd->documentroot)
    gnome_vfs_uri_unref(drd->documentroot);
  g_free(drd);
}

static void set_documentroot_dialog(Tbfwin *bfwin, GnomeVFSURI *uri) {
  /* now start the dialog to set the webroot */
  GtkWidget *table, *label;
  gchar *tmp;
  Tdocrootdialog *drd;
  drd = g_new0(Tdocrootdialog,1);
  drd->bfwin = bfwin;
  drd->documentroot = uri;
  gnome_vfs_uri_ref(drd->documentroot);
  drd->win = gtk_dialog_new_with_buttons(_("Set documentroot and webroot"),
      GTK_WINDOW(bfwin->main_window),GTK_DIALOG_DESTROY_WITH_PARENT, 
      GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, 
      NULL);
  g_signal_connect(G_OBJECT(drd->win), "response", G_CALLBACK(drd_response_lcb), drd);
  table = dialog_table_in_vbox_defaults(2, 2, 5,GTK_DIALOG(drd->win)->vbox);
  label = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(label),_("The <i>documentroot</i> is the place where Bluefish finds the file,\nthe <i>webroot</i> is the location where those same files can be viewed on the webserver"));
  gtk_table_attach_defaults(GTK_TABLE(table),label,0,2,0,1);
  
#ifdef HAVE_ATLEAST_GIO_2_16
  tmp = g_file_get_uri(uri);
#else /* no HAVE_ATLEAST_GIO_2_16  */
  tmp= gnome_vfs_uri_to_string(uri,0);
#endif /* else HAVE_ATLEAST_GIO_2_16 */
  drd->doc_entry = entry_with_text(tmp, 512);
  g_free(tmp); 
  gtk_table_attach_defaults(GTK_TABLE(table),drd->doc_entry,1,2,1,2);
  dialog_mnemonic_label_in_table(_("Documentroot"),drd->doc_entry, table,0, 1,1, 2);

#ifdef HAVE_ATLEAST_GIO_2_16
  if (g_file_has_uri_scheme(uri,"file")) {
    tmp = g_strdup("http://localhost/");
  } else {
  
  }
#else /* no HAVE_ATLEAST_GIO_2_16  */
  {
    if (strcmp(gnome_vfs_uri_get_scheme(uri),"file")==0) {
      tmp = g_strdup("http://localhost/");
    } else {
      gchar *tmp2 = get_hostname_from_uri(uri);
      tmp = g_strconcat("http://",tmp2,"/", NULL);
      g_free(tmp2);
    }
  }

#endif /* else HAVE_ATLEAST_GIO_2_16 */
  drd->web_entry = entry_with_text(tmp, 512);
  gtk_table_attach_defaults(GTK_TABLE(table),drd->web_entry,1,2,2,3);
  dialog_mnemonic_label_in_table(_("Webroot"),drd->web_entry, table,0, 1,2,3);
  
  
  gtk_widget_show_all(drd->win);
}

/**
 * fb2_get_uri_in_refresh:
 *
 */
static Turi_in_refresh *fb2_get_uri_in_refresh(GnomeVFSURI *uri) {
  GList *tmplist = g_list_first(FB2CONFIG(main_v->fb2config)->uri_in_refresh);
  while (tmplist) {
    if (((Turi_in_refresh *)tmplist->data)->uri == uri || gnome_vfs_uri_equal(((Turi_in_refresh *)tmplist->data)->uri, uri)) return tmplist->data;
    tmplist = g_list_next(tmplist);
  }
  return NULL;
}

static void fb2_uri_in_refresh_cleanup(Turi_in_refresh *uir) {
  DEBUG_MSG("fb2_uri_in_refresh_cleanup, called for %p\n",uir);
  gnome_vfs_uri_unref(uir->uri);
#ifdef HAVE_ATLEAST_GIO_2_16
  g_object_unref(uir->cancel);
#endif /* HAVE_ATLEAST_GIO_2_16 */
  g_free(uir);
}

/**
 * fb2_add_filesystem_entry:
 *
 * returns the GtkTreeIter* that is stored in the hashtable. This can be a new iter pointer
 * if there was no iter in the hashtable yet, else it is the existing iter
 *
 */
#ifdef HAVE_ATLEAST_GIO_2_16

static GFileInfo *fake_directory_fileinfo(const gchar *name) {
  GFileInfo* finfo;
  finfo = g_file_info_new();
  g_file_info_set_display_name(finfo, name);
  g_file_info_set_name(finfo, name);
  g_file_info_set_edit_name(finfo, name);
  g_file_info_set_file_type(finfo,G_FILE_TYPE_DIRECTORY);
  g_file_info_set_attribute_string(finfo,G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE,"x-directory/normal");
  return finfo;
}
static GFileInfo *fake_directory_fileinfo_for_uri(GFile *uri) {
  GFileInfo *finfo;
  gchar *name = g_file_get_basename(uri);
  finfo = fake_directory_fileinfo(name);
  g_free(name);
  return finfo;
}


static GtkTreeIter *fb2_add_filesystem_entry(GtkTreeIter *parent, GFile *child_uri, GFileInfo *finfo, gboolean load_subdirs) {
  GtkTreeIter *newiter;
  
  newiter = g_hash_table_lookup(FB2CONFIG(main_v->fb2config)->filesystem_itable, child_uri);
  if (newiter != NULL) {
    gboolean refresh;
    /* the child exists already, update the REFRESH column */
    gtk_tree_model_get(GTK_TREE_MODEL(FB2CONFIG(main_v->fb2config)->filesystem_tstore), newiter, REFRESH_COLUMN, &refresh, -1);
    if (refresh != 0) {
      gtk_tree_store_set(GTK_TREE_STORE(FB2CONFIG(main_v->fb2config)->filesystem_tstore),newiter,REFRESH_COLUMN, 0,-1);
    }
  } else { /* child does not yet exist */
    gchar *display_name, *pixmap, *mime_type;
    GIcon *icon;
    g_object_ref(child_uri);
    g_object_ref(finfo);
    display_name = gfile_display_name(child_uri,finfo);
    icon = g_file_info_get_attribute_object(finfo,G_FILE_ATTRIBUTE_STANDARD_ICON);
    
    g_object_get(icon,"name",&pixmap,NULL);
    mime_type = g_file_info_get_attribute_string(finfo,G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE);
    
    gtk_tree_store_append(GTK_TREE_STORE(FB2CONFIG(main_v->fb2config)->filesystem_tstore),newiter,parent);
    gtk_tree_store_set(GTK_TREE_STORE(FB2CONFIG(main_v->fb2config)->filesystem_tstore),newiter,
        PIXMAP_COLUMN, pixmap,
        FILENAME_COLUMN, display_name,
        URI_COLUMN, child_uri,
        REFRESH_COLUMN, 0,
        TYPE_COLUMN, mime_type,
        FILEINFO_COLUMN, finfo,
        -1);
    g_hash_table_insert(FB2CONFIG(main_v->fb2config)->filesystem_itable,child_uri,newiter);
    if (load_subdirs && g_file_info_get_file_type(finfo)==G_FILE_TYPE_DIRECTORY) {
      GnomeVFSURI *dummy_uri;
      GFileInfo* dummy_finfo;
      /* add a dummy item so the expander will be shown */
      dummy_uri = g_file_get_child(child_uri," ");
      dummy_finfo = fake_directory_fileinfo(" ");
      fb2_add_filesystem_entry(newiter, dummy_uri, dummy_finfo, FALSE);
      g_object_unref(dummy_uri);
      g_object_unref(dummy_finfo);
    } 
  }

}
#else /* no HAVE_ATLEAST_GIO_2_16  */
static GtkTreeIter *fb2_add_filesystem_entry(GtkTreeIter *parent, GnomeVFSURI *child_uri, const gchar *mime_type, gboolean load_subdirs, gboolean isdir) {
  GtkTreeIter *newiter;
  
  newiter = g_hash_table_lookup(FB2CONFIG(main_v->fb2config)->filesystem_itable, child_uri);
  if (newiter != NULL) {
    gint refresh;
/*    DEBUG_MSG("fb2_add_filesystem_entry, set refresh to 0 for iter %p, uri exists ",newiter);
    DEBUG_URI(child_uri, TRUE);*/
    /* the child exists already, update the REFRESH column */
    gtk_tree_model_get(GTK_TREE_MODEL(FB2CONFIG(main_v->fb2config)->filesystem_tstore), newiter, REFRESH_COLUMN, &refresh, -1);
    if (refresh != 0) {
      gtk_tree_store_set(GTK_TREE_STORE(FB2CONFIG(main_v->fb2config)->filesystem_tstore),newiter,REFRESH_COLUMN, 0,-1);
    }
  } else {
    gchar *tmp, *tmp2, *display_name;
    GnomeVFSURI *uri_dup;
    gpointer pixmap;
    newiter = g_new(GtkTreeIter,1);
    uri_dup = child_uri;
    gnome_vfs_uri_ref(child_uri);
    tmp = full_path_utf8_from_uri(child_uri);
    tmp2 = strrchr(tmp, '/');
    DEBUG_MSG("fb2_add_filesystem_entry, tmp2=%s for tmp=%s\n",tmp2,tmp);
    if (tmp2 && strlen(tmp2)>=2) display_name = gnome_vfs_unescape_string(tmp2+1, "");
    else display_name = gnome_vfs_uri_to_string(child_uri,GNOME_VFS_URI_HIDE_PASSWORD);
    g_free(tmp);
    {
      Tfiletype *ft = get_filetype_for_mime_type(mime_type);
      if (ft) {
        pixmap = ft->icon;
      } else {
        pixmap = FB2CONFIG(main_v->fb2config)->unknown_icon;
      }
    }
    DEBUG_MSG("fb2_add_filesystem_entry, appending iter for %s with mime_type %s\n",display_name,mime_type);
    gtk_tree_store_append(GTK_TREE_STORE(FB2CONFIG(main_v->fb2config)->filesystem_tstore),newiter,parent);
    DEBUG_MSG("fb2_add_filesystem_entry, will add ");
    DEBUG_URI(uri_dup, TRUE);
    gtk_tree_store_set(GTK_TREE_STORE(FB2CONFIG(main_v->fb2config)->filesystem_tstore),newiter,
        PIXMAP_COLUMN, pixmap,
        FILENAME_COLUMN, display_name,
        URI_COLUMN, uri_dup,
        REFRESH_COLUMN, 0,
        TYPE_COLUMN, mime_type,
        -1);
    g_free(display_name); /* a column of type string holds a copy, not the original */

    /*
    hmm I think we should unref the pixmap, but I do get errors if I unref it...
    g_object_unref(pixmap);*/
/*    DEBUG_MSG("fb2_add_filesystem_entry adding iter %p to hashtable\n",newiter);*/
    gnome_vfs_uri_ref(child_uri);
    g_hash_table_insert(FB2CONFIG(main_v->fb2config)->filesystem_itable,child_uri,newiter);
    if (load_subdirs && isdir) {
      GnomeVFSURI *dummy_uri;
      /* add a dummy item so the expander will be shown */
      dummy_uri = gnome_vfs_uri_append_string(child_uri,"%20");
      fb2_add_filesystem_entry(newiter, dummy_uri, "x-directory/normal", FALSE, TRUE);
      gnome_vfs_uri_unref(dummy_uri);
    }
  }
  return newiter;
}
#endif /* else HAVE_ATLEAST_GIO_2_16 */

/**
 * fb2_treestore_delete_children_refresh1
 *
 * deletes all children of 'iter' that have value 1 in the REFRESH_COLUMN
 */
static void fb2_treestore_delete_children_refresh1(GtkTreeStore *tstore, GtkTreeIter *iter) {
  GtkTreeIter child;
  if (gtk_tree_model_iter_children(GTK_TREE_MODEL(tstore),&child,iter)) {
    gboolean cont=TRUE;
    while (cont) {
      gboolean refresh;
      GtkTreeIter this = child;
      gtk_tree_model_get(GTK_TREE_MODEL(tstore), &child, REFRESH_COLUMN, &refresh, -1);
      cont = gtk_tree_model_iter_next(GTK_TREE_MODEL(tstore), &child);
      if (refresh == 1) {
        GnomeVFSURI *d_uri;
        /* delete 'this' ! */
        gtk_tree_model_get(GTK_TREE_MODEL(tstore), &this, URI_COLUMN, &d_uri, -1);
        gtk_tree_store_remove(tstore,&this);
        g_hash_table_remove(FB2CONFIG(main_v->fb2config)->filesystem_itable, d_uri);
        DEBUG_MSG("fb2_treestore_delete_children_refresh1, unref d_uri\n");
        gnome_vfs_uri_unref(d_uri);
      }
    }
  }
}
/**
 * fb2_treestore_mark_children_refresh1:
 *
 * sets value 1 in REFRESH_COLUMN for all children of 'iter' 
 */
static void fb2_treestore_mark_children_refresh1(GtkTreeStore *tstore, GtkTreeIter *iter) {
  GtkTreeIter child;
  DEBUG_MSG("fb2_treestore_mark_children_refresh1, started for model=%p\n",tstore);
  if (gtk_tree_model_iter_children(GTK_TREE_MODEL(tstore),&child,iter)) {
    gboolean cont=TRUE;
    while (cont) {
      gtk_tree_store_set(GTK_TREE_STORE(tstore),&child,REFRESH_COLUMN, 1,-1);
      cont = gtk_tree_model_iter_next(GTK_TREE_MODEL(tstore), &child);
    }
  }
  DEBUG_MSG("fb2_treestore_mark_children_refresh1, finished for model=%p\n",tstore);
}

#ifdef HAVE_ATLEAST_GIO_2_16

static void fb2_enumerator_close_lcb(GObject *source_object,GAsyncResult *res,gpointer user_data) {
  Turi_in_refresh *uir = user_data;
  GError *error=NULL;
  g_file_enumerator_close_finish(uir->gfe,res,&error);
  fb2_uri_in_refresh_cleanup(uir);
}

static void fb2_enumerate_next_files_lcb(GObject *source_object,GAsyncResult *res,gpointer user_data) {
  Turi_in_refresh *uir = user_data;
  GError *error=NULL;
  GList* list, *tmplist;
  list = g_file_enumerator_next_files_finish(uir->gfe,res,&error);
  if (error) {
    g_print("ERROR: unhandled error in fb2_enumerate_next_files_lcb()\n");
    return;
  }
  if (list == NULL) {
    /* done */
    g_file_enumerator_close_async(uir->gfe,G_PRIORITY_LOW,uir->cancel,fb2_enumerator_close_lcb,uir);
    return;
  }
  tmplist = g_list_first(list);
  while (tmplist) {
    GFileInfo *finfo = tmplist->data;
    const gchar *name = g_file_info_get_name(finfo);
    GFile *newchild;
    newchild = g_file_get_child(uir->p_uri,name);
    fb2_add_filesystem_entry(uir->parent, newchild, finfo, TRUE);
    g_object_unref(newchild);
    g_object_unref(finfo);
    tmplist = g_list_next(tmplist);
  }
  /* BUG: do error handling */
  g_list_free(list);
}

static void fb2_enumerate_children_lcb(GObject *source_object,GAsyncResult *res,gpointer user_data) {
  Turi_in_refresh *uir = user_data;
  GError *error=NULL;
  uir->gfe = g_file_enumerate_children_finish(uir->p_uri,res,&error);
  if (uir->gfe) {
    g_file_enumerator_next_files_async(uir->gfe,20,G_PRIORITY_LOW,uir->cancel,fb2_enumerate_next_files_lcb,uir);
  }
  /* BUG: do error handling */
}

static void fb2_fill_dir_async(GtkTreeIter *parent, GFile *uri) {
  if (fb2_get_uri_in_refresh(uri) == NULL) {
    Turi_in_refresh *uir;

    fb2_treestore_mark_children_refresh1(FB2CONFIG(main_v->fb2config)->filesystem_tstore, parent);
    
    uir = g_new(Turi_in_refresh,1);
    uir->parent = parent;
    uir->p_uri = uri;
    uir->uri = uri;
    gnome_vfs_uri_ref(uir->uri);
    DEBUG_MSG("fb2_fill_dir_async, opening ");
    DEBUG_URI(uir->p_uri, TRUE);
    uir->cancel = g_cancellable_new();
    g_file_enumerate_children_async(uir->p_uri,"standard::display-name,standard::fast-content-type,standard::icon,standard::edit-name,standard::is-backup,standard::is-hidden,standard::type",
          G_FILE_QUERY_INFO_NONE,
          G_PRIORITY_LOW,uir->cancel,fb2_enumerate_children_lcb,uir);
    FB2CONFIG(main_v->fb2config)->uri_in_refresh = g_list_prepend(FB2CONFIG(main_v->fb2config)->uri_in_refresh, uir);
  }

}
#else /* no HAVE_ATLEAST_GIO_2_16  */
/**
 * fb2_load_directory_lcb
 *
 * the callback from the asynchronous directory load
 *
 */
static void fb2_load_directory_lcb(GnomeVFSAsyncHandle *handle,GnomeVFSResult result,GList *list,guint entries_read,gpointer data) {
  GList *tmplist;
  Turi_in_refresh *uir = data;
  DEBUG_MSG("fb2_load_directory_lcb, result=%d (%s), appending %d childs to ",result,gnome_vfs_result_to_string(result),entries_read);
  DEBUG_URI(uir->p_uri, TRUE);
  tmplist = g_list_first(list);
  while (tmplist) {
    GnomeVFSFileInfo *finfo = tmplist->data;
    
    if (strcmp(finfo->name,".")!=0 && strcmp(finfo->name,"..")!=0) {
      GnomeVFSURI *child_uri;
      child_uri = gnome_vfs_uri_append_file_name(uir->p_uri,finfo->name);
      DEBUG_MSG("fb2_load_directory_lcb, %s has mime_type %s and type %d (valid: %d)\n",finfo->name,finfo->mime_type, finfo->type, finfo->valid_fields);
      /* to avoid the situation where the mime-type for a directory is different from x-directory/normal we always
      set that mime-type if that type is returned */
      fb2_add_filesystem_entry(uir->parent, child_uri, (finfo->type == GNOME_VFS_FILE_TYPE_DIRECTORY) ? "x-directory/normal" : finfo->mime_type, TRUE, (finfo->type == GNOME_VFS_FILE_TYPE_DIRECTORY));
      gnome_vfs_uri_unref(child_uri);
    }
    tmplist = g_list_next(tmplist);
  }
  if (result == GNOME_VFS_ERROR_EOF || result == GNOME_VFS_ERROR_NOT_FOUND) {
    DEBUG_MSG("fb2_load_directory_lcb, cleanup!\n");
    fb2_treestore_delete_children_refresh1(FB2CONFIG(main_v->fb2config)->filesystem_tstore, uir->parent);
    FB2CONFIG(main_v->fb2config)->uri_in_refresh = g_list_remove(FB2CONFIG(main_v->fb2config)->uri_in_refresh, uir);
    fb2_uri_in_refresh_cleanup(uir);
    
  } else if (result == GNOME_VFS_ERROR_CANCELLED) {
    /* prehaps we should restart ? */
    FB2CONFIG(main_v->fb2config)->uri_in_refresh = g_list_remove(FB2CONFIG(main_v->fb2config)->uri_in_refresh, uir);
    fb2_uri_in_refresh_cleanup(uir);
  
  } /* BUG: what to do for other errors !?! */
}

/**
 * fb2_fill_dir_async:
 *
 * starts the asynchronous diretory fill
 *
 * the GtkTreeIter* 'parent' should exist after this function returns, use the 
 * GtkTreeIter that is stored in the hashtable to make sure it is persistent
 *
 * this function will immediatly change values in the treemodel, which might
 * invalidate iters in the calling function, take care of that!
 *
 */
static void fb2_fill_dir_async(GtkTreeIter *parent, GnomeVFSURI *uri) {
  DEBUG_MSG("fb2_fill_dir_async, uri=%p\n",uri);
  if (fb2_get_uri_in_refresh(uri) == NULL) {
    Turi_in_refresh *uir;

    fb2_treestore_mark_children_refresh1(FB2CONFIG(main_v->fb2config)->filesystem_tstore, parent);
    
    uir = g_new(Turi_in_refresh,1);
    uir->parent = parent;
    uir->p_uri = uri;
    uir->uri = uri;
    gnome_vfs_uri_ref(uir->uri);
    DEBUG_MSG("fb2_fill_dir_async, opening ");
    DEBUG_URI(uir->p_uri, TRUE);
    gnome_vfs_async_load_directory_uri(&uir->handle,uri,GNOME_VFS_FILE_INFO_DEFAULT|GNOME_VFS_FILE_INFO_GET_MIME_TYPE|GNOME_VFS_FILE_INFO_FOLLOW_LINKS|GNOME_VFS_FILE_INFO_FORCE_FAST_MIME_TYPE,
                  100,GNOME_VFS_PRIORITY_MIN,fb2_load_directory_lcb,uir);
    FB2CONFIG(main_v->fb2config)->uri_in_refresh = g_list_prepend(FB2CONFIG(main_v->fb2config)->uri_in_refresh, uir);
  }
#ifdef DEBUG  
   else {
    DEBUG_MSG("fb2_fill_dir_async, uri is already refreshing!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
  }
#endif
}
#endif /* else HAVE_ATLEAST_GIO_2_16 */

/**
 * fb2_uri_from_iter:
 *
 * reads the uri stored in the treestore for iter 'iter'
 */
static GnomeVFSURI *fb2_uri_from_iter(GtkTreeIter *iter) {
  GnomeVFSURI *uri;
  gtk_tree_model_get(GTK_TREE_MODEL(FB2CONFIG(main_v->fb2config)->filesystem_tstore), iter, URI_COLUMN, &uri, -1);
  return uri;
}

/**
 * fb2_refresh_dir:
 *
 * the 'dir' iter that is passed to this function will be used in the asynchronous
 * update, so it needs to be a persistent iter. The iters that are in the hashtable 
 * are persistent. Most others are not. So better pass NULL then pass a temporary 
 * iter
 *
 * this function does call fb2_fill_dir_async() which changes the treestore and 
 * thus might invalidates iters
 *
 */
static void fb2_refresh_dir(GnomeVFSURI *uri, GtkTreeIter *dir) {
  /* first we mark all children as 'refresh=1' in the REFRESH_COLUMN, then
  we read the directory, and all existing items are set to refresh=0, after we
  have finished reading the directory, all items that still have refresh=1 can be 
  deleted */
  if (uri != NULL && dir == NULL) {
    dir = g_hash_table_lookup(FB2CONFIG(main_v->fb2config)->filesystem_itable, uri);
  }
  if (dir != NULL && uri == NULL) {
    uri = fb2_uri_from_iter(dir);
  }
  if (dir && uri) {
    gnome_vfs_uri_ref(uri);
    fb2_fill_dir_async(dir, uri);
    gnome_vfs_uri_unref(uri);
  }
}

/**
 * fb2_refresh_dir_from_uri:
 * @dir: the uri to refresh
 *
 * can be used to make the filebrowser refresh a certain directory
 * from external (other then filebrowser) code.
 *
 * will not refresh if this directory does not yet exist in the filebrowser !!
 */
void fb2_refresh_dir_from_uri(GnomeVFSURI *dir) {
  fb2_refresh_dir(dir, NULL);
}

/**
 * fb2_refresh_parent_of_uri:
 *
 * convenience function, will refresh the parent directory of child_uri
 */
void fb2_refresh_parent_of_uri(GnomeVFSURI *child_uri) {
  GnomeVFSURI *parent_uri;
  DEBUG_MSG("fb2_refresh_parent_of_uri, started\n");
  parent_uri = gnome_vfs_uri_get_parent(child_uri);
  fb2_refresh_dir(parent_uri, NULL);
  gnome_vfs_uri_unref(parent_uri);
}
/**
 * fb2_build_dir:
 *
 * builds directories up to uri (uri should point to a directory, not to a file)
 *
 * returns a pointer to the iter stored in the hashtable, so this is a persistent iter
 */
static GtkTreeIter *fb2_build_dir(GnomeVFSURI *uri) {
  GnomeVFSURI* tmp, *parent_uri=NULL;
  GtkTreeIter *parent = NULL;
  tmp = uri;
  /* first find if any directory part of this uri exists already in the treestore */
  gnome_vfs_uri_ref(tmp);
#ifdef HAVE_ATLEAST_GIO_2_16
  while (parent==NULL) {
    GFile* tmp2 = g_file_get_parent(tmp);
    if (tmp2 == NULL) {
      GFileInfo *finfo;
      /* there was no parent for this filesystem yet */
      finfo = fake_directory_fileinfo(" ");
      parent = fb2_add_filesystem_entry(NULL, tmp, finfo, FALSE);
      parent_uri = tmp;
      break;
    } else {
      g_object_unref(tmp);
    }
    tmp = tmp2;
    parent = g_hash_table_lookup(FB2CONFIG(main_v->fb2config)->filesystem_itable, tmp);
  }/* after this loop 'tmp' is newly allocated */
#else /* no HAVE_ATLEAST_GIO_2_16  */
  while (!parent && gnome_vfs_uri_has_parent(tmp)) {
    GnomeVFSURI* tmp2 = gnome_vfs_uri_get_parent(tmp);
    gnome_vfs_uri_unref(tmp);
    tmp = tmp2;
    parent = g_hash_table_lookup(FB2CONFIG(main_v->fb2config)->filesystem_itable, tmp);
    DEBUG_MSG("fb2_build_dir, found treeiter %p for uri ",parent);
    DEBUG_URI(tmp, TRUE);
    if (parent) {
      DEBUG_MSG("treeiter %p points to ", parent);
      DEBUG_DIRITER(parent);
    }
  }/* after this loop 'tmp' is newly allocated */
  if (!parent) {
    DEBUG_MSG("adding toplevel ");
    DEBUG_URI(tmp, TRUE);
    parent = fb2_add_filesystem_entry(NULL, tmp, "x-directory/normal", FALSE, TRUE);
    parent_uri = tmp;
  } else {
    parent_uri = tmp;
  }/* after this loop 'parent_uri'='tmp' is newly allocated */
#endif /* else HAVE_ATLEAST_GIO_2_16 */

  { /* now loop to create all directories in the treestore that were not in the treestore yet*/
    gboolean done=gnome_vfs_uri_equal(parent_uri, uri);
    while (!done) {
      GnomeVFSURI* tmp2 = uri;
#ifdef HAVE_ATLEAST_GIO_2_16
      GFileInfo *finfo;
#endif
      gnome_vfs_uri_ref(tmp2); /* both 'parent_uri'='tmp' and 'tmp2' are newly allocated */
      while (!gnome_vfs_uri_is_parent(parent_uri,tmp2,FALSE)) {
        GnomeVFSURI* tmp3 = gnome_vfs_uri_get_parent(tmp2);
        gnome_vfs_uri_unref(tmp2);
        tmp2 = tmp3;
      } /* after this loop both 'parent_uri'='tmp' and 'tmp2' are newly allocated */
#ifdef HAVE_ATLEAST_GIO_2_16
      finfo = fake_directory_fileinfo_for_uri(tmp2);
      parent = fb2_add_filesystem_entry(parent, tmp2, finfo, gnome_vfs_uri_equal(tmp2,uri));
      g_object_unref(finfo);
#else /* no HAVE_ATLEAST_GIO_2_16  */
      parent = fb2_add_filesystem_entry(parent, tmp2, "x-directory/normal", gnome_vfs_uri_equal(tmp2,uri), TRUE);
#endif /* else HAVE_ATLEAST_GIO_2_16 */
      gnome_vfs_uri_unref(parent_uri);
      parent_uri = tmp2; /* here 'parent_uri'='tmp2' is newly allocated */
      DEBUG_MSG("new parent_uri=");
      DEBUG_URI(parent_uri, FALSE);
      DEBUG_MSG(", requested uri=");
      DEBUG_URI(uri, TRUE);
      if (gnome_vfs_uri_equal(parent_uri,uri)) {
        DEBUG_MSG("exit loop\n");
        done = TRUE;
      }
    }
  }
  gnome_vfs_uri_unref(parent_uri); /* no memory leaks in the uri's... (I hope) */
  return parent;
}

static GtkTreePath * treepath_for_uri(Tfilebrowser2 *fb2, GnomeVFSURI *uri) {
  if (uri) {
    GtkTreeIter *iter;
    iter = g_hash_table_lookup(FB2CONFIG(main_v->fb2config)->filesystem_itable, uri);
    if (!iter) {
      fb2_build_dir(uri);
      iter = g_hash_table_lookup(FB2CONFIG(main_v->fb2config)->filesystem_itable, uri);
    }
    return gtk_tree_model_get_path(GTK_TREE_MODEL(FB2CONFIG(main_v->fb2config)->filesystem_tstore), iter);
  }
  return NULL;
}


/**
 * fb2_focus_dir:
 *
 * builds the directory tree, refreshed the directory, and (if not noselect) expands and selects the 
 * result. during the expand it will block the expand signal handler, so the expand callback
 * will not be called
 *
 */
static void fb2_focus_dir(Tfilebrowser2 *fb2, GnomeVFSURI *uri, gboolean noselect) {
  GtkTreeIter *dir;
  if (!uri) {
    DEBUG_MSG("fb2_focus_dir, WARNING, CANNOT FOCUS WITHOUT URI\n");
    return;
  }
  gnome_vfs_uri_ref(uri);
  if (fb2->filebrowser_viewmode == viewmode_flat) {
    fb2_set_basedir_backend(fb2,uri);
  } else {
    if (fb2->basedir) {
      if (!gnome_vfs_uri_is_parent(fb2->basedir,uri,TRUE) && !gnome_vfs_uri_equal(fb2->basedir,uri)) {
        fb2_set_basedir_backend(fb2,NULL);
      }
    }
  } 
  dir = g_hash_table_lookup(FB2CONFIG(main_v->fb2config)->filesystem_itable, uri);
  DEBUG_MSG("fb2_focus_dir, fb2=%p\n",fb2);
  if (!dir) {
    dir = fb2_build_dir(uri);
  }
  if (dir) {
    GtkTreePath *fs_path;
    DEBUG_DIRITER(dir);
    /* set this directory as the top tree for the file widget */
    fs_path = gtk_tree_model_get_path(GTK_TREE_MODEL(FB2CONFIG(main_v->fb2config)->filesystem_tstore),dir);
    if (fs_path) {
#ifdef DEBUG
#ifdef HAVE_ATLEAST_GIO_2_16
      gchar *basename = g_file_get_basename (uri);
      DEBUG_MSG("fb2_focus_dir, fb2=%p, set new root %s for file list..\n",fb2, basename);
      g_free (basename);
#else
      DEBUG_MSG("fb2_focus_dir, fb2=%p, set new root %s for file list..\n",fb2, gnome_vfs_uri_extract_short_name(uri));
#endif
#endif
      refilter_filelist(fb2, fs_path);
      DEBUG_MSG("fb2_focus_dir, fb2=%p, expand dir tree to this dir..\n",fb2);
      if (!noselect && fb2->filebrowser_viewmode != viewmode_flat) {
        GtkTreePath *filter_path = gtk_tree_model_filter_convert_child_path_to_path(GTK_TREE_MODEL_FILTER(fb2->dir_tfilter),fs_path);
        if (filter_path) {
          GtkTreePath *sort_path = gtk_tree_model_sort_convert_child_path_to_path(GTK_TREE_MODEL_SORT(fb2->dir_tsort),filter_path);
          if (sort_path) {
            g_signal_handler_block(fb2->dir_v, fb2->expand_signal);
            gtk_tree_view_expand_to_path(GTK_TREE_VIEW(fb2->dir_v), sort_path);
            g_signal_handler_unblock(fb2->dir_v, fb2->expand_signal);
            DEBUG_MSG("fb2_focus_dir, selecting path\n");
            gtk_tree_selection_select_path(gtk_tree_view_get_selection(GTK_TREE_VIEW(fb2->dir_v)),sort_path);
            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(fb2->dir_v),sort_path,0,TRUE,0.5,0.5);
            gtk_tree_path_free(sort_path);
          } else DEBUG_MSG("fb2_focus_dir, no sort_path\n");
          gtk_tree_path_free(filter_path);
        } else DEBUG_MSG("fb2_focus_dir, no filter_path\n");
      } else {
        /* 'uri' is not persistent, 'dir' is peristent, so only pass 'dir' 
        the "select" or "expand" signal (dual or tree view) will also refresh this directory, so 
        we call this only on 'noselect' or in the flat view
        */
        DEBUG_MSG("fb2_focus_dir, noselect, so directly call refresh\n");
        fb2_refresh_dir(NULL, dir);
        if (fb2->filebrowser_viewmode == viewmode_flat) {
          dirmenu_set_curdir(fb2, uri);
        }
      }
      gtk_tree_path_free(fs_path);
    } else {
      DEBUG_MSG("NO TREEPATH FOR THE DIR ITER WE TRY TO FOCUS ?!?!?!\n");
    }
  } else {
    DEBUG_MSG("NO dir RETURNED by fb2_build_dir or the hash table\n");
  }
  gnome_vfs_uri_unref(uri);
}
/**
 * fb2_focus_document:
 *
 * external function, will make sure the filebrowser shows the current dcoument
 * directory
 */
void fb2_focus_document(Tbfwin *bfwin, Tdocument *doc) {
  DEBUG_MSG("fb2_focus_document,doc %s\n",gtk_label_get_text(GTK_LABEL(doc->tab_menu)));
  if (bfwin->fb2 && doc->uri) {
    GnomeVFSURI *dir_uri;
    /* first we make sure we have the correct directory open, then
    we could select the document, but if the directory *was* open already, this
    could disturb the user... hmmm... */
    dir_uri = gnome_vfs_uri_get_parent(doc->uri);
#ifdef DEVELOPMENT
    if (!dir_uri) exit(334);
#endif
    fb2_focus_dir(FILEBROWSER2(bfwin->fb2), dir_uri, FALSE);
    gnome_vfs_uri_unref(dir_uri);
  }
}

/*
static gboolean name_visible_in_filter(Tfilebrowser2 *fb2, gchar *name) {
  GList *tmplist;
  
  if (!fb2->curfilter) {
    return TRUE;
  }
  if (!fb2->curfilter->filetypes) {
    return !fb2->curfilter->mode;
  }
  / * TODO: WORK ON THE FILE FILTER * /
#ifndef GNOMEVFSINT
  tmplist = g_list_first(fb2->curfilter->filetypes);
  while (tmplist) {
    if (filename_test_extensions(((Tfiletype *)tmplist->data)->extensions, name))
      return fb2->curfilter->mode;
    tmplist = g_list_next(tmplist);
  }
#endif
  return !fb2->curfilter->mode;
}*/

/**
 * tree_model_filter_func
 *
 * will return TRUE if this file should be visible in the dir view
 */
static gboolean tree_model_filter_func(GtkTreeModel *model,GtkTreeIter *iter,gpointer data) {
  Tfilebrowser2 *fb2 = data;
  gchar *name, *mime_type;
  gint len;
  GnomeVFSURI *uri;
  gboolean retval = TRUE;

  gtk_tree_model_get(GTK_TREE_MODEL(model), iter, FILENAME_COLUMN, &name, URI_COLUMN, &uri, TYPE_COLUMN, &mime_type, -1);
  if (!name) {
    DEBUG_MSG("tree_model_filter_func, model=%p, fb2=%p, item without name!!, uri=%p, mime_type=%s\n",model,fb2,uri,mime_type);
    return TRUE;
  }
  DEBUG_MSG("tree_model_filter_func, model=%p and fb2=%p, name=%s, mime=%s, and uri=",model,fb2,name,mime_type);
  DEBUG_URI(uri, TRUE);
  if (mime_type && strncmp(mime_type, "x-directory",11)!=0) { /* file */
    if (fb2->filebrowser_viewmode == viewmode_dual) {
      /* in the two paned view we don't show files in the dir view */
      retval = FALSE;
    } else {
      if (fb2->filebrowser_viewmode == viewmode_flat) {
        if (fb2->basedir) {
          if (!gnome_vfs_uri_is_parent(fb2->basedir, uri, FALSE)) {
            retval = FALSE;
          }
        }
      }
      if (retval && !fb2->filebrowser_show_backup_files) {
        len = strlen(name);
        if (len > 1 && (name[len-1] == '~')) retval = FALSE;
      }
      if (retval && !fb2->filebrowser_show_hidden_files) {
        if (name[0] == '.') retval = FALSE;
      }
      if (fb2->basedir && !gnome_vfs_uri_is_parent(fb2->basedir, uri, TRUE)) {
        retval = FALSE;
      }
      if (retval && fb2->curfilter) retval = file_visible_in_filter(fb2->curfilter, mime_type, name);
    }
    DEBUG_MSG("file %s with mime_type %s: returning %d\n",name, mime_type, retval);
    g_free(name);
    g_free(mime_type);
    return retval;
  } else { /* directory */
    if (fb2->filebrowser_viewmode == viewmode_flat) {
      if (fb2->basedir) {
        /* show only the level of our basedir no deeper directories */
        if (!gnome_vfs_uri_is_parent(fb2->basedir, uri, FALSE)) {
          retval = FALSE;
        }
      }
    } else {
      if (fb2->basedir) {
        /* show only our basedir on the root level, no other directories at that level */
        if (!gnome_vfs_uri_is_parent(fb2->basedir, uri, TRUE) && !gnome_vfs_uri_equal(fb2->basedir, uri)) {
          retval = FALSE;
        }
      }   
    }
    if (retval && !fb2->filebrowser_show_hidden_files) {
      if (name[0] == '.') retval = FALSE;
    }
  }
  g_free(name);
  g_free(mime_type);
  DEBUG_MSG("returning %d\n",retval);
  return retval;
}
/**
 * file_list_filter_func:
 *
 * will return TRUE if this file should be visible in the file list
 */
static gboolean file_list_filter_func(GtkTreeModel *model,GtkTreeIter *iter,gpointer data) {
  Tfilebrowser2 *fb2 = data;
  gchar *name, *mime_type;
  gint len;
  gboolean retval = TRUE;
/*  DEBUG_MSG("file_list_filter_func, called for model=%p and fb2=%p\n",model,fb2);*/
  gtk_tree_model_get(GTK_TREE_MODEL(model), iter, FILENAME_COLUMN, &name, TYPE_COLUMN, &mime_type, -1);
  if (!name) return FALSE;
  
  if (strncmp(mime_type, "x-directory",11) == 0) retval = FALSE;
  
  if (retval && !fb2->filebrowser_show_backup_files) {
    len = strlen(name);
    if (len > 1 && (name[len-1] == '~')) retval = FALSE;
  }
  if (retval && !fb2->filebrowser_show_hidden_files) {
    if (name[0] == '.') retval = FALSE;
  }
  if (retval && fb2->curfilter ) retval = file_visible_in_filter(fb2->curfilter, mime_type, name);
#ifdef DEBUG
  if (retval == FALSE) {
    g_print("file_list_filter_func, hiding %s (%s)\n",name,mime_type);
  }
#endif
  g_free(name);
  g_free(mime_type);

  return retval;
}

/*
 * filebrowser_sort_func
 *
 * this function is the sort function, and has the following  features:
 * - directories go before files
 * - files are first sorted without extension, only equal names are sorted by extension
 *
 */
gint filebrowser_sort_func(GtkTreeModel *model,GtkTreeIter *a,GtkTreeIter *b,gpointer user_data) {
  gchar *namea,*nameb,*mimea,*mimeb;
  gboolean isdira,isdirb;
  gint retval=0;
  gtk_tree_model_get(GTK_TREE_MODEL(model), a, FILENAME_COLUMN, &namea, TYPE_COLUMN, &mimea, -1);
  gtk_tree_model_get(GTK_TREE_MODEL(model), b, FILENAME_COLUMN, &nameb, TYPE_COLUMN, &mimeb, -1);
  isdira = (mimea && strncmp(mimea,"x-directory",11)==0);
  isdirb = (mimeb && strncmp(mimeb,"x-directory",11)==0);
  if (isdira == isdirb) { /* both files, or both directories */
    if (namea == nameb) {
      retval = 0; /* both NULL */
    } else if (namea == NULL || nameb == NULL) {
      retval = (namea - nameb);
    } else { /* sort by name, first without extension */
      gchar *dota,*dotb;
      dota = strrchr(namea,'.');
      dotb = strrchr(nameb,'.');
      if (dota) *dota = '\0';
      if (dotb) *dotb = '\0';
      retval = strcmp(namea,nameb);
      if (retval == 0) {
        if (dota) *dota = '.';
        if (dotb) *dotb = '.';
        retval = strcmp(namea,nameb);
      }
    }
  } else { /* a directory and a file */
    retval = (isdirb - isdira);
  }
  g_free(namea);
  g_free(nameb);
  g_free(mimea);
  g_free(mimeb);
  return retval;
}

static void add_uri_to_recent_dirs(Tfilebrowser2 *fb2, GnomeVFSURI *uri) {
  gchar *tmp;
#ifdef HAVE_ATLEAST_GIO_2_16
  tmp = g_file_get_uri(uri);
#else /* no HAVE_ATLEAST_GIO_2_16  */
  tmp = gnome_vfs_uri_to_string(uri, GNOME_VFS_URI_HIDE_PASSWORD);
#endif /* else HAVE_ATLEAST_GIO_2_16 */
  fb2->bfwin->session->recent_dirs = add_to_history_stringlist(fb2->bfwin->session->recent_dirs, tmp, TRUE, TRUE);
  g_free(tmp);
}

/**
 * refilter_dirlist:
 *
 * will set the root of the directory view to 'newroot'
 *
 */
static void refilter_dirlist(Tfilebrowser2 *fb2, GtkTreePath *newroot) {
  GtkTreePath *useroot=NULL;
  if (fb2->basedir) gnome_vfs_uri_unref(fb2->basedir);
  fb2->basedir = NULL;
  if (newroot) {
    /* to make it possible to select the root, we move the filter-root one up*/
    useroot = gtk_tree_path_copy(newroot);
    if (fb2->filebrowser_viewmode == viewmode_flat) {
      fb2->basedir = fb2_uri_from_fspath(fb2, newroot);
      add_uri_to_recent_dirs(fb2, fb2->basedir);
      gnome_vfs_uri_ref(fb2->basedir);
      DEBUG_MSG("refilter_dirlist, we use useroot as new root\n");
    } else {
      if (gtk_tree_path_get_depth(newroot) > 1 && gtk_tree_path_up(useroot)) { /* do not set the root as basedir, it is useless  */
        GnomeVFSURI *uri;
        /* store this basedir in fb2 */
        uri = fb2_uri_from_fspath(fb2, newroot);
        gnome_vfs_uri_ref(uri);
        fb2->basedir = uri;
        add_uri_to_recent_dirs(fb2, fb2->basedir);
      } else {
        DEBUG_MSG("there is no parent for this path, so we will set the basedir to NULL\n");
        gtk_tree_path_free(useroot);
        useroot = NULL;
      }
    }
  }
#ifdef DEBUG
#ifdef HAVE_ATLEAST_GIO_2_16
  gchar *path = g_file_get_path (fb2->basedir);
  DEBUG_MSG("refilter_dirlist, newroot=%p, basedir=%s\n",useroot,fb2->basedir ? path : "NULL");
#else
  DEBUG_MSG("refilter_dirlist, newroot=%p, basedir=%s\n",useroot,fb2->basedir ? gnome_vfs_uri_extract_short_path_name(fb2->basedir) : "NULL");
#endif
#endif
  fb2->dir_tfilter = gtk_tree_model_filter_new(GTK_TREE_MODEL(FB2CONFIG(main_v->fb2config)->filesystem_tstore),useroot);
  gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(fb2->dir_tfilter),tree_model_filter_func,fb2,NULL);
  
  fb2->dir_tsort = gtk_tree_model_sort_new_with_model(GTK_TREE_MODEL(fb2->dir_tfilter));
  if (fb2->filebrowser_viewmode != viewmode_dual) {
    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(fb2->dir_tsort), FILENAME_COLUMN,filebrowser_sort_func, NULL, NULL);
  }
  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(fb2->dir_tsort),FILENAME_COLUMN,GTK_SORT_ASCENDING);   
  
/*  fb2->dir_tsort = gtk_tree_model_sort_new_with_model(GTK_TREE_MODEL(fb2->dir_tfilter));
  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(fb2->dir_tsort),FILENAME_COLUMN,GTK_SORT_ASCENDING);*/
  DEBUG_MSG("refilter_dirlist, connect dir_v to new sort(%p)&filter(%p) model\n", fb2->dir_tsort, fb2->dir_tfilter);
  gtk_tree_view_set_model(GTK_TREE_VIEW(fb2->dir_v),GTK_TREE_MODEL(fb2->dir_tsort));
  /* we remove our reference, so the only reference is kept by the treeview, if the treeview is destroyed, the models will be destroyed */
  g_object_unref(fb2->dir_tfilter);
  g_object_unref(fb2->dir_tsort);
  if (useroot) gtk_tree_path_free(useroot);
}
/**
 * refilter_filelist:
 *
 * will set the root of the file view to 'newroot'
 *
 */
static void refilter_filelist(Tfilebrowser2 *fb2, GtkTreePath *newroot) {
  DEBUG_MSG("refilter_filelist, started for fb2=%p, file_lfilter=%p, viewmode=%d\n",fb2,fb2->file_lfilter,fb2->filebrowser_viewmode);
  if (fb2->filebrowser_viewmode == viewmode_dual) {
    if (fb2->file_lfilter) {
      GtkTreePath *curpath;
      gboolean equal;
      g_object_get(fb2->file_lfilter, "virtual-root", &curpath, NULL);
      equal = (curpath == NULL && newroot == NULL) || (curpath != NULL && newroot != NULL && gtk_tree_path_compare(curpath, newroot) == 0);
      gtk_tree_path_free(curpath);
      if (equal) {
#ifdef DEBUG
        DEBUG_MSG("refilter_filelist, root did not change!\n");
#endif
        return;
      }
    }
    fb2->file_lfilter = gtk_tree_model_filter_new(GTK_TREE_MODEL(FB2CONFIG(main_v->fb2config)->filesystem_tstore),newroot);
    DEBUG_MSG("refilter_filelist, set file list filter func, fb2=%p\n",fb2);
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(fb2->file_lfilter),file_list_filter_func,fb2,NULL);
    fb2->file_lsort = gtk_tree_model_sort_new_with_model(GTK_TREE_MODEL(fb2->file_lfilter));
    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(fb2->file_lsort), FILENAME_COLUMN,filebrowser_sort_func, NULL, NULL);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(fb2->file_lsort),FILENAME_COLUMN,GTK_SORT_ASCENDING);
    DEBUG_MSG("refilter_filelist, connect file_v to new sort(%p)&filter(%p) model\n", fb2->file_lsort, fb2->file_lfilter);
    gtk_tree_view_set_model(GTK_TREE_VIEW(fb2->file_v),GTK_TREE_MODEL(fb2->file_lsort));
    /* we remove our reference, so the only reference is kept by the treeview, if the treeview is destroyed, the models will be destroyed */
    g_object_unref(fb2->file_lfilter);
    g_object_unref(fb2->file_lsort);
  }
}

/**
 * fb2_fspath_from_uri:
 *
 * return a newly allocated treepath for the filesystem_tstore based on 'uri'
 */
static GtkTreePath *fb2_fspath_from_uri(Tfilebrowser2 *fb2, GnomeVFSURI *uri) {
  GtkTreeIter *iter;

  iter = g_hash_table_lookup(FB2CONFIG(main_v->fb2config)->filesystem_itable, uri);
  if (iter) {
    return gtk_tree_model_get_path(GTK_TREE_MODEL(FB2CONFIG(main_v->fb2config)->filesystem_tstore), iter);
  }
  return NULL;
}
/**
 * fb2_uri_from_fspath:
 *
 * returns the uri stored in the treestore based on the 'fs_path' from the filesystem treestore
 */
static GnomeVFSURI *fb2_uri_from_fspath(Tfilebrowser2 *fb2, GtkTreePath *fs_path) {
  if (fs_path) {
    GtkTreeIter fsiter;
    if (gtk_tree_model_get_iter(GTK_TREE_MODEL(FB2CONFIG(main_v->fb2config)->filesystem_tstore),&fsiter,fs_path)) {
      return fb2_uri_from_iter(&fsiter);
    } else {
      DEBUG_MSG("fb2_uri_from_fspath, WARNING, no fsiter for fs_path=%p ",fs_path);
      DEBUG_TPATH(GTK_TREE_MODEL(FB2CONFIG(main_v->fb2config)->filesystem_tstore), fs_path, TRUE);
    }
  }
  return NULL;
}
/**
 * fb2_uri_from_file_sort_path:
 *
 * returns the uri stored in the treestore based on the 'sort_path' from the file sort
 */
static GnomeVFSURI *fb2_uri_from_file_sort_path(Tfilebrowser2 *fb2, GtkTreePath *sort_path) {
  GnomeVFSURI *uri=NULL;
  GtkTreeIter iter;
  /* because of the buggy behaviour of the GtkTreeModelFilter we don't use this code
  GtkTreePath *fs_path = fb2_fspath_from_file_sortpath(fb2, sort_path);
  uri = fb2_uri_from_fspath(fb2, fs_path);
  gtk_tree_path_free(fs_path);*/
  if (gtk_tree_model_get_iter(fb2->file_lsort,&iter,sort_path)) {
    gtk_tree_model_get(fb2->file_lsort, &iter, URI_COLUMN, &uri, -1);
  }
  return uri;
}
/**
 * fb2_uri_from_dir_sort_path:
 *
 * returns the uri stored in the treestore based on the 'sort_path' from the dir sort
 */
static GnomeVFSURI *fb2_uri_from_dir_sort_path(Tfilebrowser2 *fb2, GtkTreePath *sort_path) {
  GnomeVFSURI *uri=NULL;
  GtkTreeIter iter;
  if (gtk_tree_model_get_iter(fb2->dir_tsort,&iter,sort_path)) {
    gtk_tree_model_get(fb2->dir_tsort, &iter, URI_COLUMN, &uri, -1);
  }
  return uri;
}
/**
 * fb2_isdir_from_dir_sort_path:
 *
 * returns TRUE if 'sort_path' from the directory sort model points to a directory
 */
static gboolean fb2_isdir_from_dir_sort_path(Tfilebrowser2 *fb2, GtkTreePath *sort_path) {
  GtkTreeIter iter;
  if (gtk_tree_model_get_iter(fb2->dir_tsort,&iter,sort_path)) {
    gchar * mime_type = NULL;
    gboolean is_dir = FALSE;
    gtk_tree_model_get(fb2->dir_tsort, &iter, TYPE_COLUMN, &mime_type, -1);
    DEBUG_MSG("fb2_isdir_from_file_sort_path, mime_type=%s\n",mime_type);
    if (mime_type && strncmp(mime_type, "x-directory", 11) == 0) {
      is_dir = TRUE;
    }
    g_free (mime_type);
    return (is_dir);
  } else {
    DEBUG_MSG("fb2_isdir_from_dir_sort_path, WARNING, sort_path CANNOT be converted into a valid iter!\n");
  }
  return FALSE;
}
/**
 * fb2_fspath_from_dir_selection:
 *
 * returns a newly allocated treepath for the current selected entry in the dir view
 */
static GtkTreePath *fb2_fspath_from_dir_selection(Tfilebrowser2 *fb2) {
  GtkTreeModel *sort_model;
  GtkTreeIter sort_iter;
  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(fb2->dir_v));
  if (gtk_tree_selection_get_selected(selection,&sort_model,&sort_iter)) {
    GnomeVFSURI *uri;
    gtk_tree_model_get(sort_model, &sort_iter, URI_COLUMN, &uri, -1);
    if (uri) {
      return fb2_fspath_from_uri(fb2, uri);
    }
  }
  DEBUG_MSG("fb2_fspath_from_dir_selection, returning NULL\n");
  return NULL;
}
/**
 * fb2_uri_from_file_selection:
 *
 * returns the uri stored in the treestore for the current selected entry in the file_v
 */
static GnomeVFSURI *fb2_uri_from_file_selection(Tfilebrowser2 *fb2) {
  GtkTreeModel *sort_model;
  GtkTreeIter sort_iter;
  if (fb2->file_v && gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(fb2->file_v)),&sort_model,&sort_iter)) {
    GnomeVFSURI *uri=NULL;
    gtk_tree_model_get(sort_model, &sort_iter, URI_COLUMN, &uri, -1);
    return uri;
  } else if (!fb2->file_v && fb2->dir_v && gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(fb2->dir_v)),&sort_model,&sort_iter)) {
    GnomeVFSURI *uri=NULL;
    gtk_tree_model_get(sort_model, &sort_iter, URI_COLUMN, &uri, -1);
    return uri;
  }
#ifdef DEBUG
   else DEBUG_MSG("fb2_uri_from_file_selection, no selection!\n"); 
#endif
  return NULL;
}
/**
 * fb2_uri_from_dir_selection:
 *
 * returns the uri stored in the treestore for the current selected entry in the dir_v
 */
static GnomeVFSURI *fb2_uri_from_dir_selection(Tfilebrowser2 *fb2) {
  GtkTreeModel *sort_model;
  GtkTreeIter sort_iter;
  if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(fb2->dir_v)),&sort_model,&sort_iter)) {
    GnomeVFSURI *uri=NULL;
    gtk_tree_model_get(sort_model, &sort_iter, URI_COLUMN, &uri, -1);
    return uri;
  }
  return NULL;
}
/**
 * handle_activate_on_file:
 *
 * opens the file, project or inserts the image pointer to by 'uri'
 */
static void handle_activate_on_file(Tfilebrowser2 *fb2, GnomeVFSURI *uri) {
#ifdef HAVE_ATLEAST_GIO_2_16
  gchar *mimetype = get_mimetype_for_uri(uri, NULL, FALSE);
  if (mimetype) {
    if (strncmp(mimetype,"image",5)==0) {
      /* image! */
      g_print("handle_activate_on_file, TODO, handle image activate!\n");
    } else if (strcmp(mimetype,"application/bluefish-project")==0) {
      gchar *filename;
#ifdef HAVE_ATLEAST_GIO_2_16
      filename = g_file_get_path(uri);
#else /* no HAVE_ATLEAST_GIO_2_16  */
      filename = gnome_vfs_uri_to_string(uri, GNOME_VFS_URI_HIDE_PASSWORD);
#endif /* else HAVE_ATLEAST_GIO_2_16 */
      project_open_from_file(fb2->bfwin, filename);
      g_free(filename);
      return;
    }
  }
#else /* no HAVE_ATLEAST_GIO_2_16  */
  const gchar *mimetype = get_mimetype_for_uri(uri, FALSE);
  if (mimetype) {
    if (strncmp(mimetype,"image",5)==0) {
      /* image! */
      g_print("handle_activate_on_file, TODO, handle image activate!\n");
    } else if (strcmp(mimetype,"application/bluefish-project")==0) {
      gchar *filename;
      filename = gnome_vfs_uri_to_string(uri, GNOME_VFS_URI_HIDE_PASSWORD);
      project_open_from_file(fb2->bfwin, filename);
      g_free(filename);
      g_free(mimetype);
      return;
    }
    g_free(mimetype);
  }
#endif /* else HAVE_ATLEAST_GIO_2_16 */
  doc_new_from_uri(fb2->bfwin, uri, NULL, FALSE, FALSE, -1, -1);
  DEBUG_MSG("handle_activate_on_file, finished\n");
}

static void fb2rpopup_refresh(Tfilebrowser2 *fb2) {
  GnomeVFSURI *baseuri=NULL;
  gboolean unref_baseuri = FALSE;
  if (fb2->last_popup_on_dir) {
    baseuri = fb2_uri_from_dir_selection(fb2); /* returns the uri in the treestore */
  } else {
    GnomeVFSURI *childuri = fb2_uri_from_file_selection(fb2); /* returns the uri in the treestore */
    if (childuri) { 
      baseuri = gnome_vfs_uri_get_parent(childuri);
      unref_baseuri = TRUE;
    }
  }
  if (!baseuri && (fb2->filebrowser_viewmode == viewmode_flat || fb2->filebrowser_viewmode == viewmode_dual)) {
    /* in flat view or dual view we can refresh the basedir now */
    baseuri = fb2->basedir;
  } 
  if (baseuri) {
    GtkTreeIter *iter;
    iter = g_hash_table_lookup(FB2CONFIG(main_v->fb2config)->filesystem_itable, baseuri);
    fb2_refresh_dir(NULL, iter);
    if (unref_baseuri) gnome_vfs_uri_unref(baseuri);
  }
}

static void rename_not_open_file(Tbfwin *bfwin, GnomeVFSURI *olduri) {
  gchar *newfilename=NULL, *oldfilename;
  
  /* Promt user, "File/Move To"-style. */
#ifdef HAVE_ATLEAST_GIO_2_16
  oldfilename = g_file_get_uri(olduri);
#else /* no HAVE_ATLEAST_GIO_2_16  */
  oldfilename = gnome_vfs_uri_to_string(olduri,0);
#endif /* else HAVE_ATLEAST_GIO_2_16 */
  newfilename = ask_new_filename(bfwin, oldfilename, oldfilename, TRUE);
  if (newfilename) {
    GnomeVFSURI *newuri=NULL;
    gboolean res;
#ifdef HAVE_ATLEAST_GIO_2_16
    GError *error=NULL;
    newuri = g_file_new_for_uri(newfilename);
    res = g_file_move(olduri,newuri,G_FILE_COPY_NONE,NULL,NULL,NULL,&error);
#else /* no HAVE_ATLEAST_GIO_2_16  */
    newuri = gnome_vfs_uri_new(newfilename);
    res = (gnome_vfs_move_uri(olduri,newuri,TRUE) == GNOME_VFS_OK);
#endif /* else HAVE_ATLEAST_GIO_2_16 */
    if (!res) {
      gchar *errmessage = g_strconcat(_("Could not rename\n"), oldfilename, NULL);
      message_dialog_new(bfwin->main_window, 
                   GTK_MESSAGE_ERROR, 
                   GTK_BUTTONS_CLOSE, 
                   errmessage,
                   NULL);
      g_free(errmessage);
    } else {
      GnomeVFSURI *parent1, *parent2;
      parent1 = gnome_vfs_uri_get_parent(olduri);
      parent2 = gnome_vfs_uri_get_parent(newuri);
      if (!gnome_vfs_uri_equal(parent1,parent2)) {
        fb2_refresh_parent_of_uri(olduri);
        fb2_refresh_parent_of_uri(newuri);
      } else {
        fb2_refresh_dir_from_uri(parent1);
      }
      gnome_vfs_uri_unref(parent1);
      gnome_vfs_uri_unref(parent2);
    }
    g_free(newfilename);
    gnome_vfs_uri_unref(newuri);
  }
  g_free(oldfilename);
} 

static void fb2rpopup_new(Tfilebrowser2 *fb2, gboolean newisdir, GnomeVFSURI *noselectionbaseuri) {
  GnomeVFSURI *baseuri=NULL;
  if (fb2->last_popup_on_dir) {
    baseuri = gnome_vfs_uri_dup(fb2_uri_from_dir_selection(fb2));
  } else {
    GnomeVFSURI *childuri = fb2_uri_from_file_selection(fb2);
    if (childuri)
      baseuri = gnome_vfs_uri_get_parent(childuri);
  }
  if (!baseuri) {
    /* no selection, try the noselectionbaseuri */
    baseuri = noselectionbaseuri;
  }
  
  if (baseuri) {
    GnomeVFSURI *newuri;
    gboolean done=FALSE;
    if (newisdir) {
#ifdef HAVE_ATLEAST_GIO_2_16
      GError *error=NULL;
      newuri = g_file_get_child(baseuri, _("New directory"));
      done = g_file_make_directory(newuri,NULL,&error);
#else /* no HAVE_ATLEAST_GIO_2_16  */
      newuri = gnome_vfs_uri_append_file_name(baseuri, _("New directory"));
      done = (gnome_vfs_make_directory_for_uri(newuri, 0755) == GNOME_VFS_OK);
#endif /* else HAVE_ATLEAST_GIO_2_16 */
    } else {
#ifndef HAVE_ATLEAST_GIO_2_16
      GnomeVFSHandle *handle;
#endif /* not HAVE_ATLEAST_GIO_2_16 */
      gint counter = 0;
      gchar *filename = g_strdup(_("New file"));
      while (counter < 100) {
#ifdef HAVE_ATLEAST_GIO_2_16
        GFileOutputStream* gfos;
        GError *error=NULL;
        newuri = g_file_get_child(baseuri, filename);
        gfos = g_file_create(newuri,G_FILE_CREATE_NONE,NULL,&error);
        if (gfos) {
          g_output_stream_close(gfos,NULL,&error);
          done = TRUE;
          counter = 100;
        } else if (error->code == G_IO_ERROR_EXISTS) {
          counter++;
        } else {
          counter = 100;
        }
#else /* no HAVE_ATLEAST_GIO_2_16  */
        newuri = gnome_vfs_uri_append_file_name(baseuri, filename);
        res = gnome_vfs_create_uri(&handle,newuri,GNOME_VFS_OPEN_WRITE,TRUE,0644);
        if (res == GNOME_VFS_ERROR_FILE_EXISTS) {
          counter++;
          filename = g_strdup_printf("%s%d",_("New file"),counter);
        } else if (res == GNOME_VFS_OK) {
          res = gnome_vfs_close(handle);
          done = TRUE;
        } else {
          counter = 100;
        }
#endif /* else HAVE_ATLEAST_GIO_2_16 */
      }
      if (done) {
        rename_not_open_file(fb2->bfwin, newuri);
      }
    }
    if (done) {
      fb2_refresh_parent_of_uri(newuri);
    } else {
#ifdef HAVE_ATLEAST_GIO_2_16
      DEBUG_MSG("fb2rpopup_new, failed creation.. \n");
#else
      DEBUG_MSG("fb2rpopup_new, failed creation.. (res=%d, %s)\n",res,gnome_vfs_result_to_string(res));
#endif
    }
#ifdef HAVE_ATLEAST_GIO_2_16
    g_object_unref (newuri);
    g_object_unref (baseuri);
#else
    gnome_vfs_uri_unref(newuri);
    gnome_vfs_uri_unref(baseuri);
#endif
  } else {
    DEBUG_MSG("not creating new file => no baseuri, perhaps we should use the basedir ??\n");
  }
}

static void fb2rpopup_rename(Tfilebrowser2 *fb2) {
  GnomeVFSURI *olduri;
  if (fb2->last_popup_on_dir) {
    olduri = fb2_uri_from_dir_selection(fb2);
  } else {
    olduri = fb2_uri_from_file_selection(fb2);
  }
  if (olduri) {
    Tdocument *tmpdoc;
    GList *alldocs;
    gnome_vfs_uri_ref(olduri);
    
    /* Use doc_save(doc, 1, 1) if the file is open. */
    
    alldocs = return_allwindows_documentlist();
    tmpdoc = documentlist_return_document_from_uri(alldocs,olduri);
    g_list_free(alldocs);
    if (tmpdoc != NULL) {
      DEBUG_MSG("fb2rpopup_rename, file is open. Calling doc_save() with 'do_move'.\n");
      /* If an error occurs, doc_save takes care of notifying the user.
       * Currently, nothing is done here. */  
      doc_save_backend(tmpdoc, TRUE, TRUE, FALSE, FALSE);
    } else { /* olduri is not open */
      rename_not_open_file(fb2->bfwin, olduri);
    }
    gnome_vfs_uri_unref(olduri);
  }
}

static void rcpopup_async_delete_lcb(gpointer data) {
  GnomeVFSURI *uri = data;
  if (uri) {
    GList *alldocs;
    Tdocument *exdoc;
    fb2_refresh_parent_of_uri(uri);
    
    alldocs = return_allwindows_documentlist();
    exdoc = documentlist_return_document_from_uri(alldocs, uri);
    if (exdoc) {
      document_unset_filename(exdoc);
    }
    g_list_free(alldocs);
    gnome_vfs_uri_unref(uri);
  }
}

static void fb2rpopup_delete(Tfilebrowser2 *fb2) {
  GnomeVFSURI *uri;
  
  if (fb2->last_popup_on_dir) {
    uri = fb2_uri_from_dir_selection(fb2);
  } else {
    uri = fb2_uri_from_file_selection(fb2);
  }
  if (uri) {
    const gchar *buttons[] = {GTK_STOCK_CANCEL, GTK_STOCK_DELETE, NULL};
    gchar *text,*text2;
    gint retval;
    gchar *fullpath, *filename;
    fullpath = full_path_utf8_from_uri(uri);
    filename = filename_utf8_from_uri(uri);
    /* Do we really need to display the full path here?
     *  
     * Having the filename in the both the primary and secondary text seems to be redundant.
     * Set back to just the primary text for now.
     *
     * --> Sometimes you can have a file in many directories (e.g. Makefile.in, index.html)
     * and then this might give you more indication if this is indeed the file you wanted to delete
     */
    text = g_strdup_printf (_("Are you sure you want to delete\n\"%s\"?"), filename);
    text2 = g_strdup_printf (_("If you delete %s, it will be permanently lost."), fullpath);
    retval = message_dialog_new_multi(fb2->bfwin->main_window,
          GTK_MESSAGE_QUESTION,
          buttons,
          text,
          text2);
    g_free(text);
    g_free(text2);
    if (retval == 1) {
      /* ref the uri, it is unreffed by the callback */
      gnome_vfs_uri_ref(uri);
      file_delete_file_async(uri, rcpopup_async_delete_lcb, uri);
    }
    g_free(filename);
    g_free(fullpath);
  }
}

static void fb2_refilter(Tfilebrowser2 *fb2) {
  gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(fb2->dir_tfilter));
  if (fb2->filebrowser_viewmode == viewmode_dual) {
    gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(fb2->file_lfilter));
  }
}


static void fb2rpopup_rpopup_action_lcb(Tfilebrowser2 *fb2,guint callback_action, GtkWidget *widget) {
  DEBUG_MSG("fb2rpopup_rpopup_action_lcb, called with action %d and widget %p, fb2=%p\n",callback_action,widget,fb2);
  switch (callback_action) {
    case 1:
      {
        GnomeVFSURI *uri = fb2_uri_from_file_selection(fb2);
        if (uri) handle_activate_on_file(fb2, uri);
      }
    break;
    case 2:
      fb2rpopup_rename(fb2);
    break;
    case 3:
      fb2rpopup_delete(fb2);
    break;
    case 4:
      /* BUG: in the case of a click in a dual-view fileview, we should not use the basedir
      as third argument */
      fb2rpopup_new(fb2, FALSE, fb2->basedir);
    break;
    case 5:
      /* BUG: in the case of a click in a dual-view fileview, we should not use the basedir
      as third argument */

      fb2rpopup_new(fb2, TRUE, fb2->basedir);
    break;
    case 6:
      fb2rpopup_refresh(fb2);
    break;
    case 7:
      {
        gchar *curi;
        GnomeVFSURI *uri = fb2_uri_from_dir_selection(fb2);
#ifdef HAVE_ATLEAST_GIO_2_16
        curi = g_file_get_uri(uri);
#else /* no HAVE_ATLEAST_GIO_2_16  */
        curi = gnome_vfs_uri_to_string(uri,0);
#endif /* else HAVE_ATLEAST_GIO_2_16 */
        files_advanced_win(fb2->bfwin, curi);
        g_free(curi);
      }
    break;
    case 8:
      {
        GtkTreePath *fs_path;
        fs_path = fb2_fspath_from_dir_selection(fb2);
        DEBUG_MSG("fb2rpopup_rpopup_action_lcb, fs_path=%p\n",fs_path);
        refilter_dirlist(fb2, fs_path);
        gtk_tree_path_free(fs_path);
        fb2_focus_document(fb2->bfwin, fb2->bfwin->current_document);
      }
    break;
    case 9:
      refilter_dirlist(fb2, NULL);
      fb2_focus_document(fb2->bfwin, fb2->bfwin->current_document);
    break;
    case 10:
      {
        GnomeVFSURI *uri = fb2_uri_from_dir_selection(fb2);
        set_documentroot_dialog(fb2->bfwin, uri);
      }
    break;
    case 15:
      fb2->bfwin->session->filebrowser_focus_follow = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
      if (fb2->bfwin->session->filebrowser_focus_follow) {
        fb2_focus_document(fb2->bfwin, fb2->bfwin->current_document);
      }
    break;
    case 16:
      fb2->filebrowser_show_hidden_files = fb2->bfwin->session->filebrowser_show_hidden_files = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
      fb2_refilter(fb2);
    break;
    case 17:
      fb2->filebrowser_show_backup_files = fb2->bfwin->session->filebrowser_show_backup_files = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
      fb2_refilter(fb2);
    break;
    case 20:
      BFWIN(fb2->bfwin)->session->filebrowser_viewmode = viewmode_tree;
      fb2_set_viewmode_widgets(fb2, viewmode_tree);
    break;
    case 21:
      BFWIN(fb2->bfwin)->session->filebrowser_viewmode = viewmode_dual;
      fb2_set_viewmode_widgets(fb2, viewmode_dual);
    break;
    case 22:
      BFWIN(fb2->bfwin)->session->filebrowser_viewmode = viewmode_flat;
      fb2_set_viewmode_widgets(fb2, viewmode_flat);
    break;
    
  }
}

static void fb2rpopup_filter_toggled_lcb(GtkWidget *widget, Tfilebrowser2 *fb2) {
  if (GTK_CHECK_MENU_ITEM(widget)->active) {
    /* loop trough the filters for a filter with this name */
    const gchar *name = gtk_label_get_text(GTK_LABEL(GTK_BIN(widget)->child));
    Tfilter *filter = find_filter_by_name(name);
    DEBUG_MSG("fb2rpopup_filter_toggled_lcb, setting curfilter to %p from name %s\n", filter, name);
    fb2->curfilter = filter;
    if (filter) {
      if (fb2->bfwin->session->last_filefilter) g_free(fb2->bfwin->session->last_filefilter);
      fb2->bfwin->session->last_filefilter = g_strdup(filter->name);
    }
    fb2_refilter(fb2);
  }
}

static GtkItemFactoryEntry fb2rpopup_menu_entries[] = {
  { N_("/_Open"),   NULL, fb2rpopup_rpopup_action_lcb,    1,  "<Item>" },
  { N_("/Open _Advanced..."),NULL,  fb2rpopup_rpopup_action_lcb,  7,  "<Item>" },
  { N_("/Rena_me"),   NULL, fb2rpopup_rpopup_action_lcb,    2,  "<Item>" },
  { N_("/_Delete"),   NULL, fb2rpopup_rpopup_action_lcb,    3,  "<Item>" },
  { "/sep1",            NULL, NULL,                 0,  "<Separator>" },
  { N_("/New _File"),     NULL, fb2rpopup_rpopup_action_lcb,  4,  "<Item>" },
  { N_("/_New Directory"),  NULL, fb2rpopup_rpopup_action_lcb,  5,  "<Item>" },
  { "/sep2",            NULL, NULL,                 0,  "<Separator>" },
  { N_("/_Refresh"),      NULL, fb2rpopup_rpopup_action_lcb,  6,  "<Item>" },
  { N_("/Follow active document"),  NULL, fb2rpopup_rpopup_action_lcb,  15, "<ToggleItem>" },
  { N_("/_Set as basedir"), NULL, fb2rpopup_rpopup_action_lcb,  8,  "<Item>" },
  { N_("/Set as documentroot"), NULL, fb2rpopup_rpopup_action_lcb,  10, "<Item>" },
  { "/sep3",            NULL, NULL,                 0,  "<Separator>" },
  { N_("/Show Full _Tree"), NULL, fb2rpopup_rpopup_action_lcb,  9,  "<Item>" },
  { N_("/Show hidden files"), NULL, fb2rpopup_rpopup_action_lcb,  16, "<ToggleItem>" },
  { N_("/Show backup files"), NULL, fb2rpopup_rpopup_action_lcb,  17, "<ToggleItem>" },
  { N_("/View mode"), NULL, NULL, 0,  "<Branch>" },
  { N_("/View mode/Tree"),  NULL, fb2rpopup_rpopup_action_lcb,  20, "<RadioItem>" },
  { N_("/View mode/Dual"),  NULL, fb2rpopup_rpopup_action_lcb,  21, "/View mode/Tree" },
  { N_("/View mode/Flat"),  NULL, fb2rpopup_rpopup_action_lcb,  22, "/View mode/Tree" }
};

static void edit_filefilter_lcb(GtkMenuItem *menuitem,gpointer data) {
  Tfilebrowser2 *fb2 = data;
  filefilter_gui(fb2->curfilter);
}

static void new_filefilter_lcb(GtkMenuItem *menuitem,gpointer data) {
  filefilter_gui(NULL);
}
static void delete_filefilter_lcb(GtkMenuItem *menuitem,gpointer data) {
  Tfilebrowser2 *fb2 = data;
  filter_delete(fb2->curfilter);
}


/* 
 * what should the menu look like?
 *
 * /Open              (if a file is selected)
 * /Open advanced     (if a dir is selected)
 * /Rename            (if file or dir is selected)
 * /Delete            (if file or dir is selected)
 * /-----
 * /New file
 * /New directory
 * /-----
 * /Refresh
 * /Show full tree
 * /Set as basedir    (if dir is selected)
 *
 * 
 */
static GtkWidget *fb2_rpopup_create_menu(Tfilebrowser2 *fb2, gboolean is_directory, gboolean is_file) {
  GtkWidget *menu, *menu_item;
  GtkItemFactory *menumaker;

  fb2->last_popup_on_dir = is_directory;
  menumaker = gtk_item_factory_new(GTK_TYPE_MENU, "<Filebrowser>", NULL);
#ifdef ENABLE_NLS
  gtk_item_factory_set_translate_func(menumaker,menu_translate,"<Filebrowser>",NULL);
#endif
  gtk_item_factory_create_items(menumaker, sizeof(fb2rpopup_menu_entries)/sizeof(GtkItemFactoryEntry), fb2rpopup_menu_entries, fb2);
  menu = gtk_item_factory_get_widget(menumaker, "<Filebrowser>");

  /* set toggle options */
  setup_toggle_item(menumaker, "/Follow active document", fb2->bfwin->session->filebrowser_focus_follow);
  setup_toggle_item(menumaker, "/Show hidden files", fb2->filebrowser_show_hidden_files);
  setup_toggle_item(menumaker, "/Show backup files", fb2->filebrowser_show_backup_files);
  if (!is_directory && !is_file) {
    gtk_widget_set_sensitive(gtk_item_factory_get_widget(menumaker, "/Rename"), FALSE);
    gtk_widget_set_sensitive(gtk_item_factory_get_widget(menumaker, "/Delete"), FALSE);
  }
  if (!is_directory) {
    gtk_widget_set_sensitive(gtk_item_factory_get_widget(menumaker, "/Open Advanced..."), FALSE);
    gtk_widget_set_sensitive(gtk_item_factory_get_widget(menumaker, "/Set as basedir"), FALSE);
  } 
  if (!is_file) {
    gtk_widget_set_sensitive(gtk_item_factory_get_widget(menumaker, "/Open"), FALSE);
  }
  if (fb2->basedir == NULL || fb2->filebrowser_viewmode == viewmode_flat) {
    gtk_widget_set_sensitive(gtk_item_factory_get_widget(menumaker, "/Show Full Tree"), FALSE);
  }
  setup_toggle_item(menumaker, "/View mode/Tree", (fb2->filebrowser_viewmode == viewmode_tree));
  setup_toggle_item(menumaker, "/View mode/Dual", (fb2->filebrowser_viewmode == viewmode_dual));
  setup_toggle_item(menumaker, "/View mode/Flat", (fb2->filebrowser_viewmode == viewmode_flat));

  /* Add filter submenu */
  menu_item = gtk_menu_item_new_with_label(_("Filter"));
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
  {
    GtkWidget *fmenu;
    GSList *group=NULL;
    GList *tmplist;
    fmenu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), fmenu);
    tmplist = g_list_last(main_v->filefilters);
    while (tmplist) {
      Tfilter *filter = (Tfilter *)tmplist->data;
      menu_item = gtk_radio_menu_item_new_with_label(group, filter->name);
      if (fb2->curfilter == filter) {
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(menu_item), TRUE);
      }
      g_signal_connect(GTK_OBJECT(menu_item), "toggled", G_CALLBACK(fb2rpopup_filter_toggled_lcb), fb2);
      if (!group) {
        group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(menu_item));
      }
      gtk_menu_shell_append(GTK_MENU_SHELL(fmenu), menu_item);
      tmplist = g_list_previous(tmplist);
    }
    menu_item = gtk_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(fmenu), menu_item);
    if (fb2->curfilter) {
      menu_item = gtk_menu_item_new_with_label(_("Edit filter"));
      g_signal_connect(GTK_OBJECT(menu_item), "activate", G_CALLBACK(edit_filefilter_lcb), fb2);
      gtk_menu_shell_append(GTK_MENU_SHELL(fmenu), menu_item);
      menu_item = gtk_menu_item_new_with_label(_("Delete filter"));
      g_signal_connect(GTK_OBJECT(menu_item), "activate", G_CALLBACK(delete_filefilter_lcb), fb2);
      gtk_menu_shell_append(GTK_MENU_SHELL(fmenu), menu_item);
    }
    menu_item = gtk_menu_item_new_with_label(_("New filter"));
    g_signal_connect(GTK_OBJECT(menu_item), "activate", G_CALLBACK(new_filefilter_lcb), fb2);
    gtk_menu_shell_append(GTK_MENU_SHELL(fmenu), menu_item);
  }
  
  
  gtk_widget_show_all(menu);
  g_signal_connect_after(G_OBJECT(menu), "destroy", G_CALLBACK(destroy_disposable_menu_cb), menu);
  return menu;
}

static void dir_v_row_expanded_lcb(GtkTreeView *tree,GtkTreeIter *sort_iter,GtkTreePath *sort_path,Tfilebrowser2 *fb2) {
  GnomeVFSURI *uri;
  DEBUG_MSG("dir_v_row_expanded_lcb, called for fb2=%p with sort_path=\n",fb2);
  /* refresh the directory that is being expanded */
  gtk_tree_model_get(fb2->dir_tsort, sort_iter, URI_COLUMN, &uri, -1);
  if (uri) {
    fb2_refresh_dir(uri, NULL);
  }
}

static gboolean dir_v_button_press_lcb(GtkWidget *widget, GdkEventButton *event, Tfilebrowser2 *fb2) {
  DEBUG_MSG("dir_v_button_press_lcb, called for fb2=%p\n",fb2);
  if (event->button == 3) {
    GtkWidget *menu = NULL;
    GtkTreePath *path;
    gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(fb2->dir_v), event->x, event->y, &path, NULL, NULL, NULL);
    if (path) {
      gboolean is_dir = fb2_isdir_from_dir_sort_path(fb2, path);
      menu = fb2_rpopup_create_menu(fb2, is_dir, !is_dir);
      gtk_tree_path_free(path);
    } else {
      menu = fb2_rpopup_create_menu(fb2, FALSE, FALSE);
    }
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);   
  } else if (!(fb2->filebrowser_viewmode == viewmode_dual) && event->button==1 && event->type == GDK_2BUTTON_PRESS) {
    GtkTreePath *path;
    gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(fb2->dir_v), event->x, event->y, &path, NULL, NULL, NULL);
    if (path && !fb2_isdir_from_dir_sort_path(fb2, path)) {
      GnomeVFSURI *uri;
      uri = fb2_uri_from_dir_sort_path(fb2,path);
      if (uri) {
#ifdef DEBUG
#ifdef HAVE_ATLEAST_GIO_2_16
        gchar *basename = g_file_get_basename (uri);
        DEBUG_MSG("file_v_button_press_lcb, doucleclick on %s\n", basename);
        g_free (basename);
#else
        DEBUG_MSG("file_v_button_press_lcb, doucleclick on %s\n", gnome_vfs_uri_extract_short_name(uri));
#endif
#endif
        handle_activate_on_file(fb2, uri);
      }
    }
  }
  return FALSE; /* pass the event on */
}

static gboolean file_v_button_press_lcb(GtkWidget *widget, GdkEventButton *event, Tfilebrowser2 *fb2) {
  DEBUG_MSG("file_v_button_press_lcb, called for fb2=%p\n",fb2);
  if (event->button == 3) {
    GtkWidget *menu = NULL;
    GtkTreePath *path;
    gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(fb2->file_v), event->x, event->y, &path, NULL, NULL, NULL);
    if (path) {
      menu = fb2_rpopup_create_menu(fb2, FALSE, TRUE);
      gtk_tree_path_free(path);
    } else {
      menu = fb2_rpopup_create_menu(fb2, FALSE, FALSE);
    }
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);
  } else if (event->button==1 && event->type == GDK_2BUTTON_PRESS) {
    GtkTreePath *sort_path;
    gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(fb2->file_v), event->x, event->y, &sort_path, NULL, NULL, NULL);
    if (sort_path) {
      GnomeVFSURI *uri;
      uri = fb2_uri_from_file_sort_path(fb2,sort_path);
      if (uri) {
#ifdef DEBUG
#ifdef HAVE_ATLEAST_GIO_2_16
        gchar *basename = g_file_get_basename (uri);
        DEBUG_MSG("file_v_button_press_lcb, doucleclick on %s\n", basename);
        g_free (basename);
#else
        DEBUG_MSG("file_v_button_press_lcb, doucleclick on %s\n", gnome_vfs_uri_extract_short_name(uri));
#endif
#endif
        handle_activate_on_file(fb2, uri);
      }
#ifdef DEBUG
      else {
        DEBUG_MSG("file_v_button_press_lcb, WARNING, NO URI FOUND, cannot activate NULL\n");
      }
#endif      
      gtk_tree_path_free(sort_path);
    }
  }
  return FALSE; /* pass the event on */
}

static gboolean dirmenu_idle_cleanup_lcb(gpointer callback_data) {
  GtkTreeModel *oldmodel = callback_data;
  gboolean cont;
  GtkTreeIter iter;
  DEBUG_MSG("dirmenu_idle_cleanup_lcb, cleanup the old model\n");
  /* now we cleanup the old model and it's contents */

  cont = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(oldmodel), &iter);
  while (cont) {
    GnomeVFSURI *uri;
    gtk_tree_model_get(GTK_TREE_MODEL(oldmodel), &iter, DIR_URI_COLUMN, &uri, -1);
    gnome_vfs_uri_unref(uri);
    /* hmm if this last remove results in an empty listtore there is a crash?? */
    cont = gtk_list_store_remove(GTK_LIST_STORE(oldmodel),&iter);
  }
  DEBUG_MSG("dirmenu_idle_cleanup_lcb, unref the old model\n");
  g_object_unref(oldmodel);
  return FALSE;
}

static void dirmenu_set_curdir(Tfilebrowser2 *fb2, GnomeVFSURI *newcurdir) {
  GtkTreeIter iter, setiter;
  GList *tmplist;
  GtkTreeModel *oldmodel = fb2->dirmenu_m;
  GnomeVFSURI *tmp;
  gboolean cont, havesetiter=FALSE;
  if (fb2->currentdir) {
    if (newcurdir && (fb2->currentdir == newcurdir || gnome_vfs_uri_equal(fb2->currentdir, newcurdir))) return;
#ifdef DEBUG
#ifdef HAVE_ATLEAST_GIO_2_16
        gchar *path = g_file_get_path (fb2->currentdir);
        gchar *path2 = g_file_get_path (newcurdir);
        DEBUG_MSG("dirmenu_set_curdir, old_curdir=%s, new_curdir=%s\n", path, path2);
        g_free (path);
        g_free (path2);
#else
        DEBUG_MSG("dirmenu_set_curdir, old_curdir=%s, new_curdir=%s\n",gnome_vfs_uri_get_path(fb2->currentdir),gnome_vfs_uri_get_path(newcurdir));
#endif
#endif    
    gnome_vfs_uri_unref(fb2->currentdir);
  }
  fb2->currentdir = newcurdir;
  gnome_vfs_uri_ref(fb2->currentdir);
#ifdef DEBUG
#ifdef HAVE_ATLEAST_GIO_2_16
        gchar *path = g_file_get_path (fb2->currentdir);
        DEBUG_MSG("dirmenu_set_curdir, new curdir %s\n", path);
        g_free (path);
#else
        DEBUG_MSG("dirmenu_set_curdir, new curdir %s\n",gnome_vfs_uri_get_path(fb2->currentdir));
#endif
#endif
  fb2->dirmenu_m = GTK_TREE_MODEL(gtk_list_store_new(2,G_TYPE_STRING,G_TYPE_POINTER));
  tmplist = g_list_first(fb2->bfwin->session->recent_dirs);
  while (tmplist) {
    GnomeVFSURI *uri;
    gchar *name;
#ifdef HAVE_ATLEAST_GIO_2_16
    uri = g_file_new_for_uri(tmplist->data);
#else /* no HAVE_ATLEAST_GIO_2_16  */
    uri = gnome_vfs_uri_new(tmplist->data);
#endif /* else HAVE_ATLEAST_GIO_2_16 */
    if (uri) {
      name = full_path_utf8_from_uri(uri);
      gtk_list_store_append(GTK_LIST_STORE(fb2->dirmenu_m),&iter);
      gtk_list_store_set(GTK_LIST_STORE(fb2->dirmenu_m),&iter
          ,DIR_NAME_COLUMN,name
          ,DIR_URI_COLUMN,uri
          ,-1);
      g_free(name);
    }
    tmplist = g_list_next(tmplist);
  }
  
  /* then we rebuild the current uri */
  tmp = gnome_vfs_uri_dup(newcurdir);
  do {
    gchar *name = full_path_utf8_from_uri(tmp);
    DEBUG_MSG("dirmenu_set_curdir, appending %s to the new model\n",name);
    gtk_list_store_append(GTK_LIST_STORE(fb2->dirmenu_m),&iter);
    if (!havesetiter) {
      setiter = iter;
      havesetiter = TRUE;
    }
    gtk_list_store_set(GTK_LIST_STORE(fb2->dirmenu_m),&iter
          ,DIR_NAME_COLUMN,name
          ,DIR_URI_COLUMN,tmp /* don't unref tmp at this point, because there is a reference from the model */
          ,-1);
#ifdef HAVE_ATLEAST_GIO_2_16
    tmp = g_file_get_parent(tmp);
    cont = (tmp!=NULL);
#else /* no HAVE_ATLEAST_GIO_2_16  */
    cont = gnome_vfs_uri_has_parent(tmp);
    if (cont) {
      tmp = gnome_vfs_uri_get_parent(tmp);
    }
#endif /* else HAVE_ATLEAST_GIO_2_16 */
  } while (cont);
  DEBUG_MSG("dirmenu_set_curdir, activate the new model\n");
  g_signal_handler_block(fb2->dirmenu_v, fb2->dirmenu_changed_signal);
  gtk_combo_box_set_model(GTK_COMBO_BOX(fb2->dirmenu_v),GTK_TREE_MODEL(fb2->dirmenu_m));
  if (havesetiter) gtk_combo_box_set_active_iter(GTK_COMBO_BOX(fb2->dirmenu_v),&setiter);
  g_signal_handler_unblock(fb2->dirmenu_v, fb2->dirmenu_changed_signal);
  DEBUG_MSG("dirmenu_set_curdir, activated!\n");
  g_idle_add(dirmenu_idle_cleanup_lcb, oldmodel);
}

static void dir_v_row_activated_lcb(GtkTreeView *tree, GtkTreePath *path,GtkTreeViewColumn *column, Tfilebrowser2 *fb2) {
  if (fb2->filebrowser_viewmode == viewmode_flat) {
    GnomeVFSURI *uri = fb2_uri_from_dir_sort_path(fb2,path);/* this is a pointer to the uri stored in the treemodel */
    fb2_set_basedir_backend(fb2, uri);
    dirmenu_set_curdir(fb2, uri);
  }
}

static void dir_v_selection_changed_lcb(GtkTreeSelection *treeselection,Tfilebrowser2 *fb2) {
  GtkTreeModel *sort_model = NULL;
  GtkTreeIter sort_iter;
  /* Get the current selected row and the model. */
  DEBUG_MSG("dir_v_selection_changed_lcb, treeselection=%p, fb2=%p\n",treeselection,fb2);
  if (treeselection && gtk_tree_selection_get_selected(treeselection,&sort_model,&sort_iter)) {
    GnomeVFSURI *uri;
    gchar * mime_type;
    gtk_tree_model_get(sort_model, &sort_iter, URI_COLUMN, &uri,TYPE_COLUMN, &mime_type, -1);
    DEBUG_MSG("dir_v_selection_changed_lcb, mime_type=%s and uri=",mime_type);
    DEBUG_URI(uri,TRUE);
    if (uri && (mime_type && strncmp(mime_type, "x-directory", 11) == 0)) {
      dirmenu_set_curdir(fb2, uri);
      fb2_focus_dir(fb2, uri, TRUE);
    }
    g_free (mime_type);
  } else {
    DEBUG_MSG("dir_v_selection_changed_lcb, could not get any selection, returning..\n");
  }
}

/* use NULL to unset the basedir */
static void fb2_set_basedir_backend(Tfilebrowser2 *fb2, GnomeVFSURI *uri) {
  GtkTreePath *basepath=NULL;
  
  if (uri && fb2->basedir && (fb2->basedir == uri || gnome_vfs_uri_equal(fb2->basedir, uri))) {
    DEBUG_MSG("fb2_set_basedir_backend, basedir did not change ?!?\n");
    return;
  }
  /* disconnect the dir_v and file_v for higher performance */
  gtk_tree_view_set_model(GTK_TREE_VIEW(fb2->dir_v),NULL);
  if (fb2->filebrowser_viewmode == viewmode_dual) gtk_tree_view_set_model(GTK_TREE_VIEW(fb2->file_v),NULL);
  fb2->file_lfilter = NULL; /* this is required, because the refilter_filelist function tries to check if the virtual root did change for the file filter */
  DEBUG_MSG("fb2_set_basedir_backend, disconnected current filter/sort models, lfilter=%p\n",fb2->file_lfilter);
  
  basepath = treepath_for_uri(fb2, uri);

  DEBUG_MSG("fb2_set_basedir_backend, refilter, basepath=%p\n",basepath);
  refilter_dirlist(fb2, basepath);
  if (basepath) {
    fb2_focus_dir(fb2, fb2->basedir, (fb2->filebrowser_viewmode == viewmode_flat));
    gtk_tree_path_free(basepath);
  }
}

/**
 * fb2_set_basedir:
 * bfwin: Tbfwin *
 * curi: gchar * or NULL
 *
 *
 *
 */
void fb2_set_basedir(Tbfwin *bfwin, gchar *curi) {
  if (bfwin->fb2) {
    Tfilebrowser2 *fb2 = bfwin->fb2;
    if (curi) {
      GnomeVFSURI *uri;
#ifdef HAVE_ATLEAST_GIO_2_16
      uri = g_file_new_for_uri(strip_trailing_slash(curi));
#else /* no HAVE_ATLEAST_GIO_2_16  */
      uri = gnome_vfs_uri_new(strip_trailing_slash(curi));
#endif /* else HAVE_ATLEAST_GIO_2_16 */
      if (uri) {
        fb2_set_basedir_backend(fb2, uri);
        gnome_vfs_uri_unref(uri);
      } else {
        DEBUG_MSG("fb2_set_basedir, failed to convert to GnomeVFSURI!!!!!!!\n");
      }
    } else {
      DEBUG_MSG("fb2_set_basedir, curi=NULL\n");
      refilter_dirlist(fb2, NULL);
    }
  }
}

static void dirmenu_changed_lcb(GtkComboBox *widget,gpointer data) {
  Tfilebrowser2 *fb2 = data;
  GtkTreeIter iter;
  DEBUG_MSG("dirmenu_changed_lcb, started\n");
  if (gtk_combo_box_get_active_iter(widget,&iter)) {
    GnomeVFSURI *uri;
    DEBUG_MSG("dirmenu_changed_lcb. we have an active iter\n");
    gtk_tree_model_get(GTK_TREE_MODEL(fb2->dirmenu_m), &iter, DIR_URI_COLUMN, &uri, -1);
#ifdef DEBUG
#ifdef HAVE_ATLEAST_GIO_2_16
        gchar *path = g_file_get_path (uri);
        DEBUG_MSG("dirmenu_changed_lcb. active iter has url %s\n", path);
        g_free (path);
#else
        DEBUG_MSG("dirmenu_changed_lcb. active iter has url %s\n",gnome_vfs_uri_get_path(uri));
#endif
#endif
    gnome_vfs_uri_ref(uri);
    g_signal_handler_block(fb2->dirmenu_v, fb2->dirmenu_changed_signal);
    if (fb2->basedir || fb2->filebrowser_viewmode == viewmode_flat) {
      if (fb2->filebrowser_viewmode != viewmode_flat && (gnome_vfs_uri_is_parent(fb2->basedir,uri,TRUE) || gnome_vfs_uri_equal(fb2->basedir,uri))) {
#ifdef DEBUG
#ifdef HAVE_ATLEAST_GIO_2_16
        gchar *path = g_file_get_path (fb2->basedir);
        DEBUG_MSG("dirmenu_changed_lcb, uri within basedir %s, call fb2_focus_dir\n", path);
        g_free (path);
#else
        DEBUG_MSG("dirmenu_changed_lcb, uri within basedir %s, call fb2_focus_dir\n",gnome_vfs_uri_get_path(fb2->basedir));
#endif
#endif
        fb2_focus_dir(FILEBROWSER2(fb2), uri, FALSE);
      } else {
        /* drop basedir or set as new basedir, for now we set it as new basedir */
#ifdef DEBUG
#ifdef HAVE_ATLEAST_GIO_2_16
        gchar *path = g_file_get_path (fb2->basedir);
        DEBUG_MSG("dirmenu_changed_lcb, uri NOT within basedir %s, call fb2_set_basedir_backend\n", path);
        g_free (path);
#else
        DEBUG_MSG("dirmenu_changed_lcb, uri NOT within basedir %s, call fb2_set_basedir_backend\n",gnome_vfs_uri_get_path(fb2->basedir));
#endif
#endif
        fb2_set_basedir_backend(fb2, uri);
      }
    } else { /* no basedir, we can focus it */
      DEBUG_MSG("dirmenu_changed_lcb, no basedir, call fb2_focus_dir\n");
      fb2_focus_dir(FILEBROWSER2(fb2), uri, FALSE);
    }
    gnome_vfs_uri_unref(uri);
    g_signal_handler_unblock(fb2->dirmenu_v, fb2->dirmenu_changed_signal);
  }
}
enum {
  TARGET_URI_LIST,
  TARGET_STRING
};

static void fb2_file_v_drag_data_received(GtkWidget * widget, GdkDragContext * context
      , gint x, gint y, GtkSelectionData * data
      , guint info, guint time, Tfilebrowser2 *fb2) {

  gchar *stringdata;
  GnomeVFSURI *destdir=fb2->currentdir;
  gnome_vfs_uri_ref(destdir);
  
  g_signal_stop_emission_by_name(widget, "drag_data_received");
  if ((data->length == 0) || (data->format != 8) || ((info != TARGET_STRING) && (info != TARGET_URI_LIST))) {
    gtk_drag_finish(context, FALSE, TRUE, time);
    return;
  }
  stringdata = g_strndup((gchar *)data->data, data->length);
  DEBUG_MSG("fb2_file_v_drag_data_received, stringdata='%s', len=%d\n",stringdata,data->length);
  if (destdir) {
    if (strchr(stringdata, '\n') == NULL) { /* no newlines, probably a single file */
#ifdef HAVE_ATLEAST_GIO_2_16
      GSList *list=NULL;
      GFile *uri;
      uri = g_file_new_for_commandline_arg(stringdata);
      list = g_slist_append(list,uri);
      copy_uris_async(fb2->bfwin, destdir, list);
      g_slist_free(list);
      g_object_unref(uri);
#else /* no HAVE_ATLEAST_GIO_2_16  */
      gchar *curi;
      curi = gnome_vfs_make_uri_from_input(stringdata);
      copy_files_async(fb2->bfwin, destdir, curi);
      g_free(curi);
#endif /* else HAVE_ATLEAST_GIO_2_16 */
    } else { /* there are newlines, probably this is a list of uri's */
      copy_files_async(fb2->bfwin, destdir, stringdata);
    }
    gnome_vfs_uri_unref(destdir);
    gtk_drag_finish(context, TRUE, TRUE, time);
  } else {
    gtk_drag_finish(context, FALSE, TRUE, time);
  }
  g_free(stringdata);
}

static void fb2_dir_v_drag_data_received(GtkWidget * widget, GdkDragContext * context
      , gint x, gint y, GtkSelectionData * data
      , guint info, guint time, Tfilebrowser2 *fb2) {
  gchar *stringdata;
  GnomeVFSURI *destdir=NULL;
  GtkTreePath *path;
  /* if we don't do this, we get this text: Gtk-WARNING **: You must override the default 
  'drag_data_received' handler on GtkTreeView when using models that don't support the 
  GtkTreeDragDest interface and enabling drag-and-drop. The simplest way to do this is to 
  connect to 'drag_data_received' and call g_signal_stop_emission_by_name() in your signal 
  handler to prevent the default handler from running. Look at the source code for the 
  default handler in gtktreeview.c to get an idea what your handler should do.  */
  g_signal_stop_emission_by_name(widget, "drag_data_received");

  if ((data->length == 0) || (data->format != 8) || ((info != TARGET_STRING) && (info != TARGET_URI_LIST))) {
    gtk_drag_finish(context, FALSE, TRUE, time);
    return;
  }
  stringdata = g_strndup((gchar *)data->data, data->length);
  DEBUG_MSG("fb2_dir_v_drag_data_received, stringdata='%s', len=%d\n",stringdata,data->length);
  /* first find the destination directory */
  gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(fb2->dir_v), x, y, &path, NULL, NULL, NULL);
  if (path) {
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter(fb2->dir_tsort,&iter,path)) {
      GnomeVFSURI *uri;
      gchar * mime_type;
      gtk_tree_model_get(fb2->dir_tsort, &iter, TYPE_COLUMN, &mime_type, URI_COLUMN, &uri, -1);
      if (mime_type && strncmp(mime_type, "x-directory", 11) == 0) {
        destdir = uri;
        gnome_vfs_uri_ref(destdir);
      } else {
        destdir = gnome_vfs_uri_get_parent(uri);
      }
      g_free (mime_type);
    }
  }
  if (destdir) {
    if (strchr(stringdata, '\n') == NULL) { /* no newlines, probably a single file */
#ifdef HAVE_ATLEAST_GIO_2_16
      GSList *list=NULL;
      GFile *uri;
      uri = g_file_new_for_commandline_arg(stringdata);
      list = g_slist_append(list,uri);
      copy_uris_async(fb2->bfwin, destdir, list);
      g_slist_free(list);
      g_object_unref(uri);
#else /* no HAVE_ATLEAST_GIO_2_16  */
      gchar *curi;
      curi = gnome_vfs_make_uri_from_input(stringdata);
      copy_files_async(fb2->bfwin, destdir, curi);
      g_free(curi);
#endif
    } else { /* there are newlines, probably this is a list of uri's */
      copy_files_async(fb2->bfwin, destdir, stringdata);
    }
    gnome_vfs_uri_unref(destdir);
    gtk_drag_finish(context, TRUE, TRUE, time);
  } else {
    gtk_drag_finish(context, FALSE, TRUE, time);
  }
  g_free(stringdata);
}

static void fb2_two_pane_notify_position_lcb(GObject *object,GParamSpec *pspec,gpointer data){
  gint position;
  g_object_get(object, pspec->name, &position, NULL);
  if (main_v->props.restore_dimensions) {
    main_v->globses.two_pane_filebrowser_height = position;
  }
}

static void fb2_set_viewmode_widgets(Tfilebrowser2 *fb2, gint viewmode) {
  GtkTreeViewColumn *column;
  GtkTreeSelection* dirselection;
  GtkWidget *scrolwin;
  GtkCellRenderer *renderer;
  GtkTreePath *basepath=NULL;

  const GtkTargetEntry drag_dest_types[] = {
    {"text/uri-list", 0, TARGET_URI_LIST },
    {"STRING", 0, TARGET_STRING},
  };

  DEBUG_MSG("fb2_set_viewmode_widgets, destroying old widgets (if any)\n");
  if (fb2->filebrowser_viewmode == viewmode) {
    /* the current viewmode is the same as the requested viewmode
    we can return if all the widgets exist */
    if (fb2->dir_tfilter != NULL) return;
  } else {
    if (fb2->vpaned) {
      DEBUG_MSG("remove fb2->vpaned, ");
      gtk_container_remove(GTK_CONTAINER(fb2->vbox),fb2->vpaned);
      DEBUG_MSG("fb2->vpaned, ");
      fb2->vpaned = NULL;
    } else if (fb2->dirscrolwin) {
      DEBUG_MSG("remove fb2->dirscrolwin, ");
      gtk_container_remove(GTK_CONTAINER(fb2->vbox),fb2->dirscrolwin);
      fb2->dirscrolwin = NULL;
    }
    fb2->dir_v = fb2->file_v = fb2->dir_tfilter = fb2->file_lfilter = NULL;
    DEBUG_MSG("\n");
  }
  fb2->filebrowser_viewmode = viewmode;
  
  DEBUG_MSG("fb2_set_viewmode_widgets, building new GUI\n");
  if (fb2->basedir) {
    basepath = treepath_for_uri(fb2, fb2->basedir);
    if (fb2->filebrowser_viewmode != viewmode_flat) {
      gtk_tree_path_up(basepath);
    }
  }
  
  fb2->dir_tfilter = gtk_tree_model_filter_new(GTK_TREE_MODEL(FB2CONFIG(main_v->fb2config)->filesystem_tstore),basepath);
  gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(fb2->dir_tfilter),tree_model_filter_func,fb2,NULL);

  fb2->dir_tsort = gtk_tree_model_sort_new_with_model(GTK_TREE_MODEL(fb2->dir_tfilter));
  if (fb2->filebrowser_viewmode != viewmode_dual) {
    DEBUG_MSG("setting sort function\n");
    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(fb2->dir_tsort), FILENAME_COLUMN,filebrowser_sort_func, NULL, NULL);
  } else DEBUG_MSG("sort alphabetical\n");
  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(fb2->dir_tsort),FILENAME_COLUMN,GTK_SORT_ASCENDING);   

  fb2->dir_v = gtk_tree_view_new_with_model(fb2->dir_tsort);
  /* we remove our reference, so the only reference is kept by the treeview, if the treeview is destroyed, the models will be destroyed */
  g_object_unref(G_OBJECT(fb2->dir_tfilter));
  g_object_unref(G_OBJECT(fb2->dir_tsort));
  
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(fb2->dir_v), FALSE);
  if (fb2->filebrowser_viewmode != viewmode_flat) {
    dirselection = gtk_tree_view_get_selection(GTK_TREE_VIEW(fb2->dir_v));
    DEBUG_MSG("fb2_init, NEW FILEBROWSER2, treeselection=%p, fb2=%p, dir_tfilter=%p\n",dirselection,fb2,fb2->dir_tfilter);
    g_signal_connect(G_OBJECT(dirselection), "changed", G_CALLBACK(dir_v_selection_changed_lcb), fb2);
  }
  
  renderer = gtk_cell_renderer_pixbuf_new();
  column = gtk_tree_view_column_new();
  gtk_tree_view_column_pack_start(column, renderer, FALSE);
  gtk_tree_view_column_set_attributes(column,renderer
    ,"pixbuf",PIXMAP_COLUMN
    ,"pixbuf_expander_closed",PIXMAP_COLUMN
    ,"pixbuf_expander_open",PIXMAP_COLUMN
    ,NULL);
  renderer = gtk_cell_renderer_text_new();
  g_object_set(G_OBJECT(renderer), "editable", FALSE, NULL); /* Not editable. */
  gtk_tree_view_column_pack_start(column, renderer, TRUE);
  gtk_tree_view_column_set_attributes(column,renderer,"text", FILENAME_COLUMN,NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(fb2->dir_v),column);
  fb2->dirscrolwin = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(fb2->dirscrolwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(fb2->dirscrolwin), fb2->dir_v);

  gtk_drag_dest_set(fb2->dir_v,(GTK_DEST_DEFAULT_ALL),drag_dest_types, 2
      ,(GDK_ACTION_DEFAULT | GDK_ACTION_COPY));
  g_signal_connect(G_OBJECT(fb2->dir_v), "drag_data_received", G_CALLBACK(fb2_dir_v_drag_data_received), fb2);
  
  if (fb2->filebrowser_viewmode != viewmode_dual) {
    gtk_box_pack_start(GTK_BOX(fb2->vbox), fb2->dirscrolwin, TRUE, TRUE, 0);
  } else {
    fb2->vpaned = gtk_vpaned_new();
    gtk_widget_set_size_request(fb2->vpaned, main_v->globses.left_panel_width, -1);
    gtk_paned_set_position(GTK_PANED(fb2->vpaned), main_v->globses.two_pane_filebrowser_height);
    g_signal_connect(G_OBJECT(fb2->vpaned),"notify::position",G_CALLBACK(fb2_two_pane_notify_position_lcb), NULL);

    gtk_paned_add1(GTK_PANED(fb2->vpaned), fb2->dirscrolwin);

    fb2->file_lfilter = gtk_tree_model_filter_new(GTK_TREE_MODEL(FB2CONFIG(main_v->fb2config)->filesystem_tstore),NULL);
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(fb2->file_lfilter),file_list_filter_func,fb2,NULL);

    fb2->file_lsort = gtk_tree_model_sort_new_with_model(GTK_TREE_MODEL(fb2->file_lfilter));
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(fb2->file_lsort),FILENAME_COLUMN,GTK_SORT_ASCENDING);
    DEBUG_MSG("fb2_init, file_lfilter=%p\n",fb2->file_lfilter);
    fb2->file_v = gtk_tree_view_new_with_model(fb2->file_lsort);
    /* we remove our reference, so the only reference is kept by the treeview, if the treeview is destroyed, the models will be destroyed */
    g_object_unref(G_OBJECT(fb2->file_lfilter));
    g_object_unref(G_OBJECT(fb2->file_lsort));

    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(fb2->file_v), FALSE);
    renderer = gtk_cell_renderer_pixbuf_new();
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
#ifdef HAVE_ATLEAST_GIO_2_16
    gtk_tree_view_column_set_attributes(column,renderer
      ,"icon-name",PIXMAP_COLUMN
      ,NULL);
#else /* no HAVE_ATLEAST_GIO_2_16  */
    gtk_tree_view_column_set_attributes(column,renderer
      ,"pixbuf",PIXMAP_COLUMN
      ,"pixbuf_expander_closed",PIXMAP_COLUMN
      ,"pixbuf_expander_open",PIXMAP_COLUMN
      ,NULL);
#endif /* else HAVE_ATLEAST_GIO_2_16 */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "editable", FALSE, NULL); /* Not editable. */
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_attributes(column,renderer,"text", FILENAME_COLUMN,NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(fb2->file_v), column);
    scrolwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolwin), fb2->file_v);
    gtk_paned_add2(GTK_PANED(fb2->vpaned), scrolwin);

    gtk_box_pack_start(GTK_BOX(fb2->vbox), fb2->vpaned, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(fb2->file_v), "button_press_event",G_CALLBACK(file_v_button_press_lcb),fb2);
    gtk_drag_dest_set(fb2->file_v,(GTK_DEST_DEFAULT_ALL),drag_dest_types, 2
        ,(GDK_ACTION_DEFAULT | GDK_ACTION_COPY));
    g_signal_connect(G_OBJECT(fb2->file_v), "drag_data_received", G_CALLBACK(fb2_file_v_drag_data_received), fb2);
  }

  g_signal_connect(G_OBJECT(fb2->dir_v), "row-activated",G_CALLBACK(dir_v_row_activated_lcb),fb2);
  g_signal_connect(G_OBJECT(fb2->dir_v), "button_press_event",G_CALLBACK(dir_v_button_press_lcb),fb2);
  fb2->expand_signal = g_signal_connect(G_OBJECT(fb2->dir_v), "row-expanded", G_CALLBACK(dir_v_row_expanded_lcb), fb2);
  /*gtk_container_resize_children(GTK_CONTAINER(fb2->vbox));*/
  gtk_widget_show_all(fb2->vbox);
  DEBUG_MSG("fb2_set_viewmode_widgets, new GUI finished\n");
}


void fb2_update_settings_from_session(Tbfwin *bfwin) {
  if (bfwin->fb2) {
    gboolean need_refilter=FALSE;
    Tfilebrowser2 *fb2 = bfwin->fb2;
    
    DEBUG_MSG("fb2_update_settings_from_session, started, bfwin=%p, fb2=%p, viewmode=%d\n",bfwin,fb2, fb2->filebrowser_viewmode);
    
    fb2_set_viewmode_widgets(fb2, bfwin->session->filebrowser_viewmode);
  
    if (bfwin->session->last_filefilter) {
      Tfilter *newfilter = find_filter_by_name(bfwin->session->last_filefilter);
      if (fb2->curfilter == NULL || newfilter == NULL 
              || !(newfilter == fb2->curfilter || strcmp(newfilter->name, fb2->curfilter->name)==0)) {
        fb2->curfilter = newfilter;
        need_refilter = TRUE;
      } 
    }
    if (fb2->filebrowser_show_hidden_files != bfwin->session->filebrowser_show_hidden_files) {
      fb2->filebrowser_show_hidden_files = bfwin->session->filebrowser_show_hidden_files;
      need_refilter = TRUE;
    }
    if(fb2->filebrowser_show_backup_files != bfwin->session->filebrowser_show_backup_files) {
      fb2->filebrowser_show_backup_files = bfwin->session->filebrowser_show_backup_files;
      need_refilter = TRUE;
    }
    if (need_refilter) {
      if (fb2->dir_tfilter) gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(fb2->dir_tfilter));
      if (fb2->file_lfilter && fb2->filebrowser_viewmode == viewmode_dual) gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(fb2->file_lfilter));
    }
    if (bfwin->session->recent_dirs 
          && ((GList *)g_list_first(bfwin->session->recent_dirs))->data 
          && strlen(((GList *)g_list_first(bfwin->session->recent_dirs))->data)>0) {
      /* the fb2_set_basedir function tests itself if  the basedir if changed, if not it does not refresh */
      fb2_set_basedir(bfwin, ((GList *)g_list_first(bfwin->session->recent_dirs))->data);
    } else {
      fb2_set_basedir(bfwin, NULL);
    }
  }
}

GtkWidget *fb2_init(Tbfwin *bfwin) {
  Tfilebrowser2 *fb2;
  GtkCellRenderer *renderer;  
#ifdef DEVELOPMENT
  if (!bfwin->session) exit(321);
#endif
  fb2 = g_new0(Tfilebrowser2,1);
  fb2->filebrowser_viewmode = bfwin->session->filebrowser_viewmode;
  
  bfwin->fb2 = fb2;
  fb2->bfwin = bfwin;
  DEBUG_MSG("fb2_init, started for bfwin=%p, fb2=%p, fb2->filebrowser_viewmode=%d\n",bfwin,fb2,fb2->filebrowser_viewmode);

  fb2->vbox = gtk_vbox_new(FALSE, 0);

  fb2->dirmenu_m = GTK_TREE_MODEL(gtk_list_store_new(3,G_TYPE_STRING,G_TYPE_BOOLEAN,G_TYPE_POINTER));
/*  {
    GList *tmplist;
    tmplist = g_list_first(bfwin->session->recent_dirs);
    while (tmplist) {
      GtkTreeIter iter;
      GnomeVFSURI *uri;
      gchar *name;
      uri = gnome_vfs_uri_new(tmplist->data);
      if (uri) {
        name = full_path_utf8_from_uri(uri);
        gtk_list_store_append(GTK_LIST_STORE(fb2->dirmenu_m),&iter);
        DEBUG_MSG("fb2_init, adding %s to dir menu\n",(gchar *)tmplist->data);
        gtk_list_store_set(GTK_LIST_STORE(fb2->dirmenu_m),&iter
            ,DIR_NAME_COLUMN,name
            ,DIR_URI_COLUMN ,uri
            ,-1);
      }
      tmplist = g_list_next(tmplist);
    }
  }*/
  fb2->dirmenu_v = gtk_combo_box_new_with_model(fb2->dirmenu_m);
  /*gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(fb2->dirmenu_v),3);*/
  renderer = gtk_cell_renderer_text_new();
  gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(fb2->dirmenu_v),renderer, TRUE);
  gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(fb2->dirmenu_v), renderer
      , "text", DIR_NAME_COLUMN
      , NULL);
  gtk_box_pack_start(GTK_BOX(fb2->vbox),fb2->dirmenu_v, FALSE, FALSE, 0);
  fb2->dirmenu_changed_signal = g_signal_connect(fb2->dirmenu_v, "changed", G_CALLBACK(dirmenu_changed_lcb), fb2);

  fb2_update_settings_from_session(bfwin);
  gtk_widget_show_all(fb2->vbox);
/*  {
    GnomeVFSURI *uri = NULL;
    if (bfwin->session->recent_dirs) {
      uri = gnome_vfs_uri_new(strip_trailing_slash(((GList *)g_list_first(bfwin->session->recent_dirs))->data));
    } else if (bfwin->project) {
      uri = gnome_vfs_uri_new(strip_trailing_slash(bfwin->project->basedir));
    }
    if (uri) {
      DEBUG_MSG("fb2_init, first build, and then set the basedir to ");
      DEBUG_URI(uri, TRUE);
      fb2_build_dir(uri);
      {
        GtkTreePath *basedir = fb2_fspath_from_uri(fb2, uri);
        DEBUG_MSG("fb2_init, will set the basedir to path(%p) ",basedir);
        DEBUG_TPATH(GTK_TREE_MODEL(FB2CONFIG(main_v->fb2config)->filesystem_tstore), basedir, TRUE);
        refilter_dirlist(fb2, basedir);
        gtk_tree_path_free(basedir);
      }
      DEBUG_MSG("fb2_init, focus fb2->basedir ");
      DEBUG_URI(fb2->basedir, TRUE);
      fb2_focus_dir(fb2, fb2->basedir, FALSE);
      gnome_vfs_uri_unref(uri);
    }
  }*/
  return fb2->vbox;
}

void fb2_cleanup(Tbfwin *bfwin) {
  if (bfwin->fb2) {
    Tfilebrowser2 *fb2 = FILEBROWSER2(bfwin->fb2);
    if (fb2->basedir) gnome_vfs_uri_unref(fb2->basedir);
    g_free(fb2);
    bfwin->fb2 = NULL;
  }
}

/* if a filter is deleted, this function is called to make sure
the current filebrowser isn't actually using the filter */
void fb2_unset_filter(Tbfwin *bfwin, Tfilter *filter) {
  if (bfwin->fb2 && FILEBROWSER2(bfwin->fb2)->curfilter == filter) {
    FILEBROWSER2(bfwin->fb2)->curfilter = NULL;
    fb2_refilter(FILEBROWSER2(bfwin->fb2));
  }
}

static void uri_hash_destroy(gpointer data) {
#ifdef HAVE_ATLEAST_GIO_2_16
  g_object_unref((GObject *)data);
#else /* no HAVE_ATLEAST_GIO_2_16  */
  gnome_vfs_uri_unref((GnomeVFSURI *)data);
#endif /* else HAVE_ATLEAST_GIO_2_16 */
}

void fb2config_init(void) {
  Tfilebrowser2config *fb2config;
  gchar *filename;
  DEBUG_MSG("fb2config_init, started\n");
  /* a lot of things can be the same for all windows, such as the hashtable and the treestore with
  all the files, and the pixmaps to use. All stored in Tfilebrowser2config;
  This will be initialized in this function */
  fb2config = g_new0(Tfilebrowser2config,1);
  main_v->fb2config = fb2config;

#ifdef HAVE_ATLEAST_GIO_2_16
  fb2config->filesystem_itable = g_hash_table_new_full(g_file_hash, g_file_equal,uri_hash_destroy,g_free);
  fb2config->filesystem_tstore = gtk_tree_store_new(N_COLUMNS,GDK_TYPE_PIXBUF,G_TYPE_STRING,G_TYPE_POINTER,G_TYPE_BOOLEAN,G_TYPE_STRING,G_TYPE_POINTER);

#else /* no HAVE_ATLEAST_GIO_2_16  */
  fb2config->filesystem_itable = g_hash_table_new_full(gnome_vfs_uri_hash, gnome_vfs_uri_hequal,uri_hash_destroy,g_free);
  fb2config->filesystem_tstore = gtk_tree_store_new(N_COLUMNS,GDK_TYPE_PIXBUF,G_TYPE_STRING,G_TYPE_POINTER,G_TYPE_BOOLEAN,G_TYPE_STRING);
#endif /* else HAVE_ATLEAST_GIO_2_16 */
  filename = return_first_existing_filename(main_v->props.filebrowser_unknown_icon,
          "icon_unknown.png","../images/icon_unknown.png",
          "images/icon_unknown.png",NULL);
  fb2config->unknown_icon = gdk_pixbuf_new_from_file(filename, NULL);
  g_free(filename);
  /*filename = return_first_existing_filename(main_v->props.filebrowser_dir_icon,
          "icon_dir.png","../images/icon_dir.png",
          "images/icon_dir.png",NULL);
  fb2config->dir_icon = gdk_pixbuf_new_from_file(filename, NULL);*/
  fb2config->dir_icon = get_icon_for_mime_type("x-directory/normal");
  /*g_free(filename);*/

/*  {
    GtkTreeIter *iter;
    GnomeVFSURI *uri;
    gchar *home = g_strdup(g_get_home_dir());
    uri = gnome_vfs_uri_new(strip_trailing_slash(home));
    g_free(home);
    iter = fb2_build_dir(uri);
    gnome_vfs_uri_unref(uri);
  }*/ 
  DEBUG_MSG("fb2config_init, finished\n");
}
/* avoid segfaults during bluefish exit */
void fb2config_cleanup(void) {
  GList *tmplist = g_list_first(FB2CONFIG(main_v->fb2config)->uri_in_refresh);
  DEBUG_MSG("fb2config_cleanup, stopping all async directory reads\n");
  while (tmplist) {
    Turi_in_refresh *uir = tmplist->data;
#ifdef HAVE_ATLEAST_GIO_2_16
    g_cancellable_cancel(uir->cancel);
#else /* no HAVE_ATLEAST_GIO_2_16  */
    gnome_vfs_async_cancel(uir->handle);
#endif /* else HAVE_ATLEAST_GIO_2_16 */
    gnome_vfs_uri_unref(uir->uri);
    g_free(uir);
    tmplist = g_list_next(tmplist);
  }
  g_list_free(FB2CONFIG(main_v->fb2config)->uri_in_refresh);
  FB2CONFIG(main_v->fb2config)->uri_in_refresh = NULL;
}
