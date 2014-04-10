#!/system/bin/sh
export PATH=/system/bin:/system/xbin:$PATH
BLOCK_DEVICE=$1
MOUNT_POINT=$2
LOG_FILE="/dev/null"
LOG_LOCATION="/data/.fsck_log/"

# get syspart-flag from cmdline
set -- $(cat /proc/cmdline)
for x in "$@"; do
    case "$x" in
        syspart=*)
        SYSPART=$(echo "${x#syspart=}")
        ;;
    esac
done

# storage log
if [ "${MOUNT_POINT}" == "/storage_int" ]; then
    mkdir ${LOG_LOCATION}
    busybox find /data/.fsck_log/ -type f -mtime +7  -exec rm {} \;
    TIMESTAMP=`date +%F_%H-%M-%S`
    LOG_FILE=${LOG_LOCATION}/storage_${TIMESTAMP}.log
fi

# mount partition
if [ -e ${BLOCK_DEVICE} ]; then
    # userdata
    if [ "${BLOCK_DEVICE}" == "/dev/block/platform/msm_sdcc.1/by-name/userdata" ];then
        if [ "${SYSPART}" == "system" ];then
            BINDMOUNT_PATH="/data_root/system0"
        elif [ "${SYSPART}" == "system1" ];then
            BINDMOUNT_PATH="/data_root/system1"
        else
            reboot recovery
        fi

        # mount /data_root
        mkdir -p /data_root
        chmod 0755 /data_root
        mount -t ext4 -o nosuid,nodev,barrier=1,noauto_da_alloc ${BLOCK_DEVICE} /data_root

        # bind mount
        mkdir -p ${BINDMOUNT_PATH}
        chmod 0755 ${BINDMOUNT_PATH}
        mount -o bind ${BINDMOUNT_PATH} ${MOUNT_POINT}

    # normal mount
    else
        mount -t ext4 -o nosuid,nodev,barrier=1,noauto_da_alloc ${BLOCK_DEVICE} ${MOUNT_POINT}
    fi
fi

# hide recovery partition
RECOVERY_NODE="$(busybox readlink -f /dev/block/platform/msm_sdcc.1/by-name/recovery)"
busybox mv "${RECOVERY_NODE}" /dev/recovery_moved
busybox mknod -m 0600 "${RECOVERY_NODE}" b 1 3
