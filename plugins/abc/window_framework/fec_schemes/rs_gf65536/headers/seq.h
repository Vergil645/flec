/**
 * @file seq.h
 * @author Matvey Kolesov (kolesov645@gmail.com)
 * @brief Contains symbol_seq_t definition and functions for interaction with
 * symbol sequences.
 * @date 2024-01-23
 *
 * @copyright Copyright (c) 2024
 */

#ifndef __MEMORY_SEQ_H__
#define __MEMORY_SEQ_H__

#include <stdbool.h>

#include "../../../../../helpers.h"
#include "../../../types.h"
#include "symbol.h"
#include "util.h"

/**
 * @brief Symbol sequence data type.
 */
typedef struct symbol_seq {
    /**
     * @brief Sequence length.
     */
    size_t length;

    /**
     * @brief Symbol size (similar for each symbol in sequence).
     */
    size_t symbol_size;

    /**
     * @brief Sequence symbols.
     */
    symbol_t** symbols;
} symbol_seq_t;

/**
 * @brief Create sequence.
 *
 * @param symbol_size symbol size.
 * @param length sequence length.
 * @return pointer to created sequence on success and NULL otherwise.
 */
static inline __attribute__((always_inline)) symbol_seq_t* seq_create(picoquic_cnx_t* cnx, size_t length,
                                                                      size_t symbol_size) {
    symbol_seq_t* seq;

    seq = (symbol_seq_t*)my_malloc(cnx, sizeof(symbol_seq_t));
    if (!seq)
        return NULL;
    my_memset((void*)seq, 0, sizeof(symbol_seq_t));

    seq->length = length;
    seq->symbol_size = symbol_size;

    seq->symbols = (symbol_t**)my_calloc(cnx, length, sizeof(symbol_t*));
    if (!seq->symbols) {
        my_free(cnx, seq);
        return NULL;
    }

    for (size_t i = 0; i < length; ++i) {
        seq->symbols[i] = symbol_create(cnx, symbol_size);
        if (!seq->symbols[i]) {
            for (size_t j = 0; j < i; ++j)
                symbol_destroy(cnx, seq->symbols[j]);
            my_free(cnx, seq->symbols);
            my_free(cnx, seq);
            return NULL;
        }
    }

    return seq;
}

// pre: source_symbols[i] CAN be NULL !!!
static inline __attribute__((always_inline)) symbol_seq_t*
seq_create_from_source_symbols(picoquic_cnx_t* cnx, uint16_t symbol_size, source_symbol_t** source_symbols,
                               uint16_t n_source_symbols) {
    symbol_seq_t* seq;

    seq = (symbol_seq_t*)my_malloc(cnx, sizeof(symbol_seq_t));
    if (!seq)
        return NULL;
    my_memset((void*)seq, 0, sizeof(symbol_seq_t));

    seq->length = n_source_symbols;
    seq->symbol_size = symbol_size;

    seq->symbols = (symbol_t**)my_calloc(cnx, n_source_symbols, sizeof(symbol_t*));
    if (!seq->symbols) {
        my_free(cnx, seq);
        return NULL;
    }

    for (size_t i = 0; i < n_source_symbols; ++i) {
        seq->symbols[i] = (source_symbols[i] == NULL) ? symbol_create(cnx, symbol_size)
                                                      : symbol_create_with_data(cnx, source_symbols[i]->_whole_data);
        if (!seq->symbols[i]) {
            for (size_t j = 0; j < i; ++j)
                symbol_destroy_conditionally(cnx, seq->symbols[j]);
            my_free(cnx, seq->symbols);
            my_free(cnx, seq);
            return NULL;
        }
    }

    return seq;
}

// pre: repair_symbols[i] MUST NOT be NULL !!!
static inline __attribute__((always_inline)) symbol_seq_t*
seq_create_from_repair_symbols(picoquic_cnx_t* cnx, uint16_t symbol_size, window_repair_symbol_t** repair_symbols,
                               uint16_t n_repair_symbols) {
    symbol_seq_t* seq;

    seq = (symbol_seq_t*)my_malloc(cnx, sizeof(symbol_seq_t));
    if (!seq)
        return NULL;
    my_memset((void*)seq, 0, sizeof(symbol_seq_t));

    seq->length = n_repair_symbols;
    seq->symbol_size = symbol_size;

    seq->symbols = (symbol_t**)my_calloc(cnx, n_repair_symbols, sizeof(symbol_t*));
    if (!seq->symbols) {
        my_free(cnx, seq);
        return NULL;
    }

    for (size_t i = 0; i < n_repair_symbols; ++i) {
        seq->symbols[i] = symbol_create_with_data(cnx, repair_symbols[i]->repair_symbol.repair_payload);
        if (!seq->symbols[i]) {
            for (size_t j = 0; j < i; ++j)
                symbol_destroy_keep_data(cnx, seq->symbols[j]);
            my_free(cnx, seq->symbols);
            my_free(cnx, seq);
            return NULL;
        }
    }

    return seq;
}

/**
 * @brief Destroy sequence.
 *
 * @param seq sequence.
 */
static inline __attribute__((always_inline)) void seq_destroy(picoquic_cnx_t* cnx, symbol_seq_t* seq) {
    assert(seq != NULL);

    for (size_t i = 0; i < seq->length; ++i)
        symbol_destroy(cnx, seq->symbols[i]);
    my_free(cnx, seq->symbols);
    my_free(cnx, seq);
}

static inline __attribute__((always_inline)) void seq_destroy_conditionally(picoquic_cnx_t* cnx, symbol_seq_t* seq) {
    assert(seq != NULL);

    for (size_t i = 0; i < seq->length; ++i)
        symbol_destroy_conditionally(cnx, seq->symbols[i]);
    my_free(cnx, seq->symbols);
    my_free(cnx, seq);
}

static inline __attribute__((always_inline)) void seq_destroy_keep_symbols_data(picoquic_cnx_t* cnx, symbol_seq_t* seq) {
    assert(seq != NULL);

    for (size_t i = 0; i < seq->length; ++i)
        symbol_destroy_keep_data(cnx, seq->symbols[i]);
    my_free(cnx, seq->symbols);
    my_free(cnx, seq);
}

#endif