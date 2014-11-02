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

#ifndef GUAC_VNC_VNC_H
#define GUAC_VNC_VNC_H

#include "config.h"

#include "guac_clipboard.h"
#include "guac_surface.h"
#include "settings.h"

#include <guacamole/audio.h>
#include <guacamole/client.h>
#include <guacamole/layer.h>
#include <rfb/rfbclient.h>

#ifdef ENABLE_PULSE
#include <pulse/pulseaudio.h>
#endif

#include <pthread.h>

/**
 * VNC-specific client data.
 */
typedef struct guac_vnc_client {

    /**
     * The VNC client thread.
     */
    pthread_t client_thread;

    /**
     * The underlying VNC client.
     */
    rfbClient* rfb_client;

    /**
     * The original framebuffer malloc procedure provided by the initialized
     * rfbClient.
     */
    MallocFrameBufferProc rfb_MallocFrameBuffer;

    /**
     * Whether copyrect  was used to produce the latest update received
     * by the VNC server.
     */
    int copy_rect_used;

    /**
     * Client settings, parsed from args.
     */
    guac_vnc_settings settings;

    /**
     * The layer holding the cursor image.
     */
    guac_layer* cursor;

    /**
     * The X coordinate of the current location of the mouse cursor.
     */
    int mouse_x;

    /**
     * The Y coordinate of the current location of the mouse cursor.
     */
    int mouse_y;

    /**
     * The X coordinate of the mouse hotspot.
     */
    int hotspot_x;

    /**
     * The Y coordinate of the mouse hotspot.
     */
    int hotspot_y;

    /**
     * Internal clipboard.
     */
    guac_common_clipboard* clipboard;

    /**
     * Audio output, if any.
     */
    guac_audio_stream* audio;

#ifdef ENABLE_PULSE
    /**
     * PulseAudio event loop.
     */
    pa_threaded_mainloop* pa_mainloop;
#endif

    /**
     * Default surface.
     */
    guac_common_surface* default_surface;

} guac_vnc_client;

/**
 * Allocates a new rfbClient instance given the parameters stored within the
 * client, returning NULL on failure.
 */
rfbClient* guac_vnc_get_client(guac_client* client);

/**
 * VNC client thread. This thread runs throughout the duration of the client,
 * existing as a single instance, shared by all users.
 */
void* guac_vnc_client_thread(void* data);

/**
 * Key which can be used with the rfbClientGetClientData function to return
 * the associated guac_client.
 */
extern char* GUAC_VNC_CLIENT_KEY;

#endif

