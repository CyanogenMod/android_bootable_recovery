#!/sbin/sh
cd $1
md5sum $2.img > $2.md5
return $?