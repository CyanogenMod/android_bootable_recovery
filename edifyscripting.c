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

#include "../../external/yaffs2/yaffs2/utils/mkyaffs2image.h"
#include "../../external/yaffs2/yaffs2/utils/unyaffs.h"

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

void RegisterRecoveryHooks() {
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
