/**
 * @file fft.h
 * @author Matvey Kolesov (kolesov645@gmail.com)
 * @brief Contains functions for efficiently calculating Discrete Fourier transform.
 * @date 2024-01-18
 *
 * @copyright Copyright (c) 2024
 */

#ifndef __RS_GF65536_FFT_H__
#define __RS_GF65536_FFT_H__

#include <stdint.h>

#include "../../../../../helpers.h"
#include "gf65536.h"
#include "prelude.h"
#include "seq.h"
#include "util.h"

/**
 * @brief Compute a given number of first components of Discrete Fourier transform of a given sequence using cyclotomic
 * FFT algorithm.
 *
 * @param cnx context object.
 * @param gf Galois field data.
 * @param f sequence coefficients.
 * @param positions sequence coefficients indices.
 * @param res where to place the result.
 * @return 0 on success, 1 on memory allocation error.
 */
static inline __attribute__((always_inline)) int
fft_transform_cycl(picoquic_cnx_t* cnx, GF_t* gf, const symbol_seq_t* f, const uint16_t* positions, symbol_seq_t* res) {
    assert(gf != NULL);
    assert(f != NULL);
    assert(positions != NULL);
    assert(res != NULL);
    assert(f->symbol_size == res->symbol_size);

    bool* calculated = (bool*)my_calloc(cnx, res->length, sizeof(bool));
    if (!calculated)
        return 1;

    symbol_seq_t* u = seq_create(cnx, CC_MAX_COSET_SIZE, f->symbol_size);
    if (!u) {
        my_free(cnx, calculated);
        return 1;
    }

    for (uint16_t s = 0; s < res->length; ++s) {
        if (calculated[s])
            continue;

        uint8_t m = cc_get_coset_size(s);

        for (uint8_t t = 0; t < m; ++t)
            my_memset((void*)u->symbols[t]->data, 0, f->symbol_size);

        for (uint16_t i = 0; i < f->length; ++i) {
            uint16_t repr = gf_get_normal_repr(gf, m, (s * positions[i]) % N);

            for (uint8_t t = 0; t < m; ++t) {
                if (repr & (1 << t))
                    gf_add((void*)u->symbols[t]->data, (void*)f->symbols[i]->data, f->symbol_size);
            }
        }

        uint16_t idx = s;
        for (uint8_t j = 0; j < m; ++j) {
            if (idx < res->length) {
                my_memset((void*)res->symbols[idx]->data, 0, f->symbol_size);

                for (uint8_t t = 0; t < m; ++t) {
                    element_t coef = gf_get_normal_basis_element(gf, m, (j + t) % m);
                    gf_madd(gf, (void*)res->symbols[idx]->data, coef, (void*)u->symbols[t]->data, f->symbol_size);
                }

                calculated[idx] = true;
            }

            idx = NEXT_COSET_ELEMENT(idx);
        }

        assert(idx == s);
    }

    seq_destroy(cnx, u);
    my_free(cnx, calculated);

    return 0;
}

/**
 * @brief Compute some components of Discrete Fourier transform of a given sequence using cyclotomic FFT algorithm.
 *
 * @param cnx context object.
 * @param gf Galois field data.
 * @param f sequence coefficients.
 * @param cosets cyclotomic cosets that forms negative components to be computed.
 * @param cosets_cnt number of cyclotomic cosets.
 * @param res where to place the result.
 * @return 0 on success, 1 on memory allocation error.
 */
static inline __attribute__((always_inline)) int fft_partial_transform_cycl(picoquic_cnx_t* cnx, GF_t* gf,
                                                                            const symbol_seq_t* f,
                                                                            const coset_t* cosets, uint16_t cosets_cnt,
                                                                            symbol_seq_t* res) {
    assert(gf != NULL);
    assert(f != NULL);
    assert(cosets != NULL);
    assert(res != NULL);
    assert(f->symbol_size == res->symbol_size);

    uint16_t idx = 0;

    symbol_seq_t* u = seq_create(cnx, CC_MAX_COSET_SIZE, f->symbol_size);
    if (!u)
        return 1;

    for (const coset_t* end = cosets + cosets_cnt; cosets != end; ++cosets) {
        uint16_t s = N - cosets->leader;
        uint8_t m = cosets->size;

        for (uint8_t t = 0; t < cosets->size; ++t)
            my_memset((void*)u->symbols[t]->data, 0, f->symbol_size);

        for (uint16_t i = 0; i < f->length; ++i) {
            uint16_t repr = gf_get_normal_repr(gf, m, (s * i) % N);

            for (uint8_t t = 0; t < cosets->size; ++t) {
                if (repr & (1 << t))
                    gf_add((void*)u->symbols[t]->data, f->symbols[i]->data, f->symbol_size);
            }
        }

        for (uint8_t j = 0; j < cosets->size; ++j, ++idx) {
            assert(idx < res->length);

            my_memset((void*)res->symbols[idx]->data, 0, f->symbol_size);

            for (uint8_t t = 0; t < cosets->size; ++t) {
                element_t coef = gf_get_normal_basis_element(gf, m, (j + t) % m);
                gf_madd(gf, (void*)res->symbols[idx]->data, coef, (void*)u->symbols[t]->data, f->symbol_size);
            }
        }
    }

    assert(idx == res->length);

    seq_destroy(cnx, u);

    return 0;
}

#endif