=============================
Loki
by Dan Rosenberg (@djrbliss)
=============================

Loki is a set of tools for creating and flashing custom kernels and recoveries
on the AT&T and Verizon branded Samsung Galaxy S4, the Samsung Galaxy Stellar,
and various locked LG devices. For an explanation of how the exploit works,
please see the technical blog post at:

http://blog.azimuthsecurity.com/2013/05/exploiting-samsung-galaxy-s4-secure-boot.html

Devices must be rooted in order to flash custom kernels and recoveries.

"loki_patch" is a tool primarily intended for developers to create custom
kernels and recoveries. It's designed to take a specific aboot image and an
unmodified boot or recovery image, and it generates an output image in a new
file format, ".lok". The resulting .lok image is specifically tailored for the
device build it was created with, and can be flashed directly to the recovery
or boot partition on the target device.

"loki_flash" is a sample utility that can be used to flash a .lok image to an
actual device. It will verify that the provided .lok image is safe to flash for
a given target, and then perform the flashing if validation is successful. It
is also possible to simply use "dd" to flash a .lok image directly to the boot
or recovery partition, but using loki_flash is recommended in order to validate
that the .lok matches the target device.


=============
Sample usage
=============

First, a developer must pull the aboot image from a target device:


dan@pc:~$ adb shell
shell@android:/ $ su
shell@android:/ # dd if=/dev/block/platform/msm_sdcc.1/by-name/aboot of=/data/local/tmp/aboot.img
shell@android:/ # chmod 644 /data/local/tmp/aboot.img
shell@android:/ # exit
shell@android:/ $ exit
dan@pc:~$ adb pull /data/local/tmp/aboot.img
3293 KB/s (2097152 bytes in 0.621s)


Next, a .lok image can be prepared using loki_patch:


dan@pc:~$ loki_patch
Usage: ./loki_patch [boot|recovery] [aboot.img] [in.img] [out.lok]
dan@pc:~$ loki_patch recovery aboot.img cwm.img cwm.lok
[+] Detected target AT&T build JDQ39.I337UCUAMDB or JDQ39.I337UCUAMDL
[+] Output file written to cwm.lok


Finally, the .lok image can be flashed using loki_flash:


dan@pc:~$ adb push cwm.lok /data/local/tmp
dan@pc:~$ adb push loki_flash /data/local/tmp
dan@pc:~$ adb shell
shell@android:/ $ su
shell@android:/ # /data/local/tmp/loki_flash
Usage: /data/local/tmp/loki_flash [boot|recovery] [in.lok]
shell@android:/ # /data/local/tmp/loki_flash recovery /data/local/tmp/cwm.lok
[+] Loki validation passed, flashing image.
2253+1 records in
2253+1 records out
9230848 bytes transferred in 0.656 secs (14071414 bytes/sec)
[+] Loki flashing complete!
