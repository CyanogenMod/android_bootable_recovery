#!/sbin/sh
cp /sbin/su.recovery /system/xbin/su
chmod 6755 /system/xbin/su
ln -sf /system/xbin/su /system/bin/su

# if the system is at least 4.3, and there is no su daemon built in,
# let's try to install it using install-recovery.sh
BUILD_RELEASE_VERSION=$(cat /system/build.prop | grep ro\\.build\\.version\\.release)
IS_43=$(echo $BUILD_RELEASE_VERSION | grep 4\\.3)
if [ -z "$IS_43" -o "$IS_43" \> "4.3"  -o "$IS_43" == "4.3" ]
then
    # check for rom su daemon before clobbering install-recovery.sh
    if [ ! -f "/system/etc/.has_su_daemon" ]
    then
        echo -n -e 'ui_print Installing Superuser daemon...\n' > /proc/self/fd/$2
        echo -n -e 'ui_print\n' > /proc/self/fd/$2
        cp install-recovery.sh /system/etc/install-recovery.sh
        chmod 755 /system/etc/install-recovery.sh
        # note that an post install su daemon was installed
        # so recovery doesn't freak out and recommend you disable
        # the install-recovery.sh execute bit.
        touch /system/etc/.installed_su_daemon
    fi
fi
