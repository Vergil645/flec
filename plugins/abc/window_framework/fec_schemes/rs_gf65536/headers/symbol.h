/**
 * @file symbol.h
 * @author Matvey Kolesov (kolesov645@gmail.com)
 * @brief Contains symbol_t definition and functions for interaction with
 * symbols.
 * @date 2024-01-20
 *
 * @copyright Copyright (c) 2024
 */

#ifndef __MEMORY_SYMBOL_H__
#define __MEMORY_SYMBOL_H__

#include <stdbool.h>
#include <stdint.h>

#include "../../../../../helpers.h"
#include "util.h"

/**
 * @brief Symbol data type.
 */
typedef struct symbol {
    /**
     * @brief Symbol data.
     */
    uint8_t* data;

    bool was_allocated;
} symbol_t;

/**
 * @brief Create symbol.
 *
 * @param symbol_size symbol size.
 * @return pointer to created symbol on success and NULL otherwise.
 */
static inline __attribute__((always_inline)) symbol_t* symbol_create(picoquic_cnx_t* cnx, size_t symbol_size) {
    symbol_t* s;

    s = (symbol_t*)my_malloc(cnx, sizeof(symbol_t));
    if (!s)
        return NULL;
    my_memset((void*)s, 0, sizeof(symbol_t));

    s->data = (uint8_t*)my_malloc(cnx, align(symbol_size * sizeof(uint8_t)));
    if (!s->data) {
        my_free(cnx, s);
        return NULL;
    }
    my_memset(s->data, 0, align(symbol_size * sizeof(uint8_t)));

    s->was_allocated = true;

    return s;
}

static inline __attribute__((always_inline)) symbol_t* symbol_create_with_data(picoquic_cnx_t* cnx, uint8_t* data) {
    symbol_t* s;

    s = (symbol_t*)my_malloc(cnx, sizeof(symbol_t));
    if (!s)
        return NULL;
    my_memset((void*)s, 0, sizeof(symbol_t));

    s->data = data;
    s->was_allocated = false;

    return s;
}

static inline __attribute__((always_inline)) void symbol_destroy(picoquic_cnx_t* cnx, symbol_t* s) {
    assert(s != NULL);

    my_free(cnx, s->data);
    my_free(cnx, s);
}

static inline __attribute__((always_inline)) void symbol_destroy_conditionally(picoquic_cnx_t* cnx, symbol_t* s) {
    assert(s != NULL);

    if (s->was_allocated)
        my_free(cnx, s->data);
    my_free(cnx, s);
}

static inline __attribute__((always_inline)) void symbol_destroy_keep_data(picoquic_cnx_t* cnx, symbol_t* s) {
    assert(s != NULL);

    my_free(cnx, s);
}

#endif