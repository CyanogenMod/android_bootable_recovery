#!/sbin/sh

# nandroid v2.1 - an Android backup tool for the G1 by infernix and brainaid

# Requirements:

# - a modded android in recovery mode (JF 1.3 will work by default)
# - adb shell as root in recovery mode if not using a pre-made recovery image
# - busybox in recovery mode
# - dump_image-arm-uclibc compiled and in path on phone
# - mkyaffs2image-arm-uclibc compiled and installed in path on phone

# Reference data:

# dev:    size   erasesize  name
#mtd0: 00040000 00020000 "misc"
#mtd1: 00500000 00020000 "recovery"
#mtd2: 00280000 00020000 "boot"
#mtd3: 04380000 00020000 "system"
#mtd4: 04380000 00020000 "cache"
#mtd5: 04ac0000 00020000 "userdata"
#mtd6 is everything, dump splash1 with: dd if=/dev/mtd/mtd6ro of=/sdcard/splash1.img skip=19072 bs=2048 count=150

# We don't dump misc or cache because they do not contain any useful data that we are aware of at this time.

# Logical steps (v2.1):
#
# 0.  test for a target dir and the various tools needed, if not found then exit with error.
# 1.  check "adb devices" for a device in recovery mode. set DEVICEID variable to the device ID. abort when not found.
# 2.  mount system and data partitions read-only, set up adb portforward and create destdir
# 3.  check free space on /cache, exit if less blocks than 20MB free
# 4.  push required tools to device in /cache
# 5   for partitions boot recovery misc:
# 5a  get md5sum for content of current partition *on the device* (no data transfered)
# 5b  while MD5sum comparison is incorrect (always is the first time):
# 5b1 dump current partition to a netcat session
# 5b2 start local netcat to dump image to current dir
# 5b3 compare md5sums of dumped data with dump in current dir. if correct, contine, else restart the loop (6b1)
# 6   for partitions system data:
# 6a  get md5sum for tar of content of current partition *on the device* (no data transfered)
# 6b  while MD5sum comparison is incorrect (always is the first time):
# 6b1 tar current partition to a netcat session
# 6b2 start local netcat to dump tar to current dir
# 6b3 compare md5sums of dumped data with dump in current dir. if correct, contine, else restart the loop (6b1)
# 6c  if i'm running as root:
# 6c1 create a temp dir using either tempdir command or the deviceid in /tmp
# 6c2 extract tar to tempdir
# 6c3 invoke mkyaffs2image to create the img
# 6c4 clean up
# 7.  remove tools from device /cache
# 8.  umount system and data on device
# 9.  print success.


RECOVERY=foo

echo "nandroid-mobile v2.1"
mkfstab.sh > /etc/fstab

if [ "$1" == "" ]; then
	echo "Usage: $0 {backup|restore} [/path/to/nandroid/backup/]"
	echo "- backup will store a full system backup on /sdcard/nandroid"
	echo "- restore path will restore the last made backup for boot, system, recovery and data"
	exit 0
fi

case $1 in
	backup)
		mkyaffs2image=`which mkyaffs2image`
		if [ "$mkyaffs2image" == "" ]; then
			mkyaffs2image=`which mkyaffs2image-arm-uclibc`
			if [ "$mkyaffs2image" == "" ]; then
				echo "error: mkyaffs2image or mkyaffs2image-arm-uclibc not found in path"
				exit 1
			fi
		fi
		dump_image=`which dump_image`
		if [ "$dump_image" == "" ]; then
			dump_image=`which dump_image-arm-uclibc`
			if [ "$dump_image" == "" ]; then
				echo "error: dump_image or dump_image-arm-uclibc not found in path"
				exit 2
			fi
		fi
		break
		;;
	restore)
		flash_image=`which flash_image`
		if [ "$flash_image" == "" ]; then
			flash_image=`which flash_image-arm-uclibc`
			if [ "$flash_image" == "" ]; then
				echo "error: flash_image or flash_image-arm-uclibc not found in path"
				exit 3
			fi
		fi
		unyaffs=`which unyaffs`
		if [ "$unyaffs" == "" ]; then
			echo "error: unyaffs not found in path"
			exit 4
		fi
		break
		;;
esac

# 1
RECOVERY=`cat /proc/cmdline | grep "androidboot.mode=recovery"`
if [ "$RECOVERY" == "foo" ]; then
	echo "error: not running in recovery mode, aborting"
	exit 5
fi
if [ ! "`id -u 2>/dev/null`" == "0" ]; then
	if [ "`whoami 2>&1 | grep 'uid 0'`" == "" ]; then
		echo "error: must run as root, aborting"
		exit 6
	fi
fi


case $1 in
	restore)
		ENERGY=`cat /sys/class/power_supply/battery/capacity`
		if [ "`cat /sys/class/power_supply/battery/status`" == "Charging" ]; then
			ENERGY=100
		fi
		if [ ! $ENERGY -ge 30 ]; then
			echo "Error: not enough battery power"
			echo "Connect charger or USB power and try again"
			exit 7
		fi
		RESTOREPATH=$2
		if [ ! -f $RESTOREPATH/nandroid.md5 ]; then
			echo "error: $RESTOREPATH/nandroid.md5 not found, cannot verify backup data"
			exit 8
		fi
    umount /system 2>/dev/null
    umount /data 2>/dev/null
    mount -o rw /system || FAIL=1
    mount -o rw /data || FAIL=2
    case $FAIL in
    	1) echo "Error mounting system read-write"; umount /system /data /cache; exit 9;;
    	2) echo "Error mounting data read-write"; umount /system /data /cache; exit 10;;
    esac
	
		echo "Verifying backup images..."
		CWD=$PWD
		cd $RESTOREPATH
		md5sum -c nandroid.md5
		if [ $? -eq 1 ]; then
			echo "error: md5sum mismatch, aborting"
			exit 11
		fi
		for image in boot; do
			echo "Flashing $image..."
			$flash_image $image $image.img
		done
		curdir=$(pwd)
		for image in system data cache; do
		  echo "Unpacking $image..."
      cd /$image
      rm -rf *
      unyaffs $curdir/$image.img
      cd $curdir
	  done
	  sync
    umount /system
    umount /data
    echo "Restore done"
		exit 0
		;;
	backup)
		break
		;;
	*)
		echo "Usage: $0 {backup|restore} [/path/to/nandroid/backup/] [backupname]"
		echo "- backup will store a full system backup on /sdcard/nandroid"
		echo "- restore path will restore the last made backup for boot, system, recovery and data"
		exit 12
		;;
esac

# 2.
echo "mounting system and data read-only, sdcard read-write"
umount /system 2>/dev/null
umount /data 2>/dev/null
umount /sdcard 2>/dev/null
mount -o ro /system || FAIL=1
mount -o ro /data || FAIL=2
mount /sdcard || FAIL=3
case $FAIL in
	1) echo "Error mounting system read-only"; umount /system /data /sdcard; exit 13;;
	2) echo "Error mounting data read-only"; umount /system /data /sdcard; exit 14;;
	3) echo "Error mounting sdcard read-write"; umount /system /data /sdcard; exit 15;;
esac

if [ -z "$3" ]
then
    BACKUPNAME="`date +%Y-%m-%d-%H%M`"
else
    BACKUPNAME=$3
fi
BASEDIR=/sdcard/nandroid
if [ ! -z "$2" ]; then
	BASEDIR=$2
fi
	
DESTDIR=$BASEDIR/$BACKUPNAME
if [ ! -d $DESTDIR ]; then 
	mkdir -p $DESTDIR
	if [ ! -d $DESTDIR ]; then 
		echo "error: cannot create $DESTDIR"
		umount /system 2>/dev/null
		umount /data 2>/dev/null
		umount /sdcard 2>/dev/null
		exit 16
	fi
else
	touch $DESTDIR/.nandroidwritable
	if [ ! -e $DESTDIR/.nandroidwritable ]; then
		echo "error: cannot write to $DESTDIR"
		umount /system 2>/dev/null
		umount /data 2>/dev/null
		umount /sdcard 2>/dev/null
		exit 16
	fi
	rm $DESTDIR/.nandroidwritable
fi

# 3.
echo "checking free space on sdcard"
FREEBLOCKS="`df -k /sdcard| grep sdcard | awk '{ print $4 }'`"
# we need about 130MB for the dump
if [ $FREEBLOCKS -le 130000 ]; then
	echo "error: not enough free space available on sdcard (need 130mb), aborting."
	umount /system 2>/dev/null
	umount /data 2>/dev/null
	umount /sdcard 2>/dev/null
	exit 17
fi



if [ -e /dev/mtd/mtd6ro ]; then
	echo -n "Dumping splash1 from device over tcp to $DESTDIR/splash1.img..."
	dd if=/dev/mtd/mtd6ro of=$DESTDIR/splash1.img skip=19072 bs=2048 count=150 2>/dev/null
	echo "done"
	sleep 1s
	echo -n "Dumping splash2 from device over tcp to $DESTDIR/splash2.img..."
	dd if=/dev/mtd/mtd6ro of=$DESTDIR/splash2.img skip=19456 bs=2048 count=150 2>/dev/null
	echo "done"
fi


# 5.
for image in boot recovery misc; do
	# 5a
	DEVICEMD5=`$dump_image $image - | md5sum | awk '{ print $1 }'`
	sleep 1s
	MD5RESULT=1
	# 5b
	echo -n "Dumping $image to $DESTDIR/$image.img..."
	ATTEMPT=0
	while [ $MD5RESULT -eq 1 ]; do
		let ATTEMPT=$ATTEMPT+1
		# 5b1
		$dump_image $image $DESTDIR/$image.img 
		sync
		# 5b3
		echo "${DEVICEMD5}  $DESTDIR/$image.img" | md5sum -c -s -
		if [ $? -eq 1 ]; then
			true
		else
			MD5RESULT=0
		fi
		if [ "$ATTEMPT" == "5" ]; then
			echo "fatal error while trying to dump $image, aborting"
			umount /system
			umount /data
			umount /sdcard
			exit 18
		fi
	done
	echo "done"
done

# 6
for image in system data cache; do
	# 6a
	echo -n "Dumping $image to $DESTDIR/$image.img..."
	$mkyaffs2image /$image $DESTDIR/$image.img
	sync
	echo "done"
done


# 7.
echo -n "generating md5sum file..."
CWD=$PWD
cd $DESTDIR
md5sum *img > nandroid.md5
cd $CWD
echo "done"

# 8.
echo "unmounting system, data and sdcard"
sync
sleep 2s
umount /system
umount /data
umount /sdcard

# 9.
echo "Backup successful."
