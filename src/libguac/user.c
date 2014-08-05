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
#include "pool.h"
#include "stream.h"
#include "user.h"
#include "user-handlers.h"

#include <stdlib.h>
#include <string.h>

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

int guac_user_handle_instruction(guac_user* user, guac_instruction* instruction) {

    /* For each defined instruction */
    __guac_instruction_handler_mapping* current = __guac_instruction_handler_map;
    while (current->opcode != NULL) {

        /* If recognized, call handler */
        if (strcmp(instruction->opcode, current->opcode) == 0)
            return current->handler(user, instruction);

        current++;
    }

    /* If unrecognized, ignore */
    return 0;

}


