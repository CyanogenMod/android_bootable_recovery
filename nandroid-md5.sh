#!/sbin/sh
cd $1
rm -f /tmp/nandroid.md5
for i in * .*; do
  if [ -f "$i" ]; then
    md5sum "$i" >> /tmp/nandroid.md5
    if [ $? -ne 0 ]; then
      exit 1
    fi
  fi
done
cp /tmp/nandroid.md5 .
