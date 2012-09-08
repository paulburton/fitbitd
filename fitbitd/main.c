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

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>

#include <curl/curl.h>
#include <mxml.h>

#include <fitbit.h>
#include "base64.h"
#include "control.h"
#include "devstate.h"
#include "fitbitd-utils.h"
#include "postdata.h"
#include "prefs.h"

#define LOG_TAG "fitbitd"
#include "log.h"

typedef struct fitbit_list_s {
    fitbit_t *fb;
    struct fitbit_list_s *prev, *next;
} fitbit_list_t;

typedef struct {
   uint8_t *data;
   size_t len;
} upload_response_t;

typedef struct sync_op_s {
    uint8_t op[7];

    uint8_t *payload;
    size_t payload_sz;

    struct sync_op_s *next;
} sync_op_t;

typedef struct {
    fitbit_tracker_info_t *tracker;
    long sync_time;
    char tracker_id[20];
    char user_id[20];
} record_state_t;

static void found_fitbit_base(fitbit_t *fb, void *user)
{
    fitbit_list_t **listptr = user;
    fitbit_list_t *new;

    new = malloc(sizeof(*new));
    if (!new) {
        ERR("failed to malloc fitbit list entry\n");
        return;
    }

    new->fb = fb;
    new->prev = NULL;
    new->next = *listptr;
    if (new->next)
        new->next->prev = new;
    *listptr = new;
}

static size_t upload_response_write(void *buf, size_t sz, size_t num, void *user)
{
    upload_response_t *resp = user;
    size_t rsz = sz * num;
    uint8_t *ndata, *write_pos;

    if (resp->data) {
        ndata = realloc(resp->data, resp->len + rsz + 1);
        if (!ndata) {
            ERR("failed to realloc response data\n");
            return 0;
        }
        write_pos = &ndata[resp->len];
        resp->data = ndata;
        resp->len += rsz;
    } else {
        write_pos = resp->data = malloc(rsz + 1);
        if (!resp->data) {
            ERR("failed to malloc response data\n");
            return 0;
        }
        resp->len = rsz;
    }

    memcpy(write_pos, buf, rsz);
    write_pos[rsz] = 0;
    return rsz;
}

static void parse_response_part(postdata_t *pd, const char *resp, record_state_t *rst)
{
    const char *eq;
    char name[128], val[128];

    DBG("response part %s\n", resp);

    eq = resp;
    while (*eq && (*eq != '='))
        eq++;

    if (!*eq) {
        ERR("invalid response part %s\n", resp);
        return;
    }

    memcpy(name, resp, eq - resp);
    name[eq - resp] = 0;

    strcpy(val, eq + 1);

    postdata_append(pd, name, val);

    if (!strcmp(name, "trackerPublicId")) {
        strncpy(rst->tracker_id, val, sizeof(rst->tracker_id));
    } else if (!strcmp(name, "userPublicId")) {
        strncpy(rst->user_id, val, sizeof(rst->user_id));
    }
}

static void parse_response(postdata_t *pd, const char *resp, record_state_t *rst)
{
    const char *curr_start, *curr_end;
    char buf[128];

    curr_start = curr_end = resp;

    while (*curr_end) {
        while (*curr_end && (*curr_end != '&'))
            curr_end++;

        memcpy(buf, curr_start, curr_end - curr_start);
        buf[curr_end - curr_start] = 0;
        parse_response_part(pd, buf, rst);

        if (!*curr_end)
           break;

        curr_start = curr_end = curr_end + 1;
    }
}

static void mkfiledir(const char *filename)
{
    char tmp[PATH_MAX], *p;

    strncpy(tmp, filename, sizeof(tmp));

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, S_IRWXU);
            *p = '/';
        }
    }
}

static void dump_sync_op(fitbitd_prefs_t *prefs, uint8_t serial[5], long sync_time, int op_num, uint8_t op[7], uint8_t *payload, size_t payload_sz, uint8_t *response, size_t response_sz)
{
    char fname_base[PATH_MAX], fname_op[PATH_MAX];
    char fname_payload[PATH_MAX], fname_response[PATH_MAX];
    FILE *file;

    if (!prefs->dump_directory)
        return;

    snprintf(fname_base, sizeof(fname_base), "%s/%02x%02x%02x%02x%02x-%ld/%d",
          prefs->dump_directory,
          serial[0], serial[1], serial[2], serial[3], serial[4],
          sync_time, op_num);
    mkfiledir(fname_base);

    snprintf(fname_op, sizeof(fname_op), "%s-op", fname_base);
    file = fopen(fname_op, "w");
    if (file) {
        fwrite(op, 1, 7, file);
        fclose(file);
        DBG("dumped to %s\n", fname_op);
    } else
        ERR("failed to open %s\n", fname_op);

    if (payload) {
        snprintf(fname_payload, sizeof(fname_payload), "%s-payload", fname_base);
        file = fopen(fname_payload, "w");
        if (file) {
            fwrite(payload, 1, payload_sz, file);
            fclose(file);
            DBG("dumped to %s\n", fname_payload);
        } else
            ERR("failed to open %s\n", fname_payload);
    }

    if (response) {
        snprintf(fname_response, sizeof(fname_response), "%s-response", fname_base);
        file = fopen(fname_response, "w");
        if (file) {
            fwrite(response, 1, response_sz, file);
            fclose(file);
            DBG("dumped to %s\n", fname_response);
        } else
            ERR("failed to open %s\n", fname_response);
    }
}

static void record_callback(devstate_t *dev, void *user)
{
    record_state_t *rst = user;

    dev->last_sync_time = rst->sync_time;

    if (rst->tracker_id[0])
        strncpy(dev->tracker_id, rst->tracker_id, sizeof(dev->tracker_id));
    if (rst->user_id[0])
        strncpy(dev->user_id, rst->user_id, sizeof(dev->user_id));

    DBG("record_callback tracker %s\n", rst->tracker_id);
}

static void state_syncing_callback(devstate_t *dev, void *user)
{
    dev->state |= DEV_STATE_SYNCING;
}

static void state_not_syncing_callback(devstate_t *dev, void *user)
{
    dev->state &= ~DEV_STATE_SYNCING;
}

static void sync_tracker(fitbit_t *fb, fitbit_tracker_info_t *tracker, void *user)
{
    fitbitd_prefs_t *prefs = user;
    CURL *curl = NULL;
    CURLcode response;
    upload_response_t resp_state;
    postdata_t *pd = NULL;
    mxml_node_t *xml = NULL, *xml_response, *xml_op, *xml_opcode, *xml_payload;
    char url[256], postname[30], response_enc[32768], *response_body = NULL;
    const char *attr_host, *attr_path, *attr_port, *attr_secure;
    const char *attr_encrypted, *val_opcode, *val_payload, *val_response;
    sync_op_t *ops = NULL, **last_op, *op;
    int bytes, ret, op_idx, op_num = 0;
    uint8_t payload_buf[512], response_buf[32768];
    size_t response_len;
    record_state_t rst;

    INFO("syncing tracker %s\n", tracker->serial_str);
    devstate_record(tracker->serial, state_syncing_callback, &rst);
    control_signal_state_change();

    rst.tracker = tracker;
    rst.sync_time = get_uptime();
    rst.tracker_id[0] = 0;
    rst.user_id[0] = 0;

    curl = curl_easy_init();
    if (!curl) {
        ERR("failed to init curl\n");
        goto out;
    }

    snprintf(url, sizeof(url), "%s", prefs->upload_url);

    do {
        pd = postdata_create();
        if (!pd) {
            ERR("failed to create postdata\n");
            goto out;
        }

        /* standard parameters */
        postdata_append(pd, "beaconType", "standard");
        postdata_append(pd, "clientMode", "standard");
        postdata_append(pd, "clientVersion", prefs->client_version);
        postdata_append(pd, "os", prefs->os_name);
        postdata_append(pd, "clientId", prefs->client_id);

        /* parse response body */
        if (response_body) {
            parse_response(pd, response_body, &rst);
            free(response_body);
            response_body = NULL;
    
            devstate_record(tracker->serial, record_callback, &rst);
            control_signal_state_change();
        }

        /* perform ops */
        for (op = ops, op_idx = 0; op; op = op->next, op_idx++, op_num++) {
            ret = fitbit_run_op(fb, op->op, op->payload, op->payload_sz, response_buf, sizeof(response_buf), &response_len);
            if (ret)
                ERR("op %d failed\n", op_idx);
            if (!ret) {
                ret = b64encode((uint8_t*)response_enc, sizeof(response_enc), response_buf, response_len);
                if (ret)
                    ERR("op %d base64 encode failed, len %d\n", op_idx, (int)response_len);
            }

            if (ret) {
                snprintf(postname, sizeof(postname), "opStatus[%d]", op_idx);
                postdata_append(pd, postname, "error");
                continue;
            }

            dump_sync_op(prefs, tracker->serial, rst.sync_time, op_num,
                  op->op, op->payload, op->payload_sz,
                  response_buf, response_len);

            snprintf(postname, sizeof(postname), "opResponse[%d]", op_idx);
            postdata_append(pd, postname, response_enc);

            snprintf(postname, sizeof(postname), "opStatus[%d]", op_idx);
            postdata_append(pd, postname, "success");
        }

        /* destroy ops list */
        while (ops) {
            op = ops;
            ops = ops->next;
            free(op->payload);
            free(op);
        }
        last_op = &ops;

        DBG("POST %s\n", postdata_string(pd));

        resp_state.data = NULL;
        resp_state.len = 0;

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postdata_string(pd));
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, upload_response_write);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp_state);

        response = curl_easy_perform(curl);
        if (response) {
            ERR("upload failure %d\n", (int)response);
            return;
        }

        if (!resp_state.data) {
            ERR("no POST response\n");
            goto out;
        }

        DBG("POST response %s\n", (char*)resp_state.data);

        xml = mxmlLoadString(NULL, (char*)resp_state.data, MXML_TEXT_CALLBACK);
        if (!xml) {
            ERR("failed to parse response XML\n");
            goto out;
        }

        url[0] = 0;

        xml_response = mxmlFindPath(xml, "fitbitClient/response");
        while (xml_response &&
               (mxmlGetType(xml_response) != MXML_ELEMENT ||
                strcasecmp(mxmlGetElement(xml_response), "response")))
           xml_response = mxmlGetParent(xml_response);
        if (xml_response) {
            attr_host = mxmlElementGetAttr(xml_response, "host");
            attr_path = mxmlElementGetAttr(xml_response, "path");
            attr_port = mxmlElementGetAttr(xml_response, "port");
            attr_secure = mxmlElementGetAttr(xml_response, "secure");

            if (attr_host && attr_path) {
                snprintf(url, sizeof(url), "%s://%s%s%s%s",
                         (attr_secure && !strcasecmp(attr_secure, "true")) ? "https" : "http",
                         attr_host,
                         attr_port ? ":" : "",
                         attr_port ? attr_port : "",
                         attr_path);
                DBG("new URL %s\n", url);
            }

            val_response = mxmlGetText(xml_response, NULL);
            if (val_response && val_response[0]) {
                response_body = strdup(val_response);
                if (!response_body) {
                    ERR("failed to alloc response body\n");
                    goto out;
                }
            }
        }

        xml_op = mxmlFindPath(xml, "fitbitClient/device/remoteOps");
        while (xml_op &&
               (mxmlGetType(xml_op) != MXML_ELEMENT ||
                strcasecmp(mxmlGetElement(xml_op), "remoteOps")))
           xml_op = mxmlGetParent(xml_op);
        if (xml_op) {
            for (xml_op = mxmlGetFirstChild(xml_op); xml_op; xml_op = mxmlGetNextSibling(xml_op)) {
                /* skip non-remoteOp nodes */
                if (mxmlGetType(xml_op) != MXML_ELEMENT)
                    continue;
                if (strcasecmp(mxmlGetElement(xml_op), "remoteOp"))
                    continue;

                /* check encrypted */
                attr_encrypted = mxmlElementGetAttr(xml_op, "encrypted");
                if (attr_encrypted && strcasecmp(attr_encrypted, "false")) {
                    ERR("op is encrypted - unimplemented! this probably won't work\n");
                }

                /* find opcode & payload */
                val_opcode = val_payload = NULL;
                xml_opcode = mxmlFindPath(xml_op, "opCode");
                if (xml_opcode)
                    val_opcode = mxmlGetText(xml_opcode, NULL);
                xml_payload = mxmlFindPath(xml_op, "payloadData");
                if (xml_payload)
                    val_payload = mxmlGetText(xml_payload, NULL);

                /* check opcode */
                if (!val_opcode || !val_opcode[0]) {
                    ERR("no opcode found\n");
                    goto out;
                }

                /* alloc op */
                op = calloc(1, sizeof(*op));
                if (!op) {
                    ERR("failed to malloc op\n");
                    goto out;
                }

                /* decode op */
                bytes = b64decode(op->op, sizeof(op->op), (const unsigned char *)val_opcode);
                if (bytes < 0) {
                    ERR("failed to decode op %s\n", val_opcode);
                    goto out;
                }

                if (val_payload && val_payload[0]) {
                    /* decode payload */
                    bytes = b64decode(payload_buf, sizeof(payload_buf), (const unsigned char *)val_payload);
                    if (bytes < 0) {
                        ERR("failed to decode payload %s\n", val_payload);
                        goto out;
                    }

                    /* copy to op */
                    op->payload = malloc(bytes);
                    if (!op->payload) {
                        ERR("failed to malloc op payload\n");
                        goto out;
                    }
                    memcpy(op->payload, payload_buf, bytes);
                    op->payload_sz = bytes;
                }

                /* append to ops list */
                *last_op = op;
                last_op = &op->next;
            }
        }

        postdata_destroy(pd);
        pd = NULL;

        mxmlDelete(xml);
        xml = NULL;
    } while (url[0]);

    INFO("sync %s complete\n", tracker->serial_str);
    fitbit_tracker_sleep(fb, prefs->sync_delay);

    rst.sync_time = get_uptime();
    devstate_record(tracker->serial, record_callback, &rst);

out:
    devstate_record(tracker->serial, state_not_syncing_callback, &rst);
    control_signal_state_change();

    if (response_body)
        free(response_body);
    while (ops) {
        op = ops;
        ops = ops->next;
        free(op->payload);
        free(op);
    }
    if (xml)
        mxmlDelete(xml);
    if (pd)
        postdata_destroy(pd);
    if (curl)
        curl_easy_cleanup(curl);
}

static int daemonize(void)
{
    pid_t pid, sid;

    if (getppid() == 1) {
        /* already a daemon? */
        return 0;
    }

    pid = fork();
    if (pid < 0) {
        ERR("failed to fork\n");
        return -1;
    }
    if (pid > 0) {
        /* parent process */
        exit(EXIT_SUCCESS);
    }

    /* child process */

    umask(0);

    sid = setsid();
    if (sid < 0)
        return -1;

    /* prevent locking directory fitbitd was exec'd from */
    if (chdir("/") < 0)
        return -1;

    /* reopen std files */
    if (!freopen("/dev/null", "r", stdin))
       ERR("failed to freopen stdin\n");
    if (!freopen("/dev/null", "w", stdout))
       ERR("failed to freopen stdout\n");
    if (!freopen("/dev/null", "w", stderr))
       ERR("failed to freopen stderr\n");

    return 0;
}

static void print_usage(FILE *f)
{
    fprintf(f, "Usage: fitbitd <args>\n"
          "\n"
          "Where args is any of:\n"
          "  --help             Output this message\n"
          "  --version          Output version and exit\n"
          "  --no-daemon        Don't daemonise fitbitd\n"
          "  --no-dbus          Disable DBUS control\n"
          "  --dump <dir>       Dump all sync operations to the directory <dir>\n"
          "  --log <filename>   Write log messages to <filename>\n"
          "  --exit             Request that fitbitd exits\n");
}

int main(int argc, char *argv[])
{
    fitbit_list_t *fblist = NULL, *curr;
    fitbitd_prefs_t *prefs = NULL;
    int argi, ret = EXIT_FAILURE;
    int synced, lockfile = -1;
    bool opt_version = false;
    bool opt_nodaemon = false;
    bool opt_nodbus = false;
    bool opt_exit = false;
    bool opt_help = false;
    char *opt_dump = NULL;
    char *opt_log = NULL;

    for (argi = 1; argi < argc; argi++) {
        if (!strcmp(argv[argi], "--version")) {
            opt_version = true;
            continue;
        }

        if (!strcmp(argv[argi], "--help")) {
            opt_help = true;
            continue;
        }

        if (!strcmp(argv[argi], "--no-daemon")) {
            opt_nodaemon = true;
            continue;
        }

        if (!strcmp(argv[argi], "--no-dbus")) {
            opt_nodbus = true;
            continue;
        }

        if (!strcmp(argv[argi], "--exit")) {
            opt_exit = true;
            continue;
        }

        if (!strcmp(argv[argi], "--dump")) {
            if (++argi >= argc) {
                ERR("--dump requires directory name\n");
                goto out;
            }
            opt_dump = argv[argi];
            continue;
        }

        if (!strcmp(argv[argi], "--log")) {
            if (++argi >= argc) {
                ERR("--log requires filename\n");
                goto out;
            }
            opt_log = argv[argi];
            continue;
        }

        ERR("Unknown argument '%s'\n", argv[argi]);
        print_usage(stderr);
        goto out;
    }

    if (opt_version) {
        printf("fitbitd version " VERSION "\n");
        ret = EXIT_SUCCESS;
        goto out;
    }

    if (opt_help) {
        print_usage(stdout);
        ret = EXIT_SUCCESS;
        goto out;
    }

    if (opt_exit) {
       if (!control_call_exit()) {
           ret = EXIT_SUCCESS;
       }
       goto out;
    }

    DBG("fitbitd version " VERSION "\n");

    prefs = prefs_create();
    if (!prefs) {
        ERR("failed to create prefs\n");
        goto out;
    }

    if (opt_dump) {
        if (prefs->dump_directory)
            free(prefs->dump_directory);
        prefs->dump_directory = strdup(opt_dump);
        if (!prefs->dump_directory) {
            ERR("failed to alloc dump directory\n");
            goto out;
        }
    }

    mkfiledir(prefs->lock_filename);
    lockfile = open(prefs->lock_filename, O_RDWR | O_CREAT, 0640);
    if (lockfile < 0) {
        ERR("failed to create lock file %s\n", prefs->lock_filename);
        goto out;
    }
    if (flock(lockfile, LOCK_EX | LOCK_NB)) {
        ERR("failed to lock %s\n", prefs->lock_filename);
        goto out;
    }

    if (!opt_nodaemon && daemonize())
        goto out;

    if (opt_log) {
      if (!freopen(opt_log, "w", stderr))
         ERR("failed to freopen stderr for log %s\n", opt_log);
      setvbuf(stderr, NULL, _IONBF, 0);
    }

    if (!opt_nodbus && control_start()) {
        ERR("failed to start control\n");
        goto out;
    }

    while (!control_exited()) {
        fitbit_find_bases(found_fitbit_base, &fblist);

        for (curr = fblist; curr; curr = curr->next) {
            synced = fitbit_sync_trackers(curr->fb, sync_tracker, prefs);

            if (synced < 0) {
                DBG("sync failed, destroying base\n");

                /* remove it from the list */
                if (curr == fblist) {
                    fblist = curr->next;
                    if (fblist)
                        fblist->prev = NULL;
                } else {
                    curr->prev->next = curr->next;
                    if (curr->next)
                        curr->next->prev = curr->prev;
                }

                /* cleanup */
                fitbit_destroy(curr->fb);
                free(curr);

                break;
            }

            if (control_exited())
                break;

            DBG("synced %d trackers\n", synced);
        }

        devstate_clean(get_uptime() - ((prefs->sync_delay * 3) / 2));
        if (!control_exited())
           sleep(prefs->scan_delay);
    }

    ret = EXIT_SUCCESS;

out:
    control_stop();
    while (fblist) {
        fitbit_list_t *curr = fblist;
        fblist = curr->next;
        fitbit_destroy(curr->fb);
    }
    if (prefs)
        prefs_destroy(prefs);
    if (lockfile >= 0)
        close(lockfile);
    return ret;
}
