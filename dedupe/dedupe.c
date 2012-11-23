#include <stdio.h>
#include <sys/stat.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include <assert.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/wait.h>
#include <sys/time.h>

#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <paths.h>
#include <sys/wait.h>

#define DEDUPE_VERSION 2
#define ARRAY_CAPACITY 1000

static int copy_file(const char *src, const char *dst) {
    char buf[4096];
    int dstfd, srcfd, bytes_read, bytes_written, total_read = 0;
    if (src == NULL)
        return 1;
    if (dst == NULL)
        return 2;

    srcfd = open(src, O_RDONLY);
    if (srcfd < 0)
        return 3;

    dstfd = open(dst, O_RDWR | O_CREAT | O_TRUNC, 0666);
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

typedef struct DEDUPE_STORE_CONTEXT {
    char blob_dir[PATH_MAX];
    FILE *output_manifest;
    const char** excludes;
    int exclude_count;
};

static void usage(char** argv) {
    fprintf(stderr, "usage: %s c input_directory blob_dir output_manifest [exclude...]\n", argv[0]);
    fprintf(stderr, "usage: %s x input_manifest blob_dir output_directory\n", argv[0]);
    fprintf(stderr, "usage: %s gc blob_dir input_manifests...\n", argv[0]);
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
    fprintf(context->output_manifest, "%c\t%o\t%d\t%d\t%lu\t%lu\t%lu\t%s\t", type, st.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID), st.st_uid, st.st_gid, st.st_atime, st.st_mtime, st.st_ctime, f);
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

    // if a hash is abcdefg,
    // the output blob name is abc/defg
    // this is to get around vfat having a 64k directory size limit (usually around 20k files)
    char out_blob[PATH_MAX];
    char tmp_out_blob[PATH_MAX];
    char key[SHA256_DIGEST_LENGTH * 2 + 2];
    // int i = 0;
    // int keyIndex = 0;
    // while (psum[i]) {
    //     key[keyIndex] = psum[i];
    //     i++;
    //     keyIndex++;
    //     if (i % 2 == 0 && psum[i]) {
    //         key[keyIndex] = '/';
    //         keyIndex++;
    //     }
    // }
    strcpy(key, psum);
    key[3] = '/';
    key[4] = NULL;
    strcat(key, psum + 3);
    sprintf(out_blob, "%s/%s", context->blob_dir, key);
    sprintf(tmp_out_blob, "%s.tmp", out_blob);
    mkdir(dirname(out_blob), S_IRWXU | S_IRWXG | S_IRWXO);

    // don't copy the file if it exists? not quite sure how I feel about this.
    int size = (int)st.st_size;
    struct stat file_info;
    // verify the file exists and is of the same size
    int file_ok = stat(out_blob, &file_info) == 0;
    if (file_ok) {
        int existing_size = file_info.st_size;
        if (existing_size != size)
            file_ok = 0;
    }
    if (!file_ok) {
        // copy to the tmp file
        if ((ret = copy_file(f, tmp_out_blob)) || (ret = rename(tmp_out_blob, out_blob))) {
            fprintf(stderr, "Error copying blob %s\n", f);
            return ret;
        }
    }

    fprintf(context->output_manifest, "%s\t%d\t\n", key, size);
    return 0;
}

static int store_dir(struct DEDUPE_STORE_CONTEXT *context, struct stat st, const char* d) {
    char full_path[PATH_MAX];
    printf("%s\n", d);
    DIR *dp = opendir(d);
    if (dp == NULL) {
        fprintf(stderr, "Error opening directory: %s\n", d);
        return 1;
    }
    struct dirent *ep;
    while (ep = readdir(dp)) {
        if (strcmp(ep->d_name, ".") == 0)
            continue;
        if (strcmp(ep->d_name, "..") == 0)
            continue;
        struct stat cst;
        int ret;
        sprintf(full_path, "%s/%s", d, ep->d_name);
        int i;
        for (i = 0; i < context->exclude_count; i++) {
            if (!strcmp(context->excludes[i], full_path))
                break;
        }
        if (i != context->exclude_count)
            continue;
        if (0 != (ret = lstat(full_path, &cst))) {
            fprintf(stderr, "Error opening: %s\n", full_path);
            closedir(dp);
            return ret;
        }

        if (ret = store_st(context, cst, full_path)) {
            fprintf(stderr, "Error storing: %s\n", full_path);
            closedir(dp);
            return ret;
        }
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

struct array {
    void** data;
    int size;
    int capacity;
};

static void array_init(struct array* arr, int capacity) {
    arr->data = malloc(sizeof(void*) * capacity);
    assert(arr->data != NULL);
    arr->size = 0;
    arr->capacity = capacity;
}

static void array_free(struct array* arr, int free_members) {
    if (free_members) {
        int i;
        for (i = 0; i < arr->size; i++) {
            free(arr->data[i]);
        }
    }

    if (arr->data != NULL) {
        free(arr->data);
        arr->data = NULL;
    }
    arr->size = 0;
    arr->capacity = 0;
}

static void array_add(struct array* arr, void* val) {
    if (arr->size == arr->capacity) {
        // Expand array
        arr->capacity *= 2;
        arr->data = realloc(arr->data, sizeof(void*) * arr->capacity);
        assert(arr->data != NULL);
    }
    arr->data[arr->size++] = val;
}

static int string_compare(const void* a, const void* b) {
    return strcmp(*(char**) a, *(char **) b);
}

static void recursive_list_dir(char* d, struct array *arr) {
    DIR *dp = opendir(d);
    if (dp == NULL) {
        fprintf(stderr, "Error opening directory: %s\n", d);
        return;
    }
    struct dirent *ep;
    while ((ep = readdir(dp))) {
        if (strcmp(ep->d_name, ".") == 0)
            continue;
        if (strcmp(ep->d_name, "..") == 0)
            continue;
        struct stat cst;
        int ret;
        char blob[PATH_MAX];
        sprintf(blob, "%s/%s", d, ep->d_name);
        if ((ret = lstat(blob, &cst))) {
            fprintf(stderr, "Error opening: %s\n", ep->d_name);
            continue;
        }

        if (S_ISDIR(cst.st_mode)) {
            recursive_list_dir(blob, arr);
            continue;
        }

        array_add(arr, strdup(blob));
    }
    closedir(dp);
}

static int check_file(const char* f) {
    struct stat cst;
    return lstat(f, &cst);
}

int dedupe_main(int argc, char** argv) {
    if (argc < 3) {
        usage(argv);
        return 1;
    }

    if (strcmp(argv[1], "c") == 0) {
        if (argc < 5) {
            usage(argv);
            return 1;
        }

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

        struct DEDUPE_STORE_CONTEXT context;
        context.output_manifest = fopen(argv[4], "wb");
        fprintf(context.output_manifest, "dedupe\t%d\n", DEDUPE_VERSION);
        if (context.output_manifest == NULL) {
            fprintf(stderr, "Unable to open output file %s\n", argv[4]);
            return 1;
        }
        mkdir(argv[3], S_IRWXU | S_IRWXG | S_IRWXO);
        realpath(argv[3], context.blob_dir);
        chdir(argv[2]);
        context.excludes = argv + 5;
        context.exclude_count = argc - 5;

        return store_dir(&context, st, ".");
    }
    else if (strcmp(argv[1], "x") == 0) {
        if (argc != 5) {
            usage(argv);
            return 1;
        }

        FILE *input_manifest = fopen(argv[2], "rb");
        if (input_manifest == NULL) {
            fprintf(stderr, "Unable to open input manifest %s\n", argv[2]);
            return 1;
        }

        char blob_dir[PATH_MAX];
        char *output_dir = argv[4];
        realpath(argv[3], blob_dir);

        printf("%s\n" , output_dir);
        mkdir(output_dir, S_IRWXU | S_IRWXG | S_IRWXO);
        if (chdir(output_dir)) {
            fprintf(stderr, "Unable to open output directory %s\n", output_dir);
            return 1;
        }

        char line[PATH_MAX];
        fgets(line, PATH_MAX, input_manifest);
        int version = 1;
        if (sscanf(line, "dedupe\t%d", &version) != 1) {
            fseek(input_manifest, 0, SEEK_SET);
        }
        if (version > DEDUPE_VERSION) {
            fprintf(stderr, "Attempting to restore newer dedupe file: %s\n", argv[2]);
            return 1;
        }
        while (fgets(line, PATH_MAX, input_manifest)) {
            //printf("%s", line);

            char type[4];
            char mode[8];
            char uid[32];
            char gid[32];
            char at[32];
            char mt[32];
            char ct[32];
            char filename[PATH_MAX];

            char *token = line;
            token = tokenize(type, token, '\t');
            token = tokenize(mode, token, '\t');
            token = tokenize(uid, token, '\t');
            token = tokenize(gid, token, '\t');
            if (version >= 2) {
                token = tokenize(at, token, '\t');
                token = tokenize(mt, token, '\t');
                token = tokenize(ct, token, '\t');
            }
            token = tokenize(filename, token, '\t');

            int mode_oct = dec_to_oct(atoi(mode));
            int uid_int = atoi(uid);
            int gid_int = atoi(gid);
            int ret;
            //printf("%s\t%s\t%s\t%s\t%s\t", type, mode, uid, gid, filename);
            printf("%s\n", filename);
            if (strcmp(type, "f") == 0) {
                char sha256[128];
                token = tokenize(sha256, token, '\t');
                char sizeStr[32];
                token = tokenize(sizeStr, token, '\t');
                int size = atoi(sizeStr);
                // printf("%s\t%d\n", sha256, size);

                char blob_file[PATH_MAX];
                sprintf(blob_file, "%s/%s", blob_dir, sha256);
                if (ret = copy_file(blob_file, filename)) {
                    fprintf(stderr, "Unable to copy file %s\n", filename);
                    fclose(input_manifest);
                    return ret;
                }

                chown(filename, uid_int, gid_int);
                chmod(filename, mode_oct);
            }
            else if (strcmp(type, "l") == 0) {
                char link[41];
                token = tokenize(link, token, '\t');
                // printf("%s\n", link);

                symlink(link, filename);

                // Android has no lchmod, and chmod follows symlinks
                //chmod(filename, mode_oct);
                lchown(filename, uid_int, gid_int);
            }
            else if (strcmp(type, "d") == 0) {
                // printf("\n");

                mkdir(filename, mode_oct);

                chown(filename, uid_int, gid_int);
                chmod(filename, mode_oct);
            }
            else {
                fprintf(stderr, "Unknown type %s\n", type);
                fclose(input_manifest);
                return 1;
            }
            if (version >= 2) {
                struct timeval times[2];
                times[0].tv_sec = atol(at);
                times[1].tv_sec = atol(mt);
                utimes(filename, times);
            }
        }

        fclose(input_manifest);
        return 0;
    }
    else if (strcmp(argv[1], "gc") == 0) {
        if (argc < 3) {
            usage(argv);
            return 1;
        }
        
        char blob_dir[PATH_MAX];
        realpath(argv[2], blob_dir);
        if (check_file(blob_dir)) {
            fprintf(stderr, "Unable to open blobs dir: %s\n", blob_dir);
            return 1;
        }

        struct array used_files;
        struct array all_files;
        array_init(&used_files, ARRAY_CAPACITY);
        array_init(&all_files, ARRAY_CAPACITY);

        char blob[PATH_MAX];
        int i;
        int failure = 0;
        for (i = 3; i < argc; i++) {
            FILE *input_manifest = fopen(argv[i], "rb");
            if (input_manifest == NULL) {
                fprintf(stderr, "Unable to open input manifest %s\n", argv[i]);
                failure = 1;
                goto out;
            }

            char line[PATH_MAX];
            fgets(line, PATH_MAX, input_manifest);
            int version = 1;
            if (sscanf(line, "dedupe\t%d", &version) != 1) {
                fseek(input_manifest, 0, SEEK_SET);
            }
            if (version > DEDUPE_VERSION) {
                fprintf(stderr, "Attempting to gc newer dedupe file: %s\n", argv[2]);
                failure = 1;
                fclose(input_manifest);
                break;
            }
            while (fgets(line, PATH_MAX, input_manifest)) {
                char type[4];
                char mode[8];
                char uid[32];
                char gid[32];
                char at[32];
                char mt[32];
                char ct[32];
                char filename[PATH_MAX];

                char *token = line;
                token = tokenize(type, token, '\t');
                token = tokenize(mode, token, '\t');
                token = tokenize(uid, token, '\t');
                token = tokenize(gid, token, '\t');
                if (version >= 2) {
                    token = tokenize(at, token, '\t');
                    token = tokenize(mt, token, '\t');
                    token = tokenize(ct, token, '\t');
                }
                token = tokenize(filename, token, '\t');

                int mode_oct = dec_to_oct(atoi(mode));
                int uid_int = atoi(uid);
                int gid_int = atoi(gid);
                int ret;
                // printf("%s\n", filename);
                if (strcmp(type, "f") == 0) {
                    char key[128];
                    token = tokenize(key, token, '\t');
                    char sizeStr[32];
                    token = tokenize(sizeStr, token, '\t');
                    int size = atoi(sizeStr);

                    sprintf(blob, "%s/%s", blob_dir, key);
                    array_add(&used_files, strdup(blob));
                }
            }
            fclose(input_manifest);
        }

        recursive_list_dir(blob_dir, &all_files);

        qsort(used_files.data, used_files.size, sizeof(void*), string_compare);
        qsort(all_files.data, all_files.size, sizeof(void*), string_compare);

        // Search for unused files
        int j = 0;
        for (i = 0; i < all_files.size; i++) {
            int cmp;
            while (j < used_files.size &&
                (cmp = strcmp(used_files.data[j], all_files.data[i])) < 0) {
                j++;
            }

            if (cmp > 0 || j >= used_files.size) {
                if (remove(all_files.data[i])) {
                    fprintf(stderr, "Error removing: %s\n", all_files.data[i]);
                }
                printf("Delete: %s\n", all_files.data[i]);
            }
        }

        out:
        array_free(&used_files, 1);
        array_free(&all_files, 1);

        return failure;
    }
    else {
        usage(argv);
        return 1;
    }
}
