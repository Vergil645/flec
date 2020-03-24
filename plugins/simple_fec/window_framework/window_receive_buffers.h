#ifndef PICOQUIC_WINDOW_RECEIVE_BUFFERS_H
#define PICOQUIC_WINDOW_RECEIVE_BUFFERS_H

#include "../fec.h"
#include "types.h"
#include "search_structures.h"

typedef struct {
    min_max_pq_t pq;
} received_source_symbols_buffer_t;

static __attribute__((always_inline)) received_source_symbols_buffer_t *new_source_symbols_buffer(picoquic_cnx_t *cnx,
                                                                                                  int max_size) {
    received_source_symbols_buffer_t *buffer = my_malloc(cnx, sizeof(received_source_symbols_buffer_t));
    if (!buffer)
        return NULL;
    buffer->pq = create_min_max_pq(cnx, max_size);
    if (!buffer->pq) {
        my_free(cnx, buffer);
        return NULL;
    }
    return buffer;
}

static __attribute__((always_inline)) void release_source_symbols_buffer(picoquic_cnx_t *cnx, received_source_symbols_buffer_t *buffer) {
    delete_min_max_pq(cnx, buffer->pq);
    my_free(cnx, buffer);
}

// returns the symbol that has been removed if the buffer was full
static __attribute__((always_inline)) window_source_symbol_t *add_source_symbol(picoquic_cnx_t *cnx, received_source_symbols_buffer_t *buffer, window_source_symbol_t *ss) {
    return pq_insert_and_pop_min_if_full(buffer->pq, ss->id, ss);
}

// pre: the buffer should not be empty
static __attribute__((always_inline)) source_symbol_id_t get_first_source_symbol_id(picoquic_cnx_t *cnx, received_source_symbols_buffer_t *buffer) {
    return ((window_source_symbol_t *) pq_get_min(buffer->pq))->id;
}

// pre: the buffer should not be empty
static __attribute__((always_inline)) source_symbol_id_t get_last_source_symbol_id(picoquic_cnx_t *cnx, received_source_symbols_buffer_t *buffer) {
    return ((window_source_symbol_t *) pq_get_max(buffer->pq))->id;
}

// inserts the source symbols in the provided buffer starting with the symbol with the smallest sfpid set at the first entry of the array and does not change an entry where source symbol is missing
// returns the number of received source symbols (total - missing ones)
static __attribute__((always_inline)) int get_source_symbols_between_bounds(picoquic_cnx_t *cnx, received_source_symbols_buffer_t *buffer, window_source_symbol_t **symbols, uint32_t min_symbol, uint32_t max_symbol) {
    my_memset(symbols, 0, max_symbol + 1 - min_symbol);
    return pq_get_between_bounds_ordered(buffer->pq, min_symbol, max_symbol + 1, (void **) symbols);
}

typedef struct {
    min_max_pq_t pq;
} received_repair_symbols_buffer_t;



static __attribute__((always_inline)) received_repair_symbols_buffer_t *new_repair_symbols_buffer(picoquic_cnx_t *cnx,
                                                                                                  int max_size) {
    received_repair_symbols_buffer_t *buffer = my_malloc(cnx, sizeof(received_source_symbols_buffer_t));
    if (!buffer)
        return NULL;
    buffer->pq = create_min_max_pq(cnx, max_size);
    if (!buffer->pq) {
        my_free(cnx, buffer);
        return NULL;
    }
    return buffer;
}

static __attribute__((always_inline)) void release_repair_symbols_buffer(picoquic_cnx_t *cnx, received_repair_symbols_buffer_t *buffer) {
    delete_min_max_pq(cnx, buffer->pq);
    my_free(cnx, buffer);
}

// returns the symbol that has been removed if the buffer was full
static __attribute__((always_inline)) repair_symbol_t *add_repair_symbol(picoquic_cnx_t *cnx, received_repair_symbols_buffer_t *buffer, window_repair_symbol_t *rs) {
    // FIXME: do a simple ring buffer (is it still needed ?)
    // we order it by the last protected symbol
    PROTOOP_PRINTF(cnx, "ADD SYMBOL WITH KEY %u\n", decode_u32(rs->metadata.fss.val));
//    return pq_insert_and_pop_min_if_full(buffer->pq, rs->metadata.first_id + rs->metadata.n_protected_symbols - 1, rs);
    return pq_insert_and_pop_min_if_full(buffer->pq, decode_u32(rs->metadata.fss.val), rs);
}

// returns a symbol that has been removed if the buffer was full
static __attribute__((always_inline)) void remove_and_free_unused_repair_symbols(picoquic_cnx_t *cnx, received_repair_symbols_buffer_t *buffer, uint32_t remove_under) {
    while (!pq_is_empty(buffer->pq)) {
        window_repair_symbol_t *current = pq_get_min(buffer->pq);
        if (current->metadata.first_id + current->metadata.n_protected_symbols - 1 < remove_under) {
            pq_pop_min(buffer->pq);
            delete_window_repair_symbol(cnx, current);
        } else {
            break;
        }
    }
}

// pre: symbols must have a length of at least the max size of the buffer
static __attribute__((always_inline)) int get_repair_symbols(picoquic_cnx_t *cnx, received_repair_symbols_buffer_t *buffer, window_repair_symbol_t **symbols,
        window_source_symbol_id_t *smallest_protected, uint32_t *highest_protected, window_source_symbol_id_t highest_contiguously_received_id, uint16_t max_concerned_source_symbols) {
    if (pq_is_empty(buffer->pq))
        return 0;
    my_memset(symbols, 0, buffer->pq->max_size*sizeof(window_repair_symbol_t *));
    PROTOOP_PRINTF(cnx, "HIGHEST RECEIVED = %lu, MIN : %lu, MAX = %lu\n", highest_contiguously_received_id, pq_get_min_key(buffer->pq), pq_get_max_key(buffer->pq));
    uint64_t max_key_in_pq = pq_get_max_key(buffer->pq);
    uint64_t min_key_in_pq = pq_get_min_key(buffer->pq);
//    int added = pq_get_between_bounds(buffer->pq, MAX(min_key_in_pq, highest_contiguously_received_id), max_key_in_pq + 1, (void **) symbols);
    int added = pq_get_between_bounds(buffer->pq, min_key_in_pq, max_key_in_pq + 1, (void **) symbols);
    if (added == 0) {
        return added;
    }
    window_source_symbol_id_t min_key = symbols[0]->metadata.first_id;
    window_source_symbol_id_t max_key = symbols[0]->metadata.first_id + symbols[0]->metadata.n_protected_symbols - 1;
    PROTOOP_PRINTF(cnx, "FIRST ADDED [%u, %u]\n", symbols[0]->metadata.first_id, symbols[0]->metadata.first_id + symbols[0]->metadata.n_protected_symbols - 1);
    int n_tried = 1;
    for(int i = 1 ; n_tried < added && i < buffer->pq->max_size ; i++) {
        if (symbols[i]) {
            PROTOOP_PRINTF(cnx, "ADDED [%u, %u]\n", symbols[i]->metadata.first_id, symbols[i]->metadata.first_id + symbols[i]->metadata.n_protected_symbols - 1);
            n_tried++;
            if (symbols[i]->metadata.first_id < min_key)
                min_key = symbols[i]->metadata.first_id;
            if (symbols[i]->metadata.first_id + symbols[i]->metadata.n_protected_symbols - 1 > max_key)
                max_key = symbols[i]->metadata.first_id + symbols[i]->metadata.n_protected_symbols - 1;
        }
    }
    if (max_key - min_key > max_concerned_source_symbols) {
        // too much symbols concerned, we should prune and take the most recent repair symbols
        uint64_t new_min_key = 0, new_max_key = 0;
        int new_added = 0;
        n_tried = 0;
        for (int i = 0 ; n_tried < added && i < buffer->pq->max_size ; i++) {
            if (symbols[i]) {
                if (highest_contiguously_received_id > symbols[i]->metadata.first_id + symbols[i]->metadata.n_protected_symbols - 1 || symbols[i]->metadata.first_id <= max_key - max_concerned_source_symbols) {
                    symbols[i] = NULL;
                } else {
                    if (new_min_key == 0 || symbols[i]->metadata.first_id < new_min_key)
                        new_min_key = symbols[i]->metadata.first_id;
                    if (new_max_key == 0 || symbols[i]->metadata.first_id + symbols[i]->metadata.n_protected_symbols - 1 > new_max_key)
                        new_max_key = symbols[i]->metadata.first_id + symbols[i]->metadata.n_protected_symbols - 1;
                    n_tried++;
                    new_added++;
                }
            }
        }
        added = new_added;
        min_key = new_min_key;
        max_key = new_max_key;
    }
    *smallest_protected = min_key;
    *highest_protected = max_key;
    return added;
}

#endif //PICOQUIC_WINDOW_RECEIVE_BUFFERS_H
