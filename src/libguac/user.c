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
#include "id.h"
#include "pool.h"
#include "protocol.h"
#include "socket.h"
#include "stream.h"
#include "timestamp.h"
#include "user.h"
#include "user-handlers.h"

#include <stdlib.h>
#include <string.h>

guac_user* guac_user_alloc() {

    guac_user* user = calloc(1, sizeof(guac_user));
    int i;

    /* Generate ID */
    user->user_id = guac_generate_id(GUAC_USER_ID_PREFIX);
    if (user->user_id == NULL) {
        free(user);
        return NULL;
    }

    user->last_received_timestamp = guac_timestamp_current();
    user->active = 1;

    /* Allocate stream pool */
    user->__stream_pool = guac_pool_alloc(0);

    /* Initialze streams */
    user->__input_streams = malloc(sizeof(guac_stream) * GUAC_USER_MAX_STREAMS);
    user->__output_streams = malloc(sizeof(guac_stream) * GUAC_USER_MAX_STREAMS);

    for (i=0; i<GUAC_USER_MAX_STREAMS; i++) {
        user->__input_streams[i].index = GUAC_USER_CLOSED_STREAM_INDEX;
        user->__output_streams[i].index = GUAC_USER_CLOSED_STREAM_INDEX;
    }

    return user;

}

void guac_user_free(guac_user* user) {

    /* Free streams */
    free(user->__input_streams);
    free(user->__output_streams);

    /* Free stream pool */
    guac_pool_free(user->__stream_pool);

    /* Clean up user */
    free(user->user_id);
    free(user);

}

guac_stream* guac_user_alloc_stream(guac_user* user) {

    guac_stream* allocd_stream;
    int stream_index;

    /* Refuse to allocate beyond maximum */
    if (user->__stream_pool->active == GUAC_USER_MAX_STREAMS)
        return NULL;

    /* Allocate stream */
    stream_index = guac_pool_next_int(user->__stream_pool);

    /* Initialize stream */
    allocd_stream = &(user->__output_streams[stream_index]);
    allocd_stream->index = stream_index;
    allocd_stream->data = NULL;
    allocd_stream->ack_handler = NULL;
    allocd_stream->blob_handler = NULL;
    allocd_stream->end_handler = NULL;

    return allocd_stream;

}

void guac_user_free_stream(guac_user* user, guac_stream* stream) {

    /* Release index to pool */
    guac_pool_free_int(user->__stream_pool, stream->index);

    /* Mark stream as closed */
    stream->index = GUAC_USER_CLOSED_STREAM_INDEX;

}

int guac_user_handle_instruction(guac_user* user, const char* opcode, int argc, char** argv) {

    /* For each defined instruction */
    __guac_instruction_handler_mapping* current = __guac_instruction_handler_map;
    while (current->opcode != NULL) {

        /* If recognized, call handler */
        if (strcmp(opcode, current->opcode) == 0)
            return current->handler(user, argc, argv);

        current++;
    }

    /* If unrecognized, ignore */
    return 0;

}

void guac_user_stop(guac_user* user) {
    user->active = 0;
}

void vguac_user_abort(guac_user* user, guac_protocol_status status,
        const char* format, va_list ap) {

    /* Only relevant if user is active */
    if (user->active) {

        /* Log detail of error */
        vguac_user_log_error(user, format, ap);

        /* Send error immediately, limit information given */
        guac_protocol_send_error(user->socket, "Aborted. See logs.", status);
        guac_socket_flush(user->socket);

        /* Stop user */
        guac_user_stop(user);

    }

}

void guac_user_abort(guac_user* user, guac_protocol_status status,
        const char* format, ...) {

    va_list args;
    va_start(args, format);

    vguac_user_abort(user, status, format, args);

    va_end(args);

}

void vguac_user_log_info(guac_user* user, const char* format,
        va_list ap) {

    vguac_client_log_info(user->client, format, ap);

}

void vguac_user_log_error(guac_user* user, const char* format,
        va_list ap) {

    vguac_client_log_error(user->client, format, ap);

}

void guac_user_log_info(guac_user* user, const char* format, ...) {

    va_list args;
    va_start(args, format);

    vguac_client_log_info(user->client, format, args);

    va_end(args);

}

void guac_user_log_error(guac_user* user, const char* format, ...) {

    va_list args;
    va_start(args, format);

    vguac_client_log_error(user->client, format, args);

    va_end(args);

}

