#!/sbin/sh
mkdir -p /sd-ext
kill $(ps | grep /sbin/adbd)
kill $(ps | grep /sbin/recovery)
