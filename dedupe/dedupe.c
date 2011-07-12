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

typedef struct DEDUPE_STORE_CONTEXT {
    char blob_dir[PATH_MAX];
    FILE *output_manifest;
};

static void usage(char** argv) {
    fprintf(stderr, "usage: %s c input_directory blob_dir output_manifest\n", argv[0]);
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

    while (bytes_read = read(srcfd, buf, 4096)) {
        total_read += bytes_read;
        if (write(dstfd, buf, bytes_read) != bytes_read)
            return 5;
    }

    close(dstfd);
    close(srcfd);
    
    return 0;
}

static void do_sha256sum(FILE *mfile, unsigned char *rptr) {
    char rdata[BUFSIZ];
    int rsize;
    SHA256_CTX c;
    
    SHA256_Init(&c);
    while(!feof(mfile)) {
        rsize = fread(rdata, sizeof(char), BUFSIZ, mfile);
        if(rsize > 0) {
            SHA256_Update(&c, rdata, rsize);
        }
    }

    SHA256_Final(rptr, &c);
}

static int do_sha256sum_file(const char* filename, unsigned char *rptr) {
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        fprintf(stderr, "Unable to open file: %s\n", filename);
        return 1;
    }
    do_sha256sum(f, rptr);
    fclose(f);
    return 0;
}

static int store_st(struct DEDUPE_STORE_CONTEXT *context, struct stat st, const char* s);

void print_stat(struct DEDUPE_STORE_CONTEXT *context, char type, struct stat st, const char *f) {
    fprintf(context->output_manifest, "%c\t%o\t%d\t%d\t%s\t", type, st.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID), st.st_uid, st.st_gid, f);
}

static int store_file(struct DEDUPE_STORE_CONTEXT *context, struct stat st, const char* f) {
    printf("%s\n", f);
    unsigned char sumdata[SHA256_DIGEST_LENGTH];
    int ret;
    if (ret = do_sha256sum_file(f, sumdata)) {
        fprintf(stderr, "Error calculating sha256sum of %s\n", f);
        return ret; 
    }
    char psum[128];
    int j;
    for (j = 0; j < SHA256_DIGEST_LENGTH; j++)
        sprintf(&psum[(j*2)], "%02x", (int)sumdata[j]);
    psum[(SHA256_DIGEST_LENGTH * 2)] = '\0';

    char out_blob[PATH_MAX];
    sprintf(out_blob, "%s/%s", context->blob_dir, psum);
    if (ret = copy_file(out_blob, f)) {
        fprintf(stderr, "Error copying blob %s\n", f);
        return ret;
    }
    
    fprintf(context->output_manifest, "%s\t%d\t\n", psum, st.st_size);
    return 0;
}

static int store_dir(struct DEDUPE_STORE_CONTEXT *context, struct stat st, const char* d) {
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
        
        if (ret = store_st(context, cst, full_path))
            return ret;
    }
    closedir(dp);
    return 0;
}

static int store_link(struct DEDUPE_STORE_CONTEXT *context, struct stat st, const char* l) {
    printf("%s\n", l);
    char link[PATH_MAX];
    int ret = readlink(l, link, PATH_MAX);
    if (ret < 0) {
        fprintf(stderr, "Error reading symlink\n");
        return errno;
    }
    link[ret] = '\0';
    fprintf(context->output_manifest, "%s\t\n", link);
    return 0;
}

static int store_st(struct DEDUPE_STORE_CONTEXT *context, struct stat st, const char* s) {
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
        return store_link(context, st, s);
    }
    else {
        fprintf(stderr, "Skipping special: %s\n", s);
        return 0;
    }
}

void get_full_path(char *out_path, char *rel_path) {
    char tmp[PATH_MAX];
    getcwd(tmp, PATH_MAX);
    chdir(rel_path);
    getcwd(out_path, PATH_MAX);
    chdir(tmp);
}

static char* tokenize(char *out, const char* line, const char sep) {
    while (*line != sep) {
        if (*line == '\0') {
            return NULL;
        }
        
        *out = *line;
        out++;
        line++;
    }
    
    *out = '\0';
    // resume at the next char
    return ++line;
}

static int dec_to_oct(int dec) {
    int ret = 0;
    int mult = 1;
    while (dec != 0) {
        int rem = dec % 10;
        ret += (rem * mult);
        dec /= 10;
        mult *= 8;
    }
    
    return ret;
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
            return 1;
        }
        
        char blob_dir[PATH_MAX];
        struct DEDUPE_STORE_CONTEXT context;
        context.output_manifest = fopen(argv[4], "wb");
        if (context.output_manifest == NULL) {
            fprintf(stderr, "Unable to open output file %s\n", argv[4]);
            return 1;
        }
        get_full_path(context.blob_dir, argv[3]);
        chdir(argv[2]);
        
        return store_dir(&context, st, ".");
    }
    else if (strcmp(argv[1], "x") == 0) {
        FILE *input_manifest = fopen(argv[2], "rb");
        if (input_manifest == NULL) {
            fprintf(stderr, "Unable to open input manifest %s\n", argv[2]);
            return 1;
        }

        char blob_dir[PATH_MAX];
        char *output_dir = argv[4];
        get_full_path(blob_dir, argv[3]);
    
        printf("%s\n" , output_dir);
        chdir(output_dir);
        
        char line[PATH_MAX];
        while (fgets(line, PATH_MAX, input_manifest)) {
            //printf("%s", line);
            
            char type[4];
            char mode[8];
            char uid[32];
            char gid[32];
            char filename[PATH_MAX];
            
            char *token = line;
            token = tokenize(type, token, '\t');
            token = tokenize(mode, token, '\t');
            token = tokenize(uid, token, '\t');
            token = tokenize(gid, token, '\t');
            token = tokenize(filename, token, '\t');
            
            int mode_oct = dec_to_oct(atoi(mode));
            int uid_int = atoi(uid);
            int gid_int = atoi(gid);
            int ret;
            printf("%s\t%s\t%s\t%s\t%s\t", type, mode, uid, gid, filename);
            if (strcmp(type, "f") == 0) {
                char sha256[128];
                token = tokenize(sha256, token, '\t');
                char sizeStr[32];
                token = tokenize(sizeStr, token, '\t');
                int size = atoi(sizeStr);
                printf("%s\t%d\n", sha256, size);
                
                char blob_file[PATH_MAX];
                sprintf(blob_file, "%s/%s", blob_dir, sha256);
                if (ret = copy_file(filename, blob_file)) {
                    fprintf(stderr, "Unable to copy file %s\n", filename);
                    fclose(input_manifest);
                    return ret;
                }
                
                chmod(filename, mode_oct);
                chown(filename, uid_int, gid_int);
            }
            else if (strcmp(type, "l") == 0) {
                char link[41];
                token = tokenize(link, token, '\t');
                printf("%s\n", link);
                
                symlink(link, filename);

                // Android has no lchmod, and chmod follows symlinks
                //chmod(filename, mode_oct);
                lchown(filename, uid_int, gid_int);
            }
            else if (strcmp(type, "d") == 0) {
                printf("\n");

                mkdir(filename, mode_oct);

                chmod(filename, mode_oct);
                chown(filename, uid_int, gid_int);
            }
            else {
                fprintf(stderr, "Unknown type %s\n", type);
                fclose(input_manifest);
                return 1;
            }
        }
        
        fclose(input_manifest);
        return 0;
    }
    else {
        usage(argv);
        return 1;
    }
}
