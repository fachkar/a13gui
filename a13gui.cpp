#include <gtk/gtk.h>
#include <curl/curl.h>
#include <unistd.h>
#include <stdlib.h>
#include <linux/string.h>
#include "dwa13.h"


/*
 * dw-message: Success.
 * dw-error: 0
 * dw-serialid: 0
 * dw-brands: datawind.ubi.in:datawind.ubi.uk:Datawind.ubi.zm
 * dw-models: DW-UBT7Ri:DW-UBT7R+:DW-UBT7Ci:DW-UBT7C+:DW-UBT7CZ+:DW-UBT73G
 *
 */

char * mac_add = NULL;
char *syncMsgMarkup;
GtkWidget *mainVBox;
GtkWidget *syncLabel,*syncVBox, *syncHBox, *syncAlignment;
GtkWidget *usernameHBox, *usernameLabel, *usernameEntry;
GtkWidget *passHBox, *passLabel, *passEntry;
GtkWidget *syncButton;

GtkWidget *progVBox, *brandmodelHBox, *msgHBox;
GtkWidget *brandCombo, *modelCombo, *progButton;
GtkWidget *progMsgLable;

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    mem->memory = (char*)realloc(mem->memory, mem->size + realsize + 1);
    if (mem->memory == NULL) {
        /* out of memory! */
        printf("not enough memory (realloc returned NULL)\n");
        exit(EXIT_FAILURE);
    }

    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

static gboolean
timer_handler(GtkWidget *widget)
{
    gtk_widget_hide(syncLabel);
    gtk_widget_show_all(usernameHBox);
    gtk_widget_show_all(passHBox);
    gtk_widget_set_sensitive(syncButton,TRUE);
    return FALSE;
}

void prog_clicked(GtkWidget *widget, gpointer data)
{
    gtk_widget_set_sensitive(progButton,TRUE);
    gtk_widget_set_sensitive(brandCombo,TRUE);
    gtk_widget_set_sensitive(modelCombo,TRUE);
    syncMsgMarkup = g_markup_printf_escaped ("<span foreground=\"blue\">%s</span>", "Programming ..    ");
    gtk_label_set_markup(GTK_LABEL(progMsgLable), syncMsgMarkup);
    g_free (syncMsgMarkup);
}

void sync_clicked(GtkWidget *widget, gpointer data)
{
    char* response;
    char *postrequest;
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;

    chunk.memory = (char*)malloc(1);
    chunk.size = 0;

    syncMsgMarkup = g_markup_printf_escaped ("<span foreground=\"blue\">%s</span>", "Sync-ing ..    ");
    gtk_label_set_markup(GTK_LABEL(syncLabel), syncMsgMarkup);
    g_free (syncMsgMarkup);

    gtk_widget_hide_all(usernameHBox);
    gtk_widget_hide_all(passHBox);
    gtk_widget_show(syncLabel);
    gtk_widget_set_sensitive(syncButton,FALSE);

    asprintf(&postrequest, "provider=%s&providerkey=%s&clientid=%s&action=init", "DWND_India", "818a9c29", g_get_host_name());
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://10.0.0.26/progserver/touchrequest.jsp");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postrequest);

        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEHEADER, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            goto curl_failed;
        }

        syncMsgMarkup = g_markup_printf_escaped ("<b>%lu</b>", (long)chunk.size);
        gtk_label_set_markup(GTK_LABEL(syncLabel), syncMsgMarkup);
        g_free (syncMsgMarkup);

        asprintf(&response,"%s",chunk.memory);
        char * pdw_messageStart = strstr(response, "dw-message:");
        if (pdw_messageStart != NULL) {
            char * pdw_messageEnd = strstr(pdw_messageStart + 0x0c, "\r\n");
            if (pdw_messageEnd != NULL) {
                char *dw_message = (char*) malloc(strlen(pdw_messageStart) - strlen(pdw_messageEnd));
                if (dw_message != NULL) {
                    memset(dw_message,0, strlen(pdw_messageStart) - strlen(pdw_messageEnd));
                    strncpy(dw_message, pdw_messageStart + 0x0c, strlen(pdw_messageStart) - strlen(pdw_messageEnd) - 0x0c );
                    if (strcmp(dw_message, "Success.")== 0) {
                        syncMsgMarkup = g_markup_printf_escaped ("<span foreground=\"blue\">%s</span>", "Sync Successful ;)    ");
                        gtk_label_set_markup(GTK_LABEL(syncLabel), syncMsgMarkup);
                        g_free (syncMsgMarkup);
                    } else {
                        goto server_error_msg;
                    }
                } else {
                    goto malloc_failure;
                }
            }
        } else {
            goto wrong_server_response;
        }

        curl_easy_cleanup(curl);
    } else {
        goto curl_failed;
    }

    curl_global_cleanup();

    if (chunk.memory)
        free(chunk.memory);
    return;

curl_failed:
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    if (chunk.memory)
        free(chunk.memory);

    syncMsgMarkup = g_markup_printf_escaped ("<span foreground=\"red\">%s</span>", "Error connecting to server!    ");
    gtk_label_set_markup(GTK_LABEL(syncLabel), syncMsgMarkup);
    g_free (syncMsgMarkup);
    g_timeout_add(10000, (GSourceFunc) timer_handler, NULL);

wrong_server_response:
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    if (chunk.memory)
        free(chunk.memory);

    syncMsgMarkup = g_markup_printf_escaped ("<span foreground=\"red\">%s</span>", "Wrong server response!    ");
    gtk_label_set_markup(GTK_LABEL(syncLabel), syncMsgMarkup);
    g_free (syncMsgMarkup);
    g_timeout_add(10000, (GSourceFunc) timer_handler, NULL);

malloc_failure:
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    if (chunk.memory)
        free(chunk.memory);

    syncMsgMarkup = g_markup_printf_escaped ("<span foreground=\"red\">%s</span>", "System Low on memory, try to close and re-run this application!    ");
    gtk_label_set_markup(GTK_LABEL(syncLabel), syncMsgMarkup);
    g_free (syncMsgMarkup);
    g_timeout_add(10000, (GSourceFunc) timer_handler, NULL);

server_error_msg:
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    if (chunk.memory)
        free(chunk.memory);

    syncMsgMarkup = g_markup_printf_escaped ("<span foreground=\"red\">%s</span> <b>%s</b>", "Authentication Error! ", "Check user name and password and retry");
    gtk_label_set_markup(GTK_LABEL(syncLabel), syncMsgMarkup);
    g_free (syncMsgMarkup);
    g_timeout_add(10000, (GSourceFunc) timer_handler, NULL);
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
//     gtk_window_set_default_size(GTK_WINDOW(window), 800, 480);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(window), 5);
    gtk_window_set_icon(GTK_WINDOW(window), icon_pix);

    g_signal_connect_swapped(G_OBJECT(window), "destroy",
                             G_CALLBACK(gtk_main_quit), G_OBJECT(window));

    mainVBox = gtk_vbox_new(FALSE, 5);
    gtk_container_add(GTK_CONTAINER(window), syncAlignment);

    syncAlignment = gtk_alignment_new(0.5f, 0, 0, 0);
    gtk_container_add(GTK_CONTAINER(mainVBox), syncAlignment);

    syncVBox = gtk_vbox_new(FALSE, 5);
    gtk_container_add(GTK_CONTAINER(syncAlignment), syncVBox);

    progVBox = gtk_vbox_new(FALSE, 5);
    gtk_container_add(GTK_CONTAINER(mainVBox), progVBox);

    syncHBox = gtk_hbox_new(FALSE, 5);
    gtk_container_add(GTK_CONTAINER(syncVBox), syncHBox);

    syncLabel = gtk_label_new(NULL);
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


    brandmodelHBox = gtk_hbox_new(FALSE, 5);
    gtk_container_add(GTK_CONTAINER(progVBox), brandmodelHBox);

    msgHBox = gtk_hbox_new(FALSE, 5);
    gtk_container_add(GTK_CONTAINER(progVBox), msgHBox);

    brandCombo = gtk_combo_box_new_text();
    gtk_box_pack_start(GTK_BOX(brandmodelHBox), brandCombo, FALSE, FALSE, 0);

    modelCombo = gtk_combo_box_new_text();
    gtk_box_pack_start(GTK_BOX(brandmodelHBox), modelCombo, FALSE, FALSE, 0);


    progButton = gtk_button_new_with_label("Program Tablet");
    gtk_widget_set_size_request(progButton, 100, 30);
    g_signal_connect(G_OBJECT(progButton), "clicked",
                     G_CALLBACK(prog_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(brandmodelHBox), progButton, FALSE, FALSE, 5);

    progMsgLable = gtk_label_new(NULL);
    gtk_label_set_justify(GTK_LABEL(progMsgLable), GTK_JUSTIFY_LEFT);
    gtk_container_add(GTK_CONTAINER(msgHBox), progMsgLable);


    gtk_widget_show_all(window);
    gtk_widget_hide(syncLabel);

    gtk_main();

    return 0;
}
