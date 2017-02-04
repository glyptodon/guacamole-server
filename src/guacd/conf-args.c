/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
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

