/*
 * This file is part of fitbitd.
 *
 * fitbitd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * fitbitd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with fitbitd.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include "prefs.h"

static char cfg_home_fallback[] = "/tmp/fitbitd";
static char *cfg_home = NULL;

static char *get_config_home(void)
{
    char *home;

    if (!cfg_home) {
        home = getenv("XDG_CONFIG_HOME");
        if (home) {
            cfg_home = malloc(strlen(home) + 9);
            if (cfg_home) {
                strcpy(cfg_home, home);
                strcpy(&cfg_home[strlen(home)], "/fitbitd");
            }
        }
    }

    if (!cfg_home) {
        home = getenv("HOME");
        if (home) {
            cfg_home = malloc(strlen(home) + 17);
            if (cfg_home) {
                strcpy(cfg_home, home);
                strcpy(&cfg_home[strlen(home)], "/.config/fitbitd");
            }
        }
    }

    if (!cfg_home)
        cfg_home = cfg_home_fallback;

    return cfg_home;
}

fitbitd_prefs_t *prefs_create(void)
{
    fitbitd_prefs_t *prefs;
    struct utsname un;
    char *cfg_home = get_config_home();

    prefs = malloc(sizeof(*prefs));
    if (!prefs)
        goto oom_prefs;

    prefs->upload_url = strdup("https://client.fitbit.com/device/tracker/uploadData");
    if (!prefs->upload_url)
        goto oom_upload_url;

    prefs->client_id = strdup("2ea32002-a079-48f4-8020-0badd22939e3");
    if (!prefs->client_id)
        goto oom_client_id;

    prefs->client_version = strdup(VERSION);
    if (!prefs->client_version)
        goto oom_client_version;

    if (uname(&un)) {
        prefs->os_name = strdup("fitbitd");
    } else {
        prefs->os_name = malloc(8 + strlen(un.sysname) + 1);
        if (prefs->os_name)
            sprintf(prefs->os_name, "fitbitd-%s", un.sysname);
    }
    if (!prefs->os_name)
        goto oom_os_name;

    prefs->lock_filename = malloc(strlen(cfg_home) + 6);
    if (!prefs->lock_filename)
        goto oom_lock_filename;
    strcpy(prefs->lock_filename, cfg_home);
    strcpy(&prefs->lock_filename[strlen(cfg_home)], "/lock");

    prefs->dump_directory = NULL;
    prefs->log_filename = NULL;

    prefs->scan_delay = 10;
    prefs->sync_delay = 15 * 60;

    return prefs;

oom_lock_filename:
    free(prefs->os_name);
oom_os_name:
    free(prefs->client_version);
oom_client_version:
    free(prefs->client_id);
oom_client_id:
    free(prefs->upload_url);
oom_upload_url:
    free(prefs);
oom_prefs:
    return NULL;
}

void prefs_destroy(fitbitd_prefs_t *prefs)
{
    free(prefs->log_filename);
    free(prefs->dump_directory);
    free(prefs->lock_filename);
    free(prefs->os_name);
    free(prefs->client_version);
    free(prefs->client_id);
    free(prefs->upload_url);
    free(prefs);
}
