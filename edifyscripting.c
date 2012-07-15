/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
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
#include <sys/stat.h>

#include <signal.h>
#include <sys/wait.h>

#include "bootloader.h"
#include "common.h"
#include "cutils/properties.h"
#include "firmware.h"
#include "install.h"
#include "minui/minui.h"
#include "minzip/DirUtil.h"
#include "roots.h"
#include "recovery_ui.h"

#include "extendedcommands.h"
#include "nandroid.h"
#include "mounts.h"
#include "flashutils/flashutils.h"
#include "edify/expr.h"
#include "mtdutils/mtdutils.h"
#include "mmcutils/mmcutils.h"
//#include "edify/parser.h"

Value* UIPrintFn(const char* name, State* state, int argc, Expr* argv[]) {
    char** args = ReadVarArgs(state, argc, argv);
    if (args == NULL) {
        return NULL;
    }

    int size = 0;
    int i;
    for (i = 0; i < argc; ++i) {
        size += strlen(args[i]);
    }
    char* buffer = malloc(size+1);
    size = 0;
    for (i = 0; i < argc; ++i) {
        strcpy(buffer+size, args[i]);
        size += strlen(args[i]);
        free(args[i]);
    }
    free(args);
    buffer[size] = '\0';

    char* line = strtok(buffer, "\n");
    while (line) {
        ui_print("%s\n", line);
        line = strtok(NULL, "\n");
    }

    return StringValue(buffer);
}

Value* RunProgramFn(const char* name, State* state, int argc, Expr* argv[]) {
    if (argc < 1) {
        return ErrorAbort(state, "%s() expects at least 1 arg", name);
    }
    char** args = ReadVarArgs(state, argc, argv);
    if (args == NULL) {
        return NULL;
    }

    char** args2 = malloc(sizeof(char*) * (argc+1));
    memcpy(args2, args, sizeof(char*) * argc);
    args2[argc] = NULL;

    fprintf(stderr, "about to run program [%s] with %d args\n", args2[0], argc);

    pid_t child = fork();
    if (child == 0) {
        execv(args2[0], args2);
        fprintf(stderr, "run_program: execv failed: %s\n", strerror(errno));
        _exit(1);
    }
    int status;
    waitpid(child, &status, 0);
    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) != 0) {
            fprintf(stderr, "run_program: child exited with status %d\n",
                    WEXITSTATUS(status));
        }
    } else if (WIFSIGNALED(status)) {
        fprintf(stderr, "run_program: child terminated by signal %d\n",
                WTERMSIG(status));
    }

    int i;
    for (i = 0; i < argc; ++i) {
        free(args[i]);
    }
    free(args);
    free(args2);

    char buffer[20];
    sprintf(buffer, "%d", status);

    return StringValue(strdup(buffer));
}

Value* FormatFn(const char* name, State* state, int argc, Expr* argv[]) {
    char* result = NULL;
    if (argc != 1) {
        return ErrorAbort(state, "%s() expects 1 arg, got %d", name, argc);
    }
    
    char *path;
    if (ReadArgs(state, argv, 1, &path) < 0) {
        return NULL;
    }
    
    ui_print("Formatting %s...\n", path);
    if (0 != format_volume(path)) {
        free(path);
        return StringValue(strdup(""));
    }
    
    if (strcmp(path, "/data") == 0 && has_datadata()) {
        ui_print("Formatting /datadata...\n", path);
        if (0 != format_volume("/datadata")) {
            free(path);
            return StringValue(strdup(""));
        }
        if (0 != format_volume("/sdcard/.android_secure")) {
            free(path);
            return StringValue(strdup(""));
        }
    }

done:
    return StringValue(strdup(path));
}

Value* BackupFn(const char* name, State* state, int argc, Expr* argv[]) {
    char* result = NULL;
    if (argc != 1) {
        return ErrorAbort(state, "%s() expects 1 args, got %d", name, argc);
    }
    char* path;
    if (ReadArgs(state, argv, 1, &path) < 0) {
        return NULL;
    }
    
    if (0 != nandroid_backup(path))
        return StringValue(strdup(""));
    
    return StringValue(strdup(path));
}

Value* RestoreFn(const char* name, State* state, int argc, Expr* argv[]) {
    if (argc < 1) {
        return ErrorAbort(state, "%s() expects at least 1 arg", name);
    }
    char** args = ReadVarArgs(state, argc, argv);
    if (args == NULL) {
        return NULL;
    }

    char** args2 = malloc(sizeof(char*) * (argc+1));
    memcpy(args2, args, sizeof(char*) * argc);
    args2[argc] = NULL;
    
    char* path = strdup(args2[0]);
    int restoreboot = 1;
    int restoresystem = 1;
    int restoredata = 1;
    int restorecache = 1;
    int restoresdext = 1;
    int i;
    for (i = 1; i < argc; i++)
    {
        if (args2[i] == NULL)
            continue;
        if (strcmp(args2[i], "noboot") == 0)
            restoreboot = 0;
        else if (strcmp(args2[i], "nosystem") == 0)
            restoresystem = 0;
        else if (strcmp(args2[i], "nodata") == 0)
            restoredata = 0;
        else if (strcmp(args2[i], "nocache") == 0)
            restorecache = 0;
        else if (strcmp(args2[i], "nosd-ext") == 0)
            restoresdext = 0;
    }
    
    for (i = 0; i < argc; ++i) {
        free(args[i]);
    }
    free(args);
    free(args2);

    if (0 != nandroid_restore(path, restoreboot, restoresystem, restoredata, restorecache, restoresdext, 0)) {
        free(path);
        return StringValue(strdup(""));
    }
    
    return StringValue(path);
}

Value* InstallZipFn(const char* name, State* state, int argc, Expr* argv[]) {
    char* result = NULL;
    if (argc != 1) {
        return ErrorAbort(state, "%s() expects 1 args, got %d", name, argc);
    }
    char* path;
    if (ReadArgs(state, argv, 1, &path) < 0) {
        return NULL;
    }
    
    if (0 != install_zip(path))
        return StringValue(strdup(""));
    
    return StringValue(strdup(path));
}

Value* MountFn(const char* name, State* state, int argc, Expr* argv[]) {
    char* result = NULL;
    if (argc != 1) {
        return ErrorAbort(state, "%s() expects 1 args, got %d", name, argc);
    }
    char* path;
    if (ReadArgs(state, argv, 1, &path) < 0) {
        return NULL;
    }
    
    if (0 != ensure_path_mounted(path))
        return StringValue(strdup(""));

    return StringValue(strdup(path));
}

void RegisterRecoveryHooks() {
    RegisterFunction("mount", MountFn);
    RegisterFunction("format", FormatFn);
    RegisterFunction("ui_print", UIPrintFn);
    RegisterFunction("run_program", RunProgramFn);
    RegisterFunction("backup_rom", BackupFn);
    RegisterFunction("restore_rom", RestoreFn);
    RegisterFunction("install_zip", InstallZipFn);
}

static int hasInitializedEdify = 0;
int run_script_from_buffer(char* script_data, int script_len, char* filename)
{
    if (!hasInitializedEdify) {
        RegisterBuiltins();
        RegisterRecoveryHooks();
        FinishRegistration();
        hasInitializedEdify = 1;
    }

    Expr* root;
    int error_count = 0;
    yy_scan_bytes(script_data, script_len);
    int error = yyparse(&root, &error_count);
    printf("parse returned %d; %d errors encountered\n", error, error_count);
    if (error == 0 || error_count > 0) {
        //ExprDump(0, root, buffer);

        State state;
        state.cookie = NULL;
        state.script = script_data;
        state.errmsg = NULL;

        char* result = Evaluate(&state, root);
        if (result == NULL) {
            printf("result was NULL, message is: %s\n",
                   (state.errmsg == NULL ? "(NULL)" : state.errmsg));
            free(state.errmsg);
            return -1;
        } else {
            printf("result is [%s]\n", result);
        }
    }
    return 0;
}



#define EXTENDEDCOMMAND_SCRIPT "/cache/recovery/extendedcommand"

int run_and_remove_extendedcommand()
{
    char tmp[PATH_MAX];
    sprintf(tmp, "cp %s /tmp/%s", EXTENDEDCOMMAND_SCRIPT, basename(EXTENDEDCOMMAND_SCRIPT));
    __system(tmp);
    remove(EXTENDEDCOMMAND_SCRIPT);
    int i = 0;
    for (i = 20; i > 0; i--) {
        ui_print("Waiting for SD Card to mount (%ds)\n", i);
        if (ensure_path_mounted("/sdcard") == 0) {
            ui_print("SD Card mounted...\n");
            break;
        }
        sleep(1);
    }
    remove("/sdcard/clockworkmod/.recoverycheckpoint");
    if (i == 0) {
        ui_print("Timed out waiting for SD card... continuing anyways.");
    }

    ui_print("Verifying SD Card marker...\n");
    struct stat st;
    if (stat("/sdcard/clockworkmod/.salted_hash", &st) != 0) {
        ui_print("SD Card marker not found...\n");
        if (volume_for_path("/emmc") != NULL) {
            ui_print("Checking Internal SD Card marker...\n");
            ensure_path_unmounted("/sdcard");
            if (ensure_path_mounted_at_mount_point("/emmc", "/sdcard") != 0) {
                ui_print("Internal SD Card marker not found... continuing anyways.\n");
                // unmount everything, and remount as normal
                ensure_path_unmounted("/emmc");
                ensure_path_unmounted("/sdcard");

                ensure_path_mounted("/sdcard");
            }
        }
    }

    sprintf(tmp, "/tmp/%s", basename(EXTENDEDCOMMAND_SCRIPT));
    int ret;
#ifdef I_AM_KOUSH
    if (0 != (ret = before_run_script(tmp))) {
        ui_print("Error processing ROM Manager script. Please verify that you are performing the backup, restore, or ROM installation from ROM Manager v4.4.0.0 or higher.\n");
        return ret;
    }
#endif
    return run_script(tmp);
}

int extendedcommand_file_exists()
{
    struct stat file_info;
    return 0 == stat(EXTENDEDCOMMAND_SCRIPT, &file_info);
}

int edify_main(int argc, char** argv) {
    load_volume_table();
    process_volumes();
    RegisterBuiltins();
    RegisterRecoveryHooks();
    FinishRegistration();

    if (argc != 2) {
        printf("edify <filename>\n");
        return 1;
    }

    FILE* f = fopen(argv[1], "r");
    if (f == NULL) {
        printf("%s: %s: No such file or directory\n", argv[0], argv[1]);
        return 1;
    }
    char buffer[8192];
    int size = fread(buffer, 1, 8191, f);
    fclose(f);
    buffer[size] = '\0';

    Expr* root;
    int error_count = 0;
    yy_scan_bytes(buffer, size);
    int error = yyparse(&root, &error_count);
    printf("parse returned %d; %d errors encountered\n", error, error_count);
    if (error == 0 || error_count > 0) {

        //ExprDump(0, root, buffer);

        State state;
        state.cookie = NULL;
        state.script = buffer;
        state.errmsg = NULL;

        char* result = Evaluate(&state, root);
        if (result == NULL) {
            printf("result was NULL, message is: %s\n",
                   (state.errmsg == NULL ? "(NULL)" : state.errmsg));
            free(state.errmsg);
        } else {
            printf("result is [%s]\n", result);
        }
    }
    return 0;
}

int run_script(char* filename)
{
    struct stat file_info;
    if (0 != stat(filename, &file_info)) {
        printf("Error executing stat on file: %s\n", filename);
        return 1;
    }

    int script_len = file_info.st_size;
    char* script_data = (char*)malloc(script_len + 1);
    FILE *file = fopen(filename, "rb");
    fread(script_data, script_len, 1, file);
    // supposedly not necessary, but let's be safe.
    script_data[script_len] = '\0';
    fclose(file);
    LOGI("Running script:\n");
    LOGI("\n%s\n", script_data);

    int ret = run_script_from_buffer(script_data, script_len, filename);
    free(script_data);
    return ret;
}
