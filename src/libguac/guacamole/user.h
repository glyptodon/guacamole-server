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

#ifndef _GUAC_USER_H
#define _GUAC_USER_H

/**
 * Defines the guac_user object, which represents a physical connection
 * within a larger, possibly shared, logical connection represented by a
 * guac_client.
 *
 * @file user.h
 */

#include "client-fntypes.h"
#include "user-types.h"

#include <pthread.h>

struct guac_user {

    /**
     * This user's actual socket. Data written to this socket will
     * be received by this user alone, and data sent by this specific
     * user will be received by this socket.
     */
    guac_socket* socket;

    /**
     * The previous user in the group of users within the same logical
     * connection.  This is currently only used internally by guac_client to
     * track its set of connected users. There is no public API for iterating
     * connected users.
     */
    guac_user* __prev;

    /**
     * The next user in the group of users within the same logical connection.
     * This is currently only used internally by guac_client to track its set
     * of connected users. There is no public API for iterating connected
     * users.
     */
    guac_user* __next;

    /**
     * Arbitrary user-specific data.
     */
    void* data;

    /**
     * Handler for leave events fired by the guac_client when a guac_user
     * is leaving an active connection.
     *
     * The handler takes only a guac_user which will be the guac_user that
     * left the connection. This guac_user will be disposed of immediately
     * after this event is finished.
     *
     * Example:
     * @code
     *     int leave_handler(guac_client* client, guac_user* user);
     *
     *     int my_join_handler(guac_client* client, guac_user* user) {
     *         user->leave_handler = leave_handler;
     *     }
     * @endcode
     */
    guac_client_leave_handler* leave_handler;

};

#endif

