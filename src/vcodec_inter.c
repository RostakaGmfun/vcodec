#include "vcodec/vcodec.h"
#include "vcodec_common.h"

#include <string.h>

vcodec_status_t vcodec_inter_init(vcodec_enc_ctx_t *p_ctx) {
    if (0 == p_ctx->width || 0 == p_ctx->height) {
        return VCODEC_STATUS_INVAL;
    }
    p_ctx->buffer_size = p_ctx->width * p_ctx->height;
    p_ctx->p_buffer = p_ctx->alloc(p_ctx->buffer_size);
    if (NULL == p_ctx->p_buffer) {
        return VCODEC_STATUS_NOMEM;
    }
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_inter_process_frame(vcodec_enc_ctx_t *p_ctx) {
    vcodec_status_t status = VCODEC_STATUS_OK;
    return status;
}

static vcodec_status_t vcodec_inter_reset(vcodec_enc_ctx_t *p_ctx) {
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_inter_deinit(vcodec_enc_ctx_t *p_ctx) {
    p_ctx->free(p_ctx->p_buffer);
    return VCODEC_STATUS_OK;
}
