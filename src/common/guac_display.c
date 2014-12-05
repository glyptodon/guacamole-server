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

#include <stdlib.h>

guac_common_display* guac_common_display_alloc(guac_client* client,
        int width, int height) {

    /* Allocate display */
    guac_common_display* display = malloc(sizeof(guac_common_display));
    if (display == NULL)
        return NULL;

    /* Associate display with given client */
    display->client = client;

    /* Allocate shared cursor */
    display->cursor = guac_common_cursor_alloc(client);

    display->default_surface = guac_common_surface_alloc(client->socket,
            GUAC_DEFAULT_LAYER, width, height);

    /* Allocate initial layers array */
    display->layers_size = GUAC_COMMON_DISPLAY_POOL_SIZE;
    display->layers =
        calloc(display->layers_size, sizeof(guac_common_surface*));

    /* Allocate initial buffers array */
    display->buffers_size = GUAC_COMMON_DISPLAY_POOL_SIZE;
    display->buffers =
        calloc(display->buffers_size, sizeof(guac_common_surface*));

    return display;

}

void guac_common_display_free(guac_common_display* display) {

    int i;
    guac_common_surface** current;

    /* Free shared cursor */
    guac_common_cursor_free(display->cursor);

    /* Free default surface */
    guac_common_surface_free(display->default_surface);

    /* Free layers array */
    current = display->layers;
    for (i=0; i < display->layers_size; i++) {

        /* Free layer, if allocated */
        if (*current != NULL)
            guac_common_display_free_layer(display, *current);

        current++;

    }
    free(display->layers);

    /* Free buffers array */
    current = display->buffers;
    for (i=0; i < display->buffers_size; i++) {

        /* Free buffer, if allocated */
        if (*current != NULL)
            guac_common_display_free_buffer(display, *current);

        current++;

    }
    free(display->buffers);

    free(display);

}

void guac_common_display_dup(guac_common_display* display,
        guac_socket* socket) {

    int i;
    guac_common_surface** current;

    /* Sunchronize shared cursor */
    guac_common_cursor_dup(display->cursor, socket);

    /* Synchronize default surface */
    guac_common_surface_dup(display->default_surface, socket);

    /* Synchronize all layers */
    current = display->layers;
    for (i=0; i < display->layers_size; i++) {

        /* Synchronize layer, if allocated */
        if (*current != NULL)
            guac_common_surface_dup(*current, socket);

        current++;

    }

    /* Synchronize all buffers */
    current = display->buffers;
    for (i=0; i < display->buffers_size; i++) {

        /* Synchronize buffer, if allocated */
        if (*current != NULL)
            guac_common_surface_dup(*current, socket);

        current++;

    }

}

void guac_common_display_flush(guac_common_display* display) {
    guac_common_surface_flush(display->default_surface);
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

