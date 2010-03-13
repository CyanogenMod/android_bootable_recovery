#ifndef DUMP_IMAGE_H
#define DUMP_IMAGE_H

typedef void (*dump_image_callback) (int partition_dumped, int partition_size);

int dump_image(char* partition_name, char* filename, dump_image_callback callback);

#endif
