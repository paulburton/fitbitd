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

#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "postdata.h"

struct postdata_s {
    char *str;
    size_t str_sz;
    CURL *curl;
};

postdata_t *postdata_create(void)
{
    postdata_t *pd;

    pd = calloc(1, sizeof(*pd));
    if (!pd)
        return NULL;

    pd->curl = curl_easy_init();
    if (!pd->curl)
        goto err_curl;

    return pd;

err_curl:
    free(pd);
    return NULL;
}

void postdata_destroy(postdata_t *pd)
{
    curl_easy_cleanup(pd->curl);
    free(pd->str);
    free(pd);
}

int postdata_append(postdata_t *pd, const char *name, const char *val)
{
    size_t nsz;
    char *name_esc, *val_esc, *nstr, *write_pos;

    name_esc = curl_easy_escape(pd, name, 0);
    if (!name_esc)
        goto oom_name;

    val_esc = curl_easy_escape(pd, val, 0);
    if (!val_esc)
        goto oom_val;

    nsz = strlen(name_esc) + 1 + strlen(val_esc);

    if (pd->str) {
        nstr = realloc(pd->str, pd->str_sz + 1 + nsz + 1);
        if (!nstr)
            goto oom_str;
        nstr[pd->str_sz++] = '&';
        write_pos = &nstr[pd->str_sz];
        pd->str = nstr;
    } else {
        pd->str = malloc(nsz + 1);
        if (!pd->str)
            goto oom_str;
        write_pos = pd->str;
    }

    memcpy(write_pos, name_esc, strlen(name_esc));
    write_pos += strlen(name_esc);
    *write_pos++ = '=';
    memcpy(write_pos, val_esc, strlen(val_esc));
    write_pos += strlen(val_esc);
    *write_pos = 0;
    pd->str_sz += nsz;

    curl_free(val_esc);
    curl_free(name_esc);

    return 0;

oom_str:
    curl_free(val_esc);
oom_val:
    curl_free(name_esc);
oom_name:
    return -1;
}

char *postdata_string(postdata_t *pd)
{
    return pd->str;
}
