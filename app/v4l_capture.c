#include "v4l_capture.h"

/***************************************************************************
 *   v4l2grab Version 0.1                                                  *
 *   Copyright (C) 2009 by Tobias Müller                                   *
 *   Tobias_Mueller@twam.info                                              *
 *                                                                         *
 *   based on V4L2 Specification, Appendix B: Video Capture Example        *
 *   (http://v4l2spec.bytesex.org/spec/capture-example.html)               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <linux/videodev2.h>

struct v4l_capture_context {
    int fd;
    int framebuffer_size;
};

/**
  Do ioctl and retry if error was EINTR ("A signal was caught during the ioctl() operation."). Parameters are the same as on ioctl.

  \param fd file descriptor
  \param request request
  \param argp argument
  \returns result from ioctl
*/
static int xioctl(int fd, int request, void* argp)
{
  int r;

  do r = ioctl(fd, request, argp);
  while (-1 == r && EINTR == errno);

  return r;
}

static int deviceInit(v4l_capture_context_t *p_ctx, int width, int height) {
    struct v4l2_capability cap = { 0 };
    struct v4l2_cropcap cropcap = { 0 };
    struct v4l2_crop crop = { 0 };
    struct v4l2_format fmt = { 0 };

    if (-1 == xioctl(p_ctx->fd, VIDIOC_QUERYCAP, &cap)) {
        fprintf(stderr, "ioctl failed %d", errno);
        return 1;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "No video capture device\n");
        return 1;
    }

    if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
        fprintf(stderr, "The device does not support read i/o\n");
        return 1;
    }

    /* Select video input, video standard and tune here. */
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (0 == xioctl(p_ctx->fd, VIDIOC_CROPCAP, &cropcap)) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; /* reset to default */

        if (-1 == xioctl(p_ctx->fd, VIDIOC_S_CROP, &crop)) {
        switch (errno) {
            case EINVAL:
            /* Cropping not supported. */
            break;
            default:
            /* Errors ignored. */
            break;
        }
        }
    } else {
        /* Errors ignored. */
    }

    // v4l2_format
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = width;
    fmt.fmt.pix.height      = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

    if (-1 == xioctl(p_ctx->fd, VIDIOC_S_FMT, &fmt)) {
        return 1;
    }

    /* Note VIDIOC_S_FMT may change width and height. */
    if (width != fmt.fmt.pix.width) {
        width = fmt.fmt.pix.width;
        fprintf(stderr,"Image width set to %i by device.\n",width);
    }
    if (height != fmt.fmt.pix.height) {
        height = fmt.fmt.pix.height;
        fprintf(stderr,"Image height set to %i by device.\n",height);
    }

    /* Buggy driver paranoia. */
    unsigned int min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min)
        fmt.fmt.pix.bytesperline = min;
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min)
        fmt.fmt.pix.sizeimage = min;

    p_ctx->framebuffer_size = fmt.fmt.pix.sizeimage;

    return 0;
}

v4l_capture_context_t *v4l_capture_init(const char *path, int width, int height) {
    v4l_capture_context_t *p_ctx = malloc(sizeof(v4l_capture_context_t));
    if (NULL == p_ctx) {
        return NULL;
    }
    struct stat st;

    // stat file
    if (-1 == stat(path, &st)) {
        fprintf(stderr, "Cannot identify '%s': %d, %s\n", path, errno, strerror(errno));
        free(p_ctx);
        return NULL;
    }

    // check if its device
    if (!S_ISCHR (st.st_mode)) {
        fprintf(stderr, "%s is no device\n", path);
        free(p_ctx);
        return NULL;
    }

    // open device
    p_ctx->fd = open(path, O_RDWR /* required */ | O_NONBLOCK, 0);

    // check if opening was successfull
    if (-1 == p_ctx->fd) {
        fprintf(stderr, "Cannot open '%s': %d, %s\n", path, errno, strerror(errno));
        free(p_ctx);
        return NULL;
    }

    if (0 != deviceInit(p_ctx, width, height)) {
        free(p_ctx);
        return NULL;
    }

    return p_ctx;
}


int v4l_capture_get_framebuffer_size(v4l_capture_context_t *p_ctx) {
    return p_ctx->framebuffer_size;
}

int v4l_capture_read_frame(v4l_capture_context_t *p_ctx, uint8_t *p_framebuffer) {
    for (;;) {
        fd_set fds;
        struct timeval tv;

        int r;

        FD_ZERO(&fds);
        FD_SET(p_ctx->fd, &fds);

        /* Timeout. */
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        r = select(p_ctx->fd + 1, &fds, NULL, NULL, &tv);

        if (-1 == r) {
            if (EINTR == errno) {
                continue;
            }

            return errno;
        }

        if (0 == r) {
            fprintf(stderr, "select timeout\n");
            return -1;
        }

        if (-1 == read(p_ctx->fd, p_framebuffer, p_ctx->framebuffer_size)) {
            if (EAGAIN == errno) {
                continue;
            } else {
                return 1;
            }
        }
    }

    return 0;
}

int v4l_capture_close(v4l_capture_context_t *p_ctx) {
    int ret = close(p_ctx->fd);
    free(p_ctx);
    return ret;
}
