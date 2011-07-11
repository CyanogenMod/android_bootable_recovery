#include <stdio.h>
#include <sys/stat.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <fcntl.h>
#include <limits.h>

typedef struct DEDUPE_STORE_CTX {
    char blob_dir[PATH_MAX];
    FILE *output_manifest;
};

static void usage(char** argv) {
    fprintf(stderr, "usage: %s c input_file|input_directory blob_dir output_manifest\n", argv[0]);
    fprintf(stderr, "usage: %s x input_manifest blob_dir output_directory\n", argv[0]);
}

static int copy_file(const char *dst, const char *src) {
    char buf[4096];
    int dstfd, srcfd, bytes_read, bytes_written, total_read = 0;
    if (src == NULL)
        return 1;
    if (dst == NULL)
        return 2;
    
    srcfd = open(src, O_RDONLY);
    if (srcfd < 0)
        return 3;

    dstfd = open(dst, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (dstfd < 0) {
        close(srcfd);
        return 4;
    }

    do {
        total_read += bytes_read = read(srcfd, buf, 4096);
        if (!bytes_read)
            break;
        if (bytes_read < 4096)
            memset(&buf[bytes_read], 0, 4096 - bytes_read);
        if (write(dstfd, buf, 4096) < 4096)
            return 5;
    } while(bytes_read == 4096);
    
    close(dstfd);
    close(srcfd);
    
    return 0;
}

static void do_md5sum(FILE *mfile, unsigned char *rptr) {
    char rdata[BUFSIZ];
    int rsize;
    MD5_CTX c;
    
    MD5_Init(&c);
    while(!feof(mfile)) {
        rsize = fread(rdata, sizeof(char), BUFSIZ, mfile);
        if(rsize > 0) {
            MD5_Update(&c, rdata, rsize);
        }
    }

    MD5_Final(rptr, &c);
}

static int do_md5sum_file(const char* filename, unsigned char *rptr) {
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        fprintf(stderr, "Unable to open file: %s\n", filename);
        return 1;
    }
    do_md5sum(f, rptr);
    fclose(f);
    return 0;
}

static int store_st(struct DEDUPE_STORE_CTX *context, struct stat st, const char* s);

void print_stat(struct DEDUPE_STORE_CTX *context, char type, struct stat st, const char *f) {
    fprintf(context->output_manifest, "%c\t%o\t%d\t%d\t%s\t", type, st.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID), st.st_uid, st.st_gid, f);
}

static int store_file(struct DEDUPE_STORE_CTX *context, struct stat st, const char* f) {
    printf("%s\n", f);
    unsigned char sumdata[SHA_DIGEST_LENGTH];
    int ret;
    if (ret = do_md5sum_file(f, sumdata)) {
        fprintf(stderr, "Error calculating md5sum of %s\n", f);
        return ret; 
    }
    char psum[41];
    int j;
    for (j = 0; j < MD5_DIGEST_LENGTH; j++)
        sprintf(&psum[(j*2)], "%02x", (int)sumdata[j]);
    psum[(MD5_DIGEST_LENGTH * 2)] = '\0';
    
    
    //char cmd[PATH_MAX];
    //sprintf(cmd, "cat %s > '/%s/%s'", f, context->blob_dir, psum);
    //system(cmd);
    
    char out_blob[PATH_MAX];
    sprintf(out_blob, "%s/%s", context->blob_dir, psum);
    if (ret = copy_file(out_blob, f)) {
        fprintf(stderr, "Error copying blob %s\n", f);
        return ret;
    }
    
    //sprintf("cat %s > $OUTPUT_DIR")

    fprintf(context->output_manifest, "%s\n", psum);
    return 0;
}

static int store_dir(struct DEDUPE_STORE_CTX *context, struct stat st, const char* d) {
    printf("%s\n", d);
    DIR *dp = opendir(d);
    if (d == NULL) {
        fprintf(stderr, "Error opening directory: %s\n", d);
        return 1;
    }
    struct dirent *ep;
    char full_path[PATH_MAX];
    while (ep = readdir(dp)) {
        if (strcmp(ep->d_name, ".") == 0)
            continue;
        if (strcmp(ep->d_name, "..") == 0)
            continue;
        struct stat cst;
        int ret;
        sprintf(full_path, "%s/%s", d, ep->d_name);
        if (0 != (ret = lstat(full_path, &cst))) {
            fprintf(stderr, "Error opening: %s\n", ep->d_name);
            closedir(dp);
            return ret;
        }
        
        if (ret = store_st(context->blob_dir, cst, full_path))
            return ret;
    }
    closedir(dp);
    return 0;
}

static int store_link(struct DEDUPE_STORE_CTX *context, struct stat st, const char* l) {
    printf("%s\n", l);
    char link[PATH_MAX];
    int ret = readlink(l, link, PATH_MAX);
    if (ret < 0) {
        fprintf(stderr, "Error reading symlink\n");
        return errno;
    }
    fprintf(context->output_manifest, "%s\n", link);
    return 0;
}

static int store_st(struct DEDUPE_STORE_CTX *context, struct stat st, const char* s) {
    if (S_ISREG(st.st_mode)) {
        print_stat(context, 'f', st, s);
        return store_file(context, st, s);
    }
    else if (S_ISDIR(st.st_mode)) {
        print_stat(context, 'd', st, s);
        fprintf(context->output_manifest, "\n");
        return store_dir(context, st, s);
    }
    else if (S_ISLNK(st.st_mode)) {
        print_stat(context, 'l', st, s);
        store_link(context, st, s);
    }
    else {
        return fprintf(stderr, "Skipping special: %s\n", s);
    }
}

void get_full_path(char *rel_path, char *out_path) {
    char tmp[PATH_MAX];
    getcwd(tmp, PATH_MAX);
    chdir(rel_path);
    getcwd(out_path, PATH_MAX);
    chdir(tmp);
}

int main(int argc, char** argv) {
    if (argc != 5) {
        usage(argv);
        return 1;
    }

    if (strcmp(argv[1], "c") == 0) {
        struct stat st;
        int ret;
        if (0 != (ret = lstat(argv[2], &st))) {
            fprintf(stderr, "Error opening input_file/input_directory.\n");
            return ret;
        }
        
        if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "%s must be a directory.\n", argv[2]);
            return;
        }
        
        char blob_dir[PATH_MAX];
        struct DEDUPE_STORE_CTX context;
        context.output_manifest = fopen(argv[4], "wb");
        if (context.output_manifest == NULL) {
            fprintf(stderr, "Unable to open output file %s\n", argv[4]);
            return 1;
        }
        get_full_path(argv[3], context.blob_dir);
        chdir(argv[2]);
        
        return store_dir(&context, st, ".");
    }
    else if (strcmp(argv[1], "x") == 0) {
    }
    else {
        usage(argv);
        return 1;
    }

    return 0;
}