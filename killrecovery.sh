#!/sbin/sh
mkdir -p /sd-ext
rm /cache/recovery/command
rm /cache/update.zip
touch /tmp/.ignorebootmessage
kill $(ps | grep /sbin/adbd)
kill $(ps | grep /sbin/recovery)

# On the Galaxy S, the recovery comes test signed, but the
# recovery is not automatically restarted.
if [ -f /init.smdkc110.rc ]
then
    /sbin/recovery &
fi

# Droid X
if [ -f /init.mapphone_cdma.rc ]
then
    /sbin/recovery &
fi

exit 1
