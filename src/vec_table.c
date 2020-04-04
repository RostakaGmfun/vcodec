#include "vec_table.h"

#include <stdlib.h>
#include <stddef.h>

typedef struct {
    uint32_t hash;
    int count;
} vec_table_entry_t;

struct vec_table {
    vec_table_entry_t *entries;
    uint32_t table_size;
    int total;
    int last_used;
};

vec_table_t *vec_table_init(uint32_t table_size) {
    vec_table_t *p_table = calloc(1, sizeof(struct vec_table));

    if (NULL == p_table) {
        return p_table;
    }

    p_table->entries = calloc(1, sizeof(vec_table_entry_t) * table_size);
    if (NULL == p_table->entries) {
        free(p_table);
        return NULL;
    }
    p_table->table_size = table_size;
    p_table->last_used = 0;

    return p_table;
}

int vec_table_update(vec_table_t *p_table, uint32_t hash) {
    p_table->total++;
    for (int i = 0; i < p_table->table_size; i++) {
        if (hash == p_table->entries[i].hash && p_table->entries[i].count > 0) {
            p_table->entries[i].hash = hash;
            p_table->entries[i].count++;
            int pos = i;
            // Move the updated entry upwards until the table is sorted again
            while (i >= 0 && p_table->entries[i].count < p_table->entries[pos].count + 1) {
                i--;
            }
            if (i + 1 < pos) {
                uint32_t tmp_count = p_table->entries[pos].count;
                p_table->entries[pos].hash = p_table->entries[i + 1].hash;
                p_table->entries[pos].count = p_table->entries[i + 1].count;
                p_table->entries[i + 1].hash = hash;
                p_table->entries[i + 1].count = tmp_count;
            }
            return pos;
        } else if (p_table->entries[i].count == 0) {
            // Empty found - add item here
            p_table->entries[i].count++;
            p_table->entries[i].hash = hash;
            return i;
        }
    }
    // Entry not found, and table is full, recycle the last item if count is 1
    if (1 == p_table->entries[p_table->table_size - 1].count) {
        p_table->entries[p_table->table_size - 1].hash = hash;
    }
}

int vec_table_lookup(const vec_table_t *p_table, uint32_t hash, uint32_t *p_index) {
    for (int i = 0; i < p_table->table_size; i++) {
        if (hash == p_table->entries[i].hash) {
            *p_index = i;
            return p_table->entries[i].count;
        } else if (0 == p_table->entries[i].count) {
            return 0;
        }
    }

    return 0;
}

int vec_table_get_total(const vec_table_t *p_table) {
    return p_table->total;
}
