#include "vcodec/vcodec.h"
#include "vcodec_common.h"
#include "vcodec_transform.h"
#include "vcodec/bitstream.h"
#include "vcodec_entropy_coding.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>

#define GOP 1

//#define debug_printf printf
#define debug_printf

typedef struct {
    uint8_t *p_ref_frame;
    int gop_cnt;
} vcodec_dct_ctx_t;

static const int jpeg_zigzag_order8x8[8][8] = {
  {  0,  1,  5,  6, 14, 15, 27, 28 },
  {  2,  4,  7, 13, 16, 26, 29, 42 },
  {  3,  8, 12, 17, 25, 30, 41, 43 },
  {  9, 11, 18, 24, 31, 40, 44, 53 },
  { 10, 19, 23, 32, 39, 45, 52, 54 },
  { 20, 22, 33, 38, 46, 51, 55, 60 },
  { 21, 34, 37, 47, 50, 56, 59, 61 },
  { 35, 36, 48, 49, 57, 58, 62, 63 }
};

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

static vcodec_status_t vcodec_dct_process_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame);
static vcodec_status_t vcodec_dct_reset(vcodec_enc_ctx_t *p_ctx);
static vcodec_status_t vcodec_dct_deinit(vcodec_enc_ctx_t *p_ctx);

static vcodec_status_t encode_key_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame);
static vcodec_status_t encode_p_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame);

static void encode_macroblock_i(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame, int macroblock_x, int macroblock_y, const int *p_quant, int macroblock_size);
static void encode_dc(vcodec_enc_ctx_t *p_ctx, int *p_macroblock, const int *p_quant, int macroblock_size, int block_size);

static void write_frame_header(vcodec_enc_ctx_t *p_ctx, bool is_key_frame);
static void write_macroblock_header(vcodec_enc_ctx_t *p_ctx, vcodec_prediction_mode_t pred_mode);

vcodec_status_t vcodec_dct_init(vcodec_enc_ctx_t *p_ctx) {
    if (0 == p_ctx->width || 0 == p_ctx->height) {
        return VCODEC_STATUS_INVAL;
    }

    p_ctx->encoder_ctx = p_ctx->alloc(sizeof(vcodec_dct_ctx_t));
    if (NULL == p_ctx->encoder_ctx) {
        return VCODEC_STATUS_NOMEM;
    }

    vcodec_dct_ctx_t *p_dct_ctx = p_ctx->encoder_ctx;
    p_dct_ctx->p_ref_frame = p_ctx->alloc(p_ctx->width * p_ctx->height);
    if (NULL == p_dct_ctx->p_ref_frame) {
        return VCODEC_STATUS_NOMEM;
    }
    p_dct_ctx->gop_cnt = 0;

    p_ctx->process_frame = vcodec_dct_process_frame;
    p_ctx->reset = vcodec_dct_reset;
    p_ctx->deinit = vcodec_dct_deinit;
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_dct_process_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame) {
    vcodec_dct_ctx_t *p_dct_ctx = p_ctx->encoder_ctx;
    if (0 == p_dct_ctx->gop_cnt++ % GOP) {
        debug_printf("KEYFRAME\n");
        encode_key_frame(p_ctx, p_frame);
    } else {
        encode_p_frame(p_ctx, p_frame);
    }
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_dct_reset(vcodec_enc_ctx_t *p_ctx) {
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_dct_deinit(vcodec_enc_ctx_t *p_ctx) {
    return VCODEC_STATUS_OK;
}

static vcodec_status_t encode_key_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame) {
    const uint32_t macroblock_size = 16;
    int quant[4*4] = {
        16,	11,	10,	16,
        12,	12,	14,	19,
        14,	13,	16,	24,
        14,	17,	22,	29,
    };

    vcodec_dct_ctx_t *p_dct_ctx = p_ctx->encoder_ctx;
    int h = p_ctx->height / macroblock_size * macroblock_size;
    int y = 0;
    write_frame_header(p_ctx, true);
    for (; y < h; y += macroblock_size) {
        int x = 0;
        for (; x < p_ctx->width; x += macroblock_size) {
            encode_macroblock_i(p_ctx, p_frame, x, y, quant, macroblock_size);
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
            encode_macroblock_i(p_ctx, p_frame, x, y, quant, reduced_macroblock_size);
        }
    }

    int mse = 0;
    for (int i = 0; i < p_ctx->width * p_ctx->height; i++) {
        const int diff = p_frame[i] - p_dct_ctx->p_ref_frame[i];
        mse += diff * diff;
    }
    double mse_divided = (double)mse / (p_ctx->width * p_ctx->height);
    double psnr = 20 * log10(255) - 10 * log10(mse_divided);
    fprintf(stderr, "PSNR %f mse %f\n", psnr, mse_divided);
    const char *frame_hdr = "FRAME\n";
    //p_ctx->write(frame_hdr, strlen(frame_hdr), p_ctx->io_ctx);
    //p_ctx->write(p_dct_ctx->p_ref_frame, p_ctx->width * p_ctx->height, p_ctx->io_ctx);

    return vcodec_bitstream_writer_status(p_ctx->bitstream_writer);
}

static vcodec_status_t encode_p_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame) {
    return VCODEC_STATUS_OK;
}

static void encode_macroblock_i(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame, int macroblock_x, int macroblock_y, const int *p_quant, int macroblock_size) {
    const int block_size = 4;
    vcodec_dct_ctx_t *p_dct_ctx = p_ctx->encoder_ctx;
    // Copy block to temp location
    int macroblock[macroblock_size * macroblock_size];
    const vcodec_prediction_mode_t pred_mode = vcodec_predict_block(macroblock, p_dct_ctx->p_ref_frame, macroblock_x, macroblock_y, p_frame, p_ctx->width, macroblock_size);
    debug_printf("Block predicted with %d:\n", pred_mode);
    for (int i = 0; i < macroblock_size; i++) {
        for (int j = 0; j < macroblock_size; j++) {
            debug_printf("%4d, ", macroblock[i * block_size + j]);
        }
        debug_printf("\n");
    }

    write_macroblock_header(p_ctx, pred_mode);

    for (int y = 0; y < macroblock_size; y += block_size) {
        for (int x = 0; x < macroblock_size; x += block_size) {
            int block[block_size * block_size];
            for (int i = 0; i < block_size; i++) {
                // TODO: rework transform functions to work directly with macroblock buffer to avoid this copy operations
                memcpy(block + i * block_size, macroblock + (y + i) * macroblock_size + x, sizeof(int) * block_size);
            }
            forward4x4(block, block);
            debug_printf("DCT:\n");
            for (int i = 0; i < block_size; i++) {
                for (int j = 0; j < block_size; j++) {
                    debug_printf("%4d ", block[i * block_size + j]);
                }
                debug_printf("\n");
            }
            int rescaled_block[block_size * block_size];
            int zigzag_block[block_size * block_size];
            for (int i = 0; i < block_size; i++) {
                for (int j = 0; j < block_size; j++) {
                    zigzag_block[jpeg_zigzag_order4x4[i][j]] = block[i * block_size + j] / (p_quant[i * block_size + j]);
                    rescaled_block[i * block_size + j] = zigzag_block[jpeg_zigzag_order4x4[i][j]] * (p_quant[i * block_size + j]);
                }
            }
            debug_printf("AC CODING:\n");
            for (int i = 1; i < block_size * block_size; i++) {
                debug_printf("%4d ", zigzag_block[i]);
            }
            debug_printf("\n");

            // Don't write the DC coefficient yet
            vcodec_ec_write_coeffs(p_ctx->bitstream_writer, zigzag_block + 1, block_size * block_size - 1);

            for (int i = 0; i < block_size; i++) {
                // Copy transformed coefficients ditrcly into the macroblock (reference framebuffer)
                memcpy(macroblock + (y + i) * macroblock_size + x, rescaled_block + i * block_size, sizeof(int) * block_size);
            }
        }
    }

    encode_dc(p_ctx, macroblock, p_quant, macroblock_size, block_size);

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
                // Copy reconstructed block back into reference framebuffer
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
        debug_printf("\n");
    }
    debug_printf("SAD = %d at %4d %4d\n", sad, macroblock_x, macroblock_y);
}

static void encode_dc(vcodec_enc_ctx_t *p_ctx, int *p_macroblock, const int *p_quant, int macroblock_size, int block_size) {
    const int dc_block_size = macroblock_size / block_size;
    int dc_block[dc_block_size * dc_block_size];
    for (int y = 0; y < dc_block_size; y++) {
        for (int x = 0; x < dc_block_size; x++) {
            dc_block[y * dc_block_size + x] = p_macroblock[y * block_size * macroblock_size + x * block_size];
        }
    }
    if (4 == dc_block_size) {
        hadamard4x4(dc_block, dc_block);
    } else {
        hadamard2x2(dc_block, dc_block);
    }
    debug_printf("DC hadamard:\n");
    int zigzag_block[dc_block_size * dc_block_size];
    for (int y = 0; y < dc_block_size; y++) {
        for (int x = 0; x < dc_block_size; x++) {
            dc_block[y * dc_block_size + x] /= p_quant[0];
            if (4 == dc_block_size) {
                zigzag_block[jpeg_zigzag_order4x4[x][y]] = dc_block[y * dc_block_size + x];
            } else {
                zigzag_block[jpeg_zigzag_order2x2[x][y]] = dc_block[y * dc_block_size + x];
            }
            debug_printf("%3d ", dc_block[y * dc_block_size + x]);
            dc_block[y * dc_block_size + x] *= p_quant[0];
        }
        debug_printf("\n");
    }

    vcodec_ec_write_coeffs(p_ctx->bitstream_writer, zigzag_block, dc_block_size * dc_block_size);

    if (4 == dc_block_size) {
        ihadamard4x4(dc_block, dc_block);
    } else {
        hadamard2x2(dc_block, dc_block);
    }
    debug_printf("DC ihadamard:\n");
    for (int y = 0; y < dc_block_size; y++) {
        for (int x = 0; x < dc_block_size; x++) {
            dc_block[y * dc_block_size + x] /= 8;
            debug_printf("%3d ", dc_block[y * dc_block_size + x]);
        }
        debug_printf("\n");
    }
    for (int y = 0; y < dc_block_size; y++) {
        for (int x = 0; x < dc_block_size; x++) {
            p_macroblock[y * block_size * macroblock_size + x * block_size] = dc_block[y * dc_block_size + x];
        }
    }
}

static void write_frame_header(vcodec_enc_ctx_t *p_ctx, bool is_key_frame) {
    //printf("FRM hdr %d\n", is_key_frame);
    vcodec_bitstream_writer_putbits(p_ctx->bitstream_writer, is_key_frame, 1);
}

static void write_macroblock_header(vcodec_enc_ctx_t *p_ctx, vcodec_prediction_mode_t pred_mode) {
    uint32_t val = pred_mode;
    //printf("MB hdr %d\n", val);
    vcodec_bitstream_writer_putbits(p_ctx->bitstream_writer, val, 2);
}
