#include <gtk/gtk.h>
#include <curl/curl.h>
#include <unistd.h>
#include <stdlib.h>
#include <linux/string.h>
#include "dwa13.h"

#define DEBUGO

char * mac_add = NULL;
char * firmware = NULL;
char *serialno = NULL;
char *syncMsgMarkup = NULL;
char *dw_message = NULL;
char *dw_brands = NULL;
char *dw_models = NULL;
char *allfpipe = NULL;


GtkWidget *window;
GdkPixbuf *icon_pix;

GtkWidget *mainVBox;
GtkWidget *syncLabel,*syncVBox, *syncHBox, *syncAlignment;
GtkWidget *usernameHBox, *usernameLabel, *usernameEntry;
GtkWidget *passHBox, *passLabel, *passEntry;
GtkWidget *syncButton;

GtkWidget *progVBox, *brandHBox, *modelHBox, *brandmodelHBox, *msgHBox;
GtkWidget *brandLabel, *brandCombo, *modelLabel, *modelCombo, *progButton;
GtkWidget *progHSeparator;
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

gboolean issue_adb_command(char * command)
{
    gboolean ret = TRUE;
    FILE *fpipe;
    char line[256];

    memset(line,0, sizeof line);
    if ( !(fpipe = (FILE*)popen(command,"r")) )
    {  // If fpipe is NULL
        asprintf(&allfpipe, "%s", "fpipe is NULL");
        ret = FALSE;
        return ret;
    }

    asprintf(&allfpipe, "%s", " ");
    while ( fgets( line, sizeof line, fpipe))
    {
        char *tmpstr = NULL;
        if (allfpipe!=NULL) {
            asprintf(&tmpstr, "%s", allfpipe);
            free(allfpipe);
            allfpipe = NULL;
        }
        asprintf(&allfpipe, "%s%s", tmpstr, line);
        if (tmpstr!=NULL)
            free(tmpstr);
    }
    pclose(fpipe);
    return ret;
}

static gboolean
program_handler(GtkWidget *widget)
{
    syncMsgMarkup = g_markup_printf_escaped ("<span size=\"x-large\">  You can start programming with this Brand/Model combination   </span><span foreground=\"green\" size=\"x-large\"> %s </span>", "-->>");
    gtk_label_set_markup(GTK_LABEL(progMsgLable), syncMsgMarkup);
    g_free (syncMsgMarkup);
    gtk_widget_show(progButton);
    return FALSE;
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

static gboolean
prog_clicked_handler(GtkWidget *widget)
{
    GtkWidget *dialog;
    char *command = NULL;
    char *pch = NULL;
    char* response;
    char *postrequest;
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;

    chunk.memory = (char*)malloc(1);
    chunk.size = 0;

    struct MemoryStruct bodyChunk;

    bodyChunk.memory = (char*)malloc(1);
    bodyChunk.size = 0;

    gchar * currentDir = g_get_current_dir();
    if (currentDir != NULL) {
        char * pnuclearOutputFound = strstr(currentDir, "out/target/product/nuclear-evb");
        g_free(currentDir);

        if (pnuclearOutputFound != NULL) {

            ///checking if any devices are out there
            if (asprintf(&command, "%s", "adb devices") < 0) {
                goto malloc_failure;
            }

            if (!issue_adb_command(command)) {
                goto malloc_failure;
            }

            if ( strlen(allfpipe) < strlen("List of devices attached\n\n") + 10 ) {
                syncMsgMarkup = g_markup_printf_escaped ("<span foreground=\"red\" size=\"large\">%s</span><b>Make sure:\n 1. USB cable is good\n 2. 'USB debugging' mode on the connected Tablet is checked under Settings/Developer Options\n 3. Both debug and USB connection icons are showing on the Tablet</b>", "No tablets are connected!\n");
                dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                                GTK_DIALOG_DESTROY_WITH_PARENT,
                                                GTK_MESSAGE_ERROR,
                                                GTK_BUTTONS_OK,
                                                syncMsgMarkup);
                gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog),  syncMsgMarkup);
                gtk_window_set_title(GTK_WINDOW(dialog), "Device Error");
                gtk_dialog_run(GTK_DIALOG(dialog));
                gtk_widget_destroy(dialog);
                g_free (syncMsgMarkup);

                syncMsgMarkup = g_markup_printf_escaped ("<span size=\"x-large\"> 'Program Tablet' button will be shown in 10 secs </span>");
                gtk_label_set_markup(GTK_LABEL(progMsgLable), syncMsgMarkup);
                g_free (syncMsgMarkup);
                free(command);
                command = NULL;
                free(allfpipe);
                allfpipe = NULL;

                g_timeout_add(10000, (GSourceFunc) program_handler, NULL);

                return FALSE;
            }
            free(command);
            command = NULL;
            free(allfpipe);
            allfpipe = NULL;


            if (asprintf(&command, "%s", "adb shell netcfg | grep wlan0") < 0) {
                goto malloc_failure;
            }
            if (!issue_adb_command(command)) {
                goto malloc_failure;
            }

            if ( strstr(allfpipe, "error: device not found") != NULL ) {
                goto no_device_error;
            }

            pch = strstr(allfpipe, "00:");
            if (pch) {
                char * pchEnd = strstr(pch, "\r\n");
                if (pchEnd != NULL) {
                    char * tmpmac = (char*) malloc(strlen(pch) - strlen(pchEnd));
                    if (tmpmac != NULL) {
                        memset(tmpmac, 0, strlen(pch) - strlen(pchEnd));
                        strncpy(tmpmac, pch, strlen(pch) - strlen(pchEnd) );
                        asprintf(&mac_add, "%s", tmpmac);
                    } else {
                        goto malloc_failure;
                    }
                    if (tmpmac)
                        free(tmpmac);
                } else
                    asprintf(&mac_add, "%s", pch);
            } else {
                syncMsgMarkup = g_markup_printf_escaped ("<span foreground=\"red\" size=\"large\">%s</span><b>Make sure the WiFi is enabled on the Tablet</b>", "WiFi Not Enabled!\n");
                dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                                GTK_DIALOG_DESTROY_WITH_PARENT,
                                                GTK_MESSAGE_ERROR,
                                                GTK_BUTTONS_OK,
                                                syncMsgMarkup);
                gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog),  syncMsgMarkup);
                gtk_window_set_title(GTK_WINDOW(dialog), "Device Error");
                gtk_dialog_run(GTK_DIALOG(dialog));
                gtk_widget_destroy(dialog);
                g_free (syncMsgMarkup);

                syncMsgMarkup = g_markup_printf_escaped ("<span size=\"x-large\"> 'Program Tablet' button will be shown in 10 secs </span>");
                gtk_label_set_markup(GTK_LABEL(progMsgLable), syncMsgMarkup);
                g_free (syncMsgMarkup);
                free(command);
                command = NULL;
                free(allfpipe);
                allfpipe = NULL;

                g_timeout_add(10000, (GSourceFunc) program_handler, NULL);

                return FALSE;
            }

            free(command);
            free(allfpipe);
            command = NULL;
            allfpipe = NULL;


            // get firmware version
            if (asprintf(&command, "%s", "adb shell getprop ro.build.version.release") < 0) {
                goto malloc_failure;
            }
            if (!issue_adb_command(command)) {
                goto malloc_failure;
            }

            if ( strstr(allfpipe, "error: device not found") != NULL ) {
                goto no_device_error;
            }

            pch = allfpipe;
            if (strlen(allfpipe) > 0) {
                char * pchEnd = strstr(pch, "\r\n");
                if (pchEnd != NULL) {
                    char * tmpmac = (char*) malloc(strlen(pch) - strlen(pchEnd));
                    if (tmpmac != NULL) {
                        memset(tmpmac, 0, strlen(pch) - strlen(pchEnd));
                        strncpy(tmpmac, pch, strlen(pch) - strlen(pchEnd) );
                        asprintf(&firmware, "%s", tmpmac);
                    } else {
                        goto malloc_failure;
                    }
                    if (tmpmac)
                        free(tmpmac);
                } else
                    asprintf(&firmware, "%s", pch);
            } else {
                asprintf(&firmware, "%s", "4.0.4");
            }

            free(command);
            free(allfpipe);
            command = NULL;
            allfpipe = NULL;

            asprintf(&postrequest, "provider=%s&providerkey=%s&clientid=%s&action=getserialid&brand=%s&model=%s&firmware=%s&macid=%s", gtk_entry_get_text(GTK_ENTRY(usernameEntry)), gtk_entry_get_text(GTK_ENTRY(passEntry)), g_get_host_name()
                     , gtk_combo_box_get_active_text((GtkComboBox*)brandCombo), gtk_combo_box_get_active_text((GtkComboBox*)modelCombo), firmware, mac_add);

            if (mac_add)
                free(mac_add);
            if (firmware)
                free(firmware);

            curl_global_init(CURL_GLOBAL_ALL);
            curl = curl_easy_init();
            if (curl) {
                curl_easy_setopt(curl, CURLOPT_URL, "http://10.0.0.26/progserver/touchrequest.jsp");
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postrequest);

                curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
                curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
                curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, WriteMemoryCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEHEADER, (void *)&chunk);
                curl_easy_setopt(curl,  CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&bodyChunk);
                curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

                res = curl_easy_perform(curl);

                if (res != CURLE_OK) {
                    goto curl_failed;
                }

                asprintf(&response,"%s",chunk.memory);
                char * pdw_messageStart = strstr(response, "dw-message: ");
                if (pdw_messageStart != NULL) {
                    char * pdw_messageEnd = strstr(pdw_messageStart + strlen("dw-message: "), "\r\n");
                    if (pdw_messageEnd != NULL) {
                        dw_message = (char*) malloc(strlen(pdw_messageStart) - strlen(pdw_messageEnd));
                        if (dw_message != NULL) {
                            memset(dw_message,0, strlen(pdw_messageStart) - strlen(pdw_messageEnd));
                            strncpy(dw_message, pdw_messageStart + strlen("dw-message: "), strlen(pdw_messageStart) - strlen(pdw_messageEnd) - strlen("dw-message: ") );
                            if (strcmp(dw_message, "Success.")== 0) {
                                char * pdw_serialidStart = strstr(response, "dw-serialid: ");
                                if (pdw_serialidStart != NULL) {
                                    char * pdw_serialidEnd = strstr(pdw_serialidStart + strlen("dw-serialid: "), "\r\n");
                                    if (pdw_serialidEnd != NULL) {
                                        char *dw_serialid = (char*) malloc(strlen(pdw_serialidStart) - strlen(pdw_serialidEnd));
                                        if (dw_serialid != NULL) {
                                            memset(dw_serialid,0, strlen(pdw_serialidStart) - strlen(pdw_serialidEnd));
                                            strncpy(dw_serialid, pdw_serialidStart + strlen("dw-serialid: "), strlen(pdw_serialidStart) - strlen(pdw_serialidEnd) - strlen("dw-serialid: ") );
                                            asprintf(&serialno,"%s",dw_serialid);
                                        } else {
                                            goto malloc_failure;
                                        }
                                        if (dw_serialid)
                                            free(dw_serialid);
                                    } else {
                                        goto wrong_server_response;
                                    }
                                } else {
                                    goto wrong_server_response;
                                }

                            } else {
                                goto server_error_msg;
                            }
                        } else {
                            goto malloc_failure;
                        }
                        if (dw_message)
                            free(dw_message);
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
            if (bodyChunk.memory)
                free(bodyChunk.memory);

            if (postrequest)
                free(postrequest);
            if (response)
                free(response);

            syncMsgMarkup = g_markup_printf_escaped ("<span foreground=\"blue\" size=\"large\">%s</span><b> %s </b>", "Got Serial\n", serialno);
            dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                            GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_MESSAGE_INFO,
                                            GTK_BUTTONS_OK,
                                            syncMsgMarkup);
            gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog),  syncMsgMarkup);
            gtk_window_set_title(GTK_WINDOW(dialog), "Success");
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
            g_free (syncMsgMarkup);

            syncMsgMarkup = g_markup_printf_escaped ("<span size=\"x-large\"> 'Program Tablet' button will be shown in 10 secs </span>");
            gtk_label_set_markup(GTK_LABEL(progMsgLable), syncMsgMarkup);
            g_free (syncMsgMarkup);
            g_timeout_add(10000, (GSourceFunc) program_handler, NULL);
            return FALSE;

            system("adb shell 'echo \"halt\" > /sys/power/wake_lock'");

            if (asprintf(&command, "%s", "adb shell 'cat /dev/block/nandc > /mnt/sdcard/nandc.img'") < 0) {
                goto malloc_failure;
            }

            if (!issue_adb_command(command)) {
                goto malloc_failure;
            }
            free(command);
            free(allfpipe);
            command = NULL;
            allfpipe = NULL;

        } else {
            syncMsgMarkup = g_markup_printf_escaped ("<span foreground=\"red\" size=\"large\">%s</span><b>please close program and re-launch it from console from the following path <span foreground=\"blue\" size=\"large\">%s</span></b>", "wrong program path!\n" , "~/android4.0/out/target/product/nuclear-evb/");

            GtkWidget *dialog;
            dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                            GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_OK,
                                            syncMsgMarkup);
            gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog),  syncMsgMarkup);
            gtk_window_set_title(GTK_WINDOW(dialog), "Usage Error");
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
            g_free (syncMsgMarkup);

            gtk_widget_hide(progVBox);
        }
    }

    return FALSE;

curl_failed:
    syncMsgMarkup = g_markup_printf_escaped ("<span foreground=\"red\" size=\"large\">%s</span><b> %s </b>", "Network Error!\n", "Error connecting to server!");
    dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_OK,
                                    syncMsgMarkup);
    gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog),  syncMsgMarkup);
    gtk_window_set_title(GTK_WINDOW(dialog), "Server Error");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    g_free (syncMsgMarkup);

    syncMsgMarkup = g_markup_printf_escaped ("<span size=\"x-large\"> 'Program Tablet' button will be shown in 10 secs </span>");
    gtk_label_set_markup(GTK_LABEL(progMsgLable), syncMsgMarkup);
    g_free (syncMsgMarkup);
    if (command)
        free(command);
    if (allfpipe)
        free(allfpipe);

    if (postrequest)
        free(postrequest);
    if (response)
        free(response);
    if (chunk.memory)
        free(chunk.memory);
    if (bodyChunk.memory)
        free(bodyChunk.memory);
    if (dw_message)
        free(dw_message);

    g_timeout_add(10000, (GSourceFunc) program_handler, NULL);
    return FALSE;

server_error_msg:
    syncMsgMarkup = g_markup_printf_escaped ("<span foreground=\"red\" size=\"large\">%s</span><b> %s </b>", "Wrong server response!\n", dw_message);
    dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_OK,
                                    syncMsgMarkup);
    gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog),  syncMsgMarkup);
    gtk_window_set_title(GTK_WINDOW(dialog), "Server Error");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    g_free (syncMsgMarkup);

    syncMsgMarkup = g_markup_printf_escaped ("<span size=\"x-large\"> 'Program Tablet' button will be shown in 10 secs </span>");
    gtk_label_set_markup(GTK_LABEL(progMsgLable), syncMsgMarkup);
    g_free (syncMsgMarkup);
    if (command)
        free(command);
    if (allfpipe)
        free(allfpipe);

    if (postrequest)
        free(postrequest);
    if (response)
        free(response);
    if (chunk.memory)
        free(chunk.memory);
    if (bodyChunk.memory)
        free(bodyChunk.memory);
    if (dw_message)
        free(dw_message);

    g_timeout_add(10000, (GSourceFunc) program_handler, NULL);
    return FALSE;

wrong_server_response:
    syncMsgMarkup = g_markup_printf_escaped ("<span foreground=\"red\" size=\"large\">%s</span><b> Got incorrect serial values from server! </b>", "Wrong server response!\n");
    dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_OK,
                                    syncMsgMarkup);
    gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog),  syncMsgMarkup);
    gtk_window_set_title(GTK_WINDOW(dialog), "Server Error");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    g_free (syncMsgMarkup);

    syncMsgMarkup = g_markup_printf_escaped ("<span size=\"x-large\"> 'Program Tablet' button will be shown in 10 secs </span>");
    gtk_label_set_markup(GTK_LABEL(progMsgLable), syncMsgMarkup);
    g_free (syncMsgMarkup);
    if (command)
        free(command);
    if (allfpipe)
        free(allfpipe);

    if (postrequest)
        free(postrequest);
    if (response)
        free(response);
    if (chunk.memory)
        free(chunk.memory);
    if (bodyChunk.memory)
        free(bodyChunk.memory);
    if (dw_message)
        free(dw_message);
    g_timeout_add(10000, (GSourceFunc) program_handler, NULL);

    return FALSE;

no_device_error:
    syncMsgMarkup = g_markup_printf_escaped ("<span foreground=\"red\" size=\"large\">%s</span><b>Make sure:\n 1. USB cable is good\n 2. 'USB debugging' mode on the connected Tablet is checked under Settings/Developer Options\n 3. Both debug and USB connection icons are showing on the Tablet</b>", "Tablet USB dis-connected!\n");
    dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_OK,
                                    syncMsgMarkup);
    gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog),  syncMsgMarkup);
    gtk_window_set_title(GTK_WINDOW(dialog), "Device Error");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    g_free (syncMsgMarkup);

    syncMsgMarkup = g_markup_printf_escaped ("<span size=\"x-large\"> 'Program Tablet' button will be shown in 10 secs </span>");
    gtk_label_set_markup(GTK_LABEL(progMsgLable), syncMsgMarkup);
    g_free (syncMsgMarkup);
    if (command)
        free(command);
    if (allfpipe)
        free(allfpipe);

    if (postrequest)
        free(postrequest);
    if (response)
        free(response);
    if (chunk.memory)
        free(chunk.memory);
    if (bodyChunk.memory)
        free(bodyChunk.memory);
    if (dw_message)
        free(dw_message);
    g_timeout_add(10000, (GSourceFunc) program_handler, NULL);

    return FALSE;


malloc_failure:
    if (command)
        free(command);
    if (allfpipe)
        free(allfpipe);

    if (postrequest)
        free(postrequest);
    if (response)
        free(response);
    if (chunk.memory)
        free(chunk.memory);
    if (bodyChunk.memory)
        free(bodyChunk.memory);

    if (dw_message)
        free(dw_message);

    syncMsgMarkup = g_markup_printf_escaped ("<span foreground=\"red\" size=\"large\">%s</span><b>please close any other running Apps and try again!</b>", "Not enough system resources!\n");
    dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_OK,
                                    syncMsgMarkup);
    gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog),  syncMsgMarkup);
    gtk_window_set_title(GTK_WINDOW(dialog), "Critical Error");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    g_free (syncMsgMarkup);

    gtk_widget_hide(progVBox);
    return FALSE;
}


static gboolean
sync_clicked_handler(GtkWidget *widget)
{
    char* response;
    char *postrequest;
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;

    chunk.memory = (char*)malloc(1);
    chunk.size = 0;

    struct MemoryStruct bodyChunk;

    bodyChunk.memory = (char*)malloc(1);
    bodyChunk.size = 0;

    asprintf(&postrequest, "provider=%s&providerkey=%s&clientid=%s&action=init", gtk_entry_get_text(GTK_ENTRY(usernameEntry)), gtk_entry_get_text(GTK_ENTRY(passEntry)), g_get_host_name());
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://10.0.0.26/progserver/touchrequest.jsp");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postrequest);

        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEHEADER, (void *)&chunk);
        curl_easy_setopt(curl,  CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&bodyChunk);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            goto curl_failed;
        }

        asprintf(&response,"%s",chunk.memory);
        char * pdw_messageStart = strstr(response, "dw-message: ");
        if (pdw_messageStart != NULL) {
            char * pdw_messageEnd = strstr(pdw_messageStart + strlen("dw-message: "), "\r\n");
            if (pdw_messageEnd != NULL) {
                dw_message = (char*) malloc(strlen(pdw_messageStart) - strlen(pdw_messageEnd));
                if (dw_message != NULL) {
                    memset(dw_message,0, strlen(pdw_messageStart) - strlen(pdw_messageEnd));
                    strncpy(dw_message, pdw_messageStart + strlen("dw-message: "), strlen(pdw_messageStart) - strlen(pdw_messageEnd) - strlen("dw-message: ") );
                    if (strcmp(dw_message, "Success.")== 0) {
                        char * pdw_brandsStart = strstr(response, "dw-brands: ");
                        if (pdw_brandsStart != NULL) {
                            char * pdw_brandsEnd = strstr(pdw_brandsStart + strlen("dw-brands: "), "\r\n");
                            if (pdw_brandsEnd != NULL) {
                                dw_brands = (char*) malloc(strlen(pdw_brandsStart) - strlen(pdw_brandsEnd));
                                if (dw_brands != NULL) {
                                    memset(dw_brands,0, strlen(pdw_brandsStart) - strlen(pdw_brandsEnd));
                                    strncpy(dw_brands, pdw_brandsStart + strlen("dw-brands: "), strlen(pdw_brandsStart) - strlen(pdw_brandsEnd) - strlen("dw-brands: ") );
                                    char * pch = strtok (dw_brands,":");
                                    while (pch != NULL)
                                    {
                                        gtk_combo_box_append_text ((GtkComboBox*) brandCombo, pch);
                                        pch = strtok (NULL, ":");
                                    }

                                    char * pdw_modelsStart = strstr(response, "dw-models: ");
                                    if (pdw_modelsStart != NULL) {
                                        char * pdw_modelsEnd = strstr(pdw_modelsStart + strlen("dw-models: "), "\r\n");
                                        if (pdw_modelsEnd != NULL) {
                                            dw_models = (char*) malloc(strlen(pdw_modelsStart) - strlen(pdw_modelsEnd));
                                            if (dw_models != NULL) {
                                                memset(dw_models, 0, strlen(pdw_modelsStart) - strlen(pdw_modelsEnd));
                                                strncpy(dw_models, pdw_modelsStart + strlen("dw-models: "), strlen(pdw_modelsStart) - strlen(pdw_modelsEnd) - strlen("dw-models: ") );
                                                char * pch = strtok (dw_models,":");
                                                while (pch != NULL)
                                                {
                                                    gtk_combo_box_append_text ((GtkComboBox*) modelCombo, pch);
                                                    pch = strtok (NULL, ":");
                                                }

                                                gtk_widget_hide(syncButton);
                                                syncMsgMarkup = g_markup_printf_escaped ("<span foreground=\"blue\">%s%lu bytes received ..</span>", "  Sync OK .. ", (long)chunk.size);
                                                gtk_label_set_markup(GTK_LABEL(syncLabel), syncMsgMarkup);
                                                g_free (syncMsgMarkup);

                                                syncMsgMarkup = g_markup_printf_escaped ("<span size=\"x-large\">  Select a </span><span foreground=\"blue\" size=\"x-large\">%s</span><span size=\"x-large\"> and </span><span foreground=\"blue\" size=\"x-large\">%s</span>", "Brand", "Model");
                                                gtk_label_set_markup(GTK_LABEL(progMsgLable), syncMsgMarkup);
                                                g_free (syncMsgMarkup);

                                                gtk_widget_show(brandmodelHBox);

                                            } else {
                                                goto malloc_failure;
                                            }
                                            if (dw_models)
                                                free(dw_models);
                                        } else {
                                            goto wrong_server_response;
                                        }
                                    } else {
                                        goto wrong_server_response;
                                    }

                                } else {
                                    goto malloc_failure;
                                }
                                if (dw_brands)
                                    free(dw_brands);
                            } else {
                                goto wrong_server_response;
                            }
                        } else {
                            goto wrong_server_response;
                        }

                    } else {
                        goto server_error_msg;
                    }
                } else {
                    goto malloc_failure;
                }
                if (dw_message)
                    free(dw_message);
            }
        } else {
            goto wrong_server_response;
        }

        curl_easy_cleanup(curl);
    } else {
        goto curl_failed;
    }

    curl_global_cleanup();

    if (postrequest)
        free(postrequest);
    if (response)
        free(response);
    if (chunk.memory)
        free(chunk.memory);
    if (bodyChunk.memory)
        free(bodyChunk.memory);
    return FALSE;

curl_failed:
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    if (postrequest)
        free(postrequest);
    if (response)
        free(response);
    if (chunk.memory)
        free(chunk.memory);
    if (bodyChunk.memory)
        free(bodyChunk.memory);

    if (dw_models)
        free(dw_models);
    if (dw_brands)
        free(dw_brands);
    if (dw_message)
        free(dw_message);

    syncMsgMarkup = g_markup_printf_escaped ("<span foreground=\"red\" size=\"large\">%s</span>", "Error connecting to server!    ");
    gtk_label_set_markup(GTK_LABEL(syncLabel), syncMsgMarkup);
    g_free (syncMsgMarkup);
    g_timeout_add(10000, (GSourceFunc) timer_handler, NULL);
    return FALSE;

wrong_server_response:
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    if (postrequest)
        free(postrequest);
    if (response)
        free(response);
    if (chunk.memory)
        free(chunk.memory);
    if (bodyChunk.memory)
        free(bodyChunk.memory);

    if (dw_models)
        free(dw_models);
    if (dw_brands)
        free(dw_brands);
    if (dw_message)
        free(dw_message);

    syncMsgMarkup = g_markup_printf_escaped ("<span foreground=\"red\" size=\"large\">%s</span>", "Wrong server response!    ");
    gtk_label_set_markup(GTK_LABEL(syncLabel), syncMsgMarkup);
    g_free (syncMsgMarkup);
    g_timeout_add(10000, (GSourceFunc) timer_handler, NULL);
    return FALSE;

malloc_failure:
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    if (postrequest)
        free(postrequest);
    if (response)
        free(response);
    if (chunk.memory)
        free(chunk.memory);
    if (bodyChunk.memory)
        free(bodyChunk.memory);

    if (dw_models)
        free(dw_models);
    if (dw_brands)
        free(dw_brands);
    if (dw_message)
        free(dw_message);

    syncMsgMarkup = g_markup_printf_escaped ("<span foreground=\"red\" size=\"large\">%s</span>", "System Low on memory, try to close and re-run this application!    ");
    gtk_label_set_markup(GTK_LABEL(syncLabel), syncMsgMarkup);
    g_free (syncMsgMarkup);
    g_timeout_add(10000, (GSourceFunc) timer_handler, NULL);
    return FALSE;

server_error_msg:
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    if (postrequest)
        free(postrequest);
    if (response)
        free(response);
    if (chunk.memory)
        free(chunk.memory);
    if (bodyChunk.memory)
        free(bodyChunk.memory);

    if (dw_models)
        free(dw_models);
    if (dw_brands)
        free(dw_brands);
    if (dw_message)
        free(dw_message);

    syncMsgMarkup = g_markup_printf_escaped ("<span foreground=\"red\" size=\"large\">%s</span> <b>%s</b>", "Authentication Error! ", "Check user name and password, you'll be able to retry in 10 secs ..");
    gtk_label_set_markup(GTK_LABEL(syncLabel), syncMsgMarkup);
    g_free (syncMsgMarkup);
    g_timeout_add(10000, (GSourceFunc) timer_handler, NULL);

    return FALSE;
}

void prog_clicked(GtkWidget *widget, gpointer data)
{
    gtk_widget_hide(progButton);

    syncMsgMarkup = g_markup_printf_escaped ("<span foreground=\"blue\" size=\"x-large\">%s</span>", "Programming ..    ");
    gtk_label_set_markup(GTK_LABEL(progMsgLable), syncMsgMarkup);
    g_free (syncMsgMarkup);

    g_timeout_add(1000, (GSourceFunc) prog_clicked_handler, NULL);

}

void combo_changed(GtkWidget *widget, gpointer data)
{
    gint actvBrand = gtk_combo_box_get_active((GtkComboBox*)brandCombo);
    gint actvModel = gtk_combo_box_get_active((GtkComboBox*)modelCombo);
    gchar * actvBrandText = gtk_combo_box_get_active_text((GtkComboBox*)brandCombo);
    gchar * actvModelText = gtk_combo_box_get_active_text((GtkComboBox*)modelCombo);

    if (actvBrand != -1 && actvModel != -1 && strlen(actvBrandText) > 0 && strlen(actvModelText) > 0) {
        gtk_widget_show(progButton);
        gtk_widget_set_sensitive(brandmodelHBox,FALSE);
        syncMsgMarkup = g_markup_printf_escaped ("<span size=\"x-large\">  You can start programming with this Brand/Model combination   </span><span foreground=\"green\" size=\"x-large\"> %s </span>", "-->>");
        gtk_label_set_markup(GTK_LABEL(progMsgLable), syncMsgMarkup);
        g_free (syncMsgMarkup);
    } else {
        gtk_widget_hide(progButton);
        gtk_widget_set_sensitive(brandmodelHBox,TRUE);
    }
}

void sync_clicked(GtkWidget *widget, gpointer data)
{
    gtk_widget_set_sensitive(syncButton,FALSE);
    syncMsgMarkup = g_markup_printf_escaped ("<span foreground=\"blue\">%s</span>", "Sync-ing ..    ");
    gtk_label_set_markup(GTK_LABEL(syncLabel), syncMsgMarkup);
    g_free (syncMsgMarkup);

    gtk_widget_hide_all(usernameHBox);
    gtk_widget_hide_all(passHBox);
    gtk_widget_show(syncLabel);

    g_timeout_add(1000, (GSourceFunc) sync_clicked_handler, NULL);
}

int main( int argc, char *argv[])
{
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

    syncAlignment = gtk_alignment_new(0.5f, 0, 0, 0);
    gtk_container_add(GTK_CONTAINER(window), syncAlignment);

    mainVBox = gtk_vbox_new(FALSE, 5);
    gtk_container_add(GTK_CONTAINER(syncAlignment), mainVBox);

    syncVBox = gtk_vbox_new(FALSE, 5);
    gtk_container_add(GTK_CONTAINER(mainVBox), syncVBox);

    progVBox = gtk_vbox_new(FALSE, 5);
    gtk_container_add(GTK_CONTAINER(mainVBox), progVBox);

    syncHBox = gtk_hbox_new(FALSE, 5);
    gtk_container_add(GTK_CONTAINER(syncVBox), syncHBox);

    syncLabel = gtk_label_new(NULL);
    gtk_label_set_justify(GTK_LABEL(syncLabel), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(syncHBox), syncLabel, FALSE, FALSE, 10);

    usernameHBox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(syncHBox), usernameHBox, FALSE, FALSE, 5);

    passHBox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(syncHBox), passHBox, FALSE, FALSE, 5);

    syncButton = gtk_button_new_with_label("Server Sync");
    gtk_widget_set_size_request(syncButton, 110, 30);
    g_signal_connect(G_OBJECT(syncButton), "clicked",
                     G_CALLBACK(sync_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(syncHBox), syncButton, FALSE, FALSE, 5);

    usernameLabel = gtk_label_new ("User Name");
    usernameEntry = gtk_entry_new();
#ifdef DEBUGO
    gtk_entry_set_text(GTK_ENTRY(usernameEntry),"DWND_India");
#endif

    gtk_box_pack_start(GTK_BOX(usernameHBox), usernameLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(usernameHBox), usernameEntry, FALSE, FALSE, 0);

    passLabel = gtk_label_new ("Password");
    passEntry = gtk_entry_new();
#ifdef DEBUGO
    gtk_entry_set_text(GTK_ENTRY(passEntry),"818a9c29");
#endif

    gtk_box_pack_start(GTK_BOX(passHBox), passLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(passHBox), passEntry, FALSE, FALSE, 0);


    brandmodelHBox = gtk_hbox_new(FALSE, 5);
    gtk_container_add(GTK_CONTAINER(progVBox), brandmodelHBox);

    progHSeparator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(progVBox), progHSeparator, FALSE, TRUE, 10);

    msgHBox = gtk_hbox_new(FALSE, 5);
    gtk_container_add(GTK_CONTAINER(progVBox), msgHBox);

    brandHBox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(brandmodelHBox), brandHBox, FALSE, FALSE, 5);

    modelHBox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(brandmodelHBox), modelHBox, FALSE, FALSE, 5);

    brandLabel = gtk_label_new ("Brand");
    gtk_widget_set_size_request(brandLabel, 60, 30);
    gtk_box_pack_start(GTK_BOX(brandHBox), brandLabel, FALSE, FALSE, 0);

    brandCombo = gtk_combo_box_new_text();
    gtk_widget_set_size_request(brandCombo, 170, 30);
    g_signal_connect(G_OBJECT(brandCombo), "changed",
                     G_CALLBACK(combo_changed), NULL);
    gtk_box_pack_start(GTK_BOX(brandHBox), brandCombo, FALSE, FALSE, 0);

    modelLabel = gtk_label_new ("Model");
    gtk_widget_set_size_request(modelLabel, 50, 30);
    gtk_box_pack_start(GTK_BOX(modelHBox), modelLabel, FALSE, FALSE, 0);

    modelCombo = gtk_combo_box_new_text();
    gtk_widget_set_size_request(modelCombo, 170, 30);
    g_signal_connect(G_OBJECT(modelCombo), "changed",
                     G_CALLBACK(combo_changed), NULL);
    gtk_box_pack_start(GTK_BOX(modelHBox), modelCombo, FALSE, FALSE, 0);


    progMsgLable = gtk_label_new(NULL);
    gtk_label_set_justify(GTK_LABEL(progMsgLable), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(msgHBox), progMsgLable, FALSE, FALSE, 5);

    syncMsgMarkup = g_markup_printf_escaped ("<b><span foreground=\"blue\" size=\"x-large\">%s</span></b>", "You must sync with the server first ..");
    gtk_label_set_markup(GTK_LABEL(progMsgLable), syncMsgMarkup);
    g_free (syncMsgMarkup);

    progButton = gtk_button_new_with_label("Program Tablet");
    gtk_widget_set_size_request(progButton, 110, 30);
    g_signal_connect(G_OBJECT(progButton), "clicked",
                     G_CALLBACK(prog_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(msgHBox), progButton, TRUE, FALSE, 5);

    gtk_widget_show_all(window);
    gtk_widget_hide(brandmodelHBox);
    gtk_widget_hide(progButton);
    gtk_widget_hide(syncLabel);

    gtk_main();

    return 0;
}
