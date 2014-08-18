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

#include "clipboard.h"
#include "input.h"
#include "guac_dot_cursor.h"
#include "guac_pointer_cursor.h"
#include "user.h"
#include "vnc.h"

#include <guacamole/client.h>
#include <guacamole/user.h>
#include <rfb/rfbclient.h>
#include <rfb/rfbproto.h>

int guac_vnc_user_join_handler(guac_user* user, int argc, char** argv) {

    guac_vnc_client* vnc_client = (guac_vnc_client*) user->client->data;
    guac_vnc_settings* vnc_settings = &(vnc_client->settings);

    /* Connect via VNC if owner */
    if (user->owner) {

        /* Parse arguments into client */
        if (guac_vnc_parse_args(&(vnc_client->settings), argc, (const char**) argv)) {
            guac_user_log_info(user, "Badly formatted client arguments.");
            return 1;
        }

        /* TODO: Start client thread */

    }

    /* If not read-only, set input handlers and pointer */
    if (vnc_settings->read_only == 0) {

        /* Only handle mouse/keyboard/clipboard if not read-only */
        user->mouse_handler = guac_vnc_user_mouse_handler;
        user->key_handler = guac_vnc_user_key_handler;
        user->clipboard_handler = guac_vnc_clipboard_handler;

        /* If not read-only but cursor is remote, set a dot cursor */
        if (vnc_settings->remote_cursor)
            guac_common_set_dot_cursor(user);

        /* Otherwise, set pointer until explicitly requested otherwise */
        else
            guac_common_set_pointer_cursor(user);

    }

    return 0;

}

int guac_vnc_user_leave_handler(guac_user* user) {
    /* STUB */
    return 0;
}

