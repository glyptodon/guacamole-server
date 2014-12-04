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
#include "guac_string.h"
#include "rdp.h"
#include "rdp_settings.h"
#include "resolution.h"

#include <freerdp/constants.h>
#include <guacamole/client.h>
#include <guacamole/user.h>

#ifdef ENABLE_WINPR
#include <winpr/wtypes.h>
#else
#include "compat/winpr-wtypes.h"
#endif

#include <stddef.h>

/* Client plugin arguments */
const char* GUAC_RDP_CLIENT_ARGS[] = {
    "hostname",
    "port",
    "domain",
    "username",
    "password",
    "width",
    "height",
    "dpi",
    "initial-program",
    "color-depth",
    "disable-audio",
    "enable-printing",
    "enable-drive",
    "drive-path",
    "console",
    "console-audio",
    "server-layout",
    "security",
    "ignore-cert",
    "disable-auth",
    "remote-app",
    "remote-app-dir",
    "remote-app-args",
    "static-channels",
    NULL
};

enum RDP_ARGS_IDX {

    IDX_HOSTNAME,
    IDX_PORT,
    IDX_DOMAIN,
    IDX_USERNAME,
    IDX_PASSWORD,
    IDX_WIDTH,
    IDX_HEIGHT,
    IDX_DPI,
    IDX_INITIAL_PROGRAM,
    IDX_COLOR_DEPTH,
    IDX_DISABLE_AUDIO,
    IDX_ENABLE_PRINTING,
    IDX_ENABLE_DRIVE,
    IDX_DRIVE_PATH,
    IDX_CONSOLE,
    IDX_CONSOLE_AUDIO,
    IDX_SERVER_LAYOUT,
    IDX_SECURITY,
    IDX_IGNORE_CERT,
    IDX_DISABLE_AUTH,
    IDX_REMOTE_APP,
    IDX_REMOTE_APP_DIR,
    IDX_REMOTE_APP_ARGS,
    IDX_STATIC_CHANNELS,

    RDP_ARGS_COUNT
};

int guac_rdp_parse_args(guac_rdp_settings* settings, guac_user* user,
        int argc, const char** argv) {

    /* Validate arguments count */
    if (argc != RDP_ARGS_COUNT)
        return 1;

    guac_client* client = user->client;

    /* Console */
    settings->console = (strcmp(argv[IDX_CONSOLE], "true") == 0);
    settings->console_audio = (strcmp(argv[IDX_CONSOLE_AUDIO], "true") == 0);

    /* Certificate and auth */
    settings->ignore_certificate = (strcmp(argv[IDX_IGNORE_CERT], "true") == 0);
    settings->disable_authentication = (strcmp(argv[IDX_DISABLE_AUTH], "true") == 0);

    /* NLA security */
    if (strcmp(argv[IDX_SECURITY], "nla") == 0) {
        guac_client_log(client, GUAC_LOG_INFO, "Security mode: NLA");
        settings->security_mode = GUAC_SECURITY_NLA;
    }

    /* TLS security */
    else if (strcmp(argv[IDX_SECURITY], "tls") == 0) {
        guac_client_log(client, GUAC_LOG_INFO, "Security mode: TLS");
        settings->security_mode = GUAC_SECURITY_TLS;
    }

    /* RDP security */
    else if (strcmp(argv[IDX_SECURITY], "rdp") == 0) {
        guac_client_log(client, GUAC_LOG_INFO, "Security mode: RDP");
        settings->security_mode = GUAC_SECURITY_RDP;
    }

    /* ANY security (allow server to choose) */
    else if (strcmp(argv[IDX_SECURITY], "any") == 0) {
        guac_client_log(client, GUAC_LOG_INFO, "Security mode: ANY");
        settings->security_mode = GUAC_SECURITY_ANY;
    }

    /* If nothing given, default to RDP */
    else {
        guac_client_log(client, GUAC_LOG_INFO, "No security mode specified. Defaulting to RDP.");
        settings->security_mode = GUAC_SECURITY_RDP;
    }

    /* Set hostname */
    settings->hostname = strdup(argv[IDX_HOSTNAME]);

    /* If port specified, use it */
    settings->port = RDP_DEFAULT_PORT;
    if (argv[IDX_PORT][0] != '\0')
        settings->port = atoi(argv[IDX_PORT]);

    guac_client_log(client, GUAC_LOG_DEBUG,
            "User resolution is %ix%i at %i DPI",
            user->info.optimal_width,
            user->info.optimal_height,
            user->info.optimal_resolution);

    /* Use suggested resolution unless overridden */
    settings->resolution = guac_rdp_suggest_resolution(user);
    if (argv[IDX_DPI][0] != '\0')
        settings->resolution = atoi(argv[IDX_DPI]);

    /* Use optimal width unless overridden */
    settings->width = user->info.optimal_width
                    * settings->resolution
                    / user->info.optimal_resolution;

    if (argv[IDX_WIDTH][0] != '\0')
        settings->width = atoi(argv[IDX_WIDTH]);

    /* Use default width if given width is invalid. */
    if (settings->width <= 0) {
        settings->width = RDP_DEFAULT_WIDTH;
        guac_client_log(client, GUAC_LOG_ERROR,
                "Invalid width: \"%s\". Using default of %i.",
                argv[IDX_WIDTH], settings->width);
    }

    /* Round width down to nearest multiple of 4 */
    settings->width = settings->width & ~0x3;

    /* Use optimal height unless overridden */
    settings->height = user->info.optimal_height
                     * settings->resolution
                     / user->info.optimal_resolution;

    if (argv[IDX_HEIGHT][0] != '\0')
        settings->height = atoi(argv[IDX_HEIGHT]);

    /* Use default height if given height is invalid. */
    if (settings->height <= 0) {
        settings->height = RDP_DEFAULT_HEIGHT;
        guac_client_log(client, GUAC_LOG_ERROR,
                "Invalid height: \"%s\". Using default of %i.",
                argv[IDX_WIDTH], settings->height);
    }

    guac_client_log(client, GUAC_LOG_DEBUG,
            "Using resolution of %ix%i at %i DPI",
            settings->width,
            settings->height,
            settings->resolution);

    /* Domain */
    settings->domain = NULL;
    if (argv[IDX_DOMAIN][0] != '\0')
        settings->domain = strdup(argv[IDX_DOMAIN]);

    /* Username */
    settings->username = NULL;
    if (argv[IDX_USERNAME][0] != '\0')
        settings->username = strdup(argv[IDX_USERNAME]);

    /* Password */
    settings->password = NULL;
    if (argv[IDX_PASSWORD][0] != '\0')
        settings->password = strdup(argv[IDX_PASSWORD]);

    /* Initial program */
    settings->initial_program = NULL;
    if (argv[IDX_INITIAL_PROGRAM][0] != '\0')
        settings->initial_program = strdup(argv[IDX_INITIAL_PROGRAM]);

    /* RemoteApp program */
    settings->remote_app = NULL;
    if (argv[IDX_REMOTE_APP][0] != '\0')
        settings->remote_app = strdup(argv[IDX_REMOTE_APP]);

    /* RemoteApp working directory */
    settings->remote_app_dir = NULL;
    if (argv[IDX_REMOTE_APP_DIR][0] != '\0')
        settings->remote_app_dir = strdup(argv[IDX_REMOTE_APP_DIR]);

    /* RemoteApp arguments */
    settings->remote_app_args = NULL;
    if (argv[IDX_REMOTE_APP_ARGS][0] != '\0')
        settings->remote_app_args = strdup(argv[IDX_REMOTE_APP_ARGS]);

    /* Static virtual channels */
    settings->svc_names = NULL;
    if (argv[IDX_STATIC_CHANNELS][0] != '\0')
        settings->svc_names = guac_split(argv[IDX_STATIC_CHANNELS], ',');

    /* Session color depth */
    settings->color_depth = RDP_DEFAULT_DEPTH;
    if (argv[IDX_COLOR_DEPTH][0] != '\0')
        settings->color_depth = atoi(argv[IDX_COLOR_DEPTH]);

    /* Use default depth if given depth is invalid. */
    if (settings->color_depth == 0) {
        settings->color_depth = RDP_DEFAULT_DEPTH;
        guac_client_log(client, GUAC_LOG_ERROR,
                "Invalid color-depth: \"%s\". Using default of %i.",
                argv[IDX_WIDTH], settings->color_depth);
    }

    /* Audio enable/disable */
    settings->audio_enabled =
        (strcmp(argv[IDX_DISABLE_AUDIO], "true") != 0);

    /* Printing enable/disable */
    settings->printing_enabled =
        (strcmp(argv[IDX_ENABLE_PRINTING], "true") == 0);

    /* Drive enable/disable */
    settings->drive_enabled =
        (strcmp(argv[IDX_ENABLE_DRIVE], "true") == 0);

    settings->drive_path = strdup(argv[IDX_DRIVE_PATH]);

    /* Pick keymap based on argument */
    settings->server_layout = NULL;
    if (argv[IDX_SERVER_LAYOUT][0] != '\0')
        settings->server_layout =
            guac_rdp_keymap_find(argv[IDX_SERVER_LAYOUT]);

    /* If no keymap requested, use default */
    if (settings->server_layout == NULL)
        settings->server_layout = guac_rdp_keymap_find(GUAC_DEFAULT_KEYMAP);

    /* Success */
    return 0;

}

int guac_rdp_get_width(freerdp* rdp) {
#ifdef LEGACY_RDPSETTINGS
    return rdp->settings->width;
#else
    return rdp->settings->DesktopWidth;
#endif
}

int guac_rdp_get_height(freerdp* rdp) {
#ifdef LEGACY_RDPSETTINGS
    return rdp->settings->height;
#else
    return rdp->settings->DesktopHeight;
#endif
}

int guac_rdp_get_depth(freerdp* rdp) {
#ifdef LEGACY_RDPSETTINGS
    return rdp->settings->color_depth;
#else
    return rdp->settings->ColorDepth;
#endif
}

void guac_rdp_push_settings(guac_rdp_settings* guac_settings, freerdp* rdp) {

    BOOL bitmap_cache;
    rdpSettings* rdp_settings = rdp->settings;

    /* Authentication */
#ifdef LEGACY_RDPSETTINGS
    rdp_settings->domain = guac_settings->domain;
    rdp_settings->username = guac_settings->username;
    rdp_settings->password = guac_settings->password;
#else
    rdp_settings->Domain = guac_settings->domain;
    rdp_settings->Username = guac_settings->username;
    rdp_settings->Password = guac_settings->password;
#endif

    /* Connection */
#ifdef LEGACY_RDPSETTINGS
    rdp_settings->hostname = guac_settings->hostname;
    rdp_settings->port = guac_settings->port;
#else
    rdp_settings->ServerHostname = guac_settings->hostname;
    rdp_settings->ServerPort = guac_settings->port;
#endif

    /* Session */
#ifdef LEGACY_RDPSETTINGS
    rdp_settings->color_depth = guac_settings->color_depth;
    rdp_settings->width = guac_settings->width;
    rdp_settings->height = guac_settings->height;
    rdp_settings->shell = guac_settings->initial_program;
    rdp_settings->kbd_layout = guac_settings->server_layout->freerdp_keyboard_layout;
#else
    rdp_settings->ColorDepth = guac_settings->color_depth;
    rdp_settings->DesktopWidth = guac_settings->width;
    rdp_settings->DesktopHeight = guac_settings->height;
    rdp_settings->AlternateShell = guac_settings->initial_program;
    rdp_settings->KeyboardLayout = guac_settings->server_layout->freerdp_keyboard_layout;
#endif

    /* Console */
#ifdef LEGACY_RDPSETTINGS
    rdp_settings->console_session = guac_settings->console;
    rdp_settings->console_audio = guac_settings->console_audio;
#else
    rdp_settings->ConsoleSession = guac_settings->console;
    rdp_settings->RemoteConsoleAudio = guac_settings->console_audio;
#endif

    /* Audio */
#ifdef LEGACY_RDPSETTINGS
#ifdef HAVE_RDPSETTINGS_AUDIOPLAYBACK
    rdp_settings->audio_playback = guac_settings->audio_enabled;
#endif
#else
#ifdef HAVE_RDPSETTINGS_AUDIOPLAYBACK
    rdp_settings->AudioPlayback = guac_settings->audio_enabled;
#endif
#endif

    /* Device redirection */
#ifdef LEGACY_RDPSETTINGS
#ifdef HAVE_RDPSETTINGS_DEVICEREDIRECTION
    rdp_settings->device_redirection =  guac_settings->audio_enabled
                                     || guac_settings->drive_enabled
                                     || guac_settings->printing_enabled;
#endif
#else
#ifdef HAVE_RDPSETTINGS_DEVICEREDIRECTION
    rdp_settings->DeviceRedirection =  guac_settings->audio_enabled
                                    || guac_settings->drive_enabled
                                    || guac_settings->printing_enabled;
#endif
#endif

    /* Security */
    switch (guac_settings->security_mode) {

        /* Standard RDP encryption */
        case GUAC_SECURITY_RDP:
#ifdef LEGACY_RDPSETTINGS
            rdp_settings->rdp_security = TRUE;
            rdp_settings->tls_security = FALSE;
            rdp_settings->nla_security = FALSE;
            rdp_settings->encryption_level = ENCRYPTION_LEVEL_CLIENT_COMPATIBLE;
            rdp_settings->encryption_method =
                  ENCRYPTION_METHOD_40BIT
                | ENCRYPTION_METHOD_128BIT
                | ENCRYPTION_METHOD_FIPS;
#else
            rdp_settings->RdpSecurity = TRUE;
            rdp_settings->TlsSecurity = FALSE;
            rdp_settings->NlaSecurity = FALSE;
            rdp_settings->EncryptionLevel = ENCRYPTION_LEVEL_CLIENT_COMPATIBLE;
            rdp_settings->EncryptionMethods =
                  ENCRYPTION_METHOD_40BIT
                | ENCRYPTION_METHOD_128BIT 
                | ENCRYPTION_METHOD_FIPS;
#endif
            break;

        /* TLS encryption */
        case GUAC_SECURITY_TLS:
#ifdef LEGACY_RDPSETTINGS
            rdp_settings->rdp_security = FALSE;
            rdp_settings->tls_security = TRUE;
            rdp_settings->nla_security = FALSE;
#else
            rdp_settings->RdpSecurity = FALSE;
            rdp_settings->TlsSecurity = TRUE;
            rdp_settings->NlaSecurity = FALSE;
#endif
            break;

        /* Network level authentication */
        case GUAC_SECURITY_NLA:
#ifdef LEGACY_RDPSETTINGS
            rdp_settings->rdp_security = FALSE;
            rdp_settings->tls_security = FALSE;
            rdp_settings->nla_security = TRUE;
#else
            rdp_settings->RdpSecurity = FALSE;
            rdp_settings->TlsSecurity = FALSE;
            rdp_settings->NlaSecurity = TRUE;
#endif
            break;

        /* All security types */
        case GUAC_SECURITY_ANY:
#ifdef LEGACY_RDPSETTINGS
            rdp_settings->rdp_security = TRUE;
            rdp_settings->tls_security = TRUE;
            rdp_settings->nla_security = TRUE;
#else
            rdp_settings->RdpSecurity = TRUE;
            rdp_settings->TlsSecurity = TRUE;
            rdp_settings->NlaSecurity = TRUE;
#endif
            break;

    }

    /* Authentication */
#ifdef LEGACY_RDPSETTINGS
    rdp_settings->authentication = !guac_settings->disable_authentication;
    rdp_settings->ignore_certificate = guac_settings->ignore_certificate;
    rdp_settings->encryption = TRUE;
#else
    rdp_settings->Authentication = !guac_settings->disable_authentication;
    rdp_settings->IgnoreCertificate = guac_settings->ignore_certificate;
    rdp_settings->DisableEncryption = FALSE;
#endif

    /* RemoteApp */
    if (guac_settings->remote_app != NULL) {
#ifdef LEGACY_RDPSETTINGS
        rdp_settings->workarea = TRUE;
        rdp_settings->remote_app = TRUE;
        rdp_settings->rail_langbar_supported = TRUE;
#else
        rdp_settings->Workarea = TRUE;
        rdp_settings->RemoteApplicationMode = TRUE;
        rdp_settings->RemoteAppLanguageBarSupported = TRUE;
        rdp_settings->RemoteApplicationProgram = guac_settings->remote_app;
        rdp_settings->ShellWorkingDirectory = guac_settings->remote_app_dir;
        rdp_settings->RemoteApplicationCmdLine = guac_settings->remote_app_args;
#endif
    }

    /* Order support */
#ifdef LEGACY_RDPSETTINGS
    bitmap_cache = rdp_settings->bitmap_cache;
    rdp_settings->os_major_type = OSMAJORTYPE_UNSPECIFIED;
    rdp_settings->os_minor_type = OSMINORTYPE_UNSPECIFIED;
    rdp_settings->desktop_resize = TRUE;
    rdp_settings->order_support[NEG_DSTBLT_INDEX] = TRUE;
    rdp_settings->order_support[NEG_PATBLT_INDEX] = FALSE; /* PATBLT not yet supported */
    rdp_settings->order_support[NEG_SCRBLT_INDEX] = TRUE;
    rdp_settings->order_support[NEG_OPAQUE_RECT_INDEX] = TRUE;
    rdp_settings->order_support[NEG_DRAWNINEGRID_INDEX] = FALSE;
    rdp_settings->order_support[NEG_MULTIDSTBLT_INDEX] = FALSE;
    rdp_settings->order_support[NEG_MULTIPATBLT_INDEX] = FALSE;
    rdp_settings->order_support[NEG_MULTISCRBLT_INDEX] = FALSE;
    rdp_settings->order_support[NEG_MULTIOPAQUERECT_INDEX] = FALSE;
    rdp_settings->order_support[NEG_MULTI_DRAWNINEGRID_INDEX] = FALSE;
    rdp_settings->order_support[NEG_LINETO_INDEX] = FALSE;
    rdp_settings->order_support[NEG_POLYLINE_INDEX] = FALSE;
    rdp_settings->order_support[NEG_MEMBLT_INDEX] = bitmap_cache;
    rdp_settings->order_support[NEG_MEM3BLT_INDEX] = FALSE;
    rdp_settings->order_support[NEG_MEMBLT_V2_INDEX] = bitmap_cache;
    rdp_settings->order_support[NEG_MEM3BLT_V2_INDEX] = FALSE;
    rdp_settings->order_support[NEG_SAVEBITMAP_INDEX] = FALSE;
    rdp_settings->order_support[NEG_GLYPH_INDEX_INDEX] = TRUE;
    rdp_settings->order_support[NEG_FAST_INDEX_INDEX] = TRUE;
    rdp_settings->order_support[NEG_FAST_GLYPH_INDEX] = TRUE;
    rdp_settings->order_support[NEG_POLYGON_SC_INDEX] = FALSE;
    rdp_settings->order_support[NEG_POLYGON_CB_INDEX] = FALSE;
    rdp_settings->order_support[NEG_ELLIPSE_SC_INDEX] = FALSE;
    rdp_settings->order_support[NEG_ELLIPSE_CB_INDEX] = FALSE;
#else
    bitmap_cache = rdp_settings->BitmapCacheEnabled;
    rdp_settings->OsMajorType = OSMAJORTYPE_UNSPECIFIED;
    rdp_settings->OsMinorType = OSMINORTYPE_UNSPECIFIED;
    rdp_settings->DesktopResize = TRUE;
    rdp_settings->OrderSupport[NEG_DSTBLT_INDEX] = TRUE;
    rdp_settings->OrderSupport[NEG_PATBLT_INDEX] = FALSE; /* PATBLT not yet supported */
    rdp_settings->OrderSupport[NEG_SCRBLT_INDEX] = TRUE;
    rdp_settings->OrderSupport[NEG_OPAQUE_RECT_INDEX] = TRUE;
    rdp_settings->OrderSupport[NEG_DRAWNINEGRID_INDEX] = FALSE;
    rdp_settings->OrderSupport[NEG_MULTIDSTBLT_INDEX] = FALSE;
    rdp_settings->OrderSupport[NEG_MULTIPATBLT_INDEX] = FALSE;
    rdp_settings->OrderSupport[NEG_MULTISCRBLT_INDEX] = FALSE;
    rdp_settings->OrderSupport[NEG_MULTIOPAQUERECT_INDEX] = FALSE;
    rdp_settings->OrderSupport[NEG_MULTI_DRAWNINEGRID_INDEX] = FALSE;
    rdp_settings->OrderSupport[NEG_LINETO_INDEX] = FALSE;
    rdp_settings->OrderSupport[NEG_POLYLINE_INDEX] = FALSE;
    rdp_settings->OrderSupport[NEG_MEMBLT_INDEX] = bitmap_cache;
    rdp_settings->OrderSupport[NEG_MEM3BLT_INDEX] = FALSE;
    rdp_settings->OrderSupport[NEG_MEMBLT_V2_INDEX] = bitmap_cache;
    rdp_settings->OrderSupport[NEG_MEM3BLT_V2_INDEX] = FALSE;
    rdp_settings->OrderSupport[NEG_SAVEBITMAP_INDEX] = FALSE;
    rdp_settings->OrderSupport[NEG_GLYPH_INDEX_INDEX] = TRUE;
    rdp_settings->OrderSupport[NEG_FAST_INDEX_INDEX] = TRUE;
    rdp_settings->OrderSupport[NEG_FAST_GLYPH_INDEX] = TRUE;
    rdp_settings->OrderSupport[NEG_POLYGON_SC_INDEX] = FALSE;
    rdp_settings->OrderSupport[NEG_POLYGON_CB_INDEX] = FALSE;
    rdp_settings->OrderSupport[NEG_ELLIPSE_SC_INDEX] = FALSE;
    rdp_settings->OrderSupport[NEG_ELLIPSE_CB_INDEX] = FALSE;
#endif

}

