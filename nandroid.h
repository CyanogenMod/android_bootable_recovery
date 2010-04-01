#ifndef NANDROID_H
#define NANDROID_H

int nandroid_main(int argc, char** argv);
int nandroid_backup(char* backup_path);
int nandroid_restore(char* backup_path, int restore_boot, int restore_system, int restore_data, int restore_cache, int restore_sdext);

#endif