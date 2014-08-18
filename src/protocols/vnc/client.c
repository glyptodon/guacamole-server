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

#include "client.h"
#include "user.h"
#include "vnc.h"

#include <guacamole/client.h>

#include <stdlib.h>
#include <string.h>

int guac_client_init(guac_client* client) {

    /* Alloc client data */
    guac_vnc_client* vnc_client = calloc(1, sizeof(guac_vnc_client));
    client->data = vnc_client;

    /* Init clipboard */
    vnc_client->clipboard = guac_common_clipboard_alloc(GUAC_VNC_CLIPBOARD_MAX_LENGTH);

    /* Set handlers */
    client->join_handler = guac_vnc_user_join_handler;
    client->free_handler = guac_vnc_client_free_handler;

    return 0;
}

int guac_vnc_client_free_handler(guac_client* client) {

    guac_vnc_client* vnc_client = (guac_vnc_client*) client->data;
    rfbClient* rfb_client = vnc_client->rfb_client;

#ifdef ENABLE_PULSE
    /* If audio enabled, stop streaming */
    if (vnc_client->audio_enabled)
        guac_pa_stop_stream(client);
#endif

    /* Free encodings string, if used */
    if (vnc_client->settings.encodings != NULL)
        free(vnc_client->settings.encodings);

    /* Free clipboard */
    guac_common_clipboard_free(vnc_client->clipboard);

    /* Free surface */
    guac_common_surface_free(vnc_client->default_surface);

    /* Free generic data struct */
    free(client->data);

    /* Free memory not free'd by libvncclient's rfbClientCleanup() */
    if (rfb_client->frameBuffer != NULL) free(rfb_client->frameBuffer);
    if (rfb_client->raw_buffer != NULL) free(rfb_client->raw_buffer);
    if (rfb_client->rcSource != NULL) free(rfb_client->rcSource);

    /* Free VNC rfbClientData linked list (not free'd by rfbClientCleanup()) */
    while (rfb_client->clientData != NULL) {
        rfbClientData* next = rfb_client->clientData->next;
        free(rfb_client->clientData);
        rfb_client->clientData = next;
    }

    /* Clean up VNC client*/
    rfbClientCleanup(rfb_client);

    return 0;
}

