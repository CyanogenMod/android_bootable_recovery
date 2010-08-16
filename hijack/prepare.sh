#!/preinstall/recovery/sh
export PATH=$PATH:/preinstall/recovery
busybox mount -orw,remount /
busybox mkdir -p /res/images
