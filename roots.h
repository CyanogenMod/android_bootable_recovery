/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef RECOVERY_ROOTS_H_
#define RECOVERY_ROOTS_H_

#include "common.h"
#include <fs_mgr.h>

// Load and parse volume data from /etc/recovery.fstab.
void load_volume_table();

// Return the Volume* record for this path (or NULL).
Volume* volume_for_path(const char* path);

// Make sure that the volume 'path' is on is mounted.  Returns 0 on
// success (volume is mounted).
int ensure_volume_mounted(Volume* v, bool force_rw=false);
int ensure_path_mounted(const char* path, bool force_rw=false);
// Above, plus override SELinux default context
int remount_no_selinux(const char* path);

// Similar to ensure_path_mounted, but allows one to specify the mount_point.
int ensure_path_mounted_at(const char* path, const char* mount_point, bool force_rw=false);

// Make sure that the volume 'path' is on is unmounted.  Returns 0 on
// success (volume is unmounted);
int ensure_volume_unmounted(Volume *v, bool detach=false);
int ensure_path_unmounted(const char* path, bool detach=false);

// Reformat the given volume (must be the mount point only, eg
// "/cache"), no paths permitted.  Attempts to unmount the volume if
// it is mounted.
int format_volume(const char* volume, bool force = false);

// Ensure that all and only the volumes that packages expect to find
// mounted (/tmp and /cache) are mounted.  Returns 0 on success.
int setup_install_mounts();

int get_num_volumes();

bool volume_is_mountable(Volume *v);
bool volume_is_readonly(Volume *v);
bool volume_is_verity(Volume *v);

#define MAX_NUM_MANAGED_VOLUMES 10

#endif  // RECOVERY_ROOTS_H_
