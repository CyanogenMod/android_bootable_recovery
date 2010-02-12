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

void show_choose_zip_menu()
{
    static char* headers[] = { "Choose a zip or press POWER to return",
			       "",
			       NULL };
    
    char path[PATH_MAX] = "";
    DIR *dir;
    struct dirent *de;
    int total = 0;
    int i;
    char** files;
    char** list;

    if (ensure_root_path_mounted("SDCARD:") != 0) {
        LOGE ("Can't mount /sdcard\n");
        return;
    }

    dir = opendir("/sdcard");
    if (dir == NULL) {
        LOGE("Couldn't open /sdcard");
        return;
    }
    
    const char *extension = ".zip";
    const int extension_length = strlen(extension);
    
    while ((de=readdir(dir)) != NULL) {
        if (de->d_name[0] != '.' && strlen(de->d_name) > extension_length && strcmp(de->d_name + strlen(de->d_name) - extension_length, extension) == 0) {
            total++;
	      }
    }

    if (total==0) {
        LOGE("No tar archives found\n");
        if(closedir(dir) < 0) {
            LOGE("Failed to close directory /sdcard");
            return;
	      }
    }
    else {
        files = (char**) malloc((total+1)*sizeof(char*));
        files[total]=NULL;

        list = (char**) malloc((total+1)*sizeof(char*));
        list[total]=NULL;
	
        rewinddir(dir);

        i = 0;
        while ((de = readdir(dir)) != NULL) {
            if (de->d_name[0] != '.' && strlen(de->d_name) > extension_length && strcmp(de->d_name + strlen(de->d_name) - extension_length, extension) == 0) {
                files[i] = (char*) malloc(strlen("/sdcard/")+strlen(de->d_name)+1);
                strcpy(files[i], "/sdcard/");
                strcat(files[i], de->d_name);

                list[i] = (char*) malloc(strlen(de->d_name)+1);
                strcpy(list[i], de->d_name);

                i++;
            }
        }

        if (closedir(dir) <0) {
            LOGE("Failure closing directory /sdcard\n");
            return;
        }

        int chosen_item = get_menu_selection(headers, list, 1);
        if (chosen_item >= 0 && chosen_item != GO_BACK) {
          char sdcard_package_file[1024];
          strcpy(sdcard_package_file, "SDCARD:");
          strcat(sdcard_package_file, files[chosen_item] + strlen("/sdcard"));

          ui_print("\n-- Install from sdcard...\n");
          set_sdcard_update_bootloader_message();
          int status = install_package(sdcard_package_file);
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
    }
}