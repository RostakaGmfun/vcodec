#pragma once

#include <stdint.h>
#include <string.h>

#include "vcodec.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define VCODEC_BITSTREAM_WRITER_BUFFER_LEN 8
#define VCODEC_BITSTREAM_READER_BUFFER_LEN 8

typedef struct vcodec_bitstream_writer {
    uint8_t buffer[VCODEC_BITSTREAM_WRITER_BUFFER_LEN];
    uint32_t bit_pos;
    void *p_io_ctx;
    vcodec_write_t write;
    vcodec_status_t last_status;
} vcodec_bitstream_writer_t;

typedef struct vcodec_bitstream_reader {
    uint8_t buffer[VCODEC_BITSTREAM_READER_BUFFER_LEN];
    uint32_t bits_available;
    uint32_t bit_pos;
    void *p_io_ctx;
    vcodec_read_t read;
    vcodec_status_t last_status;
} vcodec_bitstream_reader_t;

/**
 * Check I/O status (not done in all below functions for performance reasons).
 */
static vcodec_status_t vcodec_bitstream_writer_status(vcodec_bitstream_writer_t *p_writer) {
    return p_writer->last_status;
}

/**
 * Discard buffered data and reset to initial state.
 */
static inline void vcodec_bitstream_writer_reset(vcodec_bitstream_writer_t *p_writer) {
    memset(p_writer->buffer, 0, sizeof(p_writer->buffer));
    p_writer->bit_pos = 0;
    p_writer->last_status = VCODEC_STATUS_OK;
}

/**
 * Flush all buffered data and pad the last byte with zero bits.
 */
static inline void vcodec_bitstream_writer_flush(vcodec_bitstream_writer_t *p_writer) {
    p_writer->last_status = p_writer->write(p_writer->buffer, (p_writer->bit_pos + 7) / 8, p_writer->p_io_ctx);
    p_writer->bit_pos = 0;
    memset(p_writer->buffer, 0, sizeof(p_writer->buffer));
}

/**
 * Write internal buffer if full.
 */
static inline void vcodec_bitstream_writer_checkflush(vcodec_bitstream_writer_t *p_writer) {
    if (sizeof(p_writer->buffer) * 8 == p_writer->bit_pos) {
        vcodec_bitstream_writer_flush(p_writer);
    }
}

/**
 * Write @c count bits starting from LSB of @c bits into the buffered bitstream.
 * @note 0 < count < 32
 */
static inline void vcodec_bitstream_writer_putbits(vcodec_bitstream_writer_t *p_writer, uint32_t bits, uint32_t count) {
    while (count > 0) {
        const uint32_t to_write = MIN(count, 8 - p_writer->bit_pos % 8);
        const uint32_t byte_offset = p_writer->bit_pos / 8;
        const uint32_t bit_offset = (7 - p_writer->bit_pos % 8) - to_write + 1;
        const uint32_t mask = (1 << to_write) - 1;
        const uint8_t data = (bits >> (count - to_write)) & mask;
        p_writer->buffer[byte_offset] |= data << bit_offset;
        count -= to_write;
        p_writer->bit_pos += to_write;
        vcodec_bitstream_writer_checkflush(p_writer);
    }
}

/**
 * Write @c count one-bits into the buffered bitstream.
 */
static inline void vcodec_bitstream_writer_putones(vcodec_bitstream_writer_t *p_writer, uint32_t count) {
    while (count > 0) {
        const uint32_t to_write = MIN(count, 8 - p_writer->bit_pos % 8);
        const uint32_t byte_offset = p_writer->bit_pos / 8;
        const uint32_t bit_offset = (7 - p_writer->bit_pos % 8) - to_write + 1;
        const uint8_t mask = (1 << to_write) - 1;
        p_writer->buffer[byte_offset] |= mask << bit_offset;
        count -= to_write;
        p_writer->bit_pos += to_write;
        vcodec_bitstream_writer_checkflush(p_writer);
    }
}

/**
 * Write @c count zero-bits into the buffered bitstream.
 */
static inline void vcodec_bitstream_writer_putzeroes(vcodec_bitstream_writer_t *p_writer, uint32_t count) {
    while (count > 0) {
        const uint32_t to_write = MIN(count, sizeof(p_writer->buffer) * 8 - p_writer->bit_pos);
        p_writer->bit_pos += to_write;
        count -= to_write;
        vcodec_bitstream_writer_checkflush(p_writer);
    }
}

/**
 * Write @c unsigned integer encoded with exp-Golomb encoding.
 */
static inline void vcodec_bitstream_writer_write_exp_golomb(vcodec_bitstream_writer_t *p_writer, uint32_t value) {
    const int num_bits = sizeof(uint32_t) * 8 - __builtin_clz(value + 1);
    vcodec_bitstream_writer_putzeroes(p_writer, num_bits - 1);
    vcodec_bitstream_writer_putbits(p_writer, value + 1, num_bits);
}

static inline vcodec_status_t vcodec_bitstream_reader_status(vcodec_bitstream_reader_t *p_reader) {
    return p_reader->last_status;
}

/**
 * Refill buffer if needed.
 */
static inline void vcodec_bitstream_reader_check_refill(vcodec_bitstream_reader_t *p_reader) {
    if (p_reader->bit_pos == p_reader->bits_available) {
        uint32_t bytes_read = 0;
        p_reader->last_status = p_reader->read(p_reader->buffer, sizeof(p_reader->buffer), &bytes_read, p_reader->p_io_ctx);
        p_reader->bits_available = bytes_read * 8;
        p_reader->bit_pos = 0;
    }
}

/**
 * Get @c count bits into the LSBs of @c *p_out from a buffered bitstream.
 * @note 0 < count <= 32
 */
static inline void vcodec_bitstream_reader_getbits(vcodec_bitstream_reader_t *p_reader, uint32_t *p_out, uint32_t count) {
    *p_out = 0;
    while (count > 0) {
        vcodec_bitstream_reader_check_refill(p_reader);
        const uint32_t to_read = MIN(count, 8 - p_reader->bit_pos % 8);
        *p_out <<= to_read;
        const uint32_t byte_offset = p_reader->bit_pos / 8;
        const uint32_t bit_offset = (7 - p_reader->bit_pos % 8) - to_read + 1;
        const uint8_t mask = ((1 << to_read) - 1) << bit_offset;
        *p_out |= (p_reader->buffer[byte_offset] & mask) >> bit_offset;
        count -= to_read;
        p_reader->bit_pos += to_read;
    }
}

static inline uint32_t vcodec_bitstream_reader_getzeroes(vcodec_bitstream_reader_t *p_reader) {
    uint32_t ret = 0;
    do {
        vcodec_bitstream_reader_check_refill(p_reader);
        const uint32_t to_read = 8 - p_reader->bit_pos % 8;
        const uint32_t byte_offset = p_reader->bit_pos / 8;
        const uint32_t bit_offset = (7 - p_reader->bit_pos % 8) - to_read + 1;
        const uint8_t mask = ((1 << to_read) - 1) << bit_offset;
        // For 32-bit word with highest `to_read` bits of data + 1 bit always set to use _builtin_clz even when all read bits are zero
        // d...d   1   0...0
        //   ^           ^
        // to_read   31 - to_read
        const uint8_t data = p_reader->buffer[byte_offset] & mask;
        const uint32_t reg = (data << (32 - bit_offset - to_read)) | (1 << (31 - bit_offset - to_read));
        const uint32_t num_zeroes = __builtin_clz(reg);
        ret += num_zeroes;
        p_reader->bit_pos += num_zeroes;
        if (to_read != num_zeroes) {
            break;
        }
    } while (1);
    return ret;
}

/**
 * Get @c count bits into the LSBs of @c *p_out from a buffered bitstream.
 * @note 0 < count <= 32
 */
static inline uint32_t vcodec_bitstream_reader_read_exp_golomb(vcodec_bitstream_reader_t *p_reader) {
    const uint32_t num_bits = vcodec_bitstream_reader_getzeroes(p_reader) + 1;
    uint32_t ret;
    vcodec_bitstream_reader_getbits(p_reader, &ret, num_bits);
    return ret - 1;
}
