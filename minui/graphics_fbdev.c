/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include <fcntl.h>
#include <stdio.h>

#include <sys/cdefs.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>

#include <linux/fb.h>
#include <linux/kd.h>

#include "minui.h"
#include "graphics.h"

static gr_surface fbdev_init(minui_backend*);
static gr_surface fbdev_flip(minui_backend*);
static void fbdev_blank(minui_backend*, bool);
static void fbdev_exit(minui_backend*);

static GRSurface gr_framebuffer[2];
static bool double_buffered;
static GRSurface* gr_draw = NULL;
static int displayed_buffer;

static struct fb_var_screeninfo vi;
static struct fb_fix_screeninfo fi;

static int fb_fd = -1;

static minui_backend my_backend = {
    .init = fbdev_init,
    .flip = fbdev_flip,
    .blank = fbdev_blank,
    .exit = fbdev_exit,
};

minui_backend* open_fbdev() {
    return &my_backend;
}

static void fbdev_blank(minui_backend* backend __unused, bool blank)
{
    int ret;

    ret = ioctl(fb_fd, FBIOBLANK, blank ? FB_BLANK_POWERDOWN : FB_BLANK_UNBLANK);
    if (ret < 0)
        perror("ioctl(): blank");
}

static void set_displayed_framebuffer(unsigned n)
{
    if (n > 1 || !double_buffered) return;

    vi.yres_virtual = gr_framebuffer[0].height * 2;
    vi.yoffset = n * gr_framebuffer[0].height;
    vi.bits_per_pixel = gr_framebuffer[0].pixel_bytes * 8;
    if (ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vi) < 0) {
        perror("active fb swap failed");
    }
    displayed_buffer = n;
}

static gr_surface fbdev_init(minui_backend* backend) {
    int fd;
    void *bits;

    fd = open("/dev/graphics/fb0", O_RDWR);
    if (fd < 0) {
        perror("cannot open fb0");
        return NULL;
    }

    if (ioctl(fd, FBIOGET_FSCREENINFO, &fi) < 0) {
        perror("failed to get fb0 info");
        close(fd);
        return NULL;
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, &vi) < 0) {
        perror("failed to get fb0 info");
        close(fd);
        return NULL;
    }

    // We print this out for informational purposes only, but
    // throughout we assume that the framebuffer device uses an RGBX
    // pixel format.  This is the case for every development device I
    // have access to.  For some of those devices (eg, hammerhead aka
    // Nexus 5), FBIOGET_VSCREENINFO *reports* that it wants a
    // different format (XBGR) but actually produces the correct
    // results on the display when you write RGBX.
    //
    // If you have a device that actually *needs* another pixel format
    // (ie, BGRX, or 565), patches welcome...

    printf("fb0 reports (possibly inaccurate):\n"
           "  vi.bits_per_pixel = %d\n"
           "  vi.red.offset   = %3d   .length = %3d\n"
           "  vi.green.offset = %3d   .length = %3d\n"
           "  vi.blue.offset  = %3d   .length = %3d\n",
           vi.bits_per_pixel,
           vi.red.offset, vi.red.length,
           vi.green.offset, vi.green.length,
           vi.blue.offset, vi.blue.length);

    bits = mmap(0, fi.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (bits == MAP_FAILED) {
        perror("failed to mmap framebuffer");
        close(fd);
        return NULL;
    }

    memset(bits, 0, fi.smem_len);

    gr_framebuffer[0].width = vi.xres;
    gr_framebuffer[0].height = vi.yres;
    gr_framebuffer[0].row_bytes = fi.line_length;
    gr_framebuffer[0].pixel_bytes = vi.bits_per_pixel / 8;
    gr_framebuffer[0].data = bits;
    memset(gr_framebuffer[0].data, 0, gr_framebuffer[0].height * gr_framebuffer[0].row_bytes);

#if defined(RECOVERY_TRUST_FBINFO)
    // Most of the fbdev implementation happily assumes four bytes per pixel.
    // Keep it at that, we'll reorder and cut down things when we get to displaying.
    // Sorry pal, no double buffering in that case.

    double_buffered = false;
    gr_framebuffer[0].row_bytes = (fi.line_length * 32 / vi.bits_per_pixel);
    gr_framebuffer[0].pixel_bytes = 4;
#else
    /* check if we can use double buffering */
    double_buffered = (vi.yres * fi.line_length * 2 <= fi.smem_len);
#endif

    if (double_buffered) {
        memcpy(gr_framebuffer+1, gr_framebuffer, sizeof(GRSurface));
        gr_framebuffer[1].data = gr_framebuffer[0].data +
            gr_framebuffer[0].height * gr_framebuffer[0].row_bytes;

        gr_draw = gr_framebuffer+1;

    } else {
        // Without double-buffering, we allocate RAM for a buffer to
        // draw in, and then "flipping" the buffer consists of a
        // memcpy from the buffer we allocated to the framebuffer.

        gr_draw = (GRSurface*) malloc(sizeof(GRSurface));
        memcpy(gr_draw, gr_framebuffer, sizeof(GRSurface));
        gr_draw->data = (unsigned char*) malloc(gr_draw->height * gr_draw->row_bytes);
        if (!gr_draw->data) {
            perror("failed to allocate in-memory surface");
            return NULL;
        }
    }

    memset(gr_draw->data, 0, gr_draw->height * gr_draw->row_bytes);
    fb_fd = fd;
    set_displayed_framebuffer(0);

    printf("framebuffer: %d (%d x %d)\n", fb_fd, gr_draw->width, gr_draw->height);

    fbdev_blank(backend, true);
    fbdev_blank(backend, false);

    return gr_draw;
}

#if defined(RECOVERY_TRUST_FBINFO)

static void gr_pack_pixels(unsigned char* dst, uint32_t* src)
{
    // Possible alignment problems? We malloc'ed the source, so there we should be fine.

    unsigned char r_mask = (1 << (vi.red.length)) - 1;
    unsigned char g_mask = (1 << (vi.green.length)) - 1;
    unsigned char b_mask = (1 << (vi.blue.length)) - 1;
    unsigned char a_mask = (1 << (vi.transp.length)) - 1;

    unsigned int y;
    unsigned int x;
    for(y = 0; y < vi.yres; ++y)
    {
        unsigned char* dst_line = (unsigned char* ) (dst + fi.line_length * y);
        uint32_t* src_line = src + gr_framebuffer[0].row_bytes * y;
        for(x = 0; x < vi.xres; ++x)
        {
            // get the uppermost bits of each channel and move them in place
            // for the output
            uint32_t dstpp =
               ((*src >> (8  - vi.red.length))    & r_mask) << vi.red.offset |
               ((*src >> (16 - vi.green.length))  & g_mask) << vi.green.offset |
               ((*src >> (24 - vi.blue.length))   & b_mask) << vi.blue.offset |
               ((*src >> (32 - vi.transp.length)) & a_mask) << vi.transp.offset;
            ++src;

// Of course, if we do typecasting and write into uint16_t*'s for example, we'd be
// endianness-agnostic, but we could get into trouble with alignments.
#if BYTE_ORDER == LITTLE_ENDIAN
            // if(vi.bits_per_pixel >  0)
                *dst++ =  dstpp        & 0xFF;
            if(vi.bits_per_pixel >  8)
                *dst++ = (dstpp >>  8) & 0xFF;
            if(vi.bits_per_pixel > 16)
                *dst++ = (dstpp >> 16) & 0xFF;
            if(vi.bits_per_pixel > 24)
                *dst++ = (dstpp >> 24) & 0xFF;
#elif BYTE_ORDER == BIG_ENDIAN
            if(vi.bits_per_pixel > 24)
                *dst++ = (dstpp >> 24) & 0xFF;
            if(vi.bits_per_pixel > 16)
                *dst++ = (dstpp >> 16) & 0xFF;
            if(vi.bits_per_pixel >  8)
                *dst++ = (dstpp >>  8) & 0xFF;
            // if(vi.bits_per_pixel >  0)
                *dst++ =  dstpp        & 0xFF;
#else
#error "Unknown/wierd byte ordering on target platform, please extend here"
#endif
        }
     }
}

#endif

static gr_surface fbdev_flip(minui_backend* backend __unused) {
    if (double_buffered) {
#if defined(RECOVERY_BGRA)
        // In case of BGRA, do some byte swapping
        unsigned int idx;
        unsigned char tmp;
        unsigned char* ucfb_vaddr = (unsigned char*)gr_draw->data;
        for (idx = 0 ; idx < (gr_draw->height * gr_draw->row_bytes);
                idx += 4) {
            tmp = ucfb_vaddr[idx];
            ucfb_vaddr[idx    ] = ucfb_vaddr[idx + 2];
            ucfb_vaddr[idx + 2] = tmp;
        }
#endif
        // Change gr_draw to point to the buffer currently displayed,
        // then flip the driver so we're displaying the other buffer
        // instead.
        gr_draw = gr_framebuffer + displayed_buffer;
        set_displayed_framebuffer(1-displayed_buffer);
    } else {
#if defined(RECOVERY_TRUST_FBINFO)
            // Do the slow agonizing task of packing the RGBA pixels into the output
            // format the framebuffer wishes to have.
            gr_pack_pixels(gr_framebuffer[0].data, (uint32_t *) gr_draw->data);
#else
            // Simply copy from the in-memory surface to the framebuffer.
            memcpy(gr_framebuffer[0].data, gr_draw->data,
                   gr_draw->height * gr_draw->row_bytes);
#endif
    }
    return gr_draw;
}

static void fbdev_exit(minui_backend* backend __unused) {
    close(fb_fd);
    fb_fd = -1;

    if (!double_buffered && gr_draw) {
        free(gr_draw->data);
        free(gr_draw);
    }
    gr_draw = NULL;
}
