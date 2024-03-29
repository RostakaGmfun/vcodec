#include "vcodec/vcodec.h"
#include "vcodec/bitstream.h"
#include "vcodec_common.h"
#include "vcodec_transform.h"
#include "vcodec_entropy_coding.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>

//#define debug_printf printf
#define debug_printf
#define GOP 1

typedef struct {
    uint8_t *p_ref_frame;
    int gop_cnt;
} dec_ctx_t;

static const int jpeg_zigzag_order4x4[4][4] = {
  {  0,  1,  5,  6, },
  {  2,  4,  7, 12, },
  {  3,  8, 11, 13, },
  {  9, 10, 14, 15, },
};

static const int jpeg_zigzag_order2x2[2][2] = {
  {  0,  1, },
  {  2,  3, },
};

static vcodec_status_t vcodec_dec_get_frame(vcodec_dec_ctx_t *p_ctx, uint8_t *p_frame);
static vcodec_status_t vcodec_dec_deinit(vcodec_dec_ctx_t *p_ctx);

static vcodec_status_t decode_key_frame(vcodec_dec_ctx_t *p_ctx, uint8_t *p_frame);
static vcodec_status_t decode_p_frame(vcodec_dec_ctx_t *p_ctx, uint8_t *p_frame);

static vcodec_status_t decode_macroblock_i(vcodec_dec_ctx_t *p_ctx, uint8_t *p_frame, int macroblock_x, int macroblock_y, const int *p_quant, int macroblock_size);
static void decode_dc(vcodec_dec_ctx_t *p_ctx, int *p_macroblock, const int *p_quant, int macroblock_size, int block_size);

static vcodec_status_t read_frame_header(vcodec_dec_ctx_t *p_ctx, bool *p_is_key_frame);
static vcodec_status_t read_macroblock_header(vcodec_dec_ctx_t *p_ctx, vcodec_prediction_mode_t *p_pred_mode);

vcodec_status_t vcodec_dec_dct_init(vcodec_dec_ctx_t *p_ctx) {
    if (0 == p_ctx->width || 0 == p_ctx->height) {
        return VCODEC_STATUS_INVAL;
    }

    p_ctx->decoder_ctx = p_ctx->alloc(sizeof(vcodec_dec_ctx_t));
    if (NULL == p_ctx->decoder_ctx) {
        return VCODEC_STATUS_NOMEM;
    }

    dec_ctx_t *p_dct_ctx = p_ctx->decoder_ctx;
    p_dct_ctx->p_ref_frame = p_ctx->alloc(p_ctx->width * p_ctx->height);
    if (NULL == p_dct_ctx->p_ref_frame) {
        return VCODEC_STATUS_NOMEM;
    }
    p_dct_ctx->gop_cnt = 0;

    p_ctx->get_frame = vcodec_dec_get_frame;
    p_ctx->deinit = vcodec_dec_deinit;
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_dec_get_frame(vcodec_dec_ctx_t *p_ctx, uint8_t *p_frame) {
    dec_ctx_t *p_dct_ctx = p_ctx->decoder_ctx;
    bool is_key_frame = false;
    vcodec_status_t ret = read_frame_header(p_ctx, &is_key_frame);
    if (VCODEC_STATUS_OK != ret) {
        return ret;
    }
    if (is_key_frame) {
        return decode_key_frame(p_ctx, p_frame);
    } else {
        return decode_p_frame(p_ctx, p_frame);
    }
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_dct_reset(vcodec_dec_ctx_t *p_ctx) {
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_dec_deinit(vcodec_dec_ctx_t *p_ctx) {
    return VCODEC_STATUS_OK;
}

static vcodec_status_t decode_key_frame(vcodec_dec_ctx_t *p_ctx, uint8_t *p_frame) {
    const uint32_t macroblock_size = 16;
    int quant[4*4] = {
        16,	11,	10,	16,
        12,	12,	14,	19,
        14,	13,	16,	24,
        14,	17,	22,	29,
    };
    //printf("Key frame\n");
    int h = p_ctx->height / macroblock_size * macroblock_size;
    int y = 0;
    for (; y < h; y += macroblock_size) {
        int x = 0;
        for (; x < p_ctx->width; x += macroblock_size) {
            decode_macroblock_i(p_ctx, p_frame, x, y, quant, macroblock_size);
        }
    }
    int reduced_macroblock_size;
    if ((p_ctx->height - y) % 8 == 0) {
        reduced_macroblock_size = 8;
    } else {
        reduced_macroblock_size = 4;
    }
    for (; y < p_ctx->height; y += reduced_macroblock_size) {
        int x = 0;
        for (; x < p_ctx->width; x += reduced_macroblock_size) {
            decode_macroblock_i(p_ctx, p_frame, x, y, quant, reduced_macroblock_size);
        }
    }
    dec_ctx_t *p_dct_ctx = p_ctx->decoder_ctx;
    memcpy(p_frame, p_dct_ctx->p_ref_frame, p_ctx->width * p_ctx->height);
}

static vcodec_status_t decode_macroblock_i(vcodec_dec_ctx_t *p_ctx, uint8_t *p_frame, int macroblock_x, int macroblock_y, const int *p_quant, int macroblock_size) {
    const int block_size = 4;
    dec_ctx_t *p_dct_ctx = p_ctx->decoder_ctx;
    vcodec_status_t ret = VCODEC_STATUS_OK;
    // Copy block to temp location
    int macroblock[macroblock_size * macroblock_size];
    vcodec_prediction_mode_t pred_mode;
    if (VCODEC_STATUS_OK != (ret = read_macroblock_header(p_ctx, &pred_mode))) {
        return ret;
    }
    debug_printf("Block predicted with %d:\n", pred_mode);

    for (int y = 0; y < macroblock_size; y += block_size) {
        for (int x = 0; x < macroblock_size; x += block_size) {
            int zigzag_block[block_size * block_size];
            zigzag_block[0] = 0; // DC to be filled later
            ret = vcodec_ec_read_coeffs(p_ctx->bitstream_reader, zigzag_block + 1, block_size * block_size - 1);
            debug_printf("AC COEFFS:\n");
            for (int i = 1; i < block_size * block_size; i++) {
                debug_printf("%4d ", zigzag_block[i]);
            }
            debug_printf("\n");
            int rescaled_block[block_size * block_size];
            for (int i = 0; i < block_size; i++) {
                for (int j = 0; j < block_size; j++) {
                    rescaled_block[i * block_size + j] = zigzag_block[jpeg_zigzag_order4x4[i][j]] * p_quant[i * block_size + j];
                }
            }
            for (int i = 0; i < block_size; i++) {
                // Save rescaled coeffs into the framebuffer for future inverse transform when DC coefficients will be available
                memcpy(macroblock + (y + i) * macroblock_size + x, rescaled_block + i * block_size, sizeof(int) * block_size);
            }
        }
    }

    decode_dc(p_ctx, macroblock, p_quant, macroblock_size, block_size);

    for (int y = 0; y < macroblock_size; y += block_size) {
        for (int x = 0; x < macroblock_size; x += block_size) {
            int reconstructed_block[block_size * block_size];
            for (int i = 0; i < block_size; i++) {
                // TODO: rework transform functions to work directly with macroblock buffer to avoid this copy operations
                memcpy(reconstructed_block + i * block_size, macroblock + (y + i) * macroblock_size + x, sizeof(int) * block_size);
            }
            inverse4x4(reconstructed_block, reconstructed_block);
            debug_printf("IDCT out:\n");
            for (int i = 0; i < block_size; i++) {
                for (int j = 0; j < block_size; j++) {
                    reconstructed_block[i * block_size + j] /= 16;
                    debug_printf("%4d ", reconstructed_block[i * block_size + j]);
                }
                memcpy(macroblock + (y + i) * macroblock_size + x, reconstructed_block + i * block_size, sizeof(int) * block_size);
                debug_printf("\n");
            }
        }
    }

    vcodec_unpredict_block(macroblock, p_dct_ctx->p_ref_frame, macroblock_x, macroblock_y, macroblock_size, p_ctx->width, pred_mode);
    debug_printf("Reconstructed:\n");
    int sad = 0;
    for (int i = 0; i < macroblock_size; i++) {
        for (int j = 0; j < macroblock_size; j++) {
            p_dct_ctx->p_ref_frame[(macroblock_y + i) * p_ctx->width + macroblock_x + j] = MAX(MIN(macroblock[i * macroblock_size + j], 255), 0);
            debug_printf("%3d ", p_dct_ctx->p_ref_frame[(macroblock_y + i) * p_ctx->width + macroblock_x + j]);
            const int diff = p_dct_ctx->p_ref_frame[(macroblock_y + i) * p_ctx->width + macroblock_x + j] - p_frame[(macroblock_y + i) * p_ctx->width + macroblock_x + j];
            debug_printf("(%3d) ", diff);
            sad += abs(diff);
        }
        if (macroblock_size / block_size < 4) {
            debug_printf("\n");
        }
    }
    debug_printf("SAD = %d at %4d %4d\n", sad, macroblock_x, macroblock_y);
}

static void decode_dc(vcodec_dec_ctx_t *p_ctx, int *p_macroblock, const int *p_quant, int macroblock_size, int block_size) {
    const int dc_block_size = macroblock_size / block_size;
    int dc_block[dc_block_size * dc_block_size];
    int zigzag_block[dc_block_size * dc_block_size];
    vcodec_ec_read_coeffs(p_ctx->bitstream_reader, zigzag_block, dc_block_size * dc_block_size);
    for (int y = 0; y < dc_block_size; y++) {
        for (int x = 0; x < dc_block_size; x++) {
            if (4 == dc_block_size) {
                dc_block[y * dc_block_size + x] = zigzag_block[jpeg_zigzag_order4x4[x][y]];
            } else {
                dc_block[y * dc_block_size + x] = zigzag_block[jpeg_zigzag_order2x2[x][y]];
            }
            dc_block[y * dc_block_size + x] *= p_quant[0];
            debug_printf("%3d ", dc_block[y * dc_block_size + x]);
        }
        debug_printf("\n");
    }
    if (4 == dc_block_size) {
        ihadamard4x4(dc_block, dc_block);
    } else {
        hadamard2x2(dc_block, dc_block);
    }
    debug_printf("DC ihadamard:\n");
    for (int y = 0; y < dc_block_size; y++) {
        for (int x = 0; x < dc_block_size; x++) {
            dc_block[y * dc_block_size + x] /= 8;
        }
    }
    for (int y = 0; y < dc_block_size; y++) {
        for (int x = 0; x < dc_block_size; x++) {
            p_macroblock[y * block_size * macroblock_size + x * block_size] = dc_block[y * dc_block_size + x];
        }
    }
}

static vcodec_status_t decode_p_frame(vcodec_dec_ctx_t *p_ctx, uint8_t *p_frame) {
    return VCODEC_STATUS_OK;
}

static vcodec_status_t read_frame_header(vcodec_dec_ctx_t *p_ctx, bool *p_is_key_frame) {
    uint32_t val = 0;
    vcodec_bitstream_reader_getbits(p_ctx->bitstream_reader, &val, 1);
    *p_is_key_frame = (bool)val;
    //printf("FRM hdr %d\n", val);
    return vcodec_bitstream_reader_status(p_ctx->bitstream_reader);
}

static vcodec_status_t read_macroblock_header(vcodec_dec_ctx_t *p_ctx, vcodec_prediction_mode_t *p_pred_mode) {
    uint32_t val;
    vcodec_bitstream_reader_getbits(p_ctx->bitstream_reader, &val, 2);
    *p_pred_mode = val;
    //printf("MB hdr %d\n", val);
    return vcodec_bitstream_reader_status(p_ctx->bitstream_reader);
}
