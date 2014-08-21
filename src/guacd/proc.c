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
#include "proc.h"
#include "user.h"

#include <guacamole/client.h>
#include <guacamole/error.h>
#include <guacamole/instruction.h>
#include <guacamole/plugin.h>
#include <guacamole/protocol.h>
#include <guacamole/socket.h>
#include <guacamole/user.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
 * Returns a new client for the given protocol.
 */
static guac_client* guacd_create_client(guacd_proc_map* map, const char* protocol) {

    /* Create client */
    guac_client* client = guac_client_alloc();
    if (client == NULL)
        return NULL;

    /* Init logging */
    client->log_info_handler = guacd_client_log_info;
    client->log_error_handler = guacd_client_log_error;

    /* Init client for selected protocol */
    if (guac_client_load_plugin(client, protocol)) {
        guacd_log_guac_error("Protocol initialization failed");
        guac_client_free(client);
        return NULL;
    }

    /* Add client to global storage */
    if (guacd_proc_map_add(map, client)) {
        guacd_log_error("Internal failure adding client \"%s\".", client->connection_id);
        guac_client_free(client);
        return NULL;
    }

    return client;

}

/**
 * Begins a new user connection under a given process, using the given file
 * descriptor. The file descriptor is added to the running process and managed
 * as a new user.
 */
static void guacd_proc_add_fd() {

    /* Proceed with handshake and user I/O */
    int retval = guacd_handle_user(client, socket, owner);

    /* Clean up client if no more users */
    if (client->connected_users == 0) {

        guacd_log_info("Last user of connection \"%s\" disconnected", client->connection_id);

        /* Remove client */
        if (guacd_proc_map_remove(map, client->connection_id) == NULL)
            guacd_log_error("Internal failure removing client \"%s\". Client record will never be freed.",
                    client->connection_id);
        else
            guacd_log_info("Connection \"%s\" removed.", client->connection_id);

        guac_client_stop(client);
        guac_client_free(client);

    }

}

guacd_proc* guacd_create_proc(const char* protocol) {
}

