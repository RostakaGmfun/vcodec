#include "vcodec/vcodec.h"
#include "vcodec_common.h"
#include "vec_table.h"

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

// 4x4
#define VEC_BLOCK_SIZE 4

typedef struct {
    vec_table_t *vec_table;
    vec_table_t *rle_table;
} vcodec_vec_ctx_t;

static vcodec_status_t vcodec_vec_process_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame);

static vcodec_status_t vcodec_vec_reset(vcodec_enc_ctx_t *p_ctx);

static vcodec_status_t vcodec_vec_deinit(vcodec_enc_ctx_t *p_ctx);

static uint32_t vec_hash_compute_4x4(const uint8_t *p_block);

/**
 * Packs 16 6-bit pixels into 12 bytes, no compression
 */
static void vec_residual_compute_4x4(const uint8_t *p_block, uint32_t hash, uint8_t *p_residual);

/**
 * Output code of the given block and update the vector table.
 */
static uint32_t vcodec_code_block(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_block, uint32_t block_width, uint32_t (*hasher)(const uint8_t *), void (*residual)(const uint8_t *, uint32_t, uint8_t *));

static vcodec_status_t vcodec_output_escape(vcodec_enc_ctx_t *p_ctx);
static vcodec_status_t vcodec_output_immediate(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_block, uint32_t block_width);

static vcodec_status_t vcodec_output_vector(vcodec_enc_ctx_t *p_ctx, int count, int total, uint32_t hash);
static vcodec_status_t vcodec_output_residual(vcodec_enc_ctx_t *p_ctx, uint32_t block_width, const uint8_t *p_residual);

static vcodec_status_t vcodec_vec_process_key_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame);

static vcodec_status_t vcodec_code_rle(vcodec_enc_ctx_t *p_ctx, int run_length);

vcodec_status_t vcodec_vec_init(vcodec_enc_ctx_t *p_ctx) {
    if (0 == p_ctx->width || 0 == p_ctx->height) {
        return VCODEC_STATUS_INVAL;
    }

    p_ctx->encoder_ctx = p_ctx->alloc(sizeof(vcodec_vec_ctx_t));
    if (NULL == p_ctx->encoder_ctx) {
        return VCODEC_STATUS_NOMEM;
    }

    vcodec_vec_ctx_t *vec_ctx = p_ctx->encoder_ctx;

    vec_ctx->vec_table = vec_table_init(128);
    vec_ctx->rle_table = vec_table_init(64);

    p_ctx->process_frame = vcodec_vec_process_frame;
    p_ctx->reset = vcodec_vec_reset;
    p_ctx->deinit = vcodec_vec_deinit;
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_vec_process_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame) {
    uint32_t block_width = VEC_BLOCK_SIZE;
    uint8_t block[block_width * block_width];
    vcodec_vec_ctx_t *vec_ctx = p_ctx->encoder_ctx;
    static bool first = true;
    if (first) {
        vcodec_vec_process_key_frame(p_ctx, p_frame);
    }
    printf("FRAME\n");
    for (int x = 0; x < p_ctx->width; x += block_width) {
        for (int y = 0; y < p_ctx->height; y += block_width) {
            // Copy block to temp location
            for (int j = y; j < block_width; j++) {
                memcpy(block + (j - y) * block_width, &p_frame[j * p_ctx->width + x], block_width);
            }
            if (first) {
                const uint32_t hash = vec_hash_compute_4x4(block);
                vec_table_update(vec_ctx->vec_table, hash);
            } else {
                vcodec_code_block(p_ctx, block, block_width, vec_hash_compute_4x4, vec_residual_compute_4x4);
            }
        }
    }
    printf("\n");
    first = false;
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_vec_reset(vcodec_enc_ctx_t *p_ctx) {
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_vec_deinit(vcodec_enc_ctx_t *p_ctx) {
    return VCODEC_STATUS_OK;
}

static uint32_t vec_hash_compute_4x4(const uint8_t *p_block) {
    const uint32_t msb1 = ((p_block[0] & 0x80) << 15) | ((p_block[1] & 0x80) << 14) | ((p_block[2] & 0x80) << 13) | ((p_block[3] & 0x80) << 12)
        | ((p_block[4] & 0x80) << 11) | ((p_block[5] & 0x80) << 10) | ((p_block[6] & 0x80) << 9) | ((p_block[7] & 0x80) << 8)
        | ((p_block[8] & 0x80) << 7) | ((p_block[1] & 0x80) << 6) | ((p_block[10] & 0x80) << 5) | ((p_block[11] & 0x80) << 4)
        | ((p_block[12] & 0x80) << 3) | ((p_block[13] & 0x80) << 2) | ((p_block[14] & 0x80) << 1) | (p_block[15] & 0x80);

    const uint32_t msb2 = ((p_block[0] & 0x40) << 15) | ((p_block[1] & 0x40) << 14) | ((p_block[2] & 0x40) << 13) | ((p_block[3] & 0x40) << 12)
        | ((p_block[4] & 0x40) << 11) | ((p_block[5] & 0x40) << 10) | ((p_block[6] & 0x40) << 9) | ((p_block[7] & 0x40) << 8)
        | ((p_block[8] & 0x40) << 7) | ((p_block[1] & 0x40) << 6) | ((p_block[10] & 0x40) << 5) | ((p_block[11] & 0x40) << 4)
        | ((p_block[12] & 0x40) << 3) | ((p_block[13] & 0x40) << 2) | ((p_block[14] & 0x40) << 1) | (p_block[15] & 0x40);

    return (msb1 << 8) | (msb2 >> 7);
}

static void vec_residual_compute_4x4(const uint8_t *p_block, uint32_t hash, uint8_t *p_residual) {
    uint32_t *p1 = (void *)p_residual;
    uint32_t *p2 = p1 + 1;
    uint32_t *p3 = p2 + 1;
    uint8_t tmp_block[16];
    memcpy(tmp_block, p_block, sizeof(tmp_block));
    for (int i = 0; i < 16; i++) {
        tmp_block[i] <<= 2;
    }
    *p1 = vec_hash_compute_4x4(tmp_block);
    for (int i = 0; i < 16; i++) {
        tmp_block[i] <<= 2;
    }
    *p2 = vec_hash_compute_4x4(tmp_block);
    for (int i = 0; i < 16; i++) {
        tmp_block[i] <<= 2;
    }
    *p3 = vec_hash_compute_4x4(tmp_block);
}

static uint32_t vcodec_code_block(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_block, uint32_t block_width, uint32_t (*hasher)(const uint8_t *), void (*residual)(const uint8_t *, uint32_t, uint8_t *)) {
    vcodec_vec_ctx_t *vec_ctx = p_ctx->encoder_ctx;
    const uint32_t hash = hasher(p_block);
    uint32_t vec_index = 0;
    const int count = vec_table_lookup(vec_ctx->vec_table, hash, &vec_index);
    if (0 == count) {
        vcodec_output_escape(p_ctx);
        vcodec_output_immediate(p_ctx, p_block, block_width);
    } else {
        vcodec_bit_buffer_write(p_ctx, 0, 1);
        uint8_t residual_block[block_width * block_width];
        residual(p_block, hash, residual_block);
        vcodec_output_vector(p_ctx, count, vec_table_get_total(vec_ctx->vec_table), vec_index);
        vcodec_output_residual(p_ctx, block_width, residual_block);
    }
    vec_table_update(vec_ctx->vec_table, hash);
}

static vcodec_status_t vcodec_output_escape(vcodec_enc_ctx_t *p_ctx) {
    //printf("E ");
    return vcodec_bit_buffer_write(p_ctx, 1, 1);
}

static vcodec_status_t vcodec_output_immediate(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_block, uint32_t block_width) {
    for (int i = 0; i < 16; i++) {
        vcodec_bit_buffer_write(p_ctx, p_block[i], 8);
    }
}

static vcodec_status_t vcodec_output_vector(vcodec_enc_ctx_t *p_ctx, int count, int total, uint32_t index) {
    //printf("%d ", index);
    static int prev = -1;
    static int run_len = 0;
    const int run_len_min = 1;
    if (prev == index) {
        run_len++;
    }
    if (prev != index) {
        if (run_len >= run_len_min) {
            // Finish the run
            //printf("R %d ", run_len - run_len_min);
            // Predict run length from index, count and total:
            vcodec_code_rle(p_ctx, run_len - run_len_min);
            //vcodec_med_gr_write_golomb_rice_code(p_ctx, run_len - run_len_min, 3);
        }
        run_len = 0;
        prev = index;
    }

    if (run_len < run_len_min) {
        printf("X_%d ", index);
        vcodec_med_gr_write_golomb_rice_code(p_ctx, index, 3);
    }
}

static vcodec_status_t vcodec_output_residual(vcodec_enc_ctx_t *p_ctx, uint32_t block_width, const uint8_t *p_residual) {
    //printf("D %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x \n", p_residual[0], p_residual[1], p_residual[2], p_residual[3],
    //p_residual[4], p_residual[5], p_residual[6], p_residual[7],
    //p_residual[8], p_residual[9], p_residual[10], p_residual[11]);
    //const uint8_t *r = p_residual;
    //printf("D %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x \n", r[0], (uint8_t)(r[0] - p_residual[1]), (uint8_t)(r[1] - p_residual[2]), (uint8_t)(r[2] - p_residual[3]),
    //(uint8_t)(r[3] - p_residual[4]), (uint8_t)(r[4] - p_residual[5]), (uint8_t)(r[5] - p_residual[6]), (uint8_t)(r[6] - p_residual[7]),
    //(uint8_t)(r[7] - p_residual[8]), (uint8_t)(r[8] - p_residual[9]), (uint8_t)(r[9] - p_residual[10]), (uint8_t)(r[10] - p_residual[11]));
    for (int i = 0; i < 12; i++) {
        vcodec_bit_buffer_write(p_ctx, p_residual[i], 8);
    }
}

static vcodec_status_t vcodec_vec_process_key_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame) {
    uint8_t temp_line[p_ctx->width];
    memset(temp_line, 0, sizeof(temp_line));
    const uint8_t *p_prev_line = temp_line;
    const uint8_t *p_frame_start = p_frame;
    for (int i = 0; i < p_ctx->height; i++) {
        vcodec_status_t status = vcodec_med_gr_dpcm_med_predictor_golomb(p_ctx, p_frame, p_prev_line);
        if (VCODEC_STATUS_OK != status) {
            return status;
        }
        p_prev_line = p_frame;
        p_frame += p_ctx->width;
    }
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_code_rle(vcodec_enc_ctx_t *p_ctx, int run_length) {
    vcodec_vec_ctx_t *vec_ctx = p_ctx->encoder_ctx;
    uint32_t vec_index = 0;
    const int count = vec_table_lookup(vec_ctx->rle_table, run_length, &vec_index);
    if (0 == count) {
        vcodec_output_escape(p_ctx);
        printf("R_%d ", run_length);
        vcodec_med_gr_write_golomb_rice_code(p_ctx, run_length, 3);
    } else {
        printf("VI_%d ", vec_index);
        vcodec_bit_buffer_write(p_ctx, 0, 1);
        vcodec_med_gr_write_golomb_rice_code(p_ctx, vec_index, 3);
    }
    vec_table_update(vec_ctx->rle_table, run_length);
}
