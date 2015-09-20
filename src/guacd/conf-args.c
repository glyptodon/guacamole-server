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

#include "conf-args.h"
#include "conf-file.h"
#include "conf-parse.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int guacd_conf_parse_args(guacd_config* config, int argc, char** argv) {

    /* Parse arguments */
    int opt;
    while ((opt = getopt(argc, argv, "l:b:p:L:C:K:P:f")) != -1) {

        /* -l: Bind port */
        if (opt == 'l') {
            free(config->bind_port);
            config->bind_port = strdup(optarg);
        }

        /* -b: Bind host */
        else if (opt == 'b') {
            free(config->bind_host);
            config->bind_host = strdup(optarg);
        }

        /* -f: Run in foreground */
        else if (opt == 'f') {
            config->foreground = 1;
        }

        /* -p: PID file */
        else if (opt == 'p') {
            free(config->pidfile);
            config->pidfile = strdup(optarg);
        }

        /* -L: Log level */
        else if (opt == 'L') {

            /* Validate and parse log level */
            int level = guacd_parse_log_level(optarg);
            if (level == -1) {
                fprintf(stderr, "Invalid log level. Valid levels are: \"debug\", \"info\", \"warning\", and \"error\".\n");
                return 1;
            }

            config->max_log_level = level;

        }

#ifdef ENABLE_SSL
        /* -C SSL certificate */
        else if (opt == 'C') {
            free(config->cert_file);
            config->cert_file = strdup(optarg);
        }

        /* -K SSL key */
        else if (opt == 'K') {
            free(config->key_file);
            config->key_file = strdup(optarg);
        }

        /* -P TLS-PSK peer*/
        else if (opt == 'P') {
            if (add_psk_to_list(&config->psk_list, optarg)) {
                fprintf(stderr, "Failed to add PSK peer to list.\n");
                return 1;
            }
        }
#else
        else if (opt == 'C' || opt == 'K' || opt == 'P') {
            fprintf(stderr,
                    "This guacd does not have SSL/TLS support compiled in.\n\n"

                    "If you wish to enable support for the -%c option, please install libssl and\n"
                    "recompile guacd.\n",
                    opt);
            return 1;
        }
#endif
        else {

            fprintf(stderr, "USAGE: %s"
                    " [-l LISTENPORT]"
                    " [-b LISTENADDRESS]"
                    " [-p PIDFILE]"
                    " [-L LEVEL]"
#ifdef ENABLE_SSL
                    " [-C CERTIFICATE_FILE]"
                    " [-K PEM_FILE]"
                    " [-P TLS_PSK_PEER]"
#endif
                    " [-f]\n", argv[0]);

            return 1;
        }
    }

    /* Success */
    return 0;

}

