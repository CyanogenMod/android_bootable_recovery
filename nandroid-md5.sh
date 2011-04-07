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
#
# A test for empty files in the md5 sum:
# [ "`grep '^d41d8cd98f00b204e9800998ecf8427e' nandroid.md5 | wc -l`" -eq 0 ]
# because d41...27e is the md5 of a zero length file (^ matches to start of line)
# Also need to test that the nandroid.md5 is non-zero:
# [ "`wc -l < nandroid.md5`" -gt 0 ]
#
# Combined:
# [ "`wc -l < nandroid.md5`" -gt 0 -a "`grep '^d41d8cd98f00b204e9800998ecf8427e' nandroid.md5 | wc -l`" -eq 0 ]
#
#
