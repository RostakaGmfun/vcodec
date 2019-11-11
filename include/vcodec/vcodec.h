#ifndef _VCODEC_H_
#define _VCODEC_H_

#include <stdint.h>
#include <stddef.h>

typedef enum {
    VCODEC_STATUS_OK        =  0,
    VCODEC_STATUS_INVAL     = -1,
    VCODEC_STATUS_NOMEM     = -2,
    VCODEC_STATUS_IO_FAILED = -3,
} vcodec_status_t;

typedef vcodec_status_t (*vcodec_write_t)(const uint8_t *p_data, uint32_t size, void *ctx);
typedef vcodec_status_t (*vcodec_read_t)(uint8_t *p_data, uint32_t *p_size, void *ctx);
typedef void *(*vcodec_alloc_t)(size_t size);
typedef void (*vcodec_free_t)(void *ptr);

typedef struct {
    uint32_t width;
    uint32_t height;
    vcodec_write_t write;
    vcodec_read_t read;
    vcodec_alloc_t alloc;
    vcodec_free_t free;
    void *io_ctx;
    uint8_t *p_buffer;
    size_t buffer_size;
    uint32_t bit_buffer;
    int      bit_buffer_index;
} vcodec_enc_ctx_t;

typedef struct {

} vcodec_dec_ctx_t;

vcodec_status_t vcodec_enc_init(vcodec_enc_ctx_t *p_ctx);
vcodec_status_t vcodec_enc_deinit(vcodec_enc_ctx_t *p_ctx);
vcodec_status_t vcodec_enc_reset(vcodec_enc_ctx_t *p_ctx);
vcodec_status_t vcodec_enc_process_frame(vcodec_enc_ctx_t *p_ctx);

#endif // _VCODEC_H_
