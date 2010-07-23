#!/sbin/sh
mkdir -p /sd-ext
rm /cache/recovery/command
rm /cache/update.zip
touch /tmp/.ignorebootmessage
kill $(ps | grep /sbin/adbd)
kill $(ps | grep /sbin/recovery)

exit 1