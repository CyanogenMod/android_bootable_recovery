#include <stdlib.h>

extern "C" {
#include "fstools.h"
}


int
main(int argc, char **argv) {

    // Handle alternative invocations
    char* command = argv[0];
    char* stripped = strrchr(argv[0], '/');
    if (stripped)
        command = stripped + 1;

    if (strcmp(command, "fstools") != 0) {
        struct fstools_cmd cmd = get_command(command);
        if (cmd.name)
            return cmd.main_func(argc, argv);
    }
    return -1;
}
