#pgrama once

#include <stdint.h>

#include "vcodec.h"

typedef struct vcodec_bitstream {
    uint8_t *p_start;
    uint8_t *p_cursor;
    uint8_t *p_end;
    uint32_t bit_pos;
} vcodec_bitstream_t;

typedef struct vodec_bitstream_writer {
    vcodec_bitstream_t stream;

    void *p_io_ctx;
    vcodec_status_t (*write)(const uint8_t *p_buffer, uint32_t size, void *p_io_ctx);
} vcodec_bitstream_writer_t;

/**
 * Write @c count bits from LSBs of @c bits into the buffered bitstream.
 * @note 0 < count < 32
 */
vcodec_status_t vcodec_bitstream_writer_putbits(vcodec_bitsteam_writer_t *p_writer, uint32_t bits, uint32_t count);

/**
 * Write @c count one-bits into the buffered bitstream
 */
vcodec_status_t vcodec_bitstream_writer_putones(vcodec_bitstream_writer_t *p_writer, uint32_t count);

/**
 * Write @c count zero-bits into the buffered bitstream
 */
vcodec_status_t vcodec_bitstream_writer_putzeroes(vcodec_bitstream_writer_t *p_writer, uint32_t count);

/**
 * Write @c count bytes from @c p_data into the buffered bitstream.
 * @note The stream is implicitly flushed prior to writing to pad the data to the nearest byte boundary.
 */
vcodec_status_t vcodec_bitstream_writer_putbytes(vcodec_bit_steam_writer_t *p_writer, const uint8_t *p_data, uint32_t count);

/**
 * Flush all buffered data and pad the last byte with zero bits.
 */
vcodec_status_t vcodec_bitstream_writer_flush(vcodec_bit_steam_writer_t *p_writer);

typedef struct vodec_bitstream_reader {
    vcodec_bitstream_t stream;

    void *p_io_ctx;
    vcodec_status_t (*refill)(uint8_t *p_buffer, uint32_t size, void *p_io_ctx);
} vcodec_bitstream_reader_t;

/**
 * Get @c count bits into the LSBs of @c *p_out from a buffered bitstream.
 * @note 0 < count <= 32
 */
vcodec_status_t vcodec_bitstream_reader_getbits(vcodec_bitstream_reader_t *p_reader, uint32_t *p_out, uint32_t count);

/**
 * Get @c count bytes into the buffer pointed to by @c p_out from a buffered bitstream.
 * @note The stream is implicitly skipped up to a next byte boundary.
 */
vcodec_status_t vcodec_bitsteam_reader_getbytes(vcodec_bitstream_reader_t *p_readr, uint8_t *p_out, uint32_t count);
