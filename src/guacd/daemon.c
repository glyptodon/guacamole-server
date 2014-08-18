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

#include "client-map.h"
#include "log.h"
#include "user.h"

#include <guacamole/client.h>
#include <guacamole/error.h>
#include <guacamole/instruction.h>
#include <guacamole/plugin.h>
#include <guacamole/protocol.h>
#include <guacamole/socket.h>
#include <guacamole/user.h>

#ifdef ENABLE_SSL
#include <openssl/ssl.h>
#include "socket-ssl.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define GUACD_DEV_NULL "/dev/null"
#define GUACD_ROOT     "/"

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

static guac_client* guacd_get_client(guacd_client_map* map, const char* identifier) {

    /*
     * TODO: If connection ID given, client should be retrieved instead of created.
     */

    guacd_log_info("Creating new client for protocol \"%s\"", identifier);

    /* Create client */
    guac_client* client = guac_client_alloc();
    if (client == NULL)
        return NULL;

    /* Init logging */
    client->log_info_handler = guacd_client_log_info;
    client->log_error_handler = guacd_client_log_error;

    /* Init client for selected protocol */
    if (guac_client_plugin_init_client(identifier, client)) {
        guacd_log_guac_error("Protocol initialization failed");
        guac_client_free(client);
        return NULL;
    }

    /* Add client to global storage */
    if (guacd_client_map_add(map, client)) {
        guacd_log_error("Internal failure adding client \"%s\".", client->connection_id);
        guac_client_free(client);
        return NULL;
    }

    return client;

}


/**
 * Creates a new guac_client for the connection on the given socket, adding
 * it to the client map based on its ID.
 */
static int guacd_handle_connection(guacd_client_map* map, guac_socket* socket) {

    /* Reset guac_error */
    guac_error = GUAC_STATUS_SUCCESS;
    guac_error_message = NULL;

    /* Get protocol from select instruction */
    guac_instruction* select = guac_instruction_expect(socket, GUACD_USEC_TIMEOUT, "select");
    if (select == NULL) {
        guacd_log_guac_error("Error reading \"select\"");
        return 1;
    }

    /* Validate args to select */
    if (select->argc != 1) {
        guacd_log_error("Bad number of arguments to \"select\" (%i)", select->argc);
        return 1;
    }

    int owner;

    /* Get client for connection */
    guac_client* client = guacd_get_client(map, select->argv[0]);
    guac_instruction_free(select);

    if (client == NULL)
        return 1;

    owner = 1;

    /* Log connection ID */
    guacd_log_info("Connection ID is \"%s\"", client->connection_id);

    /* Proceed with handshake and user I/O */
    int retval = guacd_handle_user(client, socket, owner);

    /* Clean up client if no more users */
    if (client->connected_users == 0) {

        guacd_log_info("Last user of connection \"%s\" disconnected", client->connection_id);

        /* Remove client */
        if (guacd_client_map_remove(map, client->connection_id))
            guacd_log_error("Internal failure removing client \"%s\". Client record will never be freed.",
                    client->connection_id);

        guac_client_free(client);
    }

    return retval;

}

int redirect_fd(int fd, int flags) {

    /* Attempt to open bit bucket */
    int new_fd = open(GUACD_DEV_NULL, flags);
    if (new_fd < 0)
        return 1;

    /* If descriptor is different, redirect old to new and close new */
    if (new_fd != fd) {
        dup2(new_fd, fd);
        close(new_fd);
    }

    return 0;

}

int daemonize() {

    pid_t pid;

    /* Fork once to ensure we aren't the process group leader */
    pid = fork();
    if (pid < 0) {
        guacd_log_error("Could not fork() parent: %s", strerror(errno));
        return 1;
    }

    /* Exit if we are the parent */
    if (pid > 0) {
        guacd_log_info("Exiting and passing control to PID %i", pid);
        _exit(0);
    }

    /* Start a new session (if not already group leader) */
    setsid();

    /* Fork again so the session group leader exits */
    pid = fork();
    if (pid < 0) {
        guacd_log_error("Could not fork() group leader: %s", strerror(errno));
        return 1;
    }

    /* Exit if we are the parent */
    if (pid > 0) {
        guacd_log_info("Exiting and passing control to PID %i", pid);
        _exit(0);
    }

    /* Change to root directory */
    if (chdir(GUACD_ROOT) < 0) {
        guacd_log_error(
                "Unable to change working directory to "
                GUACD_ROOT);
        return 1;
    }

    /* Reopen the 3 stdxxx to /dev/null */

    if (redirect_fd(STDIN_FILENO, O_RDONLY)
    || redirect_fd(STDOUT_FILENO, O_WRONLY)
    || redirect_fd(STDERR_FILENO, O_WRONLY)) {

        guacd_log_error(
                "Unable to redirect standard file descriptors to "
                GUACD_DEV_NULL);
        return 1;
    }

    /* Success */
    return 0;

}

int main(int argc, char* argv[]) {

    /* Server */
    int socket_fd;
    struct addrinfo* addresses;
    struct addrinfo* current_address;
    char bound_address[1024];
    char bound_port[64];
    int opt_on = 1;

    struct addrinfo hints = {
        .ai_family   = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP
    };

    /* Client */
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;
    int connected_socket_fd;

    /* Arguments */
    char* listen_address = NULL; /* Default address of INADDR_ANY */
    char* listen_port = "4822";  /* Default port */
    char* pidfile = NULL;
    int opt;
    int foreground = 0;

#ifdef ENABLE_SSL
    /* SSL */
    char* cert_file = NULL;
    char* key_file = NULL;
    SSL_CTX* ssl_context = NULL;
#endif

    guacd_client_map* map = guacd_client_map_alloc();

    /* General */
    int retval;

    /* Parse arguments */
    while ((opt = getopt(argc, argv, "l:b:p:C:K:f")) != -1) {
        if (opt == 'l') {
            listen_port = strdup(optarg);
        }
        else if (opt == 'b') {
            listen_address = strdup(optarg);
        }
        else if (opt == 'f') {
            foreground = 1;
        }
        else if (opt == 'p') {
            pidfile = strdup(optarg);
        }
#ifdef ENABLE_SSL
        else if (opt == 'C') {
            cert_file = strdup(optarg);
        }
        else if (opt == 'K') {
            key_file = strdup(optarg);
        }
#else
        else if (opt == 'C' || opt == 'K') {
            fprintf(stderr,
                    "This guacd does not have SSL/TLS support compiled in.\n\n"

                    "If you wish to enable support for the -%c option, please install libssl and\n"
                    "recompile guacd.\n",
                    opt);
            exit(EXIT_FAILURE);
        }
#endif
        else {

            fprintf(stderr, "USAGE: %s"
                    " [-l LISTENPORT]"
                    " [-b LISTENADDRESS]"
                    " [-p PIDFILE]"
#ifdef ENABLE_SSL
                    " [-C CERTIFICATE_FILE]"
                    " [-K PEM_FILE]"
#endif
                    " [-f]\n", argv[0]);

            exit(EXIT_FAILURE);
        }
    }

    /* Set up logging prefix */
    strncpy(log_prefix, basename(argv[0]), sizeof(log_prefix));

    /* Open log as early as we can */
    openlog(NULL, LOG_PID, LOG_DAEMON);

    /* Log start */
    guacd_log_info("Guacamole proxy daemon (guacd) version " VERSION);

    /* Get addresses for binding */
    if ((retval = getaddrinfo(listen_address, listen_port,
                    &hints, &addresses))) {

        guacd_log_error("Error parsing given address or port: %s",
                gai_strerror(retval));
        exit(EXIT_FAILURE);

    }

    /* Get socket */
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        guacd_log_error("Error opening socket: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Allow socket reuse */
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR,
                (void*) &opt_on, sizeof(opt_on))) {
        guacd_log_info("Unable to set socket options for reuse: %s",
                strerror(errno));
    }

    /* Attempt binding of each address until success */
    current_address = addresses;
    while (current_address != NULL) {

        int retval;

        /* Resolve hostname */
        if ((retval = getnameinfo(current_address->ai_addr,
                current_address->ai_addrlen,
                bound_address, sizeof(bound_address),
                bound_port, sizeof(bound_port),
                NI_NUMERICHOST | NI_NUMERICSERV)))
            guacd_log_error("Unable to resolve host: %s",
                    gai_strerror(retval));

        /* Attempt to bind socket to address */
        if (bind(socket_fd,
                    current_address->ai_addr,
                    current_address->ai_addrlen) == 0) {

            guacd_log_info("Successfully bound socket to "
                    "host %s, port %s", bound_address, bound_port);

            /* Done if successful bind */
            break;

        }

        /* Otherwise log information regarding bind failure */
        else
            guacd_log_info("Unable to bind socket to "
                    "host %s, port %s: %s",
                    bound_address, bound_port, strerror(errno));

        current_address = current_address->ai_next;

    }

    /* If unable to bind to anything, fail */
    if (current_address == NULL) {
        guacd_log_error("Unable to bind socket to any addresses.");
        exit(EXIT_FAILURE);
    }

#ifdef ENABLE_SSL
    /* Init SSL if enabled */
    if (key_file != NULL || cert_file != NULL) {

        /* Init SSL */
        guacd_log_info("Communication will require SSL/TLS.");
        SSL_library_init();
        SSL_load_error_strings();
        ssl_context = SSL_CTX_new(SSLv23_server_method());

        /* Load key */
        if (key_file != NULL) {
            guacd_log_info("Using PEM keyfile %s", key_file);
            if (!SSL_CTX_use_PrivateKey_file(ssl_context, key_file, SSL_FILETYPE_PEM)) {
                guacd_log_error("Unable to load keyfile.");
                exit(EXIT_FAILURE);
            }
        }
        else
            guacd_log_info("No PEM keyfile given - SSL/TLS may not work.");

        /* Load cert file if specified */
        if (cert_file != NULL) {
            guacd_log_info("Using certificate file %s", cert_file);
            if (!SSL_CTX_use_certificate_chain_file(ssl_context, cert_file)) {
                guacd_log_error("Unable to load certificate.");
                exit(EXIT_FAILURE);
            }
        }
        else
            guacd_log_info("No certificate file given - SSL/TLS may not work.");

    }
#endif

    /* Daemonize if requested */
    if (!foreground) {

        /* Attempt to daemonize process */
        if (daemonize()) {
            guacd_log_error("Could not become a daemon.");
            exit(EXIT_FAILURE);
        }

    }

    /* Write PID file if requested */
    if (pidfile != NULL) {

        /* Attempt to open pidfile and write PID */
        FILE* pidf = fopen(pidfile, "w");
        if (pidf) {
            fprintf(pidf, "%d\n", getpid());
            fclose(pidf);
        }
        
        /* Fail if could not write PID file*/
        else {
            guacd_log_error("Could not write PID file: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

    }

    /* Ignore SIGPIPE */
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        guacd_log_info("Could not set handler for SIGPIPE to ignore. "
                "SIGPIPE may cause termination of the daemon.");
    }

    /* Ignore SIGCHLD (force automatic removal of children) */
    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
        guacd_log_info("Could not set handler for SIGCHLD to ignore. "
                "Child processes may pile up in the process table.");
    }

    /* Log listening status */
    guacd_log_info("Listening on host %s, port %s", bound_address, bound_port);

    /* Free addresses */
    freeaddrinfo(addresses);

    /* Daemon loop */
    for (;;) {

        pid_t child_pid;

        /* Listen for connections */
        if (listen(socket_fd, 5) < 0) {
            guacd_log_error("Could not listen on socket: %s", strerror(errno));
            return 3;
        }

        /* Accept connection */
        client_addr_len = sizeof(client_addr);
        connected_socket_fd = accept(socket_fd,
                (struct sockaddr*) &client_addr, &client_addr_len);

        if (connected_socket_fd < 0) {
            guacd_log_error("Could not accept client connection: %s",
                    strerror(errno));
            return 3;
        }

        /* 
         * Once connection is accepted, send child into background.
         *
         * Note that we prefer fork() over threads for connection-handling
         * processes as they give each connection its own memory area, and
         * isolate the main daemon and other connections from errors in any
         * particular client plugin.
         */

        child_pid = fork();

        /* If error, log */
        if (child_pid == -1)
            guacd_log_error("Error forking child process: %s", strerror(errno));

        /* If child, start client, and exit when finished */
        else if (child_pid == 0) {

            guac_socket* socket;

#ifdef ENABLE_SSL

            /* If SSL chosen, use it */
            if (ssl_context != NULL) {
                socket = guac_socket_open_secure(ssl_context, connected_socket_fd);
                if (socket == NULL) {
                    guacd_log_guac_error("Error opening secure connection");
                    return 0;
                }
            }
            else
                socket = guac_socket_open(connected_socket_fd);
#else
            /* Open guac_socket */
            socket = guac_socket_open(connected_socket_fd);
#endif

            guacd_handle_connection(map, socket);

            guac_socket_free(socket);
            close(connected_socket_fd);
            return 0;
        }

        /* If parent, close reference to child's descriptor */
        else if (close(connected_socket_fd) < 0) {
            guacd_log_error("Error closing daemon reference to "
                    "child descriptor: %s", strerror(errno));
        }

    }

    /* Close socket */
    if (close(socket_fd) < 0) {
        guacd_log_error("Could not close socket: %s", strerror(errno));
        return 3;
    }

    return 0;

}

