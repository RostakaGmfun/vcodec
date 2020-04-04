#pragma once

#include <stdint.h>

typedef struct vec_table vec_table_t;

vec_table_t *vec_table_init(uint32_t table_size);

/**
 * Insert/update and entry with @c hash.
 */
int vec_table_update(vec_table_t *p_table, uint32_t hash);

/**
 * Lookup freqency of the given hash. Retrieve index, where 0 is the most frequent element.
 * @retval 0 Hash not found in the table.
 */
int vec_table_lookup(const vec_table_t *p_table, uint32_t hash, uint32_t *p_index);

int vec_table_get_total(const vec_table_t *p_table);
