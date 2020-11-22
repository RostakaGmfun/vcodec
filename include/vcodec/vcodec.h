#pragma once

#include <stdint.h>
#include <stddef.h>

typedef enum {
    VCODEC_STATUS_OK        =  0,
    VCODEC_STATUS_INVAL     = -1,
    VCODEC_STATUS_NOMEM     = -2,
    VCODEC_STATUS_IO_FAILED = -3,
    VCODEC_STATUS_NOENT     = -4,
    VCODEC_STATUS_EOF       = -5,
} vcodec_status_t;

typedef enum {
    VCODEC_TYPE_INVALID,
    VCODEC_TYPE_MED_GR,
    VCODEC_TYPE_INTER,
    VCODEC_TYPE_VEC,
    VCODEC_TYPE_DCT,
} vcodec_type_t;

typedef vcodec_status_t (*vcodec_write_t)(const uint8_t *p_data, uint32_t size, void *ctx);
typedef vcodec_status_t (*vcodec_read_t)(uint8_t *p_data, uint32_t size, uint32_t *num_read, void *ctx);
typedef void *(*vcodec_alloc_t)(size_t size);
typedef void (*vcodec_free_t)(void *ptr);

typedef struct vcodec_enc_ctx vcodec_enc_ctx_t;

typedef vcodec_status_t (*vcodec_enc_process_frame_t)(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame);
typedef vcodec_status_t (*vcodec_enc_reset_t)(vcodec_enc_ctx_t *p_ctx);
typedef vcodec_status_t (*vcodec_enc_deinit_t)(vcodec_enc_ctx_t *p_ctx);

typedef struct vcodec_dec_ctx vcodec_dec_ctx_t;

typedef vcodec_status_t (*vcodec_dec_get_frame_t)(vcodec_dec_ctx_t *p_ctx, uint8_t *p_frame);
typedef vcodec_status_t (*vcodec_dec_deinit_t)(vcodec_dec_ctx_t *p_ctx);

typedef struct vcodec_enc_ctx {
    uint32_t width;
    uint32_t height;
    uint8_t *p_buffer;
    size_t buffer_size;
    uint32_t bit_buffer;
    int bit_buffer_index;

    vcodec_write_t write;
    vcodec_alloc_t alloc;
    vcodec_free_t free;
    void *io_ctx;

    vcodec_enc_process_frame_t process_frame;
    vcodec_enc_reset_t reset;
    vcodec_enc_deinit_t deinit;
    vcodec_type_t encoder_type;
    void *encoder_ctx;
} vcodec_enc_ctx_t;

typedef struct vcodec_dec_ctx {
    uint32_t width;
    uint32_t height;

    vcodec_read_t read;
    vcodec_alloc_t alloc;
    vcodec_free_t free;
    void *io_ctx;

    vcodec_dec_get_frame_t get_frame;
    vcodec_dec_deinit_t deinit;
    void *decoder_ctx;

    uint32_t bit_buffer;
    int bit_buffer_index;
    int bits_available;
} vcodec_dec_ctx_t;

vcodec_status_t vcodec_enc_init(vcodec_enc_ctx_t *p_ctx, vcodec_type_t type);

vcodec_status_t vcodec_dec_init(vcodec_dec_ctx_t *p_ctx, vcodec_type_t type);
