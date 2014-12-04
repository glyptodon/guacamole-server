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

#ifndef GUAC_COMMON_DISPLAY_H
#define GUAC_COMMON_DISPLAY_H

#include "guac_cursor.h"
#include "guac_surface.h"

#include <guacamole/client.h>
#include <guacamole/socket.h>

/**
 * Abstracts a remote Guacamole display, having an associated client,
 * default surface, mouse cursor, and various allocated buffers and layers.
 */
typedef struct guac_common_display {

    /**
     * The client associate with this display.
     */
    guac_client* client;

    /**
     * The default surface of the client display.
     */
    guac_common_surface* default_surface;

    /**
     * Client-wide cursor, synchronized across all users.
     */
    guac_common_cursor* cursor;

    /**
     * All currently-allocated layers. Each layer is stored by index, with
     * layer #1 being layers[0]. The default layer, layer #0, is stored within
     * default_surface;
     */
    guac_common_surface* layers;

    /**
     * All currently-allocated buffers. Each buffer is stored by index, with
     * buffer #-1 being buffers[0]. There are no buffers with index >= 0.
     */
    guac_common_surface* buffers;

} guac_common_display;

/**
 * Allocates a new display, abstracting the cursor and buffer/layer allocation
 * operations of the given guac_client such that client state can be easily
 * synchronized to joining users.
 *
 * @param client The guac_client to associate with this display.
 * @return The newly-allocated display.
 */
guac_common_display* guac_common_display_alloc(guac_client* client);

/**
 * Frees the given display, and any associated resources, including any
 * allocated buffers/layers.
 *
 * @param display The display to free.
 */
void guac_common_display_free(guac_common_display* display);

/**
 * Duplicates the state of the given display to the given socket. Any pending
 * changes to buffers, layers, or the default layer are not flushed.
 *
 * @param display
 *     The display whose state should be sent along the given socket.
 */
void guac_common_display_dup(guac_common_display* display,
        guac_socket* socket);

/**
 * Flushes pending changes to the given display. All pending operations will
 * become visible to any connected users.
 *
 * @param display The display to flush.
 */
void guac_common_display_flush(guac_common_display* display);

/**
 * Allocates a new layer, returning a new surface which wraps that layer. The
 * layer may be reused from a previous allocation, if that layer has since been
 * freed.
 *
 * @param display The display to allocate a new layer from.
 * @return A newly-allocated surface wrapping a new layer.
 */
guac_common_surface* guac_common_display_alloc_layer(
        guac_common_display* display);

/**
 * Allocates a new buffer, returning a new surface which wraps that buffer. The
 * buffer may be reused from a previous allocation, if that buffer has since
 * been freed.
 *
 * @param display The display to allocate a new buffer from.
 * @return A newly-allocated surface wrapping a new buffer.
 */
guac_common_surface* guac_common_display_alloc_buffer(
        guac_common_display* display);

/**
 * Frees the given surface and associated layer, returning the layer to the
 * given display for future use.
 *
 * @param display The display originally allocating the layer.
 * @param surface The surface to free.
 */
void guac_common_display_free_layer(guac_common_display* display,
        guac_common_surface* surface);

/**
 * Frees the given surface and associated buffer, returning the buffer to the
 * given display for future use.
 *
 * @param display The display originally allocating the buffer.
 * @param surface The surface to free.
 */
void guac_common_display_free_buffer(guac_common_display* display,
        guac_common_surface* surface);

#endif

