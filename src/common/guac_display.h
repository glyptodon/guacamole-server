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
 * The initial number of layers/buffers to provide to all newly-allocated
 * displays.
 */
#define GUAC_COMMON_DISPLAY_POOL_SIZE 256

/**
 * Pairing of a Guacamole layer with a corresponding surface which wraps that
 * layer.
 */
typedef struct guac_common_display_layer {

    /**
     * A Guacamole layer.
     */
    guac_layer* layer;

    /**
     * The surface which wraps the associated layer.
     */
    guac_common_surface* surface;

} guac_common_display_layer;

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
     * default_surface. Not all slots within this array will be used, and any
     * unused slots will be set to NULL.
     */
    guac_common_display_layer* layers;

    /**
     * The number of available slots within the layers array.
     */
    int layers_size;

    /**
     * All currently-allocated buffers. Each buffer is stored by index, with
     * buffer #-1 being buffers[0]. There are no buffers with index >= 0. Not
     * all slots within this array will be used, and any unused slots will be
     * set to NULL.
     */
    guac_common_display_layer* buffers;

    /**
     * The number of available slots within the buffers array.
     */
    int buffers_size;


} guac_common_display;

/**
 * Allocates a new display, abstracting the cursor and buffer/layer allocation
 * operations of the given guac_client such that client state can be easily
 * synchronized to joining users.
 *
 * @param client The guac_client to associate with this display.
 * @param width The initial width of the display, in pixels.
 * @param height The initial height of the display, in pixels.
 * @return The newly-allocated display.
 */
guac_common_display* guac_common_display_alloc(guac_client* client,
        int width, int height);

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
 *
 * @param user
 *     The user receiving the display state.
 *
 * @param socket
 *     The socket over which the display state should be sent.
 */
void guac_common_display_dup(guac_common_display* display, guac_user* user,
        guac_socket* socket);

/**
 * Flushes pending changes to the given display. All pending operations will
 * become visible to any connected users.
 *
 * @param display The display to flush.
 */
void guac_common_display_flush(guac_common_display* display);

/**
 * Allocates a new layer, returning a new wrapped layer and corresponding
 * surface. The layer may be reused from a previous allocation, if that layer
 * has since been freed.
 *
 * @param display The display to allocate a new layer from.
 * @param width The width of the layer to allocate, in pixels.
 * @param height The height of the layer to allocate, in pixels.
 * @return A newly-allocated layer.
 */
guac_common_display_layer* guac_common_display_alloc_layer(
        guac_common_display* display, int width, int height);

/**
 * Allocates a new buffer, returning a new wrapped buffer and corresponding
 * surface. The buffer may be reused from a previous allocation, if that buffer
 * has since been freed.
 *
 * @param display The display to allocate a new buffer from.
 * @param width The width of the buffer to allocate, in pixels.
 * @param height The height of the buffer to allocate, in pixels.
 * @return A newly-allocated buffer.
 */
guac_common_display_layer* guac_common_display_alloc_buffer(
        guac_common_display* display, int width, int height);

/**
 * Frees the given surface and associated layer, returning the layer to the
 * given display for future use.
 *
 * @param display The display originally allocating the layer.
 * @param layer The layer to free.
 */
void guac_common_display_free_layer(guac_common_display* display,
        guac_common_display_layer* layer);

/**
 * Frees the given surface and associated buffer, returning the buffer to the
 * given display for future use.
 *
 * @param display The display originally allocating the buffer.
 * @param buffer The buffer to free.
 */
void guac_common_display_free_buffer(guac_common_display* display,
        guac_common_display_layer* buffer);

#endif

