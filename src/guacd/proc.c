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

    /* Free contents of mimetypes array */
    while (*mimetypes != NULL) {
        free(*mimetypes);
        mimetypes++;
    }

    free(mimetypes);

}

/**
 * Handles the initial handshake of a user, associating that new user with the
 * existing client. This function blocks until the user's connection is
 * finished.
 *
 * Note that if this user is the owner of the client, the owner parameter MUST
 * be set to a non-zero value.
 */
static int guacd_handle_user(guac_client* client, guac_socket* socket, int owner) {

    guac_parser* parser = guac_parser_alloc();

    /* Send args */
    if (guac_protocol_send_args(socket, client->args)
            || guac_socket_flush(socket)) {
        guacd_log_guac_error("Error sending \"args\" to new user");
        return 1;
    }

    /* Create skeleton user */
    guac_user* user = guac_user_alloc();
    user->socket = socket;
    user->client = client;
    user->owner = owner;

    /* Get optimal screen size */
    if (guac_parser_expect(parser, socket, GUACD_USEC_TIMEOUT, "size")) {
        guacd_log_guac_error("Error reading \"size\"");
        return 1;
    }

    /* Validate content of size instruction */
    if (parser->argc < 2) {
        guacd_log_error("Received \"size\" instruction lacked required arguments.");
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
        guacd_log_guac_error("Error reading \"audio\"");
        return 1;
    }

    /* Store audio mimetypes */
    char** audio_mimetypes = __dup_mimetypes(parser->argc, parser->argv);
    user->info.audio_mimetypes = (const char**) audio_mimetypes;

    /* Get supported video formats */
    if (guac_parser_expect(parser, socket, GUACD_USEC_TIMEOUT, "video")) {
        guacd_log_guac_error("Error reading \"video\"");
        return 1;
    }

    /* Store video mimetypes */
    char** video_mimetypes = __dup_mimetypes(parser->argc, parser->argv);
    user->info.video_mimetypes = (const char**) video_mimetypes;

    /* Get args from connect instruction */
    if (guac_parser_expect(parser, socket, GUACD_USEC_TIMEOUT, "connect")) {
        guacd_log_guac_error("Error reading \"connect\"");
        return 1;
    }

    /* Acknowledge connection availability */
    guac_protocol_send_ready(socket, client->connection_id);
    guac_socket_flush(socket);

    /* Attempt join */
    if (guac_client_add_user(client, user, parser->argc, parser->argv))
        guacd_log_error("User \"%s\" could NOT join connection \"%s\"", user->user_id, client->connection_id);

    /* Begin user connection if join successful */
    else {

        guacd_log_info("User \"%s\" joined connection \"%s\" (%i users now present)",
                user->user_id, client->connection_id, client->connected_users);

        /* Handle user I/O, wait for connection to terminate */
        guacd_user_start(parser, user);

        /* Remove/free user */
        guac_client_remove_user(client, user);
        guacd_log_info("User \"%s\" disconnected (%i users remain)", user->user_id, client->connected_users);
        guac_user_free(user);

    }

    /* Free mimetype lists */
    __free_mimetypes(audio_mimetypes);
    __free_mimetypes(video_mimetypes);

    guac_parser_free(parser);

    /* Successful disconnect */
    return 0;

}

/**
 * Begins a new user connection under a given process, using the given file
 * descriptor.
 */
static void guacd_proc_add_user(guacd_proc* proc, int fd, int owner) {

    guac_client* client = proc->client;

    /* Get guac_socket for given file descriptor */
    guac_socket* socket = guac_socket_open(fd);
    if (socket == NULL)
        return;

    /* Handle user connection from handshake until disconnect/completion */
    guacd_handle_user(client, socket, owner);

    /* Stop client and prevent future users if all users are disconnected */
    if (client->connected_users == 0) {
        guacd_log_info("Last user of connection \"%s\" disconnected", client->connection_id);
        guacd_proc_stop(proc);
    }

    /* Close socket */
    guac_socket_free(socket);
    close(fd);

}

/**
 * Starts protocol-specific handling on the given process. This function does
 * NOT return. It initializes the process with protocol-specific handlers and
 * then runs until the fd_socket is closed.
 */
static void guacd_exec_proc(guacd_proc* proc, const char* protocol) {

    /* Init client for selected protocol */
    if (guac_client_load_plugin(proc->client, protocol)) {
        guacd_log_guac_error("Protocol initialization failed");
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
        guacd_log_error("Error opening socket pair: %s", strerror(errno));
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
        close(parent_socket);
        close(child_socket);
        free(proc);
        return NULL;
    }

    /* Init logging */
    proc->client->log_info_handler  = guacd_client_log_info;
    proc->client->log_error_handler = guacd_client_log_error;

    /* Fork */
    proc->pid = fork();
    if (proc->pid < 0) {
        guacd_log_error("Cannot fork child process: %s", strerror(errno));
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

