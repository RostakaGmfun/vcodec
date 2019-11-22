#include "vcodec/vcodec.h"
#include "vcodec_common.h"

vcodec_status_t vcodec_write_elias_delta_code(vcodec_enc_ctx_t *p_ctx, unsigned int value) {
    const int num_bits  = sizeof(int) * 8 - 1 - __builtin_clz(value);
    const int num_bits2 = sizeof(int) * 8 - 1 - __builtin_clz(num_bits);
    const int zero_prefix = 0;
    vcodec_status_t status = vcodec_bit_buffer_write(p_ctx, zero_prefix, num_bits2 - 1);
    if (VCODEC_STATUS_OK != status) {
        return status;
    }

    status = vcodec_bit_buffer_write(p_ctx, num_bits, num_bits2);
    if (VCODEC_STATUS_OK != status) {
        return status;
    }

    return vcodec_bit_buffer_write(p_ctx, value, num_bits - 1);
}

vcodec_status_t vcodec_bit_buffer_write(vcodec_enc_ctx_t *p_ctx, uint32_t bits, int num_bits) {
    while (num_bits > 0) {
        const int to_write = MIN(num_bits, sizeof(p_ctx->bit_buffer) * 8 - p_ctx->bit_buffer_index);
        num_bits -= to_write;
        const uint32_t mask = 32 == to_write ? 0xFFFFFFFF : (1 << to_write) - 1;
        p_ctx->bit_buffer |= (bits & mask) << p_ctx->bit_buffer_index;
        p_ctx->bit_buffer_index += to_write;
        bits >> to_write;
        if (sizeof(p_ctx->bit_buffer) * 8 == p_ctx->bit_buffer_index) {
            vcodec_status_t status = p_ctx->write((const uint8_t *)&p_ctx->bit_buffer, sizeof(p_ctx->bit_buffer), p_ctx->io_ctx);
            if (VCODEC_STATUS_OK != status) {
                return status;
            }
            p_ctx->bit_buffer = 0;
            p_ctx->bit_buffer_index = 0;
        }
    }

    return VCODEC_STATUS_OK;
}

vcodec_status_t vcodec_enc_init(vcodec_enc_ctx_t *p_ctx, vcodec_type_t type) {
    switch (type) {
    case VCODEC_TYPE_MED_GR:
        return vcodec_med_gr_init(p_ctx);
    case VCODEC_TYPE_INTER:
        return vcodec_inter_init(p_ctx);
    default:
        return VCODEC_STATUS_INVAL;
    }
}
