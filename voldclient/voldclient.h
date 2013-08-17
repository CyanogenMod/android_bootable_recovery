/*
 * Copyright (c) 2013 The CyanogenMod Project
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
#ifndef _VOLD_CLIENT_H
#define _VOLD_CLIENT_H

void vold_client_start(void);

int vold_get_volumes(int sync);
int vold_mount_volume(const char* name, int sync);
int vold_unmount_volume(const char* name, int sync);

int vold_dispatch(int code, char** tokens, int len);
int vold_command(int len, const char** command, int sync);

#endif

