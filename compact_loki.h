/*
 * By Seth Shelnutt
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

#define Loki_Image "/tmp/loki_image"
#define BOOT_PARTITION "/dev/block/platform/msm_sdcc.1/by-name/boot"
#define RECOVERY_PARTITION "/dev/block/platform/msm_sdcc.1/by-name/recovery"

int loki_patch_shellcode(unsigned int addr);

int loki_patch(char *partition, char *partitionPath);

int loki_flash(char *partition);

int loki_check_partition(char *partition);

int loki_check();
