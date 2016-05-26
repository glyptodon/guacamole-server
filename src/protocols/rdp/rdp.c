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

#include "config.h"

#include "client.h"
#include "guac_cursor.h"
#include "guac_display.h"
#include "guac_recording.h"
#include "rdp.h"
#include "rdp_bitmap.h"
#include "rdp_cliprdr.h"
#include "rdp_disp.h"
#include "rdp_gdi.h"
#include "rdp_glyph.h"
#include "rdp_keymap.h"
#include "rdp_pointer.h"
#include "rdp_rail.h"
#include "rdp_stream.h"
#include "rdp_svc.h"

#ifdef ENABLE_COMMON_SSH
#include <guac_sftp.h>
#include <guac_ssh.h>
#include <guac_ssh_user.h>
#endif

#include <freerdp/cache/bitmap.h>
#include <freerdp/cache/brush.h>
#include <freerdp/cache/glyph.h>
#include <freerdp/cache/offscreen.h>
#include <freerdp/cache/palette.h>
#include <freerdp/cache/pointer.h>
#include <freerdp/channels/channels.h>
#include <freerdp/freerdp.h>
#include <guacamole/audio.h>
#include <guacamole/client.h>
#include <guacamole/protocol.h>
#include <guacamole/socket.h>
#include <guacamole/timestamp.h>

#ifdef HAVE_FREERDP_CLIENT_CLIPRDR_H
#include <freerdp/client/cliprdr.h>
#else
#include "compat/client-cliprdr.h"
#endif

#ifdef HAVE_FREERDP_CLIENT_DISP_H
#include <freerdp/client/disp.h>
#endif

#ifdef HAVE_FREERDP_EVENT_PUBSUB
#include <freerdp/event.h>
#endif

#ifdef LEGACY_FREERDP
#include "compat/rail.h"
#else
#include <freerdp/rail.h>
#endif

#ifdef ENABLE_WINPR
#include <winpr/wtypes.h>
#else
#include "compat/winpr-wtypes.h"
#endif

#ifdef HAVE_FREERDP_ADDIN_H
#include <freerdp/addin.h>
#endif

#ifdef HAVE_FREERDP_CLIENT_CHANNELS_H
#include <freerdp/client/channels.h>
#endif

#ifdef HAVE_FREERDP_VERSION_H
#include <freerdp/version.h>
#endif

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>

/**
 * Callback invoked by FreeRDP for data received along a channel. This is the
 * most recent version of the callback and uses a 16-bit unsigned integer for
 * the channel ID, as well as different type naming for the datatype of the
 * data itself. This function does nothing more than invoke
 * freerdp_channels_data() with the given arguments. The prototypes of these
 * functions are compatible in 1.2 and later, but not necessarily prior to
 * that, hence the conditional compilation of differing prototypes.
 *
 * Beware that the official purpose of these parameters is an undocumented
 * mystery. The meanings below are derived from looking at how the function is
 * used within FreeRDP.
 *
 * @param rdp_inst
 *     The RDP client instance associated with the channel receiving the data.
 *
 * @param channelId
 *     The integer ID of the channel that received the data.
 *
 * @param data
 *     A buffer containing the received data.
 *
 * @param size
 *     The number of bytes received and contained in the given buffer (the
 *     number of bytes received within the PDU that resulted in this function
 *     being inboked).
 *
 * @param flags
 *     Channel control flags, as defined by the CHANNEL_PDU_HEADER in the RDP
 *     specification.
 *
 * @param total_size
 *     The total length of the chanel data being received, which may span
 *     multiple PDUs (see the "length" field of CHANNEL_PDU_HEADER).
 *
 * @return
 *     Zero if the received channel data was successfully handled, non-zero
 *     otherwise. Note that this return value is discarded in practice.
 */
#if defined(FREERDP_VERSION_MAJOR) \
    && (FREERDP_VERSION_MAJOR > 1 || FREERDP_VERSION_MINOR >= 2)
static int __guac_receive_channel_data(freerdp* rdp_inst, UINT16 channelId,
        BYTE* data, int size, int flags, int total_size) {
#else
static int __guac_receive_channel_data(freerdp* rdp_inst, int channelId,
        UINT8* data, int size, int flags, int total_size) {
#endif
    return freerdp_channels_data(rdp_inst, channelId,
            data, size, flags, total_size);
}

#ifdef HAVE_FREERDP_EVENT_PUBSUB
/**
 * Called whenever a channel connects via the PubSub event system within
 * FreeRDP.
 *
 * @param context
 *     The rdpContext associated with the active RDP session.
 *
 * @param e
 *     Event-specific arguments, mainly the name of the channel, and a
 *     reference to the associated plugin loaded for that channel by FreeRDP.
 */
static void guac_rdp_channel_connected(rdpContext* context,
        ChannelConnectedEventArgs* e) {

    guac_client* client = ((rdp_freerdp_context*) context)->client;
    guac_rdp_client* rdp_client = (guac_rdp_client*) client->data;
    guac_rdp_settings* settings = rdp_client->settings;

    if (settings->resize_method == GUAC_RESIZE_DISPLAY_UPDATE) {
#ifdef HAVE_RDPSETTINGS_SUPPORTDISPLAYCONTROL
        /* Store reference to the display update plugin once it's connected */
        if (strcmp(e->name, DISP_DVC_CHANNEL_NAME) == 0) {

            DispClientContext* disp = (DispClientContext*) e->pInterface;

            /* Init module with current display size */
            guac_rdp_disp_set_size(rdp_client->disp, rdp_client->settings,
                    context->instance, guac_rdp_get_width(context->instance),
                    guac_rdp_get_height(context->instance));

            /* Store connected channel */
            guac_rdp_disp_connect(rdp_client->disp, disp);
            guac_client_log(client, GUAC_LOG_DEBUG,
                    "Display update channel connected.");

        }
#endif
    }

}
#endif

BOOL rdp_freerdp_pre_connect(freerdp* instance) {

    rdpContext* context = instance->context;
    rdpChannels* channels = context->channels;

    guac_client* client = ((rdp_freerdp_context*) context)->client;
    guac_rdp_client* rdp_client = (guac_rdp_client*) client->data;
    guac_rdp_settings* settings = rdp_client->settings;

    rdpBitmap* bitmap;
    rdpGlyph* glyph;
    rdpPointer* pointer;
    rdpPrimaryUpdate* primary;
    CLRCONV* clrconv;


#ifdef HAVE_FREERDP_REGISTER_ADDIN_PROVIDER
    /* Init FreeRDP add-in provider */
    freerdp_register_addin_provider(freerdp_channels_load_static_addin_entry, 0);
#endif

#ifdef HAVE_FREERDP_EVENT_PUBSUB
    /* Subscribe to and handle channel connected events */
    PubSub_SubscribeChannelConnected(context->pubSub,
            (pChannelConnectedEventHandler) guac_rdp_channel_connected);
#endif

    /* Load virtual channel management plugin */
    if (freerdp_channels_load_plugin(channels, instance->settings,
                "drdynvc", instance->settings))
        guac_client_log(client, GUAC_LOG_WARNING,
                "Failed to load drdynvc plugin.");

    /* Init display update plugin (if available and required) */
    if (settings->resize_method == GUAC_RESIZE_DISPLAY_UPDATE)
        guac_rdp_disp_load_plugin(instance->context);

    /* Load clipboard plugin */
    if (freerdp_channels_load_plugin(channels, instance->settings,
                "cliprdr", NULL))
        guac_client_log(client, GUAC_LOG_WARNING,
                "Failed to load cliprdr plugin. Clipboard will not work.");

    /* If audio enabled, choose an encoder */
    if (settings->audio_enabled) {

        rdp_client->audio = guac_audio_stream_alloc(client, NULL,
                GUAC_RDP_AUDIO_RATE,
                GUAC_RDP_AUDIO_CHANNELS,
                GUAC_RDP_AUDIO_BPS);

        /* Warn if no audio encoding is available */
        if (rdp_client->audio == NULL)
            guac_client_log(client, GUAC_LOG_INFO,
                    "No available audio encoding. Sound disabled.");

    } /* end if audio enabled */

    /* Load filesystem if drive enabled */
    if (settings->drive_enabled)
        rdp_client->filesystem =
            guac_rdp_fs_alloc(client, settings->drive_path,
                    settings->create_drive_path);

    /* If RDPSND/RDPDR required, load them */
    if (settings->printing_enabled
        || settings->drive_enabled
        || settings->audio_enabled) {

        /* Load RDPDR plugin */
        if (freerdp_channels_load_plugin(channels, instance->settings,
                    "guacdr", client))
            guac_client_log(client, GUAC_LOG_WARNING,
                    "Failed to load guacdr plugin. Drive redirection and "
                    "printing will not work. Sound MAY not work.");

        /* Load RDPSND plugin */
        if (freerdp_channels_load_plugin(channels, instance->settings,
                    "guacsnd", client))
            guac_client_log(client, GUAC_LOG_WARNING,
                    "Failed to load guacsnd alongside guacdr plugin. Sound "
                    "will not work. Drive redirection and printing MAY not "
                    "work.");

    }

    /* Load RAIL plugin if RemoteApp in use */
    if (settings->remote_app != NULL) {

#ifdef LEGACY_FREERDP
        RDP_PLUGIN_DATA* plugin_data = malloc(sizeof(RDP_PLUGIN_DATA) * 2);

        plugin_data[0].size = sizeof(RDP_PLUGIN_DATA);
        plugin_data[0].data[0] = settings->remote_app;
        plugin_data[0].data[1] = settings->remote_app_dir;
        plugin_data[0].data[2] = settings->remote_app_args;
        plugin_data[0].data[3] = NULL;

        plugin_data[1].size = 0;

        /* Attempt to load rail */
        if (freerdp_channels_load_plugin(channels, instance->settings,
                    "rail", plugin_data))
            guac_client_log(client, GUAC_LOG_WARNING,
                    "Failed to load rail plugin. RemoteApp will not work.");
#else
        /* Attempt to load rail */
        if (freerdp_channels_load_plugin(channels, instance->settings,
                    "rail", instance->settings))
            guac_client_log(client, GUAC_LOG_WARNING,
                    "Failed to load rail plugin. RemoteApp will not work.");
#endif

    }

    /* Load SVC plugin instances for all static channels */
    if (settings->svc_names != NULL) {

        char** current = settings->svc_names;
        do {

            guac_rdp_svc* svc = guac_rdp_alloc_svc(client, *current);

            /* Attempt to load guacsvc plugin for new static channel */
            if (freerdp_channels_load_plugin(channels, instance->settings,
                        "guacsvc", svc)) {
                guac_client_log(client, GUAC_LOG_WARNING,
                        "Cannot create static channel \"%s\": failed to load guacsvc plugin.",
                        svc->name);
                guac_rdp_free_svc(svc);
            }

            /* Store and log on success */
            else {
                guac_rdp_add_svc(client, svc);
                guac_client_log(client, GUAC_LOG_INFO, "Created static channel \"%s\"...",
                        svc->name);
            }

        } while (*(++current) != NULL);

    }

    /* Init color conversion structure */
    clrconv = calloc(1, sizeof(CLRCONV));
    clrconv->alpha = 1;
    clrconv->invert = 0;
    clrconv->rgb555 = 0;
    clrconv->palette = calloc(1, sizeof(rdpPalette));
    ((rdp_freerdp_context*) context)->clrconv = clrconv;

    /* Init FreeRDP cache */
    instance->context->cache = cache_new(instance->settings);

    /* Set up bitmap handling */
    bitmap = calloc(1, sizeof(rdpBitmap));
    bitmap->size = sizeof(guac_rdp_bitmap);
    bitmap->New = guac_rdp_bitmap_new;
    bitmap->Free = guac_rdp_bitmap_free;
    bitmap->Paint = guac_rdp_bitmap_paint;
    bitmap->Decompress = guac_rdp_bitmap_decompress;
    bitmap->SetSurface = guac_rdp_bitmap_setsurface;
    graphics_register_bitmap(context->graphics, bitmap);
    free(bitmap);

    /* Set up glyph handling */
    glyph = calloc(1, sizeof(rdpGlyph));
    glyph->size = sizeof(guac_rdp_glyph);
    glyph->New = guac_rdp_glyph_new;
    glyph->Free = guac_rdp_glyph_free;
    glyph->Draw = guac_rdp_glyph_draw;
    glyph->BeginDraw = guac_rdp_glyph_begindraw;
    glyph->EndDraw = guac_rdp_glyph_enddraw;
    graphics_register_glyph(context->graphics, glyph);
    free(glyph);

    /* Set up pointer handling */
    pointer = calloc(1, sizeof(rdpPointer));
    pointer->size = sizeof(guac_rdp_pointer);
    pointer->New = guac_rdp_pointer_new;
    pointer->Free = guac_rdp_pointer_free;
    pointer->Set = guac_rdp_pointer_set;
#ifdef HAVE_RDPPOINTER_SETNULL
    pointer->SetNull = guac_rdp_pointer_set_null;
#endif
#ifdef HAVE_RDPPOINTER_SETDEFAULT
    pointer->SetDefault = guac_rdp_pointer_set_default;
#endif
    graphics_register_pointer(context->graphics, pointer);
    free(pointer);

    /* Set up GDI */
    instance->update->DesktopResize = guac_rdp_gdi_desktop_resize;
    instance->update->EndPaint = guac_rdp_gdi_end_paint;
    instance->update->Palette = guac_rdp_gdi_palette_update;
    instance->update->SetBounds = guac_rdp_gdi_set_bounds;

    primary = instance->update->primary;
    primary->DstBlt = guac_rdp_gdi_dstblt;
    primary->PatBlt = guac_rdp_gdi_patblt;
    primary->ScrBlt = guac_rdp_gdi_scrblt;
    primary->MemBlt = guac_rdp_gdi_memblt;
    primary->OpaqueRect = guac_rdp_gdi_opaquerect;

    pointer_cache_register_callbacks(instance->update);
    glyph_cache_register_callbacks(instance->update);
    brush_cache_register_callbacks(instance->update);
    bitmap_cache_register_callbacks(instance->update);
    offscreen_cache_register_callbacks(instance->update);
    palette_cache_register_callbacks(instance->update);

    /* Init channels (pre-connect) */
    if (freerdp_channels_pre_connect(channels, instance)) {
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_SERVER_ERROR, "Error initializing RDP client channel manager");
        return FALSE;
    }

    return TRUE;

}

/**
 * Callback invoked by FreeRDP just after the connection is established with
 * the RDP server. Implementations are required to manually invoke
 * freerdp_channels_post_connect().
 *
 * @param instance
 *     The FreeRDP instance that has just connected.
 *
 * @return
 *     TRUE if successful, FALSE if an error occurs.
 */
static BOOL rdp_freerdp_post_connect(freerdp* instance) {

    rdpContext* context = instance->context;
    guac_client* client = ((rdp_freerdp_context*) context)->client;
    rdpChannels* channels = instance->context->channels;

    /* Init channels (post-connect) */
    if (freerdp_channels_post_connect(channels, instance)) {
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_SERVER_ERROR, "Error initializing RDP client channel manager");
        return FALSE;
    }

    return TRUE;

}

/**
 * Callback invoked by FreeRDP when authentication is required but a username
 * and password has not already been given. In the case of Guacamole, this
 * function always succeeds but does not populate the usename or password. The
 * username/password must be given within the connection parameters.
 *
 * @param instance
 *     The FreeRDP instance associated with the RDP session requesting
 *     credentials.
 *
 * @param username
 *     Pointer to a string which will receive the user's username.
 *
 * @param password
 *     Pointer to a string which will receive the user's password.
 *
 * @param domain
 *     Pointer to a string which will receive the domain associated with the
 *     user's account.
 *
 * @return
 *     Always TRUE.
 */
static BOOL rdp_freerdp_authenticate(freerdp* instance, char** username,
        char** password, char** domain) {

    rdpContext* context = instance->context;
    guac_client* client = ((rdp_freerdp_context*) context)->client;

    /* Warn if connection is likely to fail due to lack of credentials */
    guac_client_log(client, GUAC_LOG_INFO,
            "Authentication requested but username or password not given");
    return TRUE;

}

/**
 * Callback invoked by FreeRDP when the SSL/TLS certificate of the RDP server
 * needs to be verified. If this ever happens, this function implementation
 * will always fail unless the connection has been configured to ignore
 * certificate validity.
 *
 * @param instance
 *     The FreeRDP instance associated with the RDP session whose SSL/TLS
 *     certificate needs to be verified.
 *
 * @param subject
 *     The subject to whom the certificate was issued.
 *
 * @param issuer
 *     The authority that issued the certificate,
 *
 * @param fingerprint
 *     The cryptographic fingerprint of the certificate.
 *
 * @return
 *     TRUE if the certificate passes verification, FALSE otherwise.
 */
static BOOL rdp_freerdp_verify_certificate(freerdp* instance, char* subject,
        char* issuer, char* fingerprint) {

    rdpContext* context = instance->context;
    guac_client* client = ((rdp_freerdp_context*) context)->client;
    guac_rdp_client* rdp_client =
        (guac_rdp_client*) client->data;

    /* Bypass validation if ignore_certificate given */
    if (rdp_client->settings->ignore_certificate) {
        guac_client_log(client, GUAC_LOG_INFO, "Certificate validation bypassed");
        return TRUE;
    }

    guac_client_log(client, GUAC_LOG_INFO, "Certificate validation failed");
    return FALSE;

}

/**
 * Callback invoked by FreeRDP after a new rdpContext has been allocated and
 * associated with the current FreeRDP instance. Implementations are required
 * to manually invoke freerdp_channels_new() at this point.
 *
 * @param instance
 *     The FreeRDP instance whose context has just been allocated.
 *
 * @param context
 *     The newly-allocated FreeRDP context.
 */
static void rdp_freerdp_context_new(freerdp* instance, rdpContext* context) {
    context->channels = freerdp_channels_new();
}

/**
 * Callback invoked by FreeRDP when the rdpContext is being freed. This must be
 * provided, but there is no Guacamole-specific data associated with the
 * FreeRDP context, so nothing is done here.
 *
 * @param instance
 *     The FreeRDP instance whose context is being freed.
 *
 * @param context
 *     The FreeRDP context being freed.
 */
static void rdp_freerdp_context_free(freerdp* instance, rdpContext* context) {
    /* EMPTY */
}

/**
 * Loads all keysym/scancode mappings declared within the given keymap and its
 * parent keymap, if any. These mappings are stored within the guac_rdp_client
 * structure associated with the given guac_client for future use in
 * translating keysyms to the scancodes required by RDP key events.
 *
 * @param client
 *     The guac_client whose associated guac_rdp_client should be initialized
 *     with the keysym/scancode mapping defined in the given keymap.
 *
 * @param keymap
 *     The keymap to use to populate the given client's keysym/scancode
 *     mapping.
 */
static void __guac_rdp_client_load_keymap(guac_client* client,
        const guac_rdp_keymap* keymap) {

    guac_rdp_client* rdp_client =
        (guac_rdp_client*) client->data;

    /* Get mapping */
    const guac_rdp_keysym_desc* mapping = keymap->mapping;

    /* If parent exists, load parent first */
    if (keymap->parent != NULL)
        __guac_rdp_client_load_keymap(client, keymap->parent);

    /* Log load */
    guac_client_log(client, GUAC_LOG_INFO, "Loading keymap \"%s\"", keymap->name);

    /* Load mapping into keymap */
    while (mapping->keysym != 0) {

        /* Copy mapping */
        GUAC_RDP_KEYSYM_LOOKUP(rdp_client->keymap, mapping->keysym) =
            *mapping;

        /* Next keysym */
        mapping++;

    }

}

/**
 * Waits for messages from the RDP server for the given number of microseconds.
 *
 * @param client
 *     The client associated with the current RDP session.
 *
 * @param timeout_usecs
 *     The maximum amount of time to wait, in microseconds.
 *
 * @return
 *     A positive value if messages are ready, zero if the specified timeout
 *     period elapsed, or a negative value if an error occurs.
 */
static int rdp_guac_client_wait_for_messages(guac_client* client,
        int timeout_usecs) {

    guac_rdp_client* rdp_client = (guac_rdp_client*) client->data;
    freerdp* rdp_inst = rdp_client->rdp_inst;
    rdpChannels* channels = rdp_inst->context->channels;

    int result;
    int index;
    int max_fd, fd;
    void* read_fds[32];
    void* write_fds[32];
    int read_count = 0;
    int write_count = 0;
    fd_set rfds, wfds;

    struct timeval timeout = {
        .tv_sec  = 0,
        .tv_usec = timeout_usecs
    };

    /* Get RDP fds */
    if (!freerdp_get_fds(rdp_inst, read_fds, &read_count, write_fds, &write_count)) {
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_SERVER_ERROR, "Unable to read RDP file descriptors.");
        return -1;
    }

    /* Get channel fds */
    if (!freerdp_channels_get_fds(channels, rdp_inst, read_fds, &read_count, write_fds,
                &write_count)) {
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_SERVER_ERROR, "Unable to read RDP channel file descriptors.");
        return -1;
    }

    /* Construct read fd_set */
    max_fd = 0;
    FD_ZERO(&rfds);
    for (index = 0; index < read_count; index++) {
        fd = (int)(long) (read_fds[index]);
        if (fd > max_fd)
            max_fd = fd;
        FD_SET(fd, &rfds);
    }

    /* Construct write fd_set */
    FD_ZERO(&wfds);
    for (index = 0; index < write_count; index++) {
        fd = (int)(long) (write_fds[index]);
        if (fd > max_fd)
            max_fd = fd;
        FD_SET(fd, &wfds);
    }

    /* If no file descriptors, error */
    if (max_fd == 0) {
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_SERVER_ERROR, "No file descriptors associated with RDP connection.");
        return -1;
    }

    /* Wait for all RDP file descriptors */
    result = select(max_fd + 1, &rfds, &wfds, NULL, &timeout);
    if (result < 0) {

        /* If error ignorable, pretend timout occurred */
        if (errno == EAGAIN
            || errno == EWOULDBLOCK
            || errno == EINPROGRESS
            || errno == EINTR)
            return 0;

        /* Otherwise, return as error */
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_SERVER_ERROR, "Error waiting for file descriptor.");
        return -1;

    }

    /* Return wait result */
    return result;

}

/**
 * Connects to an RDP server as described by the guac_rdp_settings structure
 * associated with the given client, allocating and freeing all objects
 * directly related to the RDP connection. It is expected that all objects
 * which are independent of FreeRDP's state (the clipboard, display update
 * management, etc.) will already be allocated and associated with the
 * guac_rdp_client associated with the given guac_client. This function blocks
 * for the duration of the RDP session, returning only after the session has
 * completely disconnected.
 *
 * @param client
 *     The guac_client associated with the RDP settings describing the
 *     connection that should be established.
 *
 * @return
 *     Zero if the connection successfully terminated and a reconnect is
 *     desired, non-zero if an error occurs or the connection was disconnected
 *     and a reconnect is NOT desired.
 */
static int guac_rdp_handle_connection(guac_client* client) {

    guac_rdp_client* rdp_client = (guac_rdp_client*) client->data;
    guac_rdp_settings* settings = rdp_client->settings;

    /* Init random number generator */
    srandom(time(NULL));

    /* Set up screen recording, if requested */
    if (settings->recording_path != NULL) {
        guac_common_recording_create(client,
                settings->recording_path,
                settings->recording_name,
                settings->create_recording_path);
    }

    /* Create display */
    rdp_client->display = guac_common_display_alloc(client,
            rdp_client->settings->width,
            rdp_client->settings->height);

    rdp_client->current_surface = rdp_client->display->default_surface;

    rdp_client->requested_clipboard_format = CB_FORMAT_TEXT;
    rdp_client->available_svc = guac_common_list_alloc();

#ifdef HAVE_FREERDP_CHANNELS_GLOBAL_INIT
    freerdp_channels_global_init();
#endif

    /* Init client */
    freerdp* rdp_inst = freerdp_new();
    rdp_inst->PreConnect = rdp_freerdp_pre_connect;
    rdp_inst->PostConnect = rdp_freerdp_post_connect;
    rdp_inst->Authenticate = rdp_freerdp_authenticate;
    rdp_inst->VerifyCertificate = rdp_freerdp_verify_certificate;
    rdp_inst->ReceiveChannelData = __guac_receive_channel_data;

    /* Allocate FreeRDP context */
#ifdef LEGACY_FREERDP
    rdp_inst->context_size = sizeof(rdp_freerdp_context);
#else
    rdp_inst->ContextSize = sizeof(rdp_freerdp_context);
#endif
    rdp_inst->ContextNew  = (pContextNew) rdp_freerdp_context_new;
    rdp_inst->ContextFree = (pContextFree) rdp_freerdp_context_free;

    freerdp_context_new(rdp_inst);
    ((rdp_freerdp_context*) rdp_inst->context)->client = client;

    /* Load keymap into client */
    __guac_rdp_client_load_keymap(client, settings->server_layout);

#ifdef ENABLE_COMMON_SSH
    /* Connect via SSH if SFTP is enabled */
    if (settings->enable_sftp) {

        /* Abort if username is missing */
        if (settings->sftp_username == NULL)
            return 1;

        guac_client_log(client, GUAC_LOG_DEBUG,
                "Connecting via SSH for SFTP filesystem access.");

        rdp_client->sftp_user =
            guac_common_ssh_create_user(settings->sftp_username);

        /* Import private key, if given */
        if (settings->sftp_private_key != NULL) {

            guac_client_log(client, GUAC_LOG_DEBUG,
                    "Authenticating with private key.");

            /* Abort if private key cannot be read */
            if (guac_common_ssh_user_import_key(rdp_client->sftp_user,
                        settings->sftp_private_key,
                        settings->sftp_passphrase)) {
                guac_common_ssh_destroy_user(rdp_client->sftp_user);
                return 1;
            }

        }

        /* Otherwise, use specified password */
        else {

            guac_client_log(client, GUAC_LOG_DEBUG,
                    "Authenticating with password.");

            guac_common_ssh_user_set_password(rdp_client->sftp_user,
                    settings->sftp_password);

        }

        /* Attempt SSH connection */
        rdp_client->sftp_session =
            guac_common_ssh_create_secondary_session(client, settings->sftp_hostname,
                    settings->sftp_port, rdp_client->sftp_user);

        /* Clean up SSH user if connection does not succeed */
        if (rdp_client->sftp_session == NULL) {
            /* Already aborted within guac_common_ssh_create_session() */
            guac_common_ssh_destroy_user(rdp_client->sftp_user);
        }

        /* Otherwise, try to initialize SFTP */
        else {
            /* Load and expose filesystem */
            rdp_client->sftp_filesystem =
                guac_common_ssh_create_sftp_filesystem(
                        rdp_client->sftp_session, "/");

            /* Expose filesystem to connection owner */
            guac_client_for_owner(client,
                    guac_common_ssh_expose_sftp_filesystem,
                    rdp_client->sftp_filesystem);

            /* Clean up SSH session and user if SFTP connection fails */
            if (rdp_client->sftp_filesystem == NULL) {
                guac_common_ssh_destroy_session(rdp_client->sftp_session);
                guac_common_ssh_destroy_user(rdp_client->sftp_user);
            }

            else {
                guac_client_log(client, GUAC_LOG_DEBUG,
                    "SFTP connection succeeded.");
            }
        }


    }
#endif

    /* Send connection name */
    guac_protocol_send_name(client->socket, settings->hostname);

    /* Set default pointer */
    guac_common_cursor_set_pointer(rdp_client->display->cursor);

    /* Push desired settings to FreeRDP */
    guac_rdp_push_settings(settings, rdp_inst);

    /* Connect to RDP server */
    if (!freerdp_connect(rdp_inst)) {
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_UPSTREAM_ERROR,
                "Error connecting to RDP server");
        return 1;
    }

    /* Connection complete */
    rdp_client->rdp_inst = rdp_inst;
    rdpChannels* channels = rdp_inst->context->channels;

    guac_timestamp last_frame_end = guac_timestamp_current();

    /* Signal that reconnect has been completed */
    guac_rdp_disp_reconnect_complete(rdp_client->disp);

    /* Handle messages from RDP server while client is running */
    while (client->state == GUAC_CLIENT_RUNNING
            && !guac_rdp_disp_reconnect_needed(rdp_client->disp)) {

        /* Update remote display size */
        pthread_mutex_lock(&(rdp_client->rdp_lock));
        guac_rdp_disp_update_size(rdp_client->disp, settings, rdp_inst);
        pthread_mutex_unlock(&(rdp_client->rdp_lock));

        /* Wait for data and construct a reasonable frame */
        int wait_result = rdp_guac_client_wait_for_messages(client,
                GUAC_RDP_FRAME_START_TIMEOUT);
        if (wait_result > 0) {

            int processing_lag = guac_client_get_processing_lag(client);
            guac_timestamp frame_start = guac_timestamp_current();

            /* Read server messages until frame is built */
            do {

                guac_timestamp frame_end;
                int frame_remaining;

                pthread_mutex_lock(&(rdp_client->rdp_lock));

                /* Check the libfreerdp fds */
                if (!freerdp_check_fds(rdp_inst)) {
                    guac_client_log(client, GUAC_LOG_DEBUG,
                            "Error handling RDP file descriptors");
                    pthread_mutex_unlock(&(rdp_client->rdp_lock));
                    return 1;
                }

                /* Check channel fds */
                if (!freerdp_channels_check_fds(channels, rdp_inst)) {
                    guac_client_log(client, GUAC_LOG_DEBUG,
                            "Error handling RDP channel file descriptors");
                    pthread_mutex_unlock(&(rdp_client->rdp_lock));
                    return 1;
                }

                /* Check for channel events */
                wMessage* event = freerdp_channels_pop_event(channels);
                if (event) {

                    /* Handle channel events (clipboard and RAIL) */
#ifdef LEGACY_EVENT
                    if (event->event_class == CliprdrChannel_Class)
                        guac_rdp_process_cliprdr_event(client, event);
                    else if (event->event_class == RailChannel_Class)
                        guac_rdp_process_rail_event(client, event);
#else
                    if (GetMessageClass(event->id) == CliprdrChannel_Class)
                        guac_rdp_process_cliprdr_event(client, event);
                    else if (GetMessageClass(event->id) == RailChannel_Class)
                        guac_rdp_process_rail_event(client, event);
#endif

                    freerdp_event_free(event);

                }

                /* Handle RDP disconnect */
                if (freerdp_shall_disconnect(rdp_inst)) {
                    guac_client_log(client, GUAC_LOG_INFO,
                            "RDP server closed connection");
                    pthread_mutex_unlock(&(rdp_client->rdp_lock));
                    return 1;
                }

                pthread_mutex_unlock(&(rdp_client->rdp_lock));

                /* Calculate time remaining in frame */
                frame_end = guac_timestamp_current();
                frame_remaining = frame_start + GUAC_RDP_FRAME_DURATION
                                - frame_end;

                /* Calculate time that client needs to catch up */
                int time_elapsed = frame_end - last_frame_end;
                int required_wait = processing_lag - time_elapsed;

                /* Increase the duration of this frame if client is lagging */
                if (required_wait > GUAC_RDP_FRAME_TIMEOUT)
                    wait_result = rdp_guac_client_wait_for_messages(client,
                            required_wait*1000);

                /* Wait again if frame remaining */
                else if (frame_remaining > 0)
                    wait_result = rdp_guac_client_wait_for_messages(client,
                            GUAC_RDP_FRAME_TIMEOUT*1000);
                else
                    break;

            } while (wait_result > 0);
        }

        /* If an error occurred, fail */
        if (wait_result < 0)
            guac_client_abort(client, GUAC_PROTOCOL_STATUS_UPSTREAM_ERROR,
                    "Connection closed.");

        /* End of frame */
        guac_common_display_flush(rdp_client->display);
        guac_client_end_frame(client);
        guac_socket_flush(client->socket);

        /* Record end of frame */
        last_frame_end = guac_timestamp_current();

    }

    guac_client_log(client, GUAC_LOG_INFO, "Internal RDP client disconnected");

    pthread_mutex_lock(&(rdp_client->rdp_lock));

    /* Disconnect client and channels */
    freerdp_channels_close(channels, rdp_inst);
    freerdp_channels_free(channels);
    freerdp_disconnect(rdp_inst);

    /* Clean up RDP client context */
    freerdp_clrconv_free(((rdp_freerdp_context*) rdp_inst->context)->clrconv);
    cache_free(rdp_inst->context->cache);
    freerdp_context_free(rdp_inst);

    /* Clean up RDP client */
    freerdp_free(rdp_inst);
    rdp_client->rdp_inst = NULL;

    /* Clean up filesystem, if allocated */
    if (rdp_client->filesystem != NULL)
        guac_rdp_fs_free(rdp_client->filesystem);

    /* Clean up audio stream, if allocated */
    if (rdp_client->audio != NULL)
        guac_audio_stream_free(rdp_client->audio);

#ifdef ENABLE_COMMON_SSH
    /* Free SFTP filesystem, if loaded */
    if (rdp_client->sftp_filesystem)
        guac_common_ssh_destroy_sftp_filesystem(rdp_client->sftp_filesystem);

    /* Free SFTP session */
    if (rdp_client->sftp_session)
        guac_common_ssh_destroy_session(rdp_client->sftp_session);

    /* Free SFTP user */
    if (rdp_client->sftp_user)
        guac_common_ssh_destroy_user(rdp_client->sftp_user);
#endif

    /* Free SVC list */
    guac_common_list_free(rdp_client->available_svc);

    /* Free display */
    guac_common_display_free(rdp_client->display);

    pthread_mutex_unlock(&(rdp_client->rdp_lock));
    return 0;

}

void* guac_rdp_client_thread(void* data) {

    guac_client* client = (guac_client*) data;

#ifdef ENABLE_COMMON_SSH
    guac_common_ssh_init(client);
#endif

    /* Continue handling connections until error or client disconnect */
    while (client->state == GUAC_CLIENT_RUNNING) {
        if (guac_rdp_handle_connection(client))
            break;
    }

#ifdef ENABLE_COMMON_SSH
    guac_common_ssh_uninit();
#endif

    return NULL;

}

