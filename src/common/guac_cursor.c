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

#include "guac_cursor.h"

#include <cairo/cairo.h>
#include <guacamole/client.h>
#include <guacamole/protocol.h>
#include <guacamole/socket.h>
#include <guacamole/user.h>

#include <stdlib.h>

guac_common_cursor* guac_common_cursor_alloc(guac_client* client) {

    guac_common_cursor* cursor = malloc(sizeof(guac_common_cursor));

    /* Associate cursor with client and allocate cursor layer */
    cursor->client = client;
    cursor->layer= guac_client_alloc_layer(client);

    /* No cursor image yet */
    cursor->width = 0;
    cursor->height = 0;
    cursor->surface = NULL;
    cursor->hotspot_x = 0;
    cursor->hotspot_y = 0;

    /* No user has moved the mouse yet */
    cursor->user = NULL;

    /* Start cursor in upper-left */
    cursor->x = 0;
    cursor->y = 0;

    return cursor;

}

void guac_common_cursor_free(guac_common_cursor* cursor) {
    guac_client_free_layer(cursor->client, cursor->layer);
    free(cursor);
}

void guac_common_cursor_dup(guac_common_cursor* cursor, guac_socket* socket) {
    /* STUB */
}

void guac_common_cursor_move(guac_common_cursor* cursor, guac_user* user,
        int x, int y) {
    /* STUB */
}

void guac_common_cursor_set_argb(guac_common_cursor* cursor, int hx, int hy,
    unsigned const char* data, int width, int height, int stride) {
    /* STUB */
}

void guac_common_cursor_set_pointer(guac_common_cursor* cursor) {
    /* STUB */
}

void guac_common_cursor_set_dot(guac_common_cursor* cursor) {
    /* STUB */
}

void guac_common_cursor_remove_user(guac_common_cursor* cursor,
        guac_user* user) {
    /* STUB */
}

