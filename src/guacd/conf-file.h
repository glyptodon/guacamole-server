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

#ifndef _GUACD_CONF_FILE_H
#define _GUACD_CONF_FILE_H

#include "config.h"

#include <guacamole/client.h>

/**
 * TLS-PSK object.
 */
typedef struct tls_psk {
    char* identity;
    char* key;
    unsigned int key_len;
    struct tls_psk* next;
} tls_psk;

/**
 * The contents of a guacd configuration file.
 */
typedef struct guacd_config {

    /**
     * The host to bind on.
     */
    char* bind_host;

    /**
     * The port to bind on.
     */
    char* bind_port;

    /**
     * The file to write the PID in, if any.
     */
    char* pidfile;

    /**
     * Whether guacd should run in the foreground.
     */
    int foreground;

#ifdef ENABLE_SSL
    /**
     * SSL certificate file.
     */
    char* cert_file;

    /**
     * SSL private key file.
     */
    char* key_file;

    /**
     * PSK list
     */
    tls_psk* psk_list;
#endif

    /**
     * The maximum log level to be logged by guacd.
     */
    guac_client_log_level max_log_level;

} guacd_config;

/**
 * Reads the given file descriptor, parsing its contents into the guacd_config.
 * On success, zero is returned. If parsing fails, non-zero is returned, and an
 * error message is printed to stderr.
 */
int guacd_conf_parse_file(guacd_config* conf, int fd);

/**
 * Loads the configuration from any of several default locations, if found. If
 * parsing fails, NULL is returned, and an error message is printed to stderr.
 */
guacd_config* guacd_conf_load();

/**
 * Parse a string of the form "identity:pre-shared-key" and add a corresponding
 * object to the list of PSK peers.
 */
int add_psk_to_list(tls_psk** list, const char* psk);
#endif

