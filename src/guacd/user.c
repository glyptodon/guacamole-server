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

#include "log.h"
#include "user.h"

#include <guacamole/client.h>
#include <guacamole/error.h>
#include <guacamole/instruction.h>
#include <guacamole/protocol.h>
#include <guacamole/socket.h>
#include <guacamole/user.h>

#include <pthread.h>
#include <stdlib.h>

void* __guacd_user_input_thread(void* data) {

    guac_user* user = (guac_user*) data;
    guac_client* client = user->client;
    guac_socket* socket = user->socket;

    /* Guacamole user input loop */
    while (client->state == GUAC_CLIENT_RUNNING && user->active) {

        /* Read instruction */
        guac_instruction* instruction =
            guac_instruction_read(socket, GUACD_USEC_TIMEOUT);

        /* Stop on error */
        if (instruction == NULL) {

            if (guac_error == GUAC_STATUS_INPUT_TIMEOUT)
                guac_user_abort(user, GUAC_PROTOCOL_STATUS_CLIENT_TIMEOUT, "User is not responding.");

            else {
                guacd_client_log_guac_error(client, "Error reading instruction");
                guac_user_stop(user);
            }

            return NULL;
        }

        /* Reset guac_error and guac_error_message (user/client handlers are not
         * guaranteed to set these) */
        guac_error = GUAC_STATUS_SUCCESS;
        guac_error_message = NULL;

        /* Call handler, stop on error */
        if (guac_user_handle_instruction(user, instruction) < 0) {

            /* Log error */
            guacd_client_log_guac_error(client,
                    "User instruction handler error");

            /* Log handler details */
            guac_user_log_info(user,
                    "Failing instruction handler in user was \"%s\"",
                    instruction->opcode);

            guac_instruction_free(instruction);
            guac_user_stop(user);
            return NULL;
        }

        /* Free allocated instruction */
        guac_instruction_free(instruction);

    }

    return NULL;

}

int guacd_user_start(guac_user* user, guac_socket* socket) {

    pthread_t input_thread;

    if (pthread_create(&input_thread, NULL, __guacd_user_input_thread, (void*) user)) {
        guac_user_log_error(user, "Unable to start input thread");
        guac_user_stop(user);
        return -1;
    }

    /* Wait for I/O threads */
    pthread_join(input_thread, NULL);

    /* Done */
    return 0;

}

