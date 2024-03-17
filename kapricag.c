#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <gtk/gtk.h>
#include "clipboard.h"
#include "database.h"
#include "xmalloc.h"

enum defaults
{
    NUMBER_OF_SOURCES = 100,
    WINDOW_WIDTH = 340,
    WINDOW_HEIGHT = 430
};

struct Widgets
{
    /* Main window */
    GtkWidget *window;
    GtkWidget *back_list;
    /* Search */
    GtkWidget *search_bar;
    GtkWidget *scrolled_window_search;
    GtkWidget *search_list;
    GtkWidget *no_match;
    /* Entry */
    GtkWidget *scrolled_window_entry;
    GtkWidget *entry_list;
    GtkWidget *no_entry;
    /* Clear all */
    GtkWidget *clear_all;
    GtkWidget *confirm_vbox;
    GtkWidget *confirm_label;
    GtkWidget *confirm_hbox;
    GtkWidget *confirm_yes;
    GtkWidget *confirm_no;
};

static sqlite3 *db;
static uint32_t offset = 0;

static void clicked(GtkWidget *button, gpointer user_data)
{
    clipboard *clip = clip_init();
    int64_t t = GPOINTER_TO_UINT(user_data);
    database_get_source(db, t, clip->selection_source);
    clip_set_selection(clip);

    pid_t pid = fork();
    if (pid < 0)
    {
        fprintf(stderr, "Failed to fork\n");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        while (wl_display_dispatch(clip->display) >= 0)
            ;
    }
}

static void delete_entry(GtkWidget *button, gpointer user_data)
{
    int64_t t = GPOINTER_TO_UINT(user_data);
    database_delete_entry(db, t);
    gtk_widget_set_visible(gtk_widget_get_parent(button), FALSE);
}

static GtkWidget *create_button_box()
{
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_set_homogeneous(GTK_BOX(button_box), FALSE);
    gtk_widget_set_halign(button_box, GTK_ALIGN_FILL);
    gtk_widget_set_valign(button_box, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand(button_box, TRUE);
    gtk_widget_set_vexpand(button_box, TRUE);

    return button_box;
}

static GtkWidget *create_entry_button(int64_t id, struct Widgets *widgets)
{
    GtkWidget *button;
    void *thumbnail = NULL, *snippet = NULL;
    size_t len = 0;

    thumbnail = database_get_thumbnail(db, id, &len);
    if (len)
    {
        /* Convert thumbnail into a gbytes structure so it converted into a
         * texture */
        GBytes *pix_array = g_bytes_new(thumbnail, len);
        GdkTexture *texture = gdk_texture_new_from_bytes(pix_array, NULL);
        GtkWidget *image =
            gtk_picture_new_for_paintable(GDK_PAINTABLE(texture));

        /* A lot of formatting code to left align the image and fill to fit the
         * button */
        gtk_widget_set_halign(image, GTK_ALIGN_START);
        gtk_widget_set_valign(image, GTK_ALIGN_FILL);
        gtk_widget_set_hexpand(image, TRUE);
        gtk_widget_set_vexpand(image, TRUE);
        gtk_picture_set_can_shrink(GTK_PICTURE(image), TRUE);
        gtk_picture_set_content_fit(GTK_PICTURE(image),
                                    GTK_CONTENT_FIT_CONTAIN);
        gtk_widget_set_size_request(image, 250, 80);
        gtk_widget_set_margin_start(image, 0);
        gtk_widget_set_margin_end(image, 0);
        gtk_widget_set_margin_top(image, 0);
        gtk_widget_set_margin_bottom(image, 0);

        button = gtk_button_new();
        gtk_button_set_child(GTK_BUTTON(button), image);
    }
    else
    {
        snippet = database_get_snippet(db, id);
        button = gtk_button_new_with_label(snippet);
        GtkWidget *label = gtk_button_get_child(GTK_BUTTON(button));

        /* Set the label to wrap and left align */
        gtk_label_set_wrap(GTK_LABEL(label), TRUE);
        gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
        gtk_label_set_xalign(GTK_LABEL(label), 0);
    }

    /* Formatting code to make the button look nice */
    gtk_button_set_can_shrink(GTK_BUTTON(button), FALSE);
    gtk_button_set_has_frame(GTK_BUTTON(button), FALSE);
    gtk_widget_set_halign(button, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand(button, TRUE);

    /* Copy the content and exit */
    g_signal_connect(button, "clicked", G_CALLBACK(clicked),
                     GUINT_TO_POINTER(id));
    g_signal_connect_swapped(button, "clicked", G_CALLBACK(gtk_window_destroy),
                             GTK_WINDOW(widgets->window));

    return button;
}

static GtkWidget *create_delete_button(int64_t id)
{
    GtkWidget *button = gtk_button_new_from_icon_name("edit-delete");

    /* More formatting */
    gtk_button_set_has_frame(GTK_BUTTON(button), FALSE);
    gtk_widget_set_halign(button, GTK_ALIGN_END);
    gtk_widget_set_valign(button, GTK_ALIGN_FILL);

    g_signal_connect(button, "clicked", G_CALLBACK(delete_entry),
                     GUINT_TO_POINTER(id));

    return button;
}

static GtkWidget *create_button(int64_t id, struct Widgets *widgets)
{
    GtkWidget *button_box = create_button_box();
    GtkWidget *button = create_entry_button(id, widgets);
    GtkWidget *delete = create_delete_button(id);

    gtk_box_prepend(GTK_BOX(button_box), button);
    gtk_box_append(GTK_BOX(button_box), delete);

    return button_box;
}

static void load_more_entries(GtkScrolledWindow *scrolled_window,
                              GtkPositionType pos, gpointer user_data)
{
    GtkWidget *viewport = gtk_scrolled_window_get_child(scrolled_window);
    GtkWidget *entry_list = gtk_viewport_get_child(GTK_VIEWPORT(viewport));

    uint32_t total_sources = database_get_total_sources(db);
    int64_t *ids = malloc(sizeof(int64_t) * total_sources);
    uint32_t found =
        database_get_latest_sources(db, total_sources, offset, ids);
    found = found > NUMBER_OF_SOURCES ? NUMBER_OF_SOURCES : found;
    offset += found;

    for (int i = 0; i < found; i++)
    {
        GtkWidget *button = create_button(ids[i], user_data);
        gtk_list_box_insert(GTK_LIST_BOX(entry_list), button, -1);
    }
    free(ids);
}

static void search_database(GtkSearchEntry *search, gpointer user_data)
{
    struct Widgets *widgets = user_data;
    const char *text = gtk_editable_get_text(GTK_EDITABLE(search));
    gtk_list_box_remove_all(GTK_LIST_BOX(widgets->search_list));

    if (!strlen(text))
    {
        gtk_widget_set_visible(widgets->scrolled_window_entry, TRUE);
        gtk_widget_set_visible(widgets->scrolled_window_search, FALSE);
        gtk_widget_set_visible(widgets->no_match, FALSE);
        return;
    }

    uint32_t total_sources = database_get_total_sources(db);
    int64_t *ids = xmalloc(sizeof(int64_t) * total_sources);
    uint32_t found = database_find_matching_sources(
        db, (void *)text, strlen(text), total_sources, ids, FALSE);

    for (int i = 0; i < found; i++)
    {
        GtkWidget *button = create_button(ids[i], widgets);
        gtk_list_box_insert(GTK_LIST_BOX(widgets->search_list), button, -1);
    }

    if (found)
    {
        gtk_widget_set_visible(widgets->scrolled_window_entry, FALSE);
        gtk_widget_set_visible(widgets->no_match, FALSE);
        gtk_widget_set_visible(widgets->scrolled_window_search, TRUE);
    }
    else
    {
        gtk_widget_set_visible(widgets->scrolled_window_entry, FALSE);
        gtk_widget_set_visible(widgets->scrolled_window_search, FALSE);
        gtk_widget_set_visible(widgets->no_match, TRUE);
    }
}

static void confirm_clear_all(GtkWidget *button, gpointer user_data)
{
    struct Widgets *widgets = user_data;

    gtk_widget_set_visible(widgets->scrolled_window_entry, FALSE);
    gtk_widget_set_visible(widgets->scrolled_window_search, FALSE);
    gtk_widget_set_visible(widgets->clear_all, FALSE);
    gtk_widget_set_visible(widgets->no_match, FALSE);
    gtk_widget_set_visible(widgets->no_entry, FALSE);
    gtk_widget_set_visible(widgets->search_bar, FALSE);

    gtk_widget_set_visible(widgets->confirm_vbox, TRUE);
}

static void clear_all_no(GtkWidget *button, gpointer user_data)
{
    struct Widgets *widgets = user_data;

    gtk_widget_set_visible(widgets->clear_all, TRUE);
    gtk_widget_set_visible(widgets->scrolled_window_entry, TRUE);
    gtk_widget_set_visible(widgets->search_bar, TRUE);

    gtk_widget_set_visible(widgets->confirm_vbox, FALSE);
}

static void clear_all_yes(GtkWidget *button, gpointer user_data)
{
    struct Widgets *widgets = user_data;

    // implement database_clear_all(db);
    gtk_list_box_remove_all(GTK_LIST_BOX(widgets->entry_list));
    gtk_widget_set_visible(widgets->clear_all, TRUE);
    gtk_widget_set_visible(widgets->no_entry, TRUE);
    gtk_widget_set_visible(widgets->search_bar, TRUE);

    gtk_widget_set_visible(widgets->confirm_vbox, FALSE);
}

static void activate(GtkApplication *app, gpointer user_data)
{
    struct Widgets *widgets = xmalloc(sizeof(struct Widgets));
    widgets->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(widgets->window), "kaprica");
    gtk_window_set_default_size(GTK_WINDOW(widgets->window), WINDOW_WIDTH,
                                WINDOW_HEIGHT);

    uint32_t total_sources = database_get_total_sources(db);
    int64_t *ids = xmalloc(sizeof(int64_t) * total_sources);
    uint32_t found =
        database_get_latest_sources(db, total_sources, offset, ids);
    found = found > NUMBER_OF_SOURCES ? NUMBER_OF_SOURCES : found;
    offset += found;

    /* Main box */
    widgets->back_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    /* Setup searching */
    widgets->search_bar = gtk_search_entry_new();
    widgets->scrolled_window_search = gtk_scrolled_window_new();
    widgets->search_list = gtk_list_box_new();
    widgets->no_match = gtk_label_new("No matches found...");
    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(widgets->scrolled_window_search),
        widgets->search_list);
    gtk_scrolled_window_set_max_content_width(
        GTK_SCROLLED_WINDOW(widgets->scrolled_window_search), WINDOW_WIDTH);
    gtk_scrolled_window_set_propagate_natural_width(
        GTK_SCROLLED_WINDOW(widgets->scrolled_window_search), TRUE);
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(widgets->scrolled_window_search), GTK_POLICY_NEVER,
        GTK_POLICY_AUTOMATIC);
    g_signal_connect(widgets->search_bar, "search-changed",
                     G_CALLBACK(search_database), widgets);

    /* Setup entries */
    widgets->scrolled_window_entry = gtk_scrolled_window_new();
    widgets->entry_list = gtk_list_box_new();
    widgets->no_entry = gtk_label_new("No entries yet...");
    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(widgets->scrolled_window_entry),
        widgets->entry_list);
    gtk_scrolled_window_set_max_content_width(
        GTK_SCROLLED_WINDOW(widgets->scrolled_window_entry), WINDOW_WIDTH);
    gtk_scrolled_window_set_propagate_natural_width(
        GTK_SCROLLED_WINDOW(widgets->scrolled_window_entry), TRUE);
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(widgets->scrolled_window_entry), GTK_POLICY_NEVER,
        GTK_POLICY_AUTOMATIC);
    g_signal_connect(widgets->scrolled_window_entry, "edge-reached",
                     G_CALLBACK(load_more_entries), widgets);
    for (int i = 0; i < found; i++)
    {
        GtkWidget *button = create_button(ids[i], widgets);
        gtk_list_box_insert(GTK_LIST_BOX(widgets->entry_list), button, -1);
    }
    free(ids);

    /* Setup clear all */
    widgets->clear_all = gtk_button_new_with_label("Clear All");
    widgets->confirm_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    widgets->confirm_label = gtk_label_new("Are you sure you want to delete everything?");
    widgets->confirm_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    widgets->confirm_yes = gtk_button_new_with_label("Yes");
    widgets->confirm_no = gtk_button_new_with_label("No");
    gtk_button_set_has_frame(GTK_BUTTON(widgets->confirm_no), FALSE);
    gtk_button_set_has_frame(GTK_BUTTON(widgets->confirm_yes), FALSE);
    g_signal_connect(widgets->clear_all, "clicked", G_CALLBACK(confirm_clear_all), widgets);
    g_signal_connect(widgets->confirm_no, "clicked", G_CALLBACK(clear_all_no), widgets);
    g_signal_connect(widgets->confirm_yes, "clicked", G_CALLBACK(clear_all_yes), widgets);
    gtk_box_prepend(GTK_BOX(widgets->confirm_vbox), widgets->confirm_label);
    gtk_box_append(GTK_BOX(widgets->confirm_vbox), widgets->confirm_hbox);
    gtk_box_append(GTK_BOX(widgets->confirm_hbox), widgets->confirm_yes);
    gtk_box_append(GTK_BOX(widgets->confirm_hbox), widgets->confirm_no);

    /* Wow, more formatting code */
    gtk_widget_set_hexpand(widgets->entry_list, TRUE);
    gtk_widget_set_vexpand(widgets->entry_list, TRUE);
    gtk_widget_set_hexpand(widgets->search_list, TRUE);
    gtk_widget_set_vexpand(widgets->search_list, TRUE);
    gtk_widget_set_hexpand(widgets->no_entry, TRUE);
    gtk_widget_set_vexpand(widgets->no_entry, TRUE);
    gtk_widget_set_hexpand(widgets->no_match, TRUE);
    gtk_widget_set_vexpand(widgets->no_match, TRUE);
    gtk_widget_set_hexpand(widgets->confirm_label, TRUE);
    gtk_widget_set_vexpand(widgets->confirm_label, TRUE);
    gtk_widget_set_hexpand(widgets->confirm_hbox, TRUE);
    gtk_widget_set_vexpand(widgets->confirm_hbox, TRUE);
    gtk_widget_set_hexpand(widgets->confirm_yes, TRUE);
    gtk_widget_set_vexpand(widgets->confirm_yes, TRUE);
    gtk_widget_set_hexpand(widgets->confirm_no, TRUE);
    gtk_widget_set_vexpand(widgets->confirm_no, TRUE);
    gtk_widget_set_valign(widgets->scrolled_window_entry, GTK_ALIGN_FILL);
    gtk_widget_set_valign(widgets->scrolled_window_search, GTK_ALIGN_FILL);
    gtk_widget_set_valign(widgets->back_list, GTK_ALIGN_FILL);
    gtk_widget_set_valign(widgets->no_entry, GTK_ALIGN_FILL);
    gtk_widget_set_valign(widgets->search_bar, GTK_ALIGN_START);
    gtk_widget_set_valign(widgets->clear_all, GTK_ALIGN_END);
    gtk_box_set_homogeneous(GTK_BOX(widgets->back_list), FALSE);

    /* Pack the main box */
    gtk_box_prepend(GTK_BOX(widgets->back_list), widgets->search_bar);
    gtk_box_append(GTK_BOX(widgets->back_list),
                   widgets->scrolled_window_search);
    gtk_box_append(GTK_BOX(widgets->back_list), widgets->scrolled_window_entry);
    gtk_box_append(GTK_BOX(widgets->back_list), widgets->no_entry);
    gtk_box_append(GTK_BOX(widgets->back_list), widgets->no_match);
    gtk_box_append(GTK_BOX(widgets->back_list), widgets->confirm_vbox);
    gtk_box_append(GTK_BOX(widgets->back_list), widgets->clear_all);

    gtk_widget_set_visible(widgets->no_match, FALSE);
    gtk_widget_set_visible(widgets->scrolled_window_search, FALSE);
    gtk_widget_set_visible(widgets->confirm_vbox, FALSE);
    if (found)
    {
        gtk_widget_set_visible(widgets->no_entry, FALSE);
    }
    else
    {
        gtk_widget_set_visible(widgets->scrolled_window_entry, FALSE);
    }

    gtk_window_set_child(GTK_WINDOW(widgets->window), widgets->back_list);
    gtk_window_present(GTK_WINDOW(widgets->window));
}

int main(int argc, char *argv[])
{
    GtkApplication *app = gtk_application_new("com.github.artsymacaw.kaprica",
                                              G_APPLICATION_DEFAULT_FLAGS);
    db = database_open();
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    database_destroy(db);
}
