#!/sbin/sh
cd $1
md5sum *img > nandroid.md5
return $?