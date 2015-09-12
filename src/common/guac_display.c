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
#include <string.h>

/**
 * Synchronizes all surfaces within the given array to the given socket. If a
 * given surface is set to NULL, it will not be synchronized.
 *
 * @param layers
 *     The array of layers to synchronize.
 *
 * @param count
 *     The number of layers in the array.
 *
 * @param user
 *     The user receiving the layers.
 *
 * @param socket
 *     The socket over which each layer should be sent.
 */
static void guac_common_display_dup_layers(guac_common_display_layer* layers,
        int count, guac_user* user, guac_socket* socket) {

    int i;

    /* Synchronize all surfaces in given array */
    for (i=0; i < count; i++) {

        guac_common_display_layer* current = layers++;

        /* Synchronize surface, if present */
        if (current->surface != NULL)
            guac_common_surface_dup(current->surface, user, socket);

    }

}

/**
 * Frees all layers and associated surfaces within the given array.
 *
 * @param layers The array of layers to synchronize.
 * @param count The number of layers in the array.
 *
 * @param client
 *     The client owning the layers wrapped by each of the layers in the array.
 */
static void guac_common_display_free_layers(guac_common_display_layer* layers,
        int count, guac_client* client) {

    int i;

    /* Free each surface in given array */
    for (i=0; i < count; i++) {

        guac_common_display_layer* current = layers++;

        /* Free layer, if present */
        if (current->layer != NULL) {
            if (current->layer->index >= 0)
                guac_client_free_buffer(client, current->layer);
            else
                guac_client_free_layer(client, current->layer);
        }

        /* Free surface, if present */
        if (current->surface != NULL)
            guac_common_surface_free(current->surface);

    }

}

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

    display->default_surface = guac_common_surface_alloc(client,
            client->socket, GUAC_DEFAULT_LAYER, width, height);

    /* Allocate initial layers array */
    display->layers_size = GUAC_COMMON_DISPLAY_POOL_SIZE;
    display->layers =
        calloc(display->layers_size, sizeof(guac_common_display_layer));

    /* Allocate initial buffers array */
    display->buffers_size = GUAC_COMMON_DISPLAY_POOL_SIZE;
    display->buffers =
        calloc(display->buffers_size, sizeof(guac_common_display_layer));

    return display;

}

void guac_common_display_free(guac_common_display* display) {

    /* Free shared cursor */
    guac_common_cursor_free(display->cursor);

    /* Free default surface */
    guac_common_surface_free(display->default_surface);

    /* Synchronize all layers/buffers */
    guac_common_display_free_layers(display->buffers, display->buffers_size, display->client);
    guac_common_display_free_layers(display->layers, display->layers_size, display->client);

    free(display->buffers);
    free(display->layers);

    free(display);

}

void guac_common_display_dup(guac_common_display* display, guac_user* user,
        guac_socket* socket) {

    /* Sunchronize shared cursor */
    guac_common_cursor_dup(display->cursor, user, socket);

    /* Synchronize default surface */
    guac_common_surface_dup(display->default_surface, user, socket);

    /* Synchronize all layers */
    guac_common_display_dup_layers(display->layers, display->layers_size,
            user, socket);

    /* Synchronize all buffers */
    guac_common_display_dup_layers(display->buffers, display->buffers_size,
            user, socket);

}

/**
 * Returns a pointer to the layer having the given index within the given
 * layers array, reallocating and resizing that array if necessary. The resized
 * array and its new size will be returned through the provided pointers.
 *
 * @param layers_ptr Pointer to the layers array.
 *
 * @param size_ptr
 *     Pointer to an integer containing the number of entries in the layers
 *     array.
 *
 * @param index The array index of the layer to return.
 * @return The layer at the given index within the layer array.
 */
static guac_common_display_layer* guac_common_display_get_layer(
        guac_common_display_layer** layers_ptr, int* size_ptr, int index) {

    guac_common_display_layer* layers = *layers_ptr;
    int size = *size_ptr;

    /* Resize layers array if it's not big enough */
    if (index >= size) {

        int new_size;

        /* Resize layers array */
        *size_ptr = new_size = index*2;
        *layers_ptr = layers =
            realloc(layers, new_size * sizeof(guac_common_display_layer));

        /* Clear newly-allocated space */
        memset(layers + size, 0, new_size - size);

    }

    /* Return reference to requested layer */
    return layers + index;

}

void guac_common_display_flush(guac_common_display* display) {
    guac_common_surface_flush(display->default_surface);
}

guac_common_display_layer* guac_common_display_alloc_layer(
        guac_common_display* display, int width, int height) {

    guac_layer* layer;
    guac_common_display_layer* display_layer;

    /* Allocate Guacamole layer */
    layer = guac_client_alloc_layer(display->client);

    /* Get slot for allocated layer */
    display_layer = guac_common_display_get_layer(
            &display->layers, &display->layers_size,
            layer->index - 1);

    /* Init display layer */
    display_layer->layer = layer;
    display_layer->surface = guac_common_surface_alloc(display->client,
            display->client->socket, layer, width, height);

    return display_layer;
}

guac_common_display_layer* guac_common_display_alloc_buffer(
        guac_common_display* display, int width, int height) {

    guac_layer* buffer;
    guac_common_display_layer* display_buffer;

    /* Allocate Guacamole buffer */
    buffer = guac_client_alloc_buffer(display->client);

    /* Get slot for allocated buffer */
    display_buffer = guac_common_display_get_layer(
            &display->buffers, &display->buffers_size,
            -1 - buffer->index);

    /* Init display buffer */
    display_buffer->layer = buffer;
    display_buffer->surface = guac_common_surface_alloc(display->client,
            display->client->socket, buffer, width, height);

    return display_buffer;
}

void guac_common_display_free_layer(guac_common_display* display,
        guac_common_display_layer* layer) {

    /* Free associated layer and surface */
    guac_client_free_layer(display->client, layer->layer);
    guac_common_surface_free(layer->surface);

    layer->layer = NULL;
    layer->surface = NULL;

}

void guac_common_display_free_buffer(guac_common_display* display,
        guac_common_display_layer* buffer) {

    /* Free associated layer and surface */
    guac_client_free_buffer(display->client, buffer->layer);
    guac_common_surface_free(buffer->surface);

    buffer->layer = NULL;
    buffer->surface = NULL;

}

