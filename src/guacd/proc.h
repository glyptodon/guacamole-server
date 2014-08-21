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

#ifndef GUACD_PROC_H
#define GUACD_PROC_H

#include "config.h"

#include <guacamole/client.h>

#include <unistd.h>

/**
 * Process information of the internal remote desktop client.
 */
typedef struct guacd_proc {

    /**
     * The process ID of the client.
     */
    pid_t pid;

    /**
     * The file descriptor of the UNIX domain socket to use for sending and
     * receiving file descriptors of new users.
     */
    int fd_socket;

    /**
     * The actual client instance.
     */
    guac_client* client;

} guacd_proc;

/**
 * Creates a new process for handling the given protocol, returning the process
 * created. The created process runs in the background relative to the calling
 * process.
 */
guacd_proc* guacd_create_proc(const char* protocol);

#endif

