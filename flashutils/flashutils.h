int restore_raw_partition(const char *partition, const char *filename);
int backup_raw_partition(const char *partition, const char *filename);
int erase_raw_partition(const char *partition);
int erase_partition(const char *partition, const char *filesystem);
int mount_partition(const char *partition, const char *mount_point, const char *filesystem, int read_only);
int get_partition_device(const char *partition, char *device);