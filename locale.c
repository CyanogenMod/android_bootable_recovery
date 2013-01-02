/*
 * Copyright (C) 2013 The CyanogenMod Project
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

#include <pthread.h>

#include "common.h"
#include "ui.h"
#include "minui/minui.h"

char *ui_translate(char *input)
{
    FILE *dict;
    int i = 0;
    int j = 0;
    char *delim = "=";
    unsigned char *tmp = "";
    char input_fmt[256] = {};
    static char output[256] = {};

    if (input == NULL)
        return "";

    if (strcmp(input, "") == 0 || strlen(input) == 1 || BOARD_FONT_CHAR_MAX == 96)
        return input;

    for (i = 0; i < strlen(input); i++) {
        if (input[i] == '\n') {
            strcat(input_fmt, "\\n");
            j++;
        } else if (input[i] == '"') {
            strcat(input_fmt, "\\\"");
            j++;
        } else {
            input_fmt[j] = input[i];
        }

        if (++j >= sizeof(input_fmt)) break;
    }
    input_fmt[j] = '\0';

    char path[PATH_MAX] = {};
    strcat(path, LOCALES_PATH);
    sprintf(path, "%s%s", path, ui_get_locale());
    dict = fopen(path, "r");

    char buffer[MAX_COLS*4+1] = {};
    while (dict != NULL && fgets(buffer, sizeof(buffer), dict) != NULL)
    {
        if (strstr(buffer, input_fmt)) {
            if (strchr(buffer, *delim) == NULL) {
                // ignore comments in locale files
                if (buffer[0] != '#')
                    LOGE("Invalid file format\n");

            } else {
                tmp = strtok(buffer, delim);

                // do not accept partial matches
                if (strcmp(tmp, input_fmt) == 0) {
                    tmp = strtok(NULL, delim);

                    if (tmp[strlen(tmp)-1] == '\n')
                        tmp[strlen(tmp)-1] = '\0';
                    break;
                } else {
                    tmp = input_fmt;
                }
            }
        }
    }
    if (dict != NULL)
        fclose(dict);

    j = 0;
    strcpy(output, "");
    for (i = 0; i < strlen(tmp); i++) {
        if (tmp[i] == '\\') {
            switch (tmp[++i]) {
                case 'n': output[j] = '\n'; break;
                case 'r': output[j] = '\r'; break;
                case 't': output[j] = '\t'; break;
                default: output[j] = tmp[i]; break;
            }

        } else if (tmp[i] > 127) {
            // format the UTF-8 value into a psuedo UTF-16 value
            output[j] = (0x40*((tmp[i])-0xC2)+(tmp[i+1]));
            i++;

        } else {
            output[j] = tmp[i];
        }

        if (++j >= sizeof(output)) break;
    }
    output[j] = '\0';

    if (strcmp(output, "") == 0)
        return input;
    return output;
}

char *ui_get_locale()
{
    return ui_locale;
}

void ui_set_locale(char *locale)
{
    if (locale == NULL)
        locale = "english";

    ui_locale = locale;
    LOGI("locale is [%s]\n", ui_locale);
}
