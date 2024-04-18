/**
 * @file gf65536.h
 * @author Matvey Kolesov (kolesov645@gmail.com)
 * @brief Contains implementation of a Galois field of size 65536.
 * @date 2024-01-17
 *
 * @copyright Copyright (c) 2024
 */

#ifndef __RS_GF65536_GF65536_H__
#define __RS_GF65536_GF65536_H__

#include <stdint.h>

#include "../../../../../helpers.h"
#include "cyclotomic_coset.h"
#include "prelude.h"
#include "util.h"

/**
 * @brief Galois field size. Equal to (N + 1).
 */
#define GF_FIELD_SIZE 65536

/**
 * @brief Primitive polynomial of a Galois field: x^16 + x^5 + x^3 + x^2 + 1.
 * @details Primitive element: \f$\alpha = x\f$.
 */
#define GF_PRIMITIVE_POLY 65581

/**
 * @brief Number of elements in normal bases of all GF(65536) subfields.
 */
#define GF_NORMAL_BASES_ELEMENTS 31

/**
 * @brief Index in normal_bases array of the first basis element for the specified subfield.
 */
#define GF_NORMAL_BASES_FIRST_IDX_BY_M(_m) ((_m)-1)

/**
 * @brief Galois field element type.
 */
typedef uint16_t element_t;

/**
 * @brief Polynomial type.
 */
typedef uint32_t poly_t;

/**
 * @brief Galois field data.
 * @details Field definition: \f$GF(2)[x] / \left<PRIMITIVE\_POLY\right>\f$.\n
 * Primitive element: \f$\alpha = x\f$.
 */
typedef struct GF_t {
    /**
     * @brief Primitive element powers. Power can belongs to range [0; 2*N-2];
     * @details \f$pow\_table_i = \alpha^i\f$
     */
    element_t pow_table[(N << 1) - 1];

    /**
     * @brief Logarithm to the base of a primitive element.
     * @details \f$log\_table_e = d\f$ s.t. \f$\alpha^d = e\f$
     */
    uint16_t log_table[GF_FIELD_SIZE];

    /**
     * @brief Normal bases of all GF(65536) subfields.
     */
    element_t normal_bases[GF_NORMAL_BASES_ELEMENTS];

    /**
     * @brief Coefficients in normal basis of subfields GF(2^m).
     * @details j-th bit of normal_repr_by_subfield[m][d] - j-th coefficient in normal basis of \f$GF(2^m)\f$ subfield
     * of element \f$\alpha^d\f$.
     */
    uint16_t* normal_repr_by_subfield[CC_MAX_COSET_SIZE + 1];

    /**
     * @brief .normal_repr_by_subfield memory.
     */
    uint16_t _normal_repr_by_subfield_memory[CC_COSET_SIZES_CNT * N];
} GF_t;

/**
 * @brief Fill normal bases array.
 *
 * @param normal_bases array to be filled.
 */
static inline __attribute__((always_inline)) void _gf_fill_normal_bases(element_t* normal_bases) {
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(1) + 0] = 1;

    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(2) + 0] = 44234;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(2) + 1] = 44235;

    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(4) + 0] = 10800;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(4) + 1] = 47860;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(4) + 2] = 34555;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(4) + 3] = 5694;

    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(8) + 0] = 16402;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(8) + 1] = 53598;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(8) + 2] = 44348;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(8) + 3] = 63986;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(8) + 4] = 22060;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(8) + 5] = 64366;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(8) + 6] = 6088;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(8) + 7] = 32521;

    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(16) + 0] = 2048;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(16) + 1] = 2880;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(16) + 2] = 7129;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(16) + 3] = 30616;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(16) + 4] = 2643;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(16) + 5] = 6897;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(16) + 6] = 29685;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(16) + 7] = 7378;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(16) + 8] = 30100;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(16) + 9] = 2743;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(16) + 10] = 20193;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(16) + 11] = 36223;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(16) + 12] = 24055;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(16) + 13] = 41458;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(16) + 14] = 41014;
    normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(16) + 15] = 61451;
}

/**
 * @brief Create Galois field data structure.
 *
 * @param cnx context object.
 * @return pointer to created Galois field data structure on success and NULL otherwise.
 */
static inline __attribute__((always_inline)) GF_t* gf_create(picoquic_cnx_t* cnx) {
    GF_t* gf;

    gf = (GF_t*)my_malloc(cnx, sizeof(GF_t));
    if (!gf)
        return NULL;
    my_memset((void*)gf, 0, sizeof(GF_t));

    _gf_fill_normal_bases(gf->normal_bases);

    gf->normal_repr_by_subfield[1] = gf->_normal_repr_by_subfield_memory;
    for (uint8_t i = 1; i < CC_COSET_SIZES_CNT; ++i)
        gf->normal_repr_by_subfield[1 << i] = gf->normal_repr_by_subfield[1 << (i - 1)] + N;

    element_t* pow_table = gf->pow_table;
    uint16_t** normal_repr_by_subfield = gf->normal_repr_by_subfield;
    uint16_t* log_table = gf->log_table;
    poly_t cur_poly = 1;

    for (uint16_t i = 0; i < N; ++i) {
        pow_table[i] = (element_t)cur_poly;
        log_table[pow_table[i]] = i;

        cur_poly <<= 1;
        if (cur_poly & GF_FIELD_SIZE)
            cur_poly ^= GF_PRIMITIVE_POLY;
    }

    for (uint32_t i = N; i < (N << 1) - 1; ++i)
        pow_table[i] = pow_table[i - N];

    for (uint8_t i = 0; i < CC_COSET_SIZES_CNT; ++i) {
        uint8_t m = 1 << i; // cyclotomic coset size
        element_t* normal_basis = gf->normal_bases + GF_NORMAL_BASES_FIRST_IDX_BY_M(m);

        my_memset((void*)normal_repr_by_subfield[m], 0, N * sizeof(uint16_t));

        for (uint32_t repr = 1; repr != (1 << m); ++repr) {
            element_t elem = 0;
            for (uint8_t j = 0; j < m; ++j) {
                if (repr & (1 << j))
                    elem ^= normal_basis[j];
            }

            assert(elem != 0);
            assert(normal_repr_by_subfield[m][log_table[elem]] == 0);

            normal_repr_by_subfield[m][log_table[elem]] = (uint16_t)repr;
        }
    }

    return gf;
}

/**
 * @brief Destroy Galois field data structure.
 *
 * @param cnx context object.
 * @param gf Galois field data.
 */
static inline __attribute__((always_inline)) void gf_destroy(picoquic_cnx_t* cnx, GF_t* gf) {
    assert(gf != NULL);

    my_free(cnx, gf);
}

/**
 * @brief Return i-th element of the normal basis of the subfield GF(2^m)
 *
 * @param gf Galois field data.
 * @param m subfield power.
 * @param i element index.
 * @return normal basis element.
 * @warning pre: i < m
 */
static inline __attribute__((always_inline)) element_t gf_get_normal_basis_element(GF_t* gf, uint8_t m, uint8_t i) {
    assert(gf != NULL);
    assert(i < m);

    return gf->normal_bases[GF_NORMAL_BASES_FIRST_IDX_BY_M(m) + i];
}

/**
 * @brief Return coefficients of alpha^d in normal basis of subfield GF(2^m).
 *
 * @param gf Galois field data.
 * @param m subfield power.
 * @param d primitive element power.
 * @return normal basis representation.
 */
static inline __attribute__((always_inline)) uint16_t gf_get_normal_repr(GF_t* gf, uint8_t m, uint16_t d) {
    assert(gf != NULL);

    return gf->normal_repr_by_subfield[m][d];
}

/**
 * @brief Compute multiplication of 2 elements in Galois field.
 * @details Use pre-computed data to optimize computation process:
 * \f$a * b = \alpha^{(\log_{\alpha} a + \log_{\alpha} b) \mod N}\f$
 *
 * @param gf Galois field data.
 * @param a first multiplier.
 * @param b second multiplier.
 * @return multiplication result.
 */
static inline __attribute__((always_inline)) element_t gf_mul_ee(GF_t* gf, element_t a, element_t b) {
    assert(gf != NULL);

    if (a == 0 || b == 0)
        return 0;

    uint16_t* log_table = gf->log_table;
    uint32_t a_log = (uint32_t)log_table[a];
    uint32_t b_log = (uint32_t)log_table[b];

    return gf->pow_table[a_log + b_log];
}

/**
 * @brief Compute quotient of 2 elements in Galois field.
 * @details Use pre-computed data to optimize computation process:
 * \f$a / b = \alpha^{(\log_{\alpha} a - \log_{\alpha} b) \mod N}\f$
 *
 * @param gf Galois field data.
 * @param a divisible element.
 * @param b divisor.
 * @return division result.
 */
static inline __attribute__((always_inline)) element_t gf_div_ee(GF_t* gf, element_t a, element_t b) {
    assert(gf != NULL);
    assert(b != 0);

    if (a == 0)
        return 0;

    uint16_t* log_table = gf->log_table;

    return gf->pow_table[(N + (uint32_t)log_table[a] - (uint32_t)log_table[b]) % N];
}

/**
 * @brief Compute the sum of 2 elements in Galois field.
 *
 * @param a first element (result will be placed here).
 * @param b second element.
 * @param symbol_size symbol size (must be divisible by 2).
 */
static inline __attribute__((always_inline)) void gf_add(void* a, const void* b, size_t symbol_size) {
    uint64_t* data64_1 = (uint64_t*)a;
    uint64_t* data64_2 = (uint64_t*)b;

    for (const uint64_t* end64_1 = data64_1 + symbol_size / sizeof(uint64_t); data64_1 != end64_1;
         ++data64_1, ++data64_2)
        *data64_1 ^= *data64_2;

    element_t* data_1 = (element_t*)data64_1;
    element_t* data_2 = (element_t*)data64_2;

    for (const element_t* end_1 = (element_t*)a + symbol_size / sizeof(element_t); data_1 != end_1; ++data_1, ++data_2)
        *data_1 ^= *data_2;
}

/**
 * @brief Compute multiplication of element and coefficient in Galois field.
 *
 * @param gf Galois field data.
 * @param a element (result will be placed here).
 * @param coef coefficient.
 * @param symbol_size symbol size (must be divisible by 2).
 */
static inline __attribute__((always_inline)) void gf_mul(GF_t* gf, void* a, element_t coef, size_t symbol_size) {
    assert(symbol_size % sizeof(element_t) == 0);

    if (coef == 0) {
        my_memset(a, 0, symbol_size);
        return;
    }

    if (coef == 1)
        return;

    element_t* pow_table_shifted;
    element_t* data = (element_t*)a;
    uint16_t* log_table = gf->log_table;

    pow_table_shifted = gf->pow_table + log_table[coef];

    for (const element_t* end = data + symbol_size / sizeof(element_t); data != end; ++data) {
        element_t val = *data;
        if (val != 0)
            *data = pow_table_shifted[log_table[val]];
    }
}

/**
 * @brief Compute "A += c * B" expression in Galois field.
 *
 * @param gf Galois field data.
 * @param a first element (result will be placed here).
 * @param coef coefficient.
 * @param b second element.
 * @param symbol_size symbol size (must be divisible by 2).
 */
static inline __attribute__((always_inline)) void gf_madd(GF_t* gf, void* a, element_t coef, const void* b,
                                                          size_t symbol_size) {
    assert(symbol_size % sizeof(element_t) == 0);

    if (coef == 0)
        return;

    if (coef == 1) {
        gf_add(a, b, symbol_size);
        return;
    }

    element_t* pow_table_shifted;
    element_t* data_1 = (element_t*)a;
    element_t* data_2 = (element_t*)b;
    uint16_t* log_table = gf->log_table;

    pow_table_shifted = gf->pow_table + log_table[coef];

    for (const element_t* end_1 = data_1 + symbol_size / sizeof(element_t); data_1 != end_1; ++data_1, ++data_2) {
        element_t val_2 = *data_2;
        if (val_2 != 0)
            *data_1 ^= pow_table_shifted[log_table[val_2]];
    }
}

#endif