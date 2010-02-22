#undef main

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    if (strstr(argv[0], "flash_image") != NULL)
        return flash_image_main(argc, argv);
    if (strstr(argv[0], "dump_image") != NULL)
        return dump_image_main(argc, argv);
    if (strstr(argv[0], "mkyaffs2image") != NULL)
        return mkyaffs2image_main(argc, argv);
    if (strstr(argv[0], "unyaffs") != NULL)
        return unyaffs_main(argc, argv);
    return 0;
}