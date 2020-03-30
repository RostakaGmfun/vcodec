#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "vcodec/vcodec.h"

typedef enum {
    VCODEC_SOURCE_PGM,
    VCODEC_SOURCE_Y4M,
    VCODEC_SOURCE_V4L2,

    VCODEC_SOURCE_MAX
} vcodec_source_type_t;

typedef struct vcodec_source {
    vcodec_source_type_t source_type;
    uint32_t frame_size;
    uint32_t width;
    uint32_t height;
    void *p_source_ctx;
    vcodec_status_t (*read_frame)(struct vcodec_source *p_ctx, uint8_t *p_framebuffer);
    vcodec_status_t (*deinit)(struct vcodec_source *p_ctx);
} vcodec_source_t;

vcodec_status_t vcodec_source_init(vcodec_source_t *p_ctx, vcodec_source_type_t source_type, const char *path);
