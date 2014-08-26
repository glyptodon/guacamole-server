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
#include <guacamole/instruction.h>
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
 * Handles the initial handshake of a user, associating that new user with the
 * existing client. This function blocks until the user's connection is
 * finished.
 *
 * Note that if this user is the owner of the client, the owner parameter MUST
 * be set to a non-zero value.
 */
static int guacd_handle_user(guac_client* client, guac_socket* socket, int owner) {

    guac_instruction* size;
    guac_instruction* audio;
    guac_instruction* video;
    guac_instruction* connect;

    /* Send args */
    if (guac_protocol_send_args(socket, client->args)
            || guac_socket_flush(socket)) {
        guacd_log_guac_error("Error sending \"args\" to new user");
        return 1;
    }

    /* Get optimal screen size */
    size = guac_instruction_expect(
            socket, GUACD_USEC_TIMEOUT, "size");
    if (size == NULL) {
        guacd_log_guac_error("Error reading \"size\"");
        return 1;
    }

    /* Get supported audio formats */
    audio = guac_instruction_expect(
            socket, GUACD_USEC_TIMEOUT, "audio");
    if (audio == NULL) {
        guacd_log_guac_error("Error reading \"audio\"");
        return 1;
    }

    /* Get supported video formats */
    video = guac_instruction_expect(
            socket, GUACD_USEC_TIMEOUT, "video");
    if (video == NULL) {
        guacd_log_guac_error("Error reading \"video\"");
        return 1;
    }

    /* Get args from connect instruction */
    connect = guac_instruction_expect(
            socket, GUACD_USEC_TIMEOUT, "connect");
    if (connect == NULL) {
        guacd_log_guac_error("Error reading \"connect\"");
        return 1;
    }

    /* Create skeleton user */
    guac_user* user = guac_user_alloc();
    user->socket = socket;
    user->client = client;
    user->owner = owner;

    /* Parse optimal screen dimensions from size instruction */
    user->info.optimal_width  = atoi(size->argv[0]);
    user->info.optimal_height = atoi(size->argv[1]);

    /* If DPI given, set the client resolution */
    if (size->argc >= 3)
        user->info.optimal_resolution = atoi(size->argv[2]);

    /* Otherwise, use a safe default for rough backwards compatibility */
    else
        user->info.optimal_resolution = 96;

    /* Store audio mimetypes */
    user->info.audio_mimetypes = malloc(sizeof(char*) * (audio->argc+1));
    memcpy(user->info.audio_mimetypes, audio->argv,
            sizeof(char*) * audio->argc);
    user->info.audio_mimetypes[audio->argc] = NULL;

    /* Store video mimetypes */
    user->info.video_mimetypes = malloc(sizeof(char*) * (video->argc+1));
    memcpy(user->info.video_mimetypes, video->argv,
            sizeof(char*) * video->argc);
    user->info.video_mimetypes[video->argc] = NULL;

    /* Acknowledge connection availability */
    guac_protocol_send_ready(socket, client->connection_id);
    guac_socket_flush(socket);

    /* Attempt join */
    if (guac_client_add_user(client, user, connect->argc, connect->argv))
        guacd_log_error("User \"%s\" could NOT join connection \"%s\"", user->user_id, client->connection_id);

    /* Begin user connection if join successful */
    else {

        guacd_log_info("User \"%s\" joined connection \"%s\" (%i users now present)",
                user->user_id, client->connection_id, client->connected_users);

        /* Handle user I/O, wait for connection to terminate */
        guacd_user_start(user, socket);

        /* Remove/free user */
        guac_client_remove_user(client, user);
        guacd_log_info("User \"%s\" disconnected (%i users remain)", user->user_id, client->connected_users);
        guac_user_free(user);

    }

    guac_instruction_free(connect);

    /* Free mimetype lists */
    free(user->info.audio_mimetypes);
    free(user->info.video_mimetypes);

    /* Free remaining instructions */
    guac_instruction_free(audio);
    guac_instruction_free(video);
    guac_instruction_free(size);

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

guacd_proc* guacd_create_proc(const char* protocol) {

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

