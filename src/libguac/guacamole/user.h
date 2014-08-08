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

#include "client-types.h"
#include "pool-types.h"
#include "stream-types.h"
#include "timestamp-types.h"
#include "user-constants.h"
#include "user-fntypes.h"
#include "user-types.h"

#include <pthread.h>

struct guac_user_info {

    /**
     * The number of pixels the remote client requests for the display width.
     * This need not be honored by a client plugin implementation, but if the
     * underlying protocol of the client plugin supports dynamic sizing of the
     * screen, honoring the display size request is recommended.
     */
    int optimal_width;

    /**
     * The number of pixels the remote client requests for the display height.
     * This need not be honored by a client plugin implementation, but if the
     * underlying protocol of the client plugin supports dynamic sizing of the
     * screen, honoring the display size request is recommended.
     */
    int optimal_height;

    /**
     * NULL-terminated array of client-supported audio mimetypes. If the client
     * does not support audio at all, this will be NULL.
     */
    const char** audio_mimetypes;

    /**
     * NULL-terminated array of client-supported video mimetypes. If the client
     * does not support video at all, this will be NULL.
     */
    const char** video_mimetypes;

    /**
     * The DPI of the physical remote display if configured for the optimal
     * width/height combination described here. This need not be honored by
     * a client plugin implementation, but if the underlying protocol of the
     * client plugin supports dynamic sizing of the screen, honoring the
     * stated resolution of the display size request is recommended.
     */
    int optimal_resolution;

};

struct guac_user {

    /**
     * The guac_client to which this user belongs.
     */
    guac_client* client;

    /**
     * This user's actual socket. Data written to this socket will
     * be received by this user alone, and data sent by this specific
     * user will be received by this socket.
     */
    guac_socket* socket;

    /**
     * The unique identifier allocated for this user, which may be used within
     * the Guacamole protocol to refer to this user.  This identifier is
     * guaranteed to be unique from all existing connections and users, and
     * will not collide with any available protocol names.
     */
    char* user_id;

    /**
     * The previous user in the group of users within the same logical
     * connection.  This is currently only used internally by guac_client to
     * track its set of connected users. To iterate connected users, use
     * guac_client_foreach_user().
     */
    guac_user* __prev;

    /**
     * The next user in the group of users within the same logical connection.
     * This is currently only used internally by guac_client to track its set
     * of connected users. To iterate connected users, use
     * guac_client_foreach_user().
     */
    guac_user* __next;

    /**
     * The time (in milliseconds) of receipt of the last sync message from
     * the client.
     */
    guac_timestamp last_received_timestamp;

    /**
     * The time (in milliseconds) that the last sync message was sent to the
     * client.
     */
    guac_timestamp last_sent_timestamp;

    /**
     * Information structure containing properties exposed by the remote
     * client during the initial handshake process.
     */
    guac_user_info info;

    /**
     * Pool of stream indices.
     */
    guac_pool* __stream_pool;

    /**
     * All available output streams (data going to connected client).
     */
    guac_stream* __output_streams;

    /**
     * All available input streams (data coming from connected client).
     */
    guac_stream* __input_streams;

    /**
     * Arbitrary user-specific data.
     */
    void* data;

    /**
     * Handler for mouse events sent by the Gaucamole web-client.
     *
     * The handler takes the integer mouse X and Y coordinates, as well as
     * a button mask containing the bitwise OR of all button values currently
     * being pressed. Those values are:
     *
     * <table>
     *     <tr><th>Button</th>          <th>Value</th></tr>
     *     <tr><td>Left</td>            <td>1</td></tr>
     *     <tr><td>Middle</td>          <td>2</td></tr>
     *     <tr><td>Right</td>           <td>4</td></tr>
     *     <tr><td>Scrollwheel Up</td>  <td>8</td></tr>
     *     <tr><td>Scrollwheel Down</td><td>16</td></tr>
     * </table>

     * Example:
     * @code
     *     int mouse_handler(guac_client* client, int x, int y, int button_mask);
     *
     *     int guac_user_init(guac_client* client, int argc, char** argv) {
     *         client->mouse_handler = mouse_handler;
     *     }
     * @endcode
     */
    guac_user_mouse_handler* mouse_handler;

    /**
     * Handler for key events sent by the Guacamole web-client.
     *
     * The handler takes the integer X11 keysym associated with the key
     * being pressed/released, and an integer representing whether the key
     * is being pressed (1) or released (0).
     *
     * Example:
     * @code
     *     int key_handler(guac_client* client, int keysym, int pressed);
     *
     *     int guac_user_init(guac_client* client, int argc, char** argv) {
     *         client->key_handler = key_handler;
     *     }
     * @endcode
     */
    guac_user_key_handler* key_handler;

    /**
     * Handler for clipboard events sent by the Guacamole web-client. This
     * handler will be called whenever the web-client sets the data of the
     * clipboard.
     *
     * The handler takes a guac_stream, which contains the stream index and
     * will persist through the duration of the transfer, and the mimetype
     * of the data being transferred.
     *
     * Example:
     * @code
     *     int clipboard_handler(guac_client* client, guac_stream* stream,
     *             char* mimetype);
     *
     *     int guac_user_init(guac_client* client, int argc, char** argv) {
     *         client->clipboard_handler = clipboard_handler;
     *     }
     * @endcode
     */
    guac_user_clipboard_handler* clipboard_handler;

    /**
     * Handler for size events sent by the Guacamole web-client.
     *
     * The handler takes an integer width and integer height, representing
     * the current visible screen area of the client.
     *
     * Example:
     * @code
     *     int size_handler(guac_client* client, int width, int height);
     *
     *     int guac_user_init(guac_client* client, int argc, char** argv) {
     *         client->size_handler = size_handler;
     *     }
     * @endcode
     */
    guac_user_size_handler* size_handler;

    /**
     * Handler for file events sent by the Guacamole web-client.
     *
     * The handler takes a guac_stream which contains the stream index and
     * will persist through the duration of the transfer, the mimetype of
     * the file being transferred, and the filename.
     *
     * Example:
     * @code
     *     int file_handler(guac_client* client, guac_stream* stream,
     *             char* mimetype, char* filename);
     *
     *     int guac_user_init(guac_client* client, int argc, char** argv) {
     *         client->file_handler = file_handler;
     *     }
     * @endcode
     */
    guac_user_file_handler* file_handler;

    /**
     * Handler for pipe events sent by the Guacamole web-client.
     *
     * The handler takes a guac_stream which contains the stream index and
     * will persist through the duration of the transfer, the mimetype of
     * the data being transferred, and the pipe name.
     *
     * Example:
     * @code
     *     int pipe_handler(guac_client* client, guac_stream* stream,
     *             char* mimetype, char* name);
     *
     *     int guac_user_init(guac_client* client, int argc, char** argv) {
     *         client->pipe_handler = pipe_handler;
     *     }
     * @endcode
     */
    guac_user_pipe_handler* pipe_handler;

    /**
     * Handler for ack events sent by the Guacamole web-client.
     *
     * The handler takes a guac_stream which contains the stream index and
     * will persist through the duration of the transfer, a string containing
     * the error or status message, and a status code.
     *
     * Example:
     * @code
     *     int ack_handler(guac_client* client, guac_stream* stream,
     *             char* error, guac_protocol_status status);
     *
     *     int guac_user_init(guac_client* client, int argc, char** argv) {
     *         client->ack_handler = ack_handler;
     *     }
     * @endcode
     */
    guac_user_ack_handler* ack_handler;

    /**
     * Handler for blob events sent by the Guacamole web-client.
     *
     * The handler takes a guac_stream which contains the stream index and
     * will persist through the duration of the transfer, an arbitrary buffer
     * containing the blob, and the length of the blob.
     *
     * Example:
     * @code
     *     int blob_handler(guac_client* client, guac_stream* stream,
     *             void* data, int length);
     *
     *     int guac_user_init(guac_client* client, int argc, char** argv) {
     *         client->blob_handler = blob_handler;
     *     }
     * @endcode
     */
    guac_user_blob_handler* blob_handler;

    /**
     * Handler for stream end events sent by the Guacamole web-client.
     *
     * The handler takes only a guac_stream which contains the stream index.
     * This guac_stream will be disposed of immediately after this event is
     * finished.
     *
     * Example:
     * @code
     *     int end_handler(guac_client* client, guac_stream* stream);
     *
     *     int guac_user_init(guac_client* client, int argc, char** argv) {
     *         client->end_handler = end_handler;
     *     }
     * @endcode
     */
    guac_user_end_handler* end_handler;

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
     *     int leave_handler(guac_user* user);
     *
     *     int my_join_handler(guac_user* user) {
     *         user->leave_handler = leave_handler;
     *     }
     * @endcode
     */
    guac_user_leave_handler* leave_handler;

};

/**
 * Allocates a new, blank user, not associated with any specific client or
 * socket.
 *
 * @return The newly allocated guac_user, or NULL if allocation failed.
 */
guac_user* guac_user_alloc();

/**
 * Frees the given user and all associated resources.
 *
 * @param user The guac_user to free.
 */
void guac_user_free(guac_user* user);

/**
 * Call the appropriate handler defined by the given user for the given
 * instruction. A comparison is made between the instruction opcode and the
 * initial handler lookup table defined in user-handlers.c. The intial handlers
 * will in turn call the user's handler (if defined).
 *
 * @param user The user whose handlers should be called.
 * @param instruction The instruction to pass to the user via the appropriate
 *                    handler.
 */
int guac_user_handle_instruction(guac_user* user, guac_instruction* instruction);

/**
 * Allocates a new stream. An arbitrary index is automatically assigned
 * if no previously-allocated stream is available for use.
 *
 * @param user The user to allocate the stream for.
 * @return The next available stream, or a newly allocated stream.
 */
guac_stream* guac_user_alloc_stream(guac_user* user);

/**
 * Returns the given stream to the pool of available streams, such that it
 * can be reused by any subsequent call to guac_user_alloc_stream().
 *
 * @param user The user to return the stream to.
 * @param stream The stream to return to the pool of available stream.
 */
void guac_user_free_stream(guac_user* user, guac_stream* stream);

#endif

