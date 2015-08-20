/*
 * Copyright (C) 2015 Glyptodon LLC
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

#include "encode-webp.h"
#include "error.h"
#include "palette.h"
#include "protocol.h"
#include "stream.h"

#include <cairo/cairo.h>
#include <webp/encode.h>

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct guac_webp_stream_writer {

    /**
     * The socket over which all WebP blobs will be written.
     */
    guac_socket* socket;

    /**
     * The Guacamole stream to associate with each WebP blob.
     */
    guac_stream* stream;

    /**
     * Buffer of pending WebP data.
     */
    char buffer[6048];

    /**
     * The number of bytes currently stored in the buffer.
     */
    int buffer_size;

} guac_webp_stream_writer;

/**
 * Writes the contents of the WebP stream writer as a blob to its associated
 * socket.
 *
 * @param writer
 *     The writer structure to flush.
 */
static void guac_webp_flush_data(guac_webp_stream_writer* writer) {

    /* Send blob */
    guac_protocol_send_blob(writer->socket, writer->stream,
            writer->buffer, writer->buffer_size);

    /* Clear buffer */
    writer->buffer_size = 0;

}

/**
 * Configures the given stream writer object to use the given Guacamole stream
 * object for WebP output.
 *
 * @param writer
 *     The Guacamole WebP stream writer structure to configure.
 *
 * @param socket
 *     The Guacamole socket to use when sending blob instructions.
 *
 * @param stream
 *     The stream over which WebP-encoded blobs of image data should be sent.
 */
static void guac_webp_stream_writer_init(guac_webp_stream_writer* writer,
        guac_socket* socket, guac_stream* stream) {

    writer->buffer_size = 0;

    /* Store Guacamole-specific objects */
    writer->socket = socket;
    writer->stream = stream;

}

/**
 * WebP output function which appends the given WebP data to the internal
 * buffer of the Guacamole stream writer structure, automatically flushing the
 * writer as necessary.
 *
 * @param data
 *     The segment of data to write.
 *
 * @param data_size
 *     The size of segment of data to write.
 *
 * @param picture
 *     The WebP picture associated with this write operation. Provides access to
 *     picture->custom_ptr which contains the Guacamole stream writer structure.
 *
 * @return
 *     True if writing was successful, false on failure.
 */
static int guac_webp_stream_write(const uint8_t* data, size_t data_size,
        const WebPPicture* picture) {

    guac_webp_stream_writer* const writer = (guac_webp_stream_writer*)picture->custom_ptr;
    if (writer == NULL) {
        return 0;
    }

    const unsigned char* current = data;
    int length = data_size;

    /* Append all data given */
    while (length > 0) {

        /* Calculate space remaining */
        int remaining = sizeof(writer->buffer) - writer->buffer_size;

        /* If no space remains, flush buffer to make room */
        if (remaining == 0) {
            guac_webp_flush_data(writer);
            remaining = sizeof(writer->buffer);
        }

        /* Calculate size of next block of data to append */
        int block_size = remaining;
        if (block_size > length)
            block_size = length;

        /* Append block */
        memcpy(writer->buffer + writer->buffer_size,
               current, block_size);

        /* Next block */
        current += block_size;
        writer->buffer_size += block_size;
        length -= block_size;

    }

    return 1;
}

int guac_webp_write(guac_socket* socket, guac_stream* stream,
        cairo_surface_t* surface, int quality) {

    int width = cairo_image_surface_get_width(surface);
    int height = cairo_image_surface_get_height(surface);
    int stride = cairo_image_surface_get_stride(surface);
    cairo_format_t format = cairo_image_surface_get_format(surface);

    if (format != CAIRO_FORMAT_RGB24 && format != CAIRO_FORMAT_ARGB32) {
        guac_error = GUAC_STATUS_INTERNAL_ERROR;
        guac_error_message = "Invalid Cairo image format. Unable to create WebP.";
        return -1;
    }

    /* Flush pending operations to surface */
    cairo_surface_flush(surface);

    unsigned char* data = cairo_image_surface_get_data(surface);

    /* Configure WebP compression bits */
    WebPConfig config;
    if (!WebPConfigPreset(&config, WEBP_HINT_DEFAULT, quality)) return -1;  // version error
    /* Add additional tuning: */
    config.lossless = 0;
    config.quality = quality;
    config.thread_level = 1; /* Multi threaded */
    config.method = 2; /* Compression method (0=fast/larger, 6=slow/smaller) */

    WebPValidateConfig(&config);  /* verify parameter ranges */

    /* Set up WebP picture and writer */
    guac_webp_stream_writer writer;
    WebPPicture picture;
    WebPPictureInit(&picture);
    picture.use_argb = 1;
    picture.width = width;
    picture.height = height;
    WebPPictureAlloc(&picture);
    picture.writer = guac_webp_stream_write;
    picture.custom_ptr = &writer;
    guac_webp_stream_writer_init(&writer, socket, stream);

    /* Copy image data into WebP picture */
    for (int y = 0; y < height; ++y) {

        unsigned char *inptr = data + (stride * y);
        unsigned char *outptr = (unsigned char*)&picture.argb[y * picture.argb_stride];

        for (int x = 0; x < width; ++x) {

            outptr[0] = *inptr++; // B
            outptr[1] = *inptr++; // G
            outptr[2] = *inptr++; // R

            if (format != CAIRO_FORMAT_ARGB32) {
                outptr[3] = 0xFF;
            }
            else {
                outptr[3] = *inptr; // A
            }

            inptr++;
            outptr += 4;

        }
    }

    /* Encode image */
    WebPEncode(&config, &picture);

    /* Free picture */
    WebPPictureFree(&picture);

    /* Ensure all data is written */
    guac_webp_flush_data(&writer);

    return 0;

}
