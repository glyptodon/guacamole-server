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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

/**
 * Copies the contents of an instruction's argc and argv into a
 * newly-allocated, NULL-terminated string array.
 */
static char** __dup_mimetypes(int argc, char** argv) {

    char** mimetypes = malloc(sizeof(char*) * (argc+1));

    int i;

    /* Copy contents of argv into NULL-terminated string array */
    for (i=0; i<argc; i++)
        mimetypes[i] = strdup(argv[i]);

    mimetypes[argc] = NULL;

    return mimetypes;

}

/**
 * Frees an array of mimetypes allocated through __dup_mimetypes().
 */
static void __free_mimetypes(char** mimetypes) {

    char** current = mimetypes;

    /* Free contents of mimetypes array */
    while (*current != NULL) {
        free(*current);
        current++;
    }

    free(mimetypes);

}

/**
 * Handles the initial handshake of a user and all subsequent I/O.
 */
static int guacd_handle_user(guac_user* user) {

    guac_socket* socket = user->socket;
    guac_client* client = user->client;

    /* Send args */
    if (guac_protocol_send_args(socket, client->args)
            || guac_socket_flush(socket)) {

        /* Log error */
        guacd_log_handshake_failure();
        guacd_log_guac_error(GUAC_LOG_DEBUG, "Error sending \"args\" to new user");

        return 1;
    }

    guac_parser* parser = guac_parser_alloc();

    /* Get optimal screen size */
    if (guac_parser_expect(parser, socket, GUACD_USEC_TIMEOUT, "size")) {

        /* Log error */
        guacd_log_handshake_failure();
        guacd_log_guac_error(GUAC_LOG_DEBUG, "Error reading \"size\"");

        guac_parser_free(parser);
        return 1;
    }

    /* Validate content of size instruction */
    if (parser->argc < 2) {
        guacd_log(GUAC_LOG_ERROR, "Received \"size\" instruction lacked required arguments.");
        guac_parser_free(parser);
        return 1;
    }

    /* Parse optimal screen dimensions from size instruction */
    user->info.optimal_width  = atoi(parser->argv[0]);
    user->info.optimal_height = atoi(parser->argv[1]);

    /* If DPI given, set the client resolution */
    if (parser->argc >= 3)
        user->info.optimal_resolution = atoi(parser->argv[2]);

    /* Otherwise, use a safe default for rough backwards compatibility */
    else
        user->info.optimal_resolution = 96;

    /* Get supported audio formats */
    if (guac_parser_expect(parser, socket, GUACD_USEC_TIMEOUT, "audio")) {

        /* Log error */
        guacd_log_handshake_failure();
        guacd_log_guac_error(GUAC_LOG_DEBUG, "Error reading \"audio\"");

        guac_parser_free(parser);
        return 1;
    }

    /* Store audio mimetypes */
    char** audio_mimetypes = __dup_mimetypes(parser->argc, parser->argv);
    user->info.audio_mimetypes = (const char**) audio_mimetypes;

    /* Get supported video formats */
    if (guac_parser_expect(parser, socket, GUACD_USEC_TIMEOUT, "video")) {

        /* Log error */
        guacd_log_handshake_failure();
        guacd_log_guac_error(GUAC_LOG_DEBUG, "Error reading \"video\"");

        guac_parser_free(parser);
        return 1;
    }

    /* Store video mimetypes */
    char** video_mimetypes = __dup_mimetypes(parser->argc, parser->argv);
    user->info.video_mimetypes = (const char**) video_mimetypes;

    /* Get args from connect instruction */
    if (guac_parser_expect(parser, socket, GUACD_USEC_TIMEOUT, "connect")) {

        /* Log error */
        guacd_log_handshake_failure();
        guacd_log_guac_error(GUAC_LOG_DEBUG, "Error reading \"connect\"");

        guac_parser_free(parser);
        return 1;
    }

    /* Acknowledge connection availability */
    guac_protocol_send_ready(socket, client->connection_id);
    guac_socket_flush(socket);

    /* Attempt join */
    if (guac_client_add_user(client, user, parser->argc, parser->argv))
        guacd_log(GUAC_LOG_ERROR, "User \"%s\" could NOT join connection \"%s\"", user->user_id, client->connection_id);

    /* Begin user connection if join successful */
    else {

        guacd_log(GUAC_LOG_INFO, "User \"%s\" joined connection \"%s\" (%i users now present)",
                user->user_id, client->connection_id, client->connected_users);

        /* Handle user I/O, wait for connection to terminate */
        guacd_user_start(parser, user);

        /* Remove/free user */
        guac_client_remove_user(client, user);
        guacd_log(GUAC_LOG_INFO, "User \"%s\" disconnected (%i users remain)", user->user_id, client->connected_users);

    }

    /* Free mimetype lists */
    __free_mimetypes(audio_mimetypes);
    __free_mimetypes(video_mimetypes);

    guac_parser_free(parser);

    /* Successful disconnect */
    return 0;

}

/**
 * Parameters for the user thread.
 */
typedef struct guacd_user_thread_params {

    /**
     * The process being joined.
     */
    guacd_proc* proc;

    /**
     * The file descriptor of the joining user's socket.
     */
    int fd;

    /**
     * Whether the joining user is the connection owner.
     */
    int owner;

} guacd_user_thread_params;

/**
 * Handles a user's entire connection and socket lifecycle.
 */
void* guacd_user_thread(void* data) {

    guacd_user_thread_params* params = (guacd_user_thread_params*) data;
    guacd_proc* proc = params->proc;
    guac_client* client = proc->client;

    /* Get guac_socket for user's file descriptor */
    guac_socket* socket = guac_socket_open(params->fd);
    if (socket == NULL)
        return NULL;

    /* Create skeleton user */
    guac_user* user = guac_user_alloc();
    user->socket = socket;
    user->client = client;
    user->owner  = params->owner;

    /* Handle user connection from handshake until disconnect/completion */
    guacd_handle_user(user);

    /* Stop client and prevent future users if all users are disconnected */
    if (client->connected_users == 0) {
        guacd_log(GUAC_LOG_INFO, "Last user of connection \"%s\" disconnected", client->connection_id);
        guacd_proc_stop(proc);
    }

    /* Clean up */
    guac_socket_free(socket);
    guac_user_free(user);
    free(params);

    return NULL;

}

/**
 * Begins a new user connection under a given process, using the given file
 * descriptor.
 */
static void guacd_proc_add_user(guacd_proc* proc, int fd, int owner) {

    guacd_user_thread_params* params = malloc(sizeof(guacd_user_thread_params));
    params->proc = proc;
    params->fd = fd;
    params->owner = owner;

    /* Start user thread */
    pthread_t user_thread;
    pthread_create(&user_thread, NULL, guacd_user_thread, params);
    pthread_detach(user_thread);

}

/**
 * Starts protocol-specific handling on the given process. This function does
 * NOT return. It initializes the process with protocol-specific handlers and
 * then runs until the fd_socket is closed.
 */
static void guacd_exec_proc(guacd_proc* proc, const char* protocol) {

    /* Init client for selected protocol */
    if (guac_client_load_plugin(proc->client, protocol)) {

        /* Log error */
        if (guac_error == GUAC_STATUS_NOT_FOUND)
            guacd_log(GUAC_LOG_WARNING,
                    "Support for protocol \"%s\" is not installed", protocol);
        else
            guacd_log_guac_error(GUAC_LOG_ERROR,
                    "Unable to load client plugin");

        guac_client_free(proc->client);
        close(proc->fd_socket);
        free(proc);
        exit(1);
    }

    /* The first file descriptor is the owner */
    int owner = 1;

    /* Add each received file descriptor as a new user */
    int received_fd;
    while ((received_fd = guacd_recv_fd(proc->fd_socket)) != -1) {

        guacd_proc_add_user(proc, received_fd, owner);

        /* Future file descriptors are not owners */
        owner = 0;

    }

    /* Stop and free client */
    guac_client_stop(proc->client);
    guac_client_free(proc->client);

    /* Child is finished */
    close(proc->fd_socket);
    free(proc);
    exit(0);

}

guacd_proc* guacd_create_proc(guac_parser* parser, const char* protocol) {

    int sockets[2];

    /* Open UNIX socket pair */
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, sockets) < 0) {
        guacd_log(GUAC_LOG_ERROR, "Error opening socket pair: %s", strerror(errno));
        return NULL;
    }

    int parent_socket = sockets[0];
    int child_socket = sockets[1];

    /* Allocate process */
    guacd_proc* proc = calloc(1, sizeof(guacd_proc));
    if (proc == NULL) {
        close(parent_socket);
        close(child_socket);
        return NULL;
    }

    /* Associate new client */
    proc->client = guac_client_alloc();
    if (proc->client == NULL) {
        guacd_log_guac_error(GUAC_LOG_ERROR, "Unable to create client");
        close(parent_socket);
        close(child_socket);
        free(proc);
        return NULL;
    }

    /* Init logging */
    proc->client->log_handler = guacd_client_log;

    /* Fork */
    proc->pid = fork();
    if (proc->pid < 0) {
        guacd_log(GUAC_LOG_ERROR, "Cannot fork child process: %s", strerror(errno));
        close(parent_socket);
        close(child_socket);
        guac_client_free(proc->client);
        free(proc);
        return NULL;
    }

    /* Child */
    else if (proc->pid == 0) {

        /* Communicate with parent */
        proc->fd_socket = parent_socket;
        close(child_socket);

        /* Start protocol-specific handling */
        guacd_exec_proc(proc, protocol);

    }

    /* Parent */
    else {

        /* Communicate with child */
        proc->fd_socket = child_socket;
        close(parent_socket);

    }

    return proc;

}

void guacd_proc_stop(guacd_proc* proc) {
    guac_client_stop(proc->client);
    close(proc->fd_socket);
}

