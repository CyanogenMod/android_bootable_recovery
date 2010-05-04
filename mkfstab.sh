#!/sbin/sh

rm -f /etc/fstab
cat /proc/mtd | while read mtdentry
do
  mtd=$(echo $mtdentry | awk '{print $1}')
  mtd=$(echo $mtd | sed s/mtd//)
  mtd=$(echo $mtd | sed s/://)
  exist=$(ls -l /dev/block/mtdblock$mtd) 2> /dev/null
  if [ -z "$exist" ]
  then
    continue
  fi
  partition=$(echo $mtdentry | awk '{print $4}')
  partition=$(echo $partition | sed s/\"//g)
  mount=$partition
  type=
  if [ "$partition" = "system" ]
  then
    type=yaffs2
  elif [ "$partition" = "userdata" ]
  then
    type=yaffs2
    mount=data
  elif [ "$partition" == "cache" ]
  then
    type=yaffs2
  else
    continue
  fi
  
  echo "/dev/block/mtdblock$mtd  /$mount $type rw" >> /etc/fstab
done
echo "/dev/block/mmcblk0p1" /sdcard vfat rw >> /etc/fstab
echo "/dev/block/mmcblk0p2" /sd-ext ext4 rw >> /etc/fstab
