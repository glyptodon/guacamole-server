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

#include "input.h"
#include "guac_cursor.h"
#include "guac_display.h"
#include "user.h"
#include "rdp.h"
#include "rdp_stream.h"

#include <guacamole/client.h>
#include <guacamole/socket.h>
#include <guacamole/user.h>

#include <pthread.h>

int guac_rdp_user_join_handler(guac_user* user, int argc, char** argv) {

    guac_rdp_client* rdp_client = (guac_rdp_client*) user->client->data;

    /* Connect via RDP if owner */
    if (user->owner) {

        /* Parse arguments into client */
        guac_rdp_settings* settings = rdp_client->settings =
            guac_rdp_parse_args(user, argc, (const char**) argv);

        /* Fail if settings cannot be parsed */
        if (settings == NULL) {
            guac_user_log(user, GUAC_LOG_INFO,
                    "Badly formatted client arguments.");
            return 1;
        }

        /* Start client thread */
        if (pthread_create(&rdp_client->client_thread, NULL,
                    guac_rdp_client_thread, user->client)) {
            guac_user_log(user, GUAC_LOG_ERROR,
                    "Unable to start VNC client thread.");
            return 1;
        }

    }

    /* If not owner, synchronize with current display */
    else {
        guac_common_display_dup(rdp_client->display, user, user->socket);
        guac_socket_flush(user->socket);
    }

    user->mouse_handler = guac_rdp_user_mouse_handler;
    user->key_handler = guac_rdp_user_key_handler;
    user->size_handler = guac_rdp_user_size_handler;
    user->clipboard_handler = guac_rdp_clipboard_handler;

    /* FIXME: Set pipe_handler, file_handler, get_handler, and put_handler */

    return 0;

}

int guac_rdp_user_leave_handler(guac_user* user) {

    guac_rdp_client* rdp_client = (guac_rdp_client*) user->client->data;

    guac_common_cursor_remove_user(rdp_client->display->cursor, user);

    return 0;
}

