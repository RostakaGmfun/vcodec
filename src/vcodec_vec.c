#include "vcodec/vcodec.h"
#include "vcodec_common.h"

#include <string.h>

typedef struct {
} vcodec_vec_ctx_t;

static vcodec_status_t vcodec_vec_process_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame);

static vcodec_status_t vcodec_vec_reset(vcodec_enc_ctx_t *p_ctx);

static vcodec_status_t vcodec_vec_deinit(vcodec_enc_ctx_t *p_ctx);

vcodec_status_t vcodec_vec_init(vcodec_enc_ctx_t *p_ctx) {
    if (0 == p_ctx->width || 0 == p_ctx->height) {
        return VCODEC_STATUS_INVAL;
    }
    p_ctx->buffer_size = p_ctx->width;
    p_ctx->p_buffer = p_ctx->alloc(p_ctx->buffer_size);
    if (NULL == p_ctx->p_buffer) {
        return VCODEC_STATUS_NOMEM;
    }

    p_ctx->process_frame = vcodec_vec_process_frame;
    p_ctx->reset = vcodec_vec_reset;
    p_ctx->deinit = vcodec_vec_deinit;
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_vec_process_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame) {
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_vec_reset(vcodec_enc_ctx_t *p_ctx) {
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_vec_deinit(vcodec_enc_ctx_t *p_ctx) {
    p_ctx->free(p_ctx->p_buffer);
    return VCODEC_STATUS_OK;
}
