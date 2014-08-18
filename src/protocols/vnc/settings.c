/*
 * Copyright (C) 2014 Glyptodon LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "config.h"

#include "client.h"
#include "settings.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Client plugin arguments */
const char* GUAC_VNC_CLIENT_ARGS[] = {
    "hostname",
    "port",
    "read-only",
    "encodings",
    "password",
    "swap-red-blue",
    "color-depth",
    "cursor",
    "autoretry",

#ifdef ENABLE_VNC_REPEATER
    "dest-host",
    "dest-port",
#endif

#ifdef ENABLE_PULSE
    "enable-audio",
    "audio-servername",
#endif

#ifdef ENABLE_VNC_LISTEN
    "reverse-connect",
    "listen-timeout",
#endif

    NULL
};

enum VNC_ARGS_IDX {

    IDX_HOSTNAME,
    IDX_PORT,
    IDX_READ_ONLY,
    IDX_ENCODINGS,
    IDX_PASSWORD,
    IDX_SWAP_RED_BLUE,
    IDX_COLOR_DEPTH,
    IDX_CURSOR,
    IDX_AUTORETRY,

#ifdef ENABLE_VNC_REPEATER
    IDX_DEST_HOST,
    IDX_DEST_PORT,
#endif

#ifdef ENABLE_PULSE
    IDX_ENABLE_AUDIO,
    IDX_AUDIO_SERVERNAME,
#endif

#ifdef ENABLE_VNC_LISTEN
    IDX_REVERSE_CONNECT,
    IDX_LISTEN_TIMEOUT,
#endif

    VNC_ARGS_COUNT
};

int guac_vnc_parse_args(guac_vnc_settings* settings, int argc, const char** argv) {

    /* Validate arg count */
    if (argc != VNC_ARGS_COUNT)
        return 1;

    settings->hostname = strdup(argv[IDX_HOSTNAME]);
    settings->port = atoi(argv[IDX_PORT]);
    settings->password = strdup(argv[IDX_PASSWORD]); /* NOTE: freed by libvncclient */

    /* Set flags */
    settings->remote_cursor = (strcmp(argv[IDX_CURSOR], "remote") == 0);
    settings->swap_red_blue = (strcmp(argv[IDX_SWAP_RED_BLUE], "true") == 0);
    settings->read_only     = (strcmp(argv[IDX_READ_ONLY], "true") == 0);

    /* Parse color depth */
    settings->color_depth = atoi(argv[IDX_COLOR_DEPTH]);

#ifdef ENABLE_VNC_REPEATER
    /* Set repeater parameters if specified */
    if (argv[IDX_DEST_HOST][0] != '\0')
        settings->dest_host = strdup(argv[IDX_DEST_HOST]);
    else
        settings->dest_host = NULL;

    if (argv[IDX_DEST_PORT][0] != '\0')
        settings->dest_port = atoi(argv[IDX_DEST_PORT]);
#endif

    /* Set encodings if specified */
    if (argv[IDX_ENCODINGS][0] != '\0')
        settings->encodings = strdup(argv[IDX_ENCODINGS]);
    else
        settings->encodings = NULL;

    /* Parse autoretry */
    if (argv[IDX_AUTORETRY][0] != '\0')
        settings->retries = atoi(argv[IDX_AUTORETRY]);
    else
        settings->retries = 0; 

#ifdef ENABLE_VNC_LISTEN
    /* Set reverse-connection flag */
    settings->reverse_connect = (strcmp(argv[IDX_REVERSE_CONNECT], "true") == 0);

    /* Parse listen timeout */
    if (argv[IDX_LISTEN_TIMEOUT][0] != '\0')
        settings->listen_timeout = atoi(argv[IDX_LISTEN_TIMEOUT]);
    else
        settings->listen_timeout = 5000;
#endif

#ifdef ENABLE_PULSE
    settings->audio_enabled = (strcmp(argv[IDX_ENABLE_AUDIO], "true") == 0);

    /* Load servername if specified and applicable */
    if (settings->audio_enabled && argv[IDX_AUDIO_SERVERNAME][0] != '\0')
        settings->pa_servername = strdup(argv[IDX_AUDIO_SERVERNAME]);
    else
        settings->pa_servername = NULL;
#endif

    return 0;

}

