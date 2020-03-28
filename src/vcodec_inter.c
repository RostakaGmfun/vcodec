#include "vcodec/vcodec.h"
#include "vcodec_common.h"

#include <string.h>
#include <stdbool.h>
#include <limits.h>

#define INTER_BLOCK_SIZE 16
#define INTER_MAX_BLOCK_MSE INTER_BLOCK_SIZE*INTER_BLOCK_SIZE*64
#define INTER_GOP 24

#define INTER_MAX_BLOCK_DISPLACEMENT 30

#define INTER_MIN_RLE_LEN 2
#define INTER_MAX_GR_PARAM 9

typedef struct {
    int gop;
    int state;
    int num_matched_blocks;
    int prev_num_matched_blocks;
} inter_encoder_ctx_t;

static vcodec_status_t vcodec_inter_process_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame);
static vcodec_status_t vcodec_inter_reset(vcodec_enc_ctx_t *p_ctx);
static vcodec_status_t vcodec_inter_deinit(vcodec_enc_ctx_t *p_ctx);

static vcodec_status_t vcodec_inter_process_key_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame);
static vcodec_status_t vcodec_inter_process_mid_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame);

static int vcodec_inter_compute_mse(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_block, int x, int y);

static bool vcodec_inter_find_matching_block(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_block, int x, int y, int *p_mx, int *p_my);
static vcodec_status_t vcodec_inter_write_matched_block(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_block, int x, int y, int mx, int my);
static vcodec_status_t vcodec_inter_write_unmatched_block(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_block);

static int vcodec_int_sqrt(int val);

static int vcodec_inter_estimate_gr_param_for_block(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_block, int x, int y);

static int vcodec_write_runlen(vcodec_enc_ctx_t *p_ctx, int runlen);

vcodec_status_t vcodec_inter_init(vcodec_enc_ctx_t *p_ctx) {
    if (0 == p_ctx->width || 0 == p_ctx->height) {
        return VCODEC_STATUS_INVAL;
    }
    p_ctx->buffer_size = p_ctx->width * p_ctx->height;
    p_ctx->p_buffer = p_ctx->alloc(p_ctx->buffer_size);
    if (NULL == p_ctx->p_buffer) {
        return VCODEC_STATUS_NOMEM;
    }
    p_ctx->process_frame = vcodec_inter_process_frame;
    p_ctx->reset = vcodec_inter_reset;
    p_ctx->deinit = vcodec_inter_deinit;
    p_ctx->encoder_ctx = p_ctx->alloc(sizeof(inter_encoder_ctx_t));
    if (NULL == p_ctx->encoder_ctx) {
        return VCODEC_STATUS_NOMEM;
    }
    inter_encoder_ctx_t *p_encoder_ctx = p_ctx->encoder_ctx;
    p_encoder_ctx->gop = INTER_GOP;
    p_encoder_ctx->state = 0;
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_inter_process_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame) {
    vcodec_status_t status = VCODEC_STATUS_OK;
    inter_encoder_ctx_t *p_encoder_ctx = p_ctx->encoder_ctx;
    if (0 == p_encoder_ctx->state % p_encoder_ctx->gop) {
        printf("Key-frame\n");
        status = vcodec_inter_process_key_frame(p_ctx, p_frame);
        p_encoder_ctx->state++;
        p_encoder_ctx->prev_num_matched_blocks = -1;
    } else {
        p_encoder_ctx->num_matched_blocks = 0;
        status = vcodec_inter_process_mid_frame(p_ctx, p_frame);
        const int total_blocks = (p_ctx->width * p_ctx->height) / (INTER_BLOCK_SIZE * INTER_BLOCK_SIZE);
        printf("%d %d%%\n", p_encoder_ctx->num_matched_blocks, p_encoder_ctx->num_matched_blocks * 100 / total_blocks);
        bool reset_needed = false;
        if (-1 != p_encoder_ctx->prev_num_matched_blocks && (p_encoder_ctx->prev_num_matched_blocks * 10) / p_encoder_ctx->num_matched_blocks > 15) {
            reset_needed = true;
        }
        if (p_encoder_ctx->num_matched_blocks < total_blocks / 3) {
            reset_needed = true;
        }
        if (reset_needed) {
            printf("Reset\n");
            p_encoder_ctx->state = 0;
        } else {
            p_encoder_ctx->state++;
        }
        p_encoder_ctx->prev_num_matched_blocks = p_encoder_ctx->num_matched_blocks;
    }
    return status;
}

static vcodec_status_t vcodec_inter_reset(vcodec_enc_ctx_t *p_ctx) {
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_inter_deinit(vcodec_enc_ctx_t *p_ctx) {
    p_ctx->free(p_ctx->p_buffer);
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_inter_process_key_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame) {
    memset(p_ctx->p_buffer, 0, p_ctx->width * p_ctx->height);
    const uint8_t *p_prev_line = p_ctx->p_buffer;
    const uint8_t *p_frame_start = p_frame;
    for (int i = 0; i < p_ctx->height; i++) {
        vcodec_status_t status = vcodec_med_gr_dpcm_med_predictor_golomb(p_ctx, p_frame, p_prev_line);
        if (VCODEC_STATUS_OK != status) {
            return status;
        }
        p_prev_line = p_frame;
        p_frame += p_ctx->width;
    }
    memcpy(p_ctx->p_buffer, p_frame_start, p_ctx->buffer_size);
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_inter_process_mid_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame) {
    const int x_blocks = p_ctx->width / INTER_BLOCK_SIZE;
    const int y_blocks = p_ctx->height / INTER_BLOCK_SIZE;

    inter_encoder_ctx_t *p_encoder_ctx = p_ctx->encoder_ctx;
    vcodec_status_t status = VCODEC_STATUS_OK;
    for (int i = 0; i < y_blocks; i++) {
        const uint8_t *p_row = p_frame + p_ctx->width * i * INTER_BLOCK_SIZE;
        for (int j = 0; j < x_blocks; j++) {
            int mx = 0;
            int my = 0;
            const uint8_t *p_block = p_row + j * INTER_BLOCK_SIZE;
            const bool match_found = vcodec_inter_find_matching_block(p_ctx, p_block, j, i, &mx, &my);
            if (match_found) {
                p_encoder_ctx->num_matched_blocks++;
                status = vcodec_inter_write_matched_block(p_ctx, p_block, j, i, mx, my);
            } else {
                status = vcodec_inter_write_unmatched_block(p_ctx, p_block);
            }

            if (VCODEC_STATUS_OK != status) {
                return status;
            }
        }
    }

    return status;
}

static int vcodec_inter_compute_mse(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_block, int x, int y) {
    const uint8_t *p_prev_block = p_ctx->p_buffer + x * p_ctx->width + y;

    int mse = 0;
    for (int i = 0; i < INTER_BLOCK_SIZE; i++) {
        for (int j = 0; j < INTER_BLOCK_SIZE; j++) {
            const int diff = p_prev_block[i * INTER_BLOCK_SIZE + j] - p_block[i * INTER_BLOCK_SIZE + j];
            mse += diff * diff;
        }
    }

    return mse;
}

static bool vcodec_inter_find_matching_block(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_block, int x, int y, int *p_mx, int *p_my) {
    bool match_found = true;

    int min_mse = INT_MAX;
    for (int i = MAX(0, x * INTER_BLOCK_SIZE - INTER_MAX_BLOCK_DISPLACEMENT); i < MIN(x * INTER_BLOCK_SIZE + INTER_MAX_BLOCK_DISPLACEMENT, p_ctx->width); i++) {
        for (int j = MAX(0, y * INTER_BLOCK_SIZE - INTER_MAX_BLOCK_DISPLACEMENT); j < MIN(y * INTER_BLOCK_SIZE + INTER_MAX_BLOCK_DISPLACEMENT, p_ctx->height); j++) {
            const int mse = vcodec_inter_compute_mse(p_ctx, p_block, i, j);
            if (mse < min_mse) {
                *p_mx = i - x * INTER_BLOCK_SIZE;
                *p_my = j - y * INTER_BLOCK_SIZE;
                min_mse = mse;
                if (mse < INTER_MAX_BLOCK_MSE / (INTER_BLOCK_SIZE * INTER_BLOCK_SIZE)) {
                    goto out;
                }
            }
        }
    }

 out:
    if (min_mse < INTER_MAX_BLOCK_MSE) {
        //printf("%d ", min_mse);
        uint8_t *p_prev_block = p_ctx->p_buffer + ((x * INTER_BLOCK_SIZE) + *p_mx) * p_ctx->width + ((y * INTER_BLOCK_SIZE) + *p_my);
        for (int i = 0; i < INTER_BLOCK_SIZE; i++) {
            for (int j = 0; j < INTER_BLOCK_SIZE; j++) {
                p_prev_block[i * INTER_BLOCK_SIZE + j] = p_block[i * INTER_BLOCK_SIZE + j];
            }
        }
        return true;
    }
    return false;
}

static vcodec_status_t vcodec_inter_write_matched_block(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_block, int x, int y, int mx, int my) {
    const uint8_t *p_prev_block = p_ctx->p_buffer + x * p_ctx->width * INTER_BLOCK_SIZE + y * INTER_BLOCK_SIZE;

    int block_len = 0;

    vcodec_status_t status = VCODEC_STATUS_OK;
    const int encoded_mx = mx >= 0 ? mx * 2 : (-mx * 2 + 1);
    status = vcodec_med_gr_write_golomb_rice_code(p_ctx, encoded_mx, 1);
    if (status != VCODEC_STATUS_OK) {
        return status;
    }
    block_len += encoded_mx / 2 + 1 + 1;
    const int encoded_my = my >= 0 ? my * 2 : (-my * 2 + 1);
    status = vcodec_med_gr_write_golomb_rice_code(p_ctx, encoded_my, 1);
    if (status != VCODEC_STATUS_OK) {
        return status;
    }
    block_len += encoded_my / 2 + 1 + 1;

    const int golomb_rice_param = vcodec_inter_estimate_gr_param_for_block(p_ctx, p_block, x, y);

    status = vcodec_med_gr_write_golomb_rice_code(p_ctx, golomb_rice_param, 1);
    if (status != VCODEC_STATUS_OK) {
        return status;
    }
    block_len += golomb_rice_param / 2 + 1 + 1;

    int prev = 0;
    int rle_runs = 0;
    for (int i = 0; i < INTER_BLOCK_SIZE; i++) {
        for (int j = 0; j < INTER_BLOCK_SIZE; j++) {
            const int diff = p_prev_block[i * INTER_BLOCK_SIZE + j] - p_block[i * INTER_BLOCK_SIZE + j];
            const int encoded_value = diff >= 0 ? (diff * 2) : (-diff * 2 + 1);
            printf("%d ", encoded_value);
            if (prev == encoded_value) {
                rle_runs++;
            } else {
                if (rle_runs >= INTER_MIN_RLE_LEN) {
                    //printf("R %d ", rle_runs - INTER_MIN_RLE_LEN);
                    status = vcodec_med_gr_write_golomb_rice_code(p_ctx, rle_runs - INTER_MIN_RLE_LEN, golomb_rice_param / 2);
                    if (status != VCODEC_STATUS_OK) {
                        return status;
                    }
                    block_len += (rle_runs - INTER_MIN_RLE_LEN) >> (golomb_rice_param / 2) + 1 + (golomb_rice_param / 2);
                }
                rle_runs = 1;
            }
            if (rle_runs < INTER_MIN_RLE_LEN) {
                //const int golomb_rice_param = sizeof(int) * 8 - 1 - __builtin_clz(prev + 1);
                //printf("P %d ", encoded_value);
                status = vcodec_med_gr_write_golomb_rice_code(p_ctx, encoded_value, golomb_rice_param);
                if (status != VCODEC_STATUS_OK) {
                    return status;
                }
                block_len += ((encoded_value) >> (golomb_rice_param)) + 1 + (golomb_rice_param);
            }
            prev = encoded_value;
        }
    }

    if (rle_runs >= INTER_MIN_RLE_LEN) {
        //printf("R %d ", rle_runs - INTER_MIN_RLE_LEN);
        status = vcodec_med_gr_write_golomb_rice_code(p_ctx, rle_runs - INTER_MIN_RLE_LEN, golomb_rice_param / 2);
        if (status != VCODEC_STATUS_OK) {
            return status;
        }
        block_len += (rle_runs - INTER_MIN_RLE_LEN) >> (golomb_rice_param / 2) + 1 + (golomb_rice_param / 2);
    }

    const int ratio = (block_len * 100) / (INTER_BLOCK_SIZE * INTER_BLOCK_SIZE * 8);
    printf("\n");
    if (ratio > 50) {
        printf(">>>>>>>>>>     <<<<<<<<<\n");
    }
    printf("%d %d %d%%\n", golomb_rice_param, block_len, ratio);

    return status;
}

static vcodec_status_t vcodec_inter_write_unmatched_block(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_block) {
    int prev = p_block[0];
    int diff = prev;
    int block_len = 0;

    vcodec_status_t status = vcodec_bit_buffer_write(p_ctx, prev, 8);
    if (status != VCODEC_STATUS_OK) {
        return status;
    }
    block_len += 8;
    for (int i = 1; i < INTER_BLOCK_SIZE; i++) {
        const int golomb_rice_param = sizeof(int) * 8 - 1 - __builtin_clz(diff + (0 == diff));
        diff = prev - p_block[i];
        const int encoded_value = diff >= 0 ? (diff * 2) : (-diff * 2 + 1);
        status = vcodec_med_gr_write_golomb_rice_code(p_ctx, encoded_value, golomb_rice_param);
        block_len += 1 + encoded_value >> golomb_rice_param + golomb_rice_param;
        if (status != VCODEC_STATUS_OK) {
            return status;
        }
    }

    const uint8_t *p_prev_line = p_block;

    prev = 0;
    for (int i = 1; i < INTER_BLOCK_SIZE; i++) {
        const uint8_t *p_current_line = p_block + i * INTER_BLOCK_SIZE;
        for (int j = 0; j < INTER_BLOCK_SIZE; j++) {
            const int A = p_current_line[j - 1];
            const int B = p_prev_line[j];
            const int C = p_prev_line[j - 1];
            int predicted_value;
            if (A == B && B == C && j > 0) {
                int pp = A;
                int rle_runs = 0;
                while (i < p_ctx->width) {
                    if (pp != p_current_line[i]) {
                        break;
                    }
                    rle_runs++;
                    pp = p_current_line[i];
                    i++;
                }
                vcodec_status_t status = vcodec_med_gr_write_golomb_rice_code(p_ctx, rle_runs, 3); // TODO: predict optimal GR code parameter
                if (VCODEC_STATUS_OK != status) {
                    return status;
                }
                block_len += 1 + rle_runs >> 3 + 3;
            } else {
                if (0 == j) {
                    predicted_value = B;
                } else {
                    predicted_value = (C >= MAX(A, B)) ? MIN(A, B) : (C <= MIN(A, B)) ? MAX(A, B) : (A + B - C);
                }
                const int diff = p_current_line[j] - predicted_value;
                const int encoded_value = diff >= 0 ? (diff * 2) : (-diff * 2 + 1);
                const int golomb_rice_param = sizeof(int) * 8 - 1 - __builtin_clz(prev + (0 == prev));
                prev = encoded_value;
                //printf("%d | %d ", encoded_value, golomb_rice_param);
                status = vcodec_med_gr_write_golomb_rice_code(p_ctx, encoded_value, golomb_rice_param);
                if (status != VCODEC_STATUS_OK) {
                    return status;
                }
                block_len += 1 + encoded_value >> golomb_rice_param + golomb_rice_param;
            }
        }
        p_prev_line = p_block + i * INTER_BLOCK_SIZE;
    }
    //printf("\n");

    return VCODEC_STATUS_OK;
}

static int vcodec_int_sqrt(int val) {
    int i = 0;
    while (i * i < val) {
        i++;
    }
    return i;
}

static int vcodec_inter_estimate_gr_param_for_block(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_block, int x, int y) {
    const uint8_t *p_prev_block = p_ctx->p_buffer + x * p_ctx->width * INTER_BLOCK_SIZE + y * INTER_BLOCK_SIZE;

    int prev = 0;
    int rle_runs = 0;
    int n = 0;
    int lengths[INTER_MAX_GR_PARAM + 1] = { 0 };
    for (int i = 0; i < INTER_BLOCK_SIZE; i++) {
        for (int j = 0; j < INTER_BLOCK_SIZE; j++) {
            const int diff = p_prev_block[i * INTER_BLOCK_SIZE + j] - p_block[i * INTER_BLOCK_SIZE + j];
            const int encoded_value = diff >= 0 ? (diff * 2) : (-diff * 2 + 1);
            if (prev == encoded_value) {
                rle_runs++;
            } else {
                if (rle_runs >= INTER_MIN_RLE_LEN) {
                    //sum += rle_runs - INTER_MIN_RLE_LEN;
                }
                rle_runs = 1;
            }
            if (rle_runs < INTER_MIN_RLE_LEN) {
                //printf("PE %d ", encoded_value);
                for (int k = 0; k < INTER_MAX_GR_PARAM + 1; k++) {
                    lengths[k] += encoded_value >> k;
                }
                n++;
            }
            prev = encoded_value;
        }
    }
    if (rle_runs >= INTER_MIN_RLE_LEN) {
        //sum += rle_runs - INTER_MIN_RLE_LEN;
    }

    //printf("\nE ");

    int min_len = INT_MAX;
    int k = 0;
    for (; k < INTER_MAX_GR_PARAM + 1; k++) {
        const int len = n * (k + 1) + lengths[k];
        //printf("%d ", len);
        if (len < min_len) {
            min_len = len;
        } else if (len > min_len) {
            break;
        }
    }

    //printf("\n");

    return k > 0 ? k - 1 : 0;
}

static vcodec_status_t vcodec_write_runlen(vcodec_enc_ctx_t *p_ctx, int runlen) {
    if (0 == runlen) {
        return vcodec_bit_buffer_write(p_ctx, 0, 1);
    }
    vcodec_status_t status = vcodec_bit_buffer_write(p_ctx, 1, 1);
    if (VCODEC_STATUS_OK != status) {
        return status;
    }

    return status;
}
