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
#include "guac_rect.h"

void guac_common_rect_init(guac_common_rect* rect, int x, int y, int width, int height) {
    rect->x      = x;
    rect->y      = y;
    rect->width  = width;
    rect->height = height;
}

void guac_common_rect_extend(guac_common_rect* rect, const guac_common_rect* min) {

    /* Calculate extents of existing dirty rect */
    int left   = rect->x;
    int top    = rect->y;
    int right  = left + rect->width;
    int bottom = top  + rect->height;

    /* Calculate missing extents of given new rect */
    int min_left   = min->x;
    int min_top    = min->y;
    int min_right  = min_left + min->width;
    int min_bottom = min_top  + min->height;

    /* Update minimums */
    if (min_left   < left)   left   = min_left;
    if (min_top    < top)    top    = min_top;
    if (min_right  > right)  right  = min_right;
    if (min_bottom > bottom) bottom = min_bottom;

    /* Commit rect */
    guac_common_rect_init(rect, left, top, right - left, bottom - top);

}

void guac_common_rect_constrain(guac_common_rect* rect, const guac_common_rect* max) {

    /* Calculate extents of existing dirty rect */
    int left   = rect->x;
    int top    = rect->y;
    int right  = left + rect->width;
    int bottom = top  + rect->height;

    /* Calculate missing extents of given new rect */
    int max_left   = max->x;
    int max_top    = max->y;
    int max_right  = max_left + max->width;
    int max_bottom = max_top  + max->height;

    /* Update maximums */
    if (max_left   > left)   left   = max_left;
    if (max_top    > top)    top    = max_top;
    if (max_right  < right)  right  = max_right;
    if (max_bottom < bottom) bottom = max_bottom;

    /* Commit rect */
    guac_common_rect_init(rect, left, top, right - left, bottom - top);

}

int guac_common_rect_adjust(int divisor, guac_common_rect* rect,
                            const guac_common_rect* max) {

    /* Invalid divisor received */
    if (divisor < 0)
        return -1;

    /* Calculate how much the rectangle must be adjusted to not produce a
     * remainder if divided by the divisor. */
    int dw = divisor - rect->width % divisor;
    int dh = divisor - rect->height % divisor;

    int dx = dw / 2;
    int dy = dh / 2;

    /* Set initial extents of adjusted rectangle. */
    int top = rect->y - dy;
    int left = rect->x - dx;
    int bottom = top + rect->height + dh;
    int right = left + rect->width + dw;

    /* The max rectangle (clipping rectangle) */
    int max_left   = max->x;
    int max_top    = max->y;
    int max_right  = max_left + max->width;
    int max_bottom = max_top  + max->height;

    /* If the adjusted rectangle has sides beyond the max rectangle, or is larger
     * in any direction; shift or adjust the rectangle while trying to
     * maintain it's quotient without a remainder.
     */

    /* Adjust left/right */
    if (right > max_right) {

        /* shift to left */
        dw = right - max_right;
        right -= dw;
        left -= dw;

        /* clamp left if too far */
        if (left < max_left) {
            left = max_left;
        }
    }
    else if (left < max_left) {

        /* shift to right */
        dw = max_left - left;
        left += dw;
        right += dw;

        /* clamp right if too far */
        if (right > max_right) {
            right = max_right;
        }
    }

    /* Adjust top/bottom */
    if (bottom > max_bottom) {

        /* shift up */
        dh = bottom - max_bottom;
        bottom -= dh;
        top -= dh;

        /* clamp top if too far */
        if (top < max_top) {
            top = max_top;
        }
    }
    else if (top < max_top) {

        /* shift down */
        dh = max_top - top;
        top += dh;
        bottom += dh;

        /* clamp bottom if too far */
        if (bottom > max_bottom) {
            bottom = max_bottom;
        }
    }

    /* Commit rect */
    guac_common_rect_init(rect, left, top, right - left, bottom - top);

    return 0;

}

int guac_common_rect_intersects(guac_common_rect* rect, const guac_common_rect* min) {

    /* Empty (no intersection) */
    if (min->x + min->width < rect->x || rect->x + rect->width < min->x ||
        min->y + min->height < rect->y || rect->y + rect->height < min->y) {
        return -1;
    }
    /* Complete */
    else if (min->x <= rect->x && (min->x + min->width) >= (rect->x + rect->width) &&
        min->y <= rect->y && (min->y + min->height) >= (rect->y + rect->height)) {
        return 1;
    }
    /* Partial intersection */
    else {
        return 0;
    }
}

int guac_common_rect_clip_and_split(guac_common_rect* rect,
        const guac_common_rect* min, guac_common_rect* split_rect) {

    /* Only continue if the rectangles partially intersects */
    if (guac_common_rect_intersects(rect, min) != 0)
        return 0;

    int top, left, bottom, right;

    /* Clip and split top */
    if (rect->y < min->y) {
        top = rect->y;
        left = rect->x;
        bottom = min->y;
        right = rect->x + rect->width;
        guac_common_rect_init(split_rect, left, top, right - left, bottom - top);

        /* Re-initialize original rect */
        top = min->y;
        bottom = rect->y + rect->height;
        guac_common_rect_init(rect, left, top, right - left, bottom - top);

        return 1;
    }

    /* Clip and split left */
    else if (rect->x < min->x) {
        top = rect->y;
        left = rect->x;
        bottom = rect->y + rect->height;
        right = min->x;
        guac_common_rect_init(split_rect, left, top, right - left, bottom - top);

        /* Re-initialize original rect */
        left = min->x;
        right = rect->x + rect->width;
        guac_common_rect_init(rect, left, top, right - left, bottom - top);

        return 1;
    }

    /* Clip and split bottom */
    else if (rect->y + rect->height > min->y + min->height) {
        top = min->y + min->height;
        left = rect->x;
        bottom = rect->y + rect->height;
        right = rect->x + rect->width;
        guac_common_rect_init(split_rect, left, top, right - left, bottom - top);

        /* Re-initialize original rect */
        top = rect->y;
        bottom = min->y + min->height;
        guac_common_rect_init(rect, left, top, right - left, bottom - top);

        return 1;
    }

    /* Clip and split right */
    else if (rect->x + rect->width > min->x + min->width) {
        top = rect->y;
        left = min->x + min->width;
        bottom = rect->y + rect->height;
        right = rect->x + rect->width;
        guac_common_rect_init(split_rect, left, top, right - left, bottom - top);

        /* Re-initialize original rect */
        left = rect->x;
        right = min->x + min->width;
        guac_common_rect_init(rect, left, top, right - left, bottom - top);

        return 1;
    }

    return 0;
}
