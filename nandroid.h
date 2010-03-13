#ifndef NANDROID_H
#define NANDROID_H

int nandroid_main(int argc, char** argv);
int nandroid_backup(char* backup_path);
int nandroid_restore(char* backup_path);

#endif