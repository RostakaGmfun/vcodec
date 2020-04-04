#pragma once

#include <stdint.h>

/**
 * Morton code-based locality-sensitive hash for 4x4 16-byte block
 */
uint32_t vec_hash_compute_4x4(const uint8_t *p_block);

/**
 * Packs 16 6-bit pixels into 12 bytes, no compression
 */
void vec_residual_compute_4x4(const uint8_t *p_block, uint32_t hash, uint8_t *p_residual);
