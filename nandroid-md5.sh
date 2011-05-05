#!/sbin/sh
cd $1
op=nandroid.md5
ret=0
rm -f ${op}
for f in *img; do
    md5sum ${f} >> ${op}
    if [ ! -s ${f} ]; then
	ret=`expr ${ret} + 1`
    fi
done
if [ ! -s ${op} ]; then
    ret=`expr ${ret} + 1`
    echo Error: "${op}" zero length
fi
return ${ret}
#
# Script to compute md5sums of a series of files,
# returning an error if any of the input files
# is zero length or if the output is zero length:
# All indicate an sdram that is too full for the
# backup to succeed.
#
