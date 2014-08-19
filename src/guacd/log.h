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


#ifndef __GUACD_LOG_H
#define __GUACD_LOG_H

#include "config.h"

#include <guacamole/client.h>

/**
 * The string to prepend to all log messages.
 */
extern char log_prefix[64];

/**
 * Prints an informational message to guacd's logs. This function takes a
 * format and va_list, similar to vprintf.
 */
void vguacd_log_info(const char* format, va_list args);

/**
 * Prints an error message to guacd's logs. This function takes a format and
 * va_list, similar to vprintf.
 */
void vguacd_log_error(const char* format, va_list args);

/**
 * Prints an informational message to guacd's logs. This function accepts
 * parameters identically to printf.
 */
void guacd_log_info(const char* format, ...);

/**
 * Prints an error message to guacd's logs. This function accepts parameters
 * identically to printf.
 */
void guacd_log_error(const char* format, ...);

/**
 * Prints an error message to guacd's logs. This function accepts parameters
 * identically to printf.
 */
void guacd_log_error(const char* format, ...);

/**
 * Prints an error message to guacd's logs, automatically including any
 * information present in guac_error. This function accepts parameters
 * identically to printf.
 */
void guacd_log_guac_error(const char* message);

/**
 * Prints an error message using the logging facilities of the given client,
 * automatically including any information present in guac_error. This function
 * accepts parameters identically to printf.
 */
void guacd_client_log_guac_error(guac_client* client, const char* message);

/**
 * Handler for logging informational messages at the guac_client level.
 */
void guacd_client_log_info(guac_client* client, const char* format, va_list args);

/**
 * Handler for logging error messages at the guac_client level.
 */
void guacd_client_log_error(guac_client* client, const char* format, va_list args);

#endif

