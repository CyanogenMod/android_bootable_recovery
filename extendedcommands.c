#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <sys/wait.h>
#include <sys/limits.h>
#include <dirent.h>

#include "bootloader.h"
#include "common.h"
#include "cutils/properties.h"
#include "firmware.h"
#include "install.h"
#include "minui/minui.h"
#include "minzip/DirUtil.h"
#include "roots.h"
#include "recovery_ui.h"

int signature_check_enabled = 1;
int script_assert_enabled = 1;
static const char *SDCARD_PACKAGE_FILE = "SDCARD:update.zip";

void
toggle_signature_check()
{
    signature_check_enabled = !signature_check_enabled;
    ui_print("Signature Check: %s\n", signature_check_enabled ? "Enabled" : "Disabled");
}

void toggle_script_asserts()
{
    script_assert_enabled = !script_assert_enabled;
    ui_print("Script Asserts: %s\n", script_assert_enabled ? "Enabled" : "Disabled");
}

void install_zip(const char* packagefilepath)
{
    ui_print("\n-- Installing: %s\n", packagefilepath);
    set_sdcard_update_bootloader_message();
    int status = install_package(packagefilepath);
    if (status != INSTALL_SUCCESS) {
        ui_set_background(BACKGROUND_ICON_ERROR);
        ui_print("Installation aborted.\n");
    } else if (!ui_text_visible()) {
        return;  // reboot if logs aren't visible
    } else {
        if (firmware_update_pending()) {
            ui_print("\nReboot via menu to complete\n"
                     "installation.\n");
        } else {
            ui_print("\nInstall from sdcard complete.\n");
        }
    }
}

char* INSTALL_MENU_ITEMS[] = {  "apply sdcard:update.zip",
                                "choose zip from sdcard",
                                "toggle signature verification",
                                "toggle script asserts",
                                NULL };
#define ITEM_APPLY_SDCARD     0
#define ITEM_CHOOSE_ZIP       1
#define ITEM_SIG_CHECK        2
#define ITEM_ASSERTS          3

void show_install_update_menu()
{
    static char* headers[] = {  "Apply update from .zip file on SD card",
                                "",
                                NULL 
    };
    for (;;)
    {
        int chosen_item = get_menu_selection(headers, INSTALL_MENU_ITEMS, 0);
        switch (chosen_item)
        {
            case ITEM_ASSERTS:
                toggle_script_asserts();
                break;
            case ITEM_SIG_CHECK:
                toggle_signature_check();
                break;
            case ITEM_APPLY_SDCARD:
                install_zip(SDCARD_PACKAGE_FILE);
                break;
            case ITEM_CHOOSE_ZIP:
                show_choose_zip_menu();
                break;
            default:
                return;
        }
        
    }
}

char* choose_file_menu(const char* directory, const char* extension, const char* headers[])
{
    char path[PATH_MAX] = "";
    DIR *dir;
    struct dirent *de;
    int total = 0;
    int i;
    char** files;
    char** list;

    dir = opendir(directory);
    if (dir == NULL) {
        ui_print("Couldn't open directory.\n");
        return NULL;
    }
  
    const int extension_length = strlen(extension);
  
    while ((de=readdir(dir)) != NULL) {
        if (de->d_name[0] != '.' && strlen(de->d_name) > extension_length && strcmp(de->d_name + strlen(de->d_name) - extension_length, extension) == 0) {
            total++;
        }
    }

    if (total==0) {
        ui_print("No files found.\n");
        if(closedir(dir) < 0) {
            LOGE("Failed to close directory.");
        }
        return NULL;
    }

    files = (char**) malloc((total+1)*sizeof(char*));
    files[total]=NULL;

    list = (char**) malloc((total+1)*sizeof(char*));
    list[total]=NULL;

    rewinddir(dir);

    i = 0;
    while ((de = readdir(dir)) != NULL) {
        if (de->d_name[0] != '.' && strlen(de->d_name) > extension_length && strcmp(de->d_name + strlen(de->d_name) - extension_length, extension) == 0) {
            files[i] = (char*) malloc(strlen(directory)+strlen(de->d_name)+1);
            strcpy(files[i], directory);
            strcat(files[i], de->d_name);

            list[i] = (char*) malloc(strlen(de->d_name)+1);
            strcpy(list[i], de->d_name);

            i++;
        }
    }

    if (closedir(dir) <0) {
        LOGE("Failure closing directory.");
        return NULL;
    }

    for (;;)
    {
        int chosen_item = get_menu_selection(headers, list, 0);
        if (chosen_item == GO_BACK)
            break;
        static char ret[PATH_MAX];
        strcpy(ret, files[chosen_item]);
        return ret;
    }
    return NULL;
}

void show_choose_zip_menu()
{
    if (ensure_root_path_mounted("SDCARD:") != 0) {
        LOGE ("Can't mount /sdcard\n");
        return;
    }

    static char* headers[] = {  "Choose a zip to apply",
                                "",
                                NULL 
    };
    
    char* file = choose_file_menu("/sdcard/", ".zip", headers);
    if (file == NULL)
        return;
    char sdcard_package_file[1024];
    strcpy(sdcard_package_file, "SDCARD:");
    strcat(sdcard_package_file,  file + strlen("/sdcard/"));
    install_zip(sdcard_package_file);
}

void do_nandroid_backup()
{
    ui_print("Performing backup...\n");
    if (system("/sbin/nandroid-mobile.sh backup") != 0)
    {
        ui_print("Error while backing up!\n");
        return;
    }
    ui_print("Backup complete.\n");
}

void show_nandroid_restore_menu()
{
    if (ensure_root_path_mounted("SDCARD:") != 0) {
        LOGE ("Can't mount /sdcard\n");
        return;
    }

    static char* headers[] = {  "Choose an image to restore",
                                "",
                                NULL 
    };

    char* file = choose_file_menu("/sdcard/nandroid/", "", headers);
    if (file == NULL)
        return;
    char* command[PATH_MAX];
    sprintf(command, "nandroid-mobile.sh restore %s", file);
    ui_print("Performing restore...\n");
    int ret = system(command);
    if (ret != 0)
    {
        ui_print("Error while restoring!\n");
        return;
    }
    ui_print("Restore complete.\n");
}