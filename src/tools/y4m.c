#include "tools/y4m.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    FILE *infile;
} vcodec_y4m_ctx_t;

static vcodec_status_t y4m_read_frame(struct vcodec_source *p_ctx, uint8_t *p_framebuffer) {
    vcodec_y4m_ctx_t *p_y4m_ctx = p_ctx->p_source_ctx;
    const char *frame_hdr = "FRAME\n";
    char hdr[16];
    if (fread(hdr, strlen(frame_hdr), 1, p_y4m_ctx->infile) != 1) {
        return VCODEC_STATUS_IO_FAILED;
    }
    if (memcmp(hdr, frame_hdr, strlen(frame_hdr)) != 0) {
        return VCODEC_STATUS_INVAL;
    }
    const size_t ret = fread(p_framebuffer, p_ctx->frame_size, 1, p_y4m_ctx->infile);
    if (ret != 1) {
        return VCODEC_STATUS_IO_FAILED;
    }
    return VCODEC_STATUS_OK;
}

static vcodec_status_t y4m_deinit(struct vcodec_source *p_ctx) {
    vcodec_y4m_ctx_t *p_y4m_ctx = p_ctx->p_source_ctx;
    if (0 != fclose(p_y4m_ctx->infile)) {
        return VCODEC_STATUS_INVAL;
    }
    free(p_y4m_ctx);
    p_ctx->p_source_ctx = NULL;
    return VCODEC_STATUS_OK;
}

vcodec_status_t vcodec_y4m_init(vcodec_source_t *p_ctx, const char *path) {
    FILE *f = fopen(path, "rb");
    if (NULL == f) {
        return VCODEC_STATUS_NOENT;
    }

    p_ctx->p_source_ctx = malloc(sizeof(vcodec_y4m_ctx_t));
    if (NULL == p_ctx->p_source_ctx) {
        return VCODEC_STATUS_NOMEM;
    }

    int frame_nom = 0;
    int frame_denom = 0;
    char interlacing_mode = 0;
    int aspect_ratio_nom = 0;
    int aspect_ratio_denom = 0;
    char color_space[16 + 1] = { 0 };
    if (fscanf(f, "YUV4MPEG2 W%d H%d F%d:%d I%c A%d:%d C%16s%*s\n", &p_ctx->width, &p_ctx->height, &frame_nom,
                    &frame_denom, &interlacing_mode, &aspect_ratio_nom, &aspect_ratio_denom, color_space) != 8) {
        fprintf(stderr, "Bad header format\n");
        fclose(f);
        return VCODEC_STATUS_IO_FAILED;
    }

    printf("Opened Y4M: YUV4MPEG2 W%d H%d F%d:%d I%c A%d:%d C%s\n", p_ctx->width, p_ctx->height, frame_nom,
                    frame_denom, interlacing_mode, aspect_ratio_nom, aspect_ratio_denom, color_space);

    p_ctx->read_frame = y4m_read_frame;
    p_ctx->deinit = y4m_deinit;
    p_ctx->frame_size = (p_ctx->width * p_ctx->height * 3) / 2;
    ((vcodec_y4m_ctx_t *)p_ctx->p_source_ctx)->infile = f;

    return VCODEC_STATUS_OK;
}
