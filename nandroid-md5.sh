#!/sbin/sh
ERROR_STATUS=0
cd $1
rm -f /tmp/nandroid.md5
for i in $(ls -a); do
  if [ -f $i ]; then
    md5sum $i >> /tmp/nandroid.md5
    if [ $? -ne 0 ]; then
      ERROR_STATUS=1
    fi
  fi
done
cp /tmp/nandroid.md5 .

return $ERROR_STATUS