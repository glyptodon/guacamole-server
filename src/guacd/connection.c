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

#include "connection.h"
#include "log.h"
#include "move-fd.h"
#include "proc.h"
#include "proc-map.h"
#include "user.h"

#include <guacamole/client.h>
#include <guacamole/error.h>
#include <guacamole/parser.h>
#include <guacamole/plugin.h>
#include <guacamole/protocol.h>
#include <guacamole/socket.h>
#include <guacamole/user.h>

#ifdef ENABLE_SSL
#include <openssl/ssl.h>
#include "socket-ssl.h"
#endif

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

/**
 * Continuously reads from a guac_socket, writing all data read to a file
 * descriptor.
 */
static void* guacd_connection_write_thread(void* data) {

    guacd_connection_io_thread_params* params = (guacd_connection_io_thread_params*) data;
    char buffer[8192];

    int length;

    /* Transfer data from file descriptor to socket */
    while ((length = guac_socket_read(params->socket, buffer, sizeof(buffer))) > 0) {

        char* remaining = buffer;
        while (length > 0) {

            int written = write(params->fd, remaining, length);
            if (written < 0)
                return NULL;

            length    -= written;
            remaining += written;

        }

    }

    return NULL;

}

void* guacd_connection_io_thread(void* data) {

    guacd_connection_io_thread_params* params = (guacd_connection_io_thread_params*) data;
    char buffer[8192];

    int length;

    pthread_t write_thread;
    pthread_create(&write_thread, NULL, guacd_connection_write_thread, params);

    /* Transfer data from file descriptor to socket */
    while ((length = read(params->fd, buffer, sizeof(buffer))) > 0) {
        if (guac_socket_write(params->socket, buffer, length))
            break;
    }

    /* Wait for write thread to die */
    pthread_join(write_thread, NULL);

    /* Clean up */
    guac_socket_free(params->socket);
    close(params->fd);
    free(params);

    return NULL;

}

/**
 * Adds the given socket as a new user to the given process, automatically
 * reading/writing from the socket via read/write threads. The given socket and
 * any associated resources will be freed unless the user is not added
 * successfully.
 *
 * If adding the user fails for any reason, non-zero is returned. Zero is
 * returned upon success.
 */
static int guacd_add_user(guacd_proc* proc, guac_parser* parser, guac_socket* socket) {

    int sockets[2];

    /* Set up socket pair */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0) {
        guacd_log_error("Unable to allocate file descriptors for I/O transfer: %s", strerror(errno));
        return 1;
    }

    int user_fd = sockets[0];
    int proc_fd = sockets[1];

    /* Send user file descriptor to process */
    if (!guacd_send_fd(proc->fd_socket, proc_fd)) {
        guacd_log_error("Unable to add user.");
        return 1;
    }

    /* Close our end of the process file descriptor */
    close(proc_fd);

    guacd_connection_io_thread_params* params = malloc(sizeof(guacd_connection_io_thread_params));
    params->socket = socket;
    params->fd = user_fd;

    /* FIXME: Read from parser before reading from socket in I/O thread */

    /* Start I/O thread */
    pthread_t io_thread;
    pthread_create(&io_thread,  NULL, guacd_connection_io_thread,  params);
    pthread_detach(io_thread);

    return 0;

}

/**
 * Routes the connection on the given socket according to the Guacamole
 * protocol on the given socket, adding new users and creating new client
 * processes as needed.
 *
 * The socket provided will be automatically freed when the connection
 * terminates unless routing fails, in which case non-zero is returned.
 */
static int guacd_route_connection(guacd_proc_map* map, guac_socket* socket) {

    guac_parser* parser = guac_parser_alloc();

    /* Reset guac_error */
    guac_error = GUAC_STATUS_SUCCESS;
    guac_error_message = NULL;

    /* Get protocol from select instruction */
    if (guac_parser_expect(parser, socket, GUACD_USEC_TIMEOUT, "select")) {
        guacd_log_guac_error("Error reading \"select\"");
        guac_parser_free(parser);
        return 1;
    }

    /* Validate args to select */
    if (parser->argc != 1) {
        guacd_log_error("Bad number of arguments to \"select\" (%i)", parser->argc);
        guac_parser_free(parser);
        return 1;
    }

    guacd_proc* proc;

    const char* identifier = parser->argv[0];

    /* If connection ID, retrieve existing process */
    if (identifier[0] == GUAC_CLIENT_ID_PREFIX) {

        proc = guacd_proc_map_retrieve(map, identifier);
        if (proc == NULL)
            guacd_log_info("Connection \"%s\" does not exist.", identifier);
        else
            guacd_log_info("Joining existing connection \"%s\"", identifier);

    }

    /* Otherwise, create new client */
    else {

        guacd_log_info("Creating new client for protocol \"%s\"", identifier);
        proc = guacd_create_proc(parser, identifier);

    }

    if (proc == NULL) {
        guac_parser_free(parser);
        return 1;
    }

    /* Log connection ID */
    guacd_log_info("Connection ID is \"%s\"", proc->client->connection_id);

    /* Add new user (in the case of a new process, this will be the owner */
    if (guacd_add_user(proc, parser, socket) == 0) {

        /* FIXME: The following should ONLY be done for new processes */
#if 0
        /* Store process, allowing other users to join */
        guacd_proc_map_add(map, proc);

        /* Wait for child to finish */
        waitpid(proc->pid, NULL, 0);

        /* Remove client */
        if (guacd_proc_map_remove(map, proc->client->connection_id) == NULL)
            guacd_log_error("Internal failure removing client \"%s\". Client record will never be freed.",
                    proc->client->connection_id);
        else
            guacd_log_info("Connection \"%s\" removed.", proc->client->connection_id);

        /* Free skeleton client */
        guac_client_free(proc->client);

        /* Clean up */
        close(proc->fd_socket);
        free(proc);
#endif

        return 0;
    }

    /* Add of user failed */
    else {
        guac_parser_free(parser);
        return 1;
    }

}

void* guacd_connection_thread(void* data) {

    guacd_connection_thread_params* params = (guacd_connection_thread_params*) data;

    guacd_proc_map* map = params->map;
    int connected_socket_fd = params->connected_socket_fd;

    guac_socket* socket;

#ifdef ENABLE_SSL

    SSL_CTX* ssl_context = params->ssl_context;

    /* If SSL chosen, use it */
    if (ssl_context != NULL) {
        socket = guac_socket_open_secure(ssl_context, connected_socket_fd);
        if (socket == NULL) {
            guacd_log_guac_error("Error opening secure connection");
            free(params);
            return NULL;
        }
    }
    else
        socket = guac_socket_open(connected_socket_fd);

#else
    /* Open guac_socket */
    socket = guac_socket_open(connected_socket_fd);
#endif

    /* Route connection according to Guacamole, creating a new process if needed */
    if (guacd_route_connection(map, socket))
        guac_socket_free(socket);

    free(params);
    return NULL;

}

