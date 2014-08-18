/*
 * Copyright (C) 2013 Glyptodon LLC
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

#include "auth.h"
#include "client.h"
#include "clipboard.h"
#include "cursor.h"
#include "display.h"
#include "guac_clipboard.h"
#include "log.h"
#include "settings.h"
#include "vnc.h"

#ifdef ENABLE_PULSE
#include "pulse.h"
#endif

#include <guacamole/client.h>
#include <guacamole/protocol.h>
#include <guacamole/socket.h>
#include <guacamole/timestamp.h>
#include <rfb/rfbclient.h>
#include <rfb/rfbproto.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>

char* GUAC_VNC_CLIENT_KEY = "GUAC_VNC";

rfbClient* guac_vnc_get_client(guac_client* client) {

    rfbClient* rfb_client = rfbGetClient(8, 3, 4); /* 32-bpp client */
    guac_vnc_client* vnc_client = (guac_vnc_client*) client->data;
    guac_vnc_settings* vnc_settings = &(vnc_client->settings);

    /* Store Guac client in rfb client */
    rfbClientSetClientData(rfb_client, GUAC_VNC_CLIENT_KEY, client);

    /* Framebuffer update handler */
    rfb_client->GotFrameBufferUpdate = guac_vnc_update;
    rfb_client->GotCopyRect = guac_vnc_copyrect;

    /* Do not handle clipboard and local cursor if read-only */
    if (vnc_settings->read_only == 0) {

        /* Clipboard */
        rfb_client->GotXCutText = guac_vnc_cut_text;

        /* Set remote cursor */
        if (vnc_settings->remote_cursor)
            rfb_client->appData.useRemoteCursor = FALSE;

        else {
            /* Enable client-side cursor */
            rfb_client->appData.useRemoteCursor = TRUE;
            rfb_client->GotCursorShape = guac_vnc_cursor;
        }
    }

    /* Password */
    rfb_client->GetPassword = guac_vnc_get_password;

    /* Depth */
    guac_vnc_set_pixel_format(rfb_client, vnc_settings->color_depth);

    /* Hook into allocation so we can handle resize. */
    vnc_client->rfb_MallocFrameBuffer = rfb_client->MallocFrameBuffer;
    rfb_client->MallocFrameBuffer = guac_vnc_malloc_framebuffer;
    rfb_client->canHandleNewFBSize = 1;

    /* Set hostname and port */
    rfb_client->serverHost = strdup(vnc_settings->hostname);
    rfb_client->serverPort = vnc_settings->port;

#ifdef ENABLE_VNC_REPEATER
    /* Set repeater parameters if specified */
    if (vnc_settings->dest_host) {
        rfb_client->destHost = strdup(vnc_settings->dest_host);
        rfb_client->destPort = vnc_settings->dest_port;
    }
#endif

#ifdef ENABLE_VNC_LISTEN
    /* If reverse connection enabled, start listening */
    if (vnc_settings->reverse_connect) {

        guac_client_log_info(client, "Listening for connections on port %i", vnc_settings->port);

        /* Listen for connection from server */
        rfb_client->listenPort = vnc_settings->port;
        if (listenForIncomingConnectionsNoFork(rfb_client, vnc_settings->listen_timeout*1000) <= 0)
            return NULL;

    }
#endif

    /* Set encodings if provided */
    if (vnc_settings->encodings)
        rfb_client->appData.encodingsString = strdup(vnc_settings->encodings);

    /* Connect */
    if (rfbInitClient(rfb_client, NULL, NULL))
        return rfb_client;

    /* If connection fails, return NULL */
    return NULL;

}

void* guac_vnc_client_thread(void* data) {

    guac_client* client = (guac_client*) data;
    guac_vnc_client* vnc_client = (guac_vnc_client*) client->data;

    /* Ensure connection is kept alive during lengthy connects */
    guac_socket_require_keep_alive(client->socket);

    /* Set up libvncclient logging */
    rfbClientLog = guac_vnc_client_log_info;
    rfbClientErr = guac_vnc_client_log_error;

    /* Attempt connection */
    rfbClient* rfb_client = guac_vnc_get_client(client);
    int retries_remaining = vnc_client->settings.retries;

    /* If unsuccessful, retry as many times as specified */
    while (!rfb_client && retries_remaining > 0) {

        struct timespec guac_vnc_connect_interval = {
            .tv_sec  =  GUAC_VNC_CONNECT_INTERVAL/1000,
            .tv_nsec = (GUAC_VNC_CONNECT_INTERVAL%1000)*1000000
        };

        guac_client_log_info(client,
                "Connect failed. Waiting %ims before retrying...",
                GUAC_VNC_CONNECT_INTERVAL);

        /* Wait for given interval then retry */
        nanosleep(&guac_vnc_connect_interval, NULL);
        rfb_client = guac_vnc_get_client(client);
        retries_remaining--;

    }

    /* If the final connect attempt fails, return error */
    if (!rfb_client) {
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_UPSTREAM_ERROR, "Unable to connect to VNC server.");
        return NULL;
    }

#ifdef ENABLE_PULSE
    /* If an encoding is available, load an audio stream */
    if (guac_client_data->audio_enabled) {    

        guac_client_data->audio = guac_audio_stream_alloc(client, NULL);

        /* Load servername if specified */
        if (argv[IDX_AUDIO_SERVERNAME][0] != '\0')
            guac_client_data->pa_servername =
                strdup(argv[IDX_AUDIO_SERVERNAME]);
        else
            guac_client_data->pa_servername = NULL;

        /* If successful, init audio system */
        if (guac_client_data->audio != NULL) {
            
            guac_client_log_info(client,
                    "Audio will be encoded as %s",
                    guac_client_data->audio->encoder->mimetype);

            /* Require threadsafe sockets if audio enabled */
            guac_socket_require_threadsafe(client->socket);

            /* Start audio stream */
            guac_pa_start_stream(client);
            
        }

        /* Otherwise, audio loading failed */
        else
            guac_client_log_info(client,
                    "No available audio encoding. Sound disabled.");

    } /* end if audio enabled */
#endif

    /* Set remaining client data */
    vnc_client->rfb_client = rfb_client;
    vnc_client->cursor = guac_client_alloc_buffer(client);

    /* Send name */
    guac_protocol_send_name(client->socket, rfb_client->desktopName);

    /* Create default surface */
    vnc_client->default_surface = guac_common_surface_alloc(client->socket, GUAC_DEFAULT_LAYER,
                                                            rfb_client->width, rfb_client->height);

    guac_socket_flush(client->socket);

    /* Handle messages from VNC server while client is running */
    while (client->state == GUAC_CLIENT_RUNNING) {

        /* Initially wait for messages */
        int wait_result = WaitForMessage(rfb_client, 1000000);
        guac_timestamp frame_start = guac_timestamp_current();
        while (wait_result > 0) {

            guac_timestamp frame_end;
            int frame_remaining;

            /* Handle any message received */
            if (!HandleRFBServerMessage(rfb_client)) {
                guac_client_abort(client, GUAC_PROTOCOL_STATUS_UPSTREAM_ERROR,
                                  "Error handling message from VNC server.");
                break;
            }

            /* Calculate time remaining in frame */
            frame_end = guac_timestamp_current();
            frame_remaining = frame_start + GUAC_VNC_FRAME_DURATION - frame_end;

            /* Wait again if frame remaining */
            if (frame_remaining > 0)
                wait_result = WaitForMessage(rfb_client, GUAC_VNC_FRAME_TIMEOUT*1000);
            else
                break;

        }

        /* If an error occurs, log it and fail */
        if (wait_result < 0)
            guac_client_abort(client, GUAC_PROTOCOL_STATUS_UPSTREAM_ERROR, "Connection closed.");

        guac_common_surface_flush(vnc_client->default_surface);
        guac_client_end_frame(client);
        guac_socket_flush(client->socket);

    }

    return NULL;

}

