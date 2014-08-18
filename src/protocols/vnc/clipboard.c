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
#include "clipboard.h"
#include "guac_clipboard.h"
#include "guac_iconv.h"
#include "user.h"
#include "vnc.h"

#include <guacamole/client.h>
#include <guacamole/stream.h>
#include <guacamole/user.h>
#include <rfb/rfbclient.h>
#include <rfb/rfbproto.h>

int guac_vnc_clipboard_handler(guac_user* user, guac_stream* stream,
        char* mimetype) {

    /* Clear clipboard and prepare for new data */
    guac_vnc_client* vnc_client = (guac_vnc_client*) user->client->data;
    guac_common_clipboard_reset(vnc_client->clipboard, mimetype);

    /* Set handlers for clipboard stream */
    stream->blob_handler = guac_vnc_clipboard_blob_handler;
    stream->end_handler = guac_vnc_clipboard_end_handler;

    return 0;
}

int guac_vnc_clipboard_blob_handler(guac_user* user, guac_stream* stream,
        void* data, int length) {

    /* Append new data */
    guac_vnc_client* vnc_client = (guac_vnc_client*) user->client->data;
    guac_common_clipboard_append(vnc_client->clipboard, (char*) data, length);

    return 0;
}

int guac_vnc_clipboard_end_handler(guac_user* user, guac_stream* stream) {

    guac_vnc_client* vnc_client = (guac_vnc_client*) user->client->data;
    rfbClient* rfb_client = vnc_client->rfb_client;

    char output_data[GUAC_VNC_CLIPBOARD_MAX_LENGTH];

    const char* input = vnc_client->clipboard->buffer;
    char* output = output_data;

    /* Convert clipboard to ISO 8859-1 */
    guac_iconv(GUAC_READ_UTF8, &input, vnc_client->clipboard->length,
               GUAC_WRITE_ISO8859_1, &output, sizeof(output_data));

    /* Send via VNC */
    SendClientCutText(rfb_client, output_data, output - output_data);

    return 0;
}

void guac_vnc_cut_text(rfbClient* client, const char* text, int textlen) {

    guac_client* gc = rfbClientGetClientData(client, GUAC_VNC_CLIENT_KEY);
    guac_vnc_client* vnc_client = (guac_vnc_client*) gc->data;

    char received_data[GUAC_VNC_CLIPBOARD_MAX_LENGTH];

    const char* input = text;
    char* output = received_data;

    /* Convert clipboard contents */
    guac_iconv(GUAC_READ_ISO8859_1, &input, textlen,
               GUAC_WRITE_UTF8, &output, sizeof(received_data));

    /* Send converted data */
    guac_common_clipboard_reset(vnc_client->clipboard, "text/plain");
    guac_common_clipboard_append(vnc_client->clipboard, received_data, output - received_data);
    guac_common_clipboard_send(vnc_client->clipboard, gc);

}

