/*
 * Copyright (C) 2016 Glyptodon, Inc.
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
#include "display.h"
#include "log.h"

#include <guacamole/client.h>

#include <stdlib.h>

int guacenc_handle_cfill(guacenc_display* display, int argc, char** argv) {
 
    /* Verify argument count */
    if (argc < 6) {
        guacenc_log(GUAC_LOG_DEBUG, "\"cfill\" instruction incomplete");
        return 1;
    }

    /* Parse arguments */
    int mask = atoi(argv[0]);
    int index = atoi(argv[1]);
    int r = atoi(argv[2]);
    int g = atoi(argv[3]);
    int b = atoi(argv[4]);
    int a = atoi(argv[5]);

    /* STUB */
    guacenc_log(GUAC_LOG_DEBUG, "cfill: mask=0x%X layer=%i "
            "rgba(%i, %i, %i, %i)", mask, index, r, g, b, a);
    return 0;

}
