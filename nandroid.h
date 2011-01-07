#ifndef NANDROID_H
#define NANDROID_H

int nandroid_main(int argc, char** argv);
int nandroid_backup(const char* backup_path);
int nandroid_restore(const char* backup_path, char restore_flags);
void nandroid_generate_timestamp_path(char* backup_path);

#define RESTORE_BOOT    (1 << 0)
#define RESTORE_SYSTEM  (1 << 1)
#define RESTORE_DATA    (1 << 2)
#define RESTORE_CACHE   (1 << 3)
#define RESTORE_SDEXT   (1 << 4)
#define RESTORE_WIMAX   (1 << 5)

#endif