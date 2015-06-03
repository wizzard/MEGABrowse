#include <string.h>
#include <locale.h>
#include <libintl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/vfs.h>
#include <dirent.h>
#include <sys/time.h>
#include <execinfo.h>
#include <signal.h>
#include <libgen.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>
#include <gtk/gtktable.h>

#include "megaapi_wrapper.h"

typedef struct {
    CMegaApi* api;

    GtkWidget *window;
    gchar *tmp_dir;

    GtkTreeStore *store;
    GtkWidget *treeview;

    GtkWidget *pdlg;
    GtkWidget *pbar;
    guint32 progress_sig_h;
    GMutex m_notify;
    gint result_code;

    GtkWidget **lbls;

    GtkWidget *preview_img;

    gchar *email;
    gchar *password;
    gboolean logged_in;
} Application;

enum {
  COL_ICON,
  COL_NAME,
  COL_IS_DIR,
  COL_NODE,
  NUM_COLUMNS
};

enum {
    LBL_name,
    LBL_size,
    LBL_type,
    LBL_ctime,
    LBL_mtime,
    LBL_key,
    LBL_handle,
    LBL_is_removed,
    LBL_has_thumbnail,
    LBL_has_preview,
    LBL_is_public,
    NUM_LBLS
};
const char * const lbls[] = {"Name:", "Size:", "Type:", "Creation time:", "Modification time:", "Key:", "Handle:", "is removed:", "has thumbnail:", "has preview:", "is public:"};

/*{{{ declr */
static void display_login_dlg(G_GNUC_UNUSED GtkAction *action, gpointer data);
static void process_login(Application *app);

static GtkActionEntry a_entries[] = {
  { "MegaMenu", NULL, "Mega", NULL, NULL, NULL },
  { "FileMenu", NULL, "File", NULL, NULL, NULL },

  { "Login", NULL, "Login", NULL, NULL, G_CALLBACK(display_login_dlg) },
  { "Quit", NULL, "Quit", NULL, NULL, G_CALLBACK(gtk_main_quit) },
};
static guint n_a_entries = G_N_ELEMENTS(a_entries);

static const gchar *ui_info =
"<ui>"
"  <menubar name='MenuBar'>"
"    <menu action='FileMenu'>"
"      <menuitem action='Quit'/>"
"    </menu>"
"    <menu action='MegaMenu'>"
"      <menuitem action='Login'/>"
"    </menu>"
"  </menubar>"
"</ui>";
/*}}}*/

/*{{{ display_login_dlg */
static void display_login_dlg(G_GNUC_UNUSED GtkAction *action, gpointer data)
{
    Application *app = (Application *)data;
    GtkWidget *dialog;
    GtkWidget *vbox;
    GtkWidget *table;
    GtkWidget *label;
    GtkWidget *entry1;
    GtkWidget *entry2;
    GtkEntryBuffer *email_buf;
    GtkEntryBuffer *password_buf;
    gint response;

    dialog = gtk_dialog_new_with_buttons("Login",
       GTK_WINDOW(app->window),
       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
       GTK_STOCK_OK, GTK_RESPONSE_OK,
       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
       NULL
    );

    vbox = gtk_vbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), vbox, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);

    table = gtk_table_new(2, 3, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
    gtk_table_set_row_spacings(GTK_TABLE(table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);

    label = gtk_label_new_with_mnemonic("email:");
    gtk_table_attach(GTK_TABLE(table), label,
	    0, 1, 0, 1,
		GTK_EXPAND | GTK_FILL, 0,
		0, 0
    );

    email_buf = gtk_entry_buffer_new(NULL, 0);
    entry1 = gtk_entry_new_with_buffer(email_buf);
    gtk_table_attach(GTK_TABLE(table), entry1,
	    1, 2, 0, 1,
		GTK_EXPAND | GTK_FILL, 0,
		0, 0
    );

    label = gtk_label_new_with_mnemonic("Password:");
    gtk_table_attach(GTK_TABLE(table), label,
	    0, 1, 1, 2,
		GTK_EXPAND | GTK_FILL, 0,
		0, 0
    );

    password_buf = gtk_entry_buffer_new(NULL, 0);
    entry2 = gtk_entry_new_with_buffer(password_buf);
    gtk_table_attach(GTK_TABLE(table), entry2,
	    1, 2, 1, 2,
		GTK_EXPAND | GTK_FILL, 0,
		0, 0
    );

    gtk_widget_show_all(dialog);

    while (1) {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        if (response == GTK_RESPONSE_OK) {
            const gchar *email = gtk_entry_get_text(GTK_ENTRY(entry1));
            const gchar *pwd = gtk_entry_get_text(GTK_ENTRY(entry2));

            if (email && pwd && strlen(email) > 1 && strlen(pwd) > 1) {
                app->email = g_strdup(email);
                app->password = g_strdup(pwd);
                break;
            } else {
                gtk_widget_grab_focus(entry1);
            }
        } else
            break;
    }

    gtk_widget_destroy(dialog);

    if (app->email && app->password) {
        process_login(app);
    }
}
/*}}}*/

/*{{{ request */
static gboolean on_request_progress_cb(gpointer data)
{
    Application *app = (Application *)data;

    if (!app->pbar)
        return FALSE;

    if (g_mutex_trylock(&app->m_notify)) {
        g_mutex_unlock(&app->m_notify);
        g_source_remove(app->progress_sig_h);
        gtk_widget_destroy(app->pdlg);
    } else {
        gtk_progress_bar_pulse(GTK_PROGRESS_BAR(app->pbar));
    }

    return TRUE;
}

static void on_request_start_cb(void *ctx)
{
    Application *app = (Application *)ctx;
    g_mutex_lock(&app->m_notify);
}

static void on_request_finish_cb(void *ctx, int errorCode)
{
    Application *app = (Application *)ctx;
    app->result_code = errorCode;
    g_mutex_unlock(&app->m_notify);
}

static void on_request_wait_cb(void *ctx)
{
    Application *app = (Application *)ctx;
    GtkWidget *vbox;

    app->pdlg = gtk_dialog_new_with_buttons("Login",
       GTK_WINDOW(app->window),
       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
       GTK_STOCK_STOP, GTK_RESPONSE_CANCEL,
       NULL
    );

    vbox = gtk_vbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(app->pdlg))), vbox, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
    app->pbar = gtk_progress_bar_new();
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(app->pbar), 0.0f);
    gtk_box_pack_start(GTK_BOX(vbox), app->pbar, TRUE, TRUE, 5);
    app->progress_sig_h = g_timeout_add(100, on_request_progress_cb, app);

    gtk_widget_show_all(app->pdlg);

    gtk_dialog_run(GTK_DIALOG(app->pdlg));
}
/*}}}*/

/*{{{ SDK */
static void download_file(Application *app, CMegaNode *node, const gchar *local_path)
{
    CMegaTransferListener *listener;
    listener = mega_transfer_listener_create(app);
    mega_api_start_download(app->api, node, local_path, listener);
    mega_transfer_listener_destroy(listener);
}

static void fill_tree(Application *app, CMegaNodeList *node_list, GtkTreeIter *parent_iter)
{
    GtkTreeIter iter;
    int i;

    for (i = 0; i < mega_api_node_list_get_n_children(app->api, node_list); i++) {
        const char *name;
        gboolean is_dir;
        GIcon *icon = NULL;
        CMegaNode *c = mega_api_node_list_get_child(node_list, i);

        name = mega_api_node_get_name(c);
        is_dir = !mega_api_node_is_file(c);

        if (is_dir) {
            icon = g_content_type_get_icon("folder");
        } else {
            icon = g_content_type_get_icon("text-x-generic");
        }
        gtk_tree_store_insert_with_values(app->store, &iter, parent_iter, G_MAXINT,
            COL_ICON, icon,
            COL_NAME, name,
            COL_IS_DIR, is_dir,
            COL_NODE, c,
        -1);

        if (is_dir && mega_api_get_num_children(app->api, c)) {
            // append a dummy child
            gtk_tree_store_insert_with_values(app->store, NULL, &iter, G_MAXINT, -1);
        }
    }
}

static void fetch_nodes(Application *app)
{
    CMegaRequestListener *listener;
    listener = mega_request_listener_create(app, on_request_start_cb, on_request_finish_cb, on_request_wait_cb);

    mega_api_fetch_nodes(app->api, listener);
    mega_request_listener_wait(listener);
    mega_request_listener_destroy(listener);

    if (app->result_code == API_OK) {
        CMegaNode *node;
        CMegaNodeList *node_list;

        node = mega_api_get_node_by_path(app->api, "/");
        node_list = mega_api_node_get_children(app->api, node);

        fill_tree(app, node_list, NULL);
    } else {
        exit(1);
    }
}

static void process_login(Application *app)
{
    CMegaRequestListener *listener;
    listener = mega_request_listener_create(app, on_request_start_cb, on_request_finish_cb, on_request_wait_cb);

    mega_api_login(app->api, app->email, app->password, listener);
    mega_request_listener_wait(listener);
    mega_request_listener_destroy(listener);

    if (app->result_code == API_OK) {
        fetch_nodes(app);
    } else {
        exit(1);
    }
}
/*}}}*/

/*{{{ columns_create */
static void columns_create(Application *app)
{
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column_name;

    // icon + name
	column_name = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column_name, "Name");

    // icon
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column_name, renderer, FALSE);
    g_object_set (G_OBJECT(renderer), "xpad", 3, "stock-size", GTK_ICON_SIZE_SMALL_TOOLBAR, NULL);
	gtk_tree_view_column_set_attributes(column_name, renderer, "gicon", COL_ICON, NULL);

    // text
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column_name, renderer, TRUE);
	gtk_tree_view_column_set_attributes(column_name, renderer, "text", COL_NAME, NULL);
    g_object_set(renderer, "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE, 0, NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(app->treeview), column_name);

    // settings
    gtk_tree_view_set_expander_column(GTK_TREE_VIEW(app->treeview), column_name);
}
/*}}}*/

/*{{{ Sorting */
// Sorting function
static gint on_filename_sort(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, G_GNUC_UNUSED gpointer user_data)
{
    gchar *name1;
    gchar *name2;
    gint is_dir1;
    gint is_dir2;

    gint ret = 0;

    gtk_tree_model_get(model, a, COL_NAME, &name1, COL_IS_DIR, &is_dir1, -1);
    gtk_tree_model_get(model, b, COL_NAME, &name2, COL_IS_DIR, &is_dir2, -1);

    if (!name1 || !name2)
        return 0;

    if (is_dir1 && !is_dir2) {
        ret = -1;
    // assuming UTF8 strings
    // XXX: slow !
    } else if (is_dir2 && !is_dir1)  {
        ret = 1;
    } else {

        if (name1 == NULL || name2 == NULL) {
          if (name1 == NULL && name2 == NULL)
          ret = (name1 == NULL) ? -1 : 1;
        } else {
          ret = g_utf8_collate(name1,name2);
        }

    }
    g_free(name1);
    g_free(name2);

    return ret;
}
/*}}}*/

#define AGB 1073741824
#define AMB 1048576
#define AKB 1024

static const gchar *get_size_str(long long s)
{
    static gchar str[50];

    if (s >= AGB) {
        g_snprintf(str, sizeof(str), "%0.2f GB", (gdouble)(s / AGB));
    } else if (s >= AMB) {
        g_snprintf(str, sizeof(str), "%0.2f MB", (gdouble)(s / AMB));
    } else if (s >= AKB) {
        g_snprintf(str, sizeof(str), "%0.2f KB", (gdouble)(s / AKB));
    } else {
        g_snprintf(str, sizeof(str), "%lld B", s);
    }

    return str;
}

static const gchar *get_type_str(int t)
{
    static gchar str[50];

    switch (t) {
        case TYPE_UNKNOWN:
            g_snprintf(str, sizeof(str), "%s", "Unknown node");
            break;
        case TYPE_FILE:
            g_snprintf(str, sizeof(str), "%s", "File");
            break;
        case TYPE_FOLDER:
            g_snprintf(str, sizeof(str), "%s", "Folder");
            break;
        case TYPE_ROOT:
            g_snprintf(str, sizeof(str), "%s", "Root");
            break;
        case TYPE_INCOMING:
            g_snprintf(str, sizeof(str), "%s", "Inbox");
            break;
        case TYPE_RUBBISH:
            g_snprintf(str, sizeof(str), "%s", "Rubbish Bin");
            break;
        default:
            g_snprintf(str, sizeof(str), "%s", "Unknown node");
            break;
    }

    return str;
}

static const gchar *get_bool_str(int b)
{
    static gchar str[5];

    if (b)
        g_snprintf(str, sizeof(str), "%s", "Yes");
    else
        g_snprintf(str, sizeof(str), "%s", "No");

    return str;
}

static void on_row_selected_cb(GtkTreeSelection *treeselection, Application *app)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    CMegaNode *node;
    gchar cstr[20];
    gchar mstr[20];

    model = GTK_TREE_MODEL(app->store);

    if (!gtk_tree_selection_get_selected(treeselection, &model, &iter)) {
        return;
    }

    gtk_tree_model_get(GTK_TREE_MODEL(app->store), &iter, COL_NODE, &node, -1);
    gtk_label_set_text(GTK_LABEL(app->lbls[LBL_name]), mega_api_node_get_name(node));
    if (mega_api_node_is_file(node))
        gtk_label_set_text(GTK_LABEL(app->lbls[LBL_size]), get_size_str(mega_api_node_get_size(node)));
    else
        gtk_label_set_text(GTK_LABEL(app->lbls[LBL_size]), "-");
    g_snprintf(cstr, sizeof(cstr), "%lu", mega_api_node_get_creation_time(node));
    gtk_label_set_text(GTK_LABEL(app->lbls[LBL_ctime]), cstr);
    g_snprintf(mstr, sizeof(mstr), "%lu", mega_api_node_get_modification_time(node));
    gtk_label_set_text(GTK_LABEL(app->lbls[LBL_mtime]), mstr);
    gtk_label_set_text(GTK_LABEL(app->lbls[LBL_type]), get_type_str(mega_api_node_get_type(node)));
    gtk_label_set_text(GTK_LABEL(app->lbls[LBL_key]), mega_api_node_get_key(node));
    gtk_label_set_text(GTK_LABEL(app->lbls[LBL_handle]), mega_api_node_get_handle(node));
    gtk_label_set_text(GTK_LABEL(app->lbls[LBL_is_removed]), get_bool_str(mega_api_node_is_removed(node)));
    gtk_label_set_text(GTK_LABEL(app->lbls[LBL_has_thumbnail]), get_bool_str(mega_api_node_has_thumbnail(node)));
    gtk_label_set_text(GTK_LABEL(app->lbls[LBL_has_preview]), get_bool_str(mega_api_node_has_preview(node)));
    gtk_label_set_text(GTK_LABEL(app->lbls[LBL_is_public]), get_bool_str(mega_api_node_is_public(node)));

    if (mega_api_node_has_preview(node)) {
        CMegaRequestListener *listener;
        listener = mega_request_listener_create(app, on_request_start_cb, on_request_finish_cb, on_request_wait_cb);

        mega_api_get_preview(app->api, node, app->tmp_dir, listener);
        mega_request_listener_wait(listener);
        mega_request_listener_destroy(listener);

        if (app->result_code == API_OK) {
            gchar fname[1024];
            g_snprintf(fname, sizeof(fname),"%s/%s1.jpg", app->tmp_dir, mega_api_node_get_handle(node));
            gtk_image_set_from_file(GTK_IMAGE(app->preview_img), fname);
        }
    } else {
        gtk_image_clear(GTK_IMAGE(app->preview_img));
    }
}

static void on_btn_download_clicked_cb(G_GNUC_UNUSED GtkWidget *widget, gpointer data)
{
    Application *app = (Application *)data;
    GtkWidget *dlg;
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;
    CMegaNode *node;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->treeview));
    model = GTK_TREE_MODEL(app->store);

    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return;
    }

    gtk_tree_model_get(GTK_TREE_MODEL(app->store), &iter, COL_NODE, &node, -1);

    dlg = gtk_file_chooser_dialog_new("Select directory", GTK_WINDOW(app->window),
        GTK_FILE_CHOOSER_ACTION_SAVE,
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
        GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
        NULL
    );
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dlg), TRUE);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg), mega_api_node_get_name(node));
    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        gchar *fname;
        fname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        download_file(app, node, fname);
        g_free(fname);
    }
    gtk_widget_destroy(dlg);
}

/*{{{ on_expand */
// Expand UI is clicked
// FALSE to allow expansion, TRUE to reject
static gboolean on_test_expand_row_cb(G_GNUC_UNUSED GtkTreeView *tree_view, GtkTreeIter *iter, G_GNUC_UNUSED GtkTreePath *path, gpointer user_data)
{
    Application *app = (Application *)user_data;
    CMegaNode *node;
    GtkTreeIter ch_iter;
    CMegaNodeList *node_list;

    // if it was previously expanded
    if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(app->store), iter) > 1) {
        return FALSE;
    }

    // remove "fake" child
    while (gtk_tree_model_iter_children(GTK_TREE_MODEL(app->store), &ch_iter, iter)) {
        gtk_tree_store_remove(app->store, &ch_iter);
    }

    gtk_tree_model_get(GTK_TREE_MODEL(app->store), iter, COL_NODE, &node, -1);
    node_list = mega_api_node_get_children(app->api, node);
    fill_tree(app, node_list, iter);

    return FALSE;
}
/*}}}*/

int main(int argc, char *argv[])
{
    Application *app;
    GtkWidget *vbox;
    GtkWidget *vbox1;
    GtkWidget *bar;
    GtkWidget *hbox;
    GtkWidget *btn;
    GtkWidget *table;
    GtkWidget *sbar;
    GtkWidget *lbl;
    GtkWidget *vpan1;
    GtkWidget *vpan2;
    GtkWidget *sw;
    GtkWidget *align;
    GtkActionGroup *actions;
    GtkUIManager *ui;
    GtkTreeSelection *selection;
    GError *error = NULL;
    gint i;
    gint width;
    char template[] = "/tmp/MEGABrowse.XXXXXX/";
    GOptionContext *context;

    // parse command line options
    context = g_option_context_new("");
    g_option_context_set_help_enabled(context, TRUE);
    g_option_context_set_description (context, "Set both MEGA_EMAIL and MEGA_PWD environment variables to automatically login.");
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_fprintf(stderr, "Failed to parse command line options: %s\n", error->message);
        g_option_context_free(context);
        return -1;
    }
    g_option_context_free(context);

    // init GTK+
    gtk_init(&argc, &argv);

    app = g_new0(Application, 1);
    g_mutex_init(&app->m_notify);

    app->tmp_dir = g_mkdtemp(template);

    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_position(GTK_WINDOW(app->window), GTK_WIN_POS_CENTER_ALWAYS);
    width = 600;
    gtk_widget_set_size_request(app->window, width, 600);

    g_printf("APP: %p\n", app);
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(app->window), vbox);

    actions = gtk_action_group_new("Actions");
    gtk_action_group_add_actions(actions, a_entries, n_a_entries, app);
    ui = gtk_ui_manager_new();
    gtk_ui_manager_insert_action_group(ui, actions, 0);
    if (!gtk_ui_manager_add_ui_from_string(ui, ui_info, -1, &error)) {
        g_error_free (error);
        exit(1);
    }
    bar = gtk_ui_manager_get_widget(ui, "/MenuBar");
    gtk_box_pack_start(GTK_BOX(vbox), bar, FALSE, FALSE, 0);

    vpan1 = gtk_hpaned_new();
    gtk_box_pack_start(GTK_BOX(vbox), vpan1, TRUE, TRUE, 0);

    sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_paned_pack1(GTK_PANED(vpan1), sw, TRUE, FALSE);
    gtk_widget_set_size_request(sw, width/3, -1);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    app->store = gtk_tree_store_new(NUM_COLUMNS, G_TYPE_ICON, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_POINTER);
    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(app->store), COL_NAME, on_filename_sort, app, NULL);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(app->store), COL_NAME, GTK_SORT_ASCENDING);

    app->treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(app->store));
    gtk_container_add(GTK_CONTAINER(sw), app->treeview);
    columns_create(app);
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(app->treeview), TRUE);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(app->treeview), FALSE);
    gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(app->treeview), TRUE);
    gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(app->treeview), GTK_TREE_VIEW_GRID_LINES_NONE);
	gtk_tree_view_columns_autosize(GTK_TREE_VIEW(app->treeview));
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(app->treeview), FALSE);
	g_signal_connect(app->treeview, "test-expand-row", G_CALLBACK(on_test_expand_row_cb), app);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->treeview));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
    g_signal_connect(selection, "changed", G_CALLBACK(on_row_selected_cb), app);

    vpan2 = gtk_hpaned_new();
    gtk_paned_pack2(GTK_PANED(vpan1), vpan2, TRUE, TRUE);

    sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_paned_pack1(GTK_PANED(vpan2), sw, TRUE, FALSE);
    gtk_widget_set_size_request(sw, width/3, -1);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    app->preview_img = gtk_image_new();
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), app->preview_img);

    // right information panel
    vbox1 = gtk_vbox_new(FALSE, 0);
    gtk_paned_pack2(GTK_PANED(vpan2), vbox1, FALSE, TRUE);
    sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(vbox1), sw, TRUE, TRUE, 0);
    gtk_widget_set_size_request(sw, width/3, -1);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    table = gtk_table_new(2, 4, FALSE);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), table);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);

    app->lbls = g_new0(GtkWidget *, NUM_LBLS);
    for (i = 0; i < NUM_LBLS; i++) {
        align = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
        gtk_table_attach(GTK_TABLE(table), align, 0, 1, i, i + 1, GTK_FILL, 0, 0, 0);
        lbl = gtk_label_new(lbls[i]);
        gtk_container_add(GTK_CONTAINER(align), lbl);

        align = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
        gtk_table_attach(GTK_TABLE(table), align, 1, 2, i, i + 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
        app->lbls[i] = gtk_label_new("-");
        gtk_container_add(GTK_CONTAINER(align), app->lbls[i]);
    }

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, FALSE, 5);

    btn = gtk_button_new_with_label("Download");
    gtk_box_pack_start(GTK_BOX(hbox), btn, FALSE, FALSE, 5);
    g_signal_connect(G_OBJECT(btn), "clicked", G_CALLBACK(on_btn_download_clicked_cb), app);


    sbar = gtk_statusbar_new();
    gtk_box_pack_start(GTK_BOX(vbox), sbar, FALSE, FALSE, 0);

    app->api = mega_api_create("lBMRXLiA", NULL, "MEGABROWSER");
    mega_api_set_loglevel(app->api, LOG_LEVEL_DEBUG);

    gtk_widget_show_all(app->window);

    if (getenv("MEGA_PWD") && getenv("MEGA_EMAIL")) {
        app->email = g_strdup(getenv("MEGA_EMAIL"));
        app->password = g_strdup(getenv("MEGA_PWD"));
        process_login(app);
    }

    // run it
    gtk_main();

    return 0;
}
