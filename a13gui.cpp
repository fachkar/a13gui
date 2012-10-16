#include <gtk/gtk.h>
#include "dwa13.h"

GtkWidget *syncLabel,*syncVBox, *syncHBox, *syncAlignment;
GtkWidget *usernameHBox, *usernameLabel, *usernameEntry;
GtkWidget *passHBox, *passLabel, *passEntry;
GtkWidget *syncButton;


static gboolean
timer_handler(GtkWidget *widget)
{
    gtk_widget_hide(syncLabel);
    gtk_widget_show_all(usernameHBox);
    gtk_widget_show_all(passHBox);
    gtk_widget_set_sensitive(syncButton,TRUE);
    return FALSE;
}


void sync_clicked(GtkWidget *widget, gpointer data)
{
    gtk_widget_hide_all(usernameHBox);
    gtk_widget_hide_all(passHBox);
    gtk_widget_show(syncLabel);
    gtk_widget_set_sensitive(syncButton,FALSE);
    g_timeout_add(5000, (GSourceFunc) timer_handler, NULL);
}

int main( int argc, char *argv[])
{
    GtkWidget *window;
    GdkPixbuf *icon_pix;

    gtk_init(&argc, &argv);

    icon_pix = gdk_pixbuf_new ( GDK_COLORSPACE_RGB, FALSE, 8, iconwidth, iconheight );
    guchar * icon_pixels = gdk_pixbuf_get_pixels ( icon_pix );
    gint iconrowstride = gdk_pixbuf_get_rowstride ( icon_pix );
    for ( int y = 0; y < iconheight; ++y )
    {
        for ( int x = 0; x < iconwidth; ++x )
        {
            ICON_HEADER_PIXEL ( icon_header_data, ( icon_pixels + ( y * iconrowstride ) + ( x * 3 ) ) );
        }
    }

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Datawind A13 Apps Installer");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 480);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(window), 5);
    gtk_window_set_icon(GTK_WINDOW(window), icon_pix);

    g_signal_connect_swapped(G_OBJECT(window), "destroy",
                             G_CALLBACK(gtk_main_quit), G_OBJECT(window));

    syncAlignment = gtk_alignment_new(0.5f, 0, 0, 0);
    gtk_container_add(GTK_CONTAINER(window), syncAlignment);

    syncVBox = gtk_vbox_new(FALSE, 5);
    gtk_container_add(GTK_CONTAINER(syncAlignment), syncVBox);

    syncHBox = gtk_hbox_new(FALSE, 5);
    gtk_container_add(GTK_CONTAINER(syncVBox), syncHBox);

    syncLabel = gtk_label_new("Checking ...");
    gtk_label_set_justify(GTK_LABEL(syncLabel), GTK_JUSTIFY_LEFT);
    gtk_container_add(GTK_CONTAINER(syncHBox), syncLabel);

    usernameHBox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(syncHBox), usernameHBox, FALSE, FALSE, 5);

    passHBox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(syncHBox), passHBox, FALSE, FALSE, 5);

    syncButton = gtk_button_new_with_label("Server Sync");
    gtk_widget_set_size_request(syncButton, 100, 30);
    g_signal_connect(G_OBJECT(syncButton), "clicked",
                     G_CALLBACK(sync_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(syncHBox), syncButton, FALSE, FALSE, 5);

    usernameLabel = gtk_label_new ("User Name");
    usernameEntry = gtk_entry_new();


    gtk_box_pack_start(GTK_BOX(usernameHBox), usernameLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(usernameHBox), usernameEntry, FALSE, FALSE, 0);

    passLabel = gtk_label_new ("Password");
    passEntry = gtk_entry_new();


    gtk_box_pack_start(GTK_BOX(passHBox), passLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(passHBox), passEntry, FALSE, FALSE, 0);

    gtk_widget_show_all(window);
    gtk_widget_hide(syncLabel);

    gtk_main();

    return 0;
}
