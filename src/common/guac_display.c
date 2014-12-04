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
#include "guac_display.h"
#include "guac_surface.h"

#include <guacamole/client.h>
#include <guacamole/socket.h>

guac_common_display* guac_common_display_alloc(guac_client* client) {
    /* STUB */
    return NULL;
}

void guac_common_display_free(guac_common_display* display) {
    /* STUB */
}

void guac_common_display_dup(guac_common_display* display,
        guac_socket* socket) {
    /* STUB */
}

void guac_common_display_flush(guac_common_display* display) {
    /* STUB */
}

guac_common_surface* guac_common_display_alloc_layer(
        guac_common_display* display) {
    /* STUB */
    return NULL;
}

guac_common_surface* guac_common_display_alloc_buffer(
        guac_common_display* display) {
    /* STUB */
    return NULL;
}

void guac_common_display_free_layer(guac_common_display* display,
        guac_common_surface* surface) {
    /* STUB */
}

void guac_common_display_free_buffer(guac_common_display* display,
        guac_common_surface* surface) {
    /* STUB */
}

