
**twrpTar command usages**


 
   	twrpTar <action> [options]

  actions: 
  
        -c create 
        -x extract

 options:
 
    -d    target directory
    -t    output file
	-m    skip media subfolder (has data media)
	-z    compress backup (/sbin/pigz must be present)
	-e    encrypt/decrypt backup followed by password (/sbin/openaes must be present)
	-u    encrypt using userdata encryption (must be used with -e)

 Example: 
 
       twrpTar -c -d /cache -t /sdcard/test.tar
       twrpTar -x -d /cache -t /sdcard/test.tar



当压缩文件的大小超过了最大的大小即
backup_size > MAX_ARCHIVE_SIZE ，twrpTar会自动分卷，生成的文件后面自动加入000 -- 00i
而这个MAX_ARCHIVE_SIZE定义在variables.h头文件中
有如下定义：


       // Max archive size for tar backups before we split (1.5GB)
       #define MAX_ARCHIVE_SIZE 1610612736LLU
       //#define MAX_ARCHIVE_SIZE 52428800LLU // 50MB split for testing
当要备份的分区的大小大于MAX_ARCHIVE_SIZE的时候，就会自动分割生成的文件
例如我使用的命令是
         twrpTar -c -z -d /data/system0 -t /sdcard/TWRP/data-system0.tar.gz
因为我的/data/system0大小是200MB,而我临时设定的MAX_ARCHIVE_SIZE = 50MB,
所以会生成几个分卷
文件名如下：
         data-system0.tar.gz000
         data-system0.tar.gz001
         data-system0.tar.gz002

恢复命令使用：
         twrpTar -x -d /data/system0 -t /sdcard/TWRP/data-system0.tar.gz 
 这个时候， twrpTar会先检查是否存在data-system0.tar.gz先，如果不存在，再检测
data-system0.tar.gz000,
当检查到data-system0.tar.gz000的时候，会启用分卷还原功能。



------------------------------------------------------------------------------------

**Backup cache partitions (ext4) logs**



      ~ # twrpTar -c -d /cache -t /sdcard/test-cache.tar
      I:Creating backup...
      I:Creating tar file '/sdcard/test-cache.tar'
      I:addFile '/cache/recovery' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/log' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/last_log' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/last_log.1' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/last_log.3' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/last_install' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/last_log.2' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/last_log.4' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/last_log.5' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/last_log.6' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/last_log.7' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/last_log.8' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/last_log.9' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/recovery/last_log.10' including root: 0
      setting selinux context: u:object_r:cache_file:s0
      I:addFile '/cache/dalvik-cache' including root: 0
      setting selinux context: u:object_r:dalvikcache_data_file:s0
      I:addFile '/cache/backup' including root: 0
      setting selinux context: u:object_r:cache_backup_file:s0
      I:Thread id 0 tarList done, 0 archives.
      I:Thread ID 0 finished successfully.
      I:createTarFork() process ended with RC=0


      tar created successfully.



