#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define BML_UNLOCK_ALL 0x8A29 ///< unlock all partition RO -> RW

int main(int argc, char** argv) {
    char buf[4096];
    int dstfd, srcfd, bytes_read, bytes_written, total_read = 0;
    if (argc != 3)
        return 1;
    if (argv[1][0] == '-' && argv[1][1] == 0)
        srcfd = 0;
    else {
        srcfd = open(argv[1], O_RDONLY | O_LARGEFILE);
        if (srcfd < 0)
            return 2;
    }
    dstfd = open(argv[2], O_RDWR | O_LARGEFILE);
    if (dstfd < 0)
        return 3;
    if (ioctl(dstfd, BML_UNLOCK_ALL, 0))
        return 4;
    do {
        total_read += bytes_read = read(srcfd, buf, 4096);
        if (!bytes_read)
            break;
        if (bytes_read < 4096)
            memset(&buf[bytes_read], 0, 4096 - bytes_read);
        if (write(dstfd, buf, 4096) < 4096)
            return 5;
    } while(bytes_read == 4096);
    return 0;
}
