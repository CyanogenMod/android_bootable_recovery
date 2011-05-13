#ifndef LIBCRECOVERY_COMMON_H
#define LIBCRECOVERY_COMMON_H

int __system(const char *command);
FILE * __popen(const char *program, const char *type);
int __pclose(FILE *iop);

#endif