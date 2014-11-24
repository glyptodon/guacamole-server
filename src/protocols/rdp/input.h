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

#ifndef GUAC_RDP_INPUT_H
#define GUAC_RDP_INPUT_H

#include <guacamole/client.h>
#include <guacamole/user.h>

/**
 * Presses or releases the given keysym, sending an appropriate set of key
 * events to the RDP server. The key events sent will depend on the current
 * keymap.
 */
int guac_rdp_send_keysym(guac_client* client, int keysym, int pressed);

/**
 * For every keysym in the given NULL-terminated array of keysyms, update
 * the current state of that key conditionally. For each key in the "from"
 * state (0 being released and 1 being pressed), that key will be updated
 * to the "to" state.
 */
void guac_rdp_update_keysyms(guac_client* client, const int* keysym_string,
        int from, int to);

/**
 * Handler for Guacamole user mouse events.
 */
int guac_rdp_user_mouse_handler(guac_user* user, int x, int y, int mask);

/**
 * Handler for Guacamole user key events.
 */
int guac_rdp_user_key_handler(guac_user* user, int keysym, int pressed);

/**
 * Handler for Guacamole user size events.
 */
int guac_rdp_user_size_handler(guac_user* user, int width, int height);

#endif

