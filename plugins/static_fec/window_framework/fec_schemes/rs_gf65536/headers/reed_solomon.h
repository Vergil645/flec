/**
 * @file reed_solomon.h
 * @author Matvey Kolesov (kolesov645@gmail.com)
 * @brief Contains implementaion of Reed-Solomon codes over GF(65536).
 * @date 2024-01-18
 *
 * @copyright Copyright (c) 2024
 */

#ifndef __RS_GF65536_H__
#define __RS_GF65536_H__

#include <stdbool.h>
#include <stdint.h>

#include "../../../types.h"
#include "cyclotomic_coset.h"
#include "fft.h"

/**
 * @brief Maximum number of cyclotomic coset locator polynomial coefficients.
 */
#define RS_COSET_LOCATOR_MAX_LEN 17

/**
 * @brief Return code for cases when erases cannot be restored due to code
 * parameters.
 */
#define RS_ERR_CANNOT_RESTORE 100

/**
 * @brief Context data.
 */
typedef struct RS {
    GF_t* gf;
    CC_t* cc;
} RS_t;

/**
 * @brief Create context object.
 *
 * @param cnx context object.
 * @return pointer to created context object on success and NULL otherwise.
 */
static inline __attribute__((always_inline)) RS_t* rs_create(picoquic_cnx_t* cnx) {
    RS_t* rs;

    rs = (RS_t*)my_malloc(cnx, sizeof(RS_t));
    if (!rs)
        return NULL;
    my_memset((void*)rs, 0, sizeof(RS_t));

    PROTOOP_PRINTF(cnx, "CREATE GF_t\n");
    rs->gf = gf_create(cnx);
    if (!rs->gf) {
        my_free(cnx, rs);
        return NULL;
    }

    PROTOOP_PRINTF(cnx, "CREATE CC_t\n");
    rs->cc = cc_create(cnx);
    if (!rs->cc) {
        gf_destroy(cnx, rs->gf);
        my_free(cnx, rs);
        return NULL;
    }

    PROTOOP_PRINTF(cnx, "DONE RS_t CREATION\n");

    return rs;
}

/**
 * @brief Destroy context object.
 *
 * @param cnx context object.
 * @param rs Reed-Solomon data.
 */
static inline __attribute__((always_inline)) void rs_destroy(picoquic_cnx_t* cnx, RS_t* rs) {
    assert(rs != NULL);

    cc_destroy(cnx, rs->cc);
    gf_destroy(cnx, rs->gf);
    my_free(cnx, rs);
}

/**
 * @brief Compute syndrome polynomial.
 *
 * @param cnx context object.
 * @param rs Reed-Solomon data.
 * @param seq symbol sequence.
 * @param positions symbol positions.
 * @param syndrome_poly where to place the result.
 * @return 0 on success, !0 on error.
 */
static inline __attribute__((always_inline)) int _rs_get_syndrome_poly(picoquic_cnx_t* cnx, RS_t* rs,
                                                                       const symbol_seq_t* seq,
                                                                       const uint16_t* positions,
                                                                       symbol_seq_t* syndrome_poly) {
    assert(rs != NULL);
    assert(seq != NULL);
    assert(positions != NULL);
    assert(syndrome_poly != NULL);

    int err;

    err = fft_transform_cycl(cnx, rs->gf, seq, positions, syndrome_poly);
    if (err)
        return err;

    return 0;
}

/**
 * @brief Compute locator polynomial.
 *
 * @param rs Reed-Solomon data.
 * @param positions positions.
 * @param positions_cnt number of positions.
 * @param locator_poly where to place locator polynomial coefficients.
 * @param locator_max_len max number of locator polynomial coefficients.
 */
static inline __attribute__((always_inline)) void _rs_get_locator_poly(RS_t* rs, const uint16_t* positions,
                                                                       uint16_t positions_cnt, element_t* locator_poly,
                                                                       uint16_t locator_max_len) {
    assert(rs != NULL);
    assert(positions != NULL);
    assert(locator_poly != NULL);
    assert(positions_cnt + 1 <= locator_max_len);

    locator_poly[0] = 1;
    for (uint16_t d = 0; d < positions_cnt; ++d) {
        locator_poly[d + 1] = 0;

        element_t coef = rs->gf->pow_table[positions[d]];
        for (uint16_t i = d + 1; i > 0; --i)
            locator_poly[i] ^= gf_mul_ee(rs->gf, locator_poly[i - 1], coef);
    }
}

/**
 * @brief Compute repair symbols locator polynomial.
 * @details All locator polynomial coefficients will belongs to GF(2) subfield ({0, 1}) of GF(65536).
 * Locator polynomial will have degree equal to number of repair symbols.
 *
 * @param cnx context object.
 * @param rs Reed-Solomon data.
 * @param r number of repair symbols.
 * @param rep_cosets cyclotomic cosets that form repair symbol positions.
 * @param rep_cosets_cnt number of cyclotomic cosets.
 * @param locator_poly where to place locator polynomial coefficients.
 * @param locator_max_len max number of locator polynomial coefficients.
 */
static inline __attribute__((always_inline)) int
_rs_get_rep_symbols_locator_poly(picoquic_cnx_t* cnx, RS_t* rs, uint16_t r, const coset_t* rep_cosets,
                                 uint16_t rep_cosets_cnt, element_t* locator_poly, uint16_t locator_max_len) {
    assert(rs != NULL);
    assert(rep_cosets != NULL);
    assert(locator_poly != NULL);
    assert(r + 1 <= locator_max_len);

    element_t* coset_locator_poly = (element_t*)my_calloc(cnx, RS_COSET_LOCATOR_MAX_LEN, sizeof(element_t));
    if (!coset_locator_poly)
        return PICOQUIC_ERROR_MEMORY;

    uint16_t d; // locator polynomial degree

    d = 0;
    my_memset((void*)locator_poly, 0, (r + 1) * sizeof(element_t));
    locator_poly[0] = 1;

    for (uint16_t coset_idx = 0; coset_idx < rep_cosets_cnt; ++coset_idx) {
        coset_t coset = rep_cosets[coset_idx];

        { // get coset locator poly
            uint16_t coset_elements[CC_MAX_COSET_SIZE];

            coset_elements[0] = coset.leader;
            for (uint16_t i = 1; i < coset.size; ++i)
                coset_elements[i] = NEXT_COSET_ELEMENT(coset_elements[i - 1]);

            my_memset((void*)coset_locator_poly, 0, RS_COSET_LOCATOR_MAX_LEN * sizeof(element_t));

            _rs_get_locator_poly(rs, coset_elements, coset.size, coset_locator_poly, RS_COSET_LOCATOR_MAX_LEN);
        }

        for (uint16_t i = d;; --i) {
            if (locator_poly[i] == 1) {
                for (uint16_t j = 1; j <= coset.size; ++j)
                    locator_poly[i + j] ^= coset_locator_poly[j];
            }
            if (i == 0)
                break;
        }

        d += coset.size;
        assert(locator_poly[d] == 1);
    }

    assert(d == r);

    my_free(cnx, coset_locator_poly);

    return 0;
}

/**
 * @brief Compute Forney coefficient for given symbol postion.
 *
 * @param rs Reed-Solomon data.
 * @param locator_poly locator polynomial.
 * @param d degree of locator polynomial (number of repair symbols or erasures).
 * @param pos symbol position.
 * @return Forney coefficient.
 */
static inline __attribute__((always_inline)) element_t _rs_get_forney_coef(RS_t* rs, const element_t* locator_poly,
                                                                           uint16_t d, uint16_t pos) {
    assert(rs != NULL);
    assert(locator_poly != NULL);

    GF_t* gf = rs->gf;
    element_t* pow_table = gf->pow_table;
    element_t p; // divisible element = alpha^{position}
    element_t q; // divisor = locator_poly'(alpha^{-position})

    p = pow_table[pos];
    q = 0;
    for (uint16_t j = 0; j < d; j += 2) {
        element_t coef = locator_poly[j + 1];

        if (coef == 0) {
            continue;
        } else if (coef == 1) {
            q ^= pow_table[(j * (N - pos)) % N];
        } else {
            q ^= gf_mul_ee(gf, pow_table[(j * (N - pos)) % N], coef);
        }
    }

    return gf_div_ee(gf, p, q);
}

/**
 * @brief Compute evaluator polynomial modulo x^t (t - number of repair symbols or erasures).
 *
 * @param rs Reed-Solomon data.
 * @param syndrome_poly information symbols syndrome polynomial (deg == t - 1).
 * @param locator_poly repair symbols locator polynomial (deg == t).
 * @param evaluator_poly where to place the result.
 */
static inline __attribute__((always_inline)) void _rs_get_evaluator_poly(RS_t* rs, const symbol_seq_t* syndrome_poly,
                                                                         const element_t* locator_poly,
                                                                         symbol_seq_t* evaluator_poly) {
    assert(rs != NULL);
    assert(syndrome_poly != NULL);
    assert(locator_poly != NULL);
    assert(evaluator_poly != NULL);
    assert(syndrome_poly->symbol_size == evaluator_poly->symbol_size);

    uint16_t r = syndrome_poly->length;

    // Evaluator polynomial initialization.
    for (uint16_t i = 0; i < r; ++i)
        my_memset((void*)evaluator_poly->symbols[i]->data, 0, evaluator_poly->symbol_size);

    for (uint16_t i = 0; i < r; ++i) {
        element_t coef = locator_poly[i];

        if (coef == 0)
            continue;

        for (uint16_t j = 0; j < r - i; ++j)
            gf_madd(rs->gf, (void*)evaluator_poly->symbols[i + j]->data, coef, (void*)syndrome_poly->symbols[j]->data,
                    evaluator_poly->symbol_size);
    }
}

/**
 * @brief Compute repair symbols.
 *
 * @param cnx context object.
 * @param rs Reed-Solomon data.
 * @param locator_poly repair symbols locator polynomial.
 * @param evaluator_poly repair symbols evaluator polynomial.
 * @param rep_positions repair symbol positions.
 * @param rep_cosets repair symbol positions in form of cyclotomic cosets union.
 * @param rep_cosets_cnt number of repair symbol cyclotomic cosets.
 * @param rep_symbols where to place the result.
 * @return 0 on success, !0 on error.
 */
static inline __attribute__((always_inline)) int
_rs_get_repair_symbols(picoquic_cnx_t* cnx, RS_t* rs, const element_t* locator_poly, const symbol_seq_t* evaluator_poly,
                       const uint16_t* rep_positions, const coset_t* rep_cosets, uint16_t rep_cosets_cnt,
                       symbol_seq_t* rep_symbols) {
    assert(rs != NULL);
    assert(locator_poly != NULL);
    assert(evaluator_poly != NULL);
    assert(rep_positions != NULL);
    assert(rep_cosets != NULL);
    assert(rep_symbols != NULL);
    assert(evaluator_poly->symbol_size == rep_symbols->symbol_size);

    GF_t* gf = rs->gf;
    size_t symbol_size = evaluator_poly->symbol_size;
    uint16_t r = rep_symbols->length;
    int err;

    err = fft_partial_transform_cycl(cnx, gf, evaluator_poly, rep_cosets, rep_cosets_cnt, rep_symbols);
    if (err)
        return err;

    for (uint16_t i = 0; i < r; ++i) {
        element_t coef = _rs_get_forney_coef(rs, locator_poly, r, rep_positions[i]);
        gf_mul(gf, (void*)rep_symbols->symbols[i]->data, coef, symbol_size);
    }

    return 0;
}

/**
 * @brief Restore erased symbols if it is possible.
 *
 * @param rs Reed-Solomon data.
 * @param k number of information symbols.
 * @param locator_poly erased symbols locator polynomial.
 * @param evaluator_poly erased symbols evaluator polynomial.
 * @param positions positions of all symbols.
 * @param is_erased indicates which symbols has been erased.
 * @param rcv_symbols received symbols, restored symbols will be written here.
 */
static inline __attribute__((always_inline)) void
_rs_restore_erased(RS_t* rs, uint16_t k, const element_t* locator_poly, const symbol_seq_t* evaluator_poly,
                   const uint16_t* positions, const bool* is_erased, symbol_seq_t* rcv_symbols) {
    assert(rs != NULL);
    assert(locator_poly != NULL);
    assert(evaluator_poly != NULL);
    assert(positions != NULL);
    assert(is_erased != NULL);
    assert(rcv_symbols != NULL);
    assert(evaluator_poly->symbol_size == rcv_symbols->symbol_size);

    GF_t* gf = rs->gf;
    size_t symbol_size = evaluator_poly->symbol_size;
    element_t* pow_table = gf->pow_table;
    element_t forney_coef;
    element_t coef;
    uint16_t t = evaluator_poly->length;
    uint16_t pos;
    uint16_t j;

    for (uint16_t id = 0; id < k; ++id) {
        if (!is_erased[id])
            continue;

        pos = positions[id];
        forney_coef = _rs_get_forney_coef(rs, locator_poly, t, pos);

        my_memset((void*)rcv_symbols->symbols[id]->data, 0, symbol_size);

        j = (N - pos) % N;

        for (uint16_t i = 0; i < t; ++i) {
            coef = gf_mul_ee(gf, forney_coef, pow_table[(i * j) % N]);
            gf_madd(gf, (void*)rcv_symbols->symbols[id]->data, coef, (void*)evaluator_poly->symbols[i]->data,
                    symbol_size);
        }
    }
}

/**
 * @brief Temporary data for encoding.
 */
typedef struct {
    size_t symbol_size;
    uint16_t k;
    uint16_t r;
    uint16_t inf_max_cnt;
    uint16_t rep_max_cnt;
    uint16_t inf_cosets_cnt;
    uint16_t rep_cosets_cnt;
    coset_t* _cosets;
    coset_t* inf_cosets;
    coset_t* rep_cosets;
    uint16_t* positions;
    uint16_t* inf_positions;
    uint16_t* rep_positions;
    element_t* locator_poly;
    symbol_seq_t* syndrome_poly;
    symbol_seq_t* evaluator_poly;
} encoding_temporary_data_t;

static inline __attribute__((always_inline)) encoding_temporary_data_t*
_etd_create(picoquic_cnx_t* cnx, size_t symbol_size, uint16_t k, uint16_t r) {
    encoding_temporary_data_t* etd = (encoding_temporary_data_t*)my_malloc(cnx, sizeof(encoding_temporary_data_t));
    if (!etd)
        return NULL;
    my_memset(etd, 0, sizeof(encoding_temporary_data_t));

    etd->symbol_size = symbol_size;
    etd->k = k;
    etd->r = r;

    cc_estimate_cosets_cnt(k, r, &etd->inf_max_cnt, &etd->rep_max_cnt);

    etd->_cosets = (coset_t*)my_calloc(cnx, etd->inf_max_cnt + etd->rep_max_cnt, sizeof(coset_t));
    if (!etd->_cosets) {
        my_free(cnx, etd);
        return NULL;
    }
    etd->inf_cosets = etd->_cosets;
    etd->rep_cosets = etd->_cosets + etd->inf_max_cnt;

    etd->positions = (uint16_t*)my_calloc(cnx, k + r, sizeof(uint16_t));
    if (!etd->positions) {
        my_free(cnx, etd->_cosets);
        my_free(cnx, etd);
        return NULL;
    }
    etd->inf_positions = etd->positions;
    etd->rep_positions = etd->positions + k;

    etd->locator_poly = (element_t*)my_calloc(cnx, r + 1, sizeof(element_t));
    if (!etd->locator_poly) {
        my_free(cnx, etd->positions);
        my_free(cnx, etd->_cosets);
        my_free(cnx, etd);
        return NULL;
    }

    etd->syndrome_poly = seq_create(cnx, r, symbol_size);
    if (!etd->syndrome_poly) {
        my_free(cnx, etd->locator_poly);
        my_free(cnx, etd->positions);
        my_free(cnx, etd->_cosets);
        my_free(cnx, etd);
        return NULL;
    }

    etd->evaluator_poly = seq_create(cnx, r, symbol_size);
    if (!etd->evaluator_poly) {
        seq_destroy(cnx, etd->syndrome_poly);
        my_free(cnx, etd->locator_poly);
        my_free(cnx, etd->positions);
        my_free(cnx, etd->_cosets);
        my_free(cnx, etd);
        return NULL;
    }

    return etd;
}

static inline __attribute__((always_inline)) void _etd_destroy(picoquic_cnx_t* cnx, encoding_temporary_data_t* etd) {
    seq_destroy(cnx, etd->evaluator_poly);
    seq_destroy(cnx, etd->syndrome_poly);
    my_free(cnx, etd->locator_poly);
    my_free(cnx, etd->positions);
    my_free(cnx, etd->_cosets);
    my_free(cnx, etd);
}

/**
 * @brief Generate repair symbols for the given information symbols.
 *
 * @param cnx context object.
 * @param rs Reed-Solomon data.
 * @param inf_symbols information symbols.
 * @param rep_symbols where to place the result.
 * @return 0 on success, 1 on memory allocation error.
 */
static inline __attribute__((always_inline)) int
rs_generate_repair_symbols(picoquic_cnx_t* cnx, RS_t* rs, const symbol_seq_t* inf_symbols, symbol_seq_t* rep_symbols) {
    assert(rs != NULL);
    assert(inf_symbols != NULL);
    assert(rep_symbols != NULL);
    assert(inf_symbols->length + rep_symbols->length <= N);
    assert(inf_symbols->symbol_size == rep_symbols->symbol_size);

    int ret = 0;

    encoding_temporary_data_t* etd =
        _etd_create(cnx, inf_symbols->symbol_size, inf_symbols->length, rep_symbols->length);
    if (!etd)
        return PICOQUIC_ERROR_MEMORY;

    cc_select_cosets(rs->cc, etd->k, etd->r, etd->inf_cosets, etd->inf_max_cnt, &etd->inf_cosets_cnt, etd->rep_cosets,
                     etd->rep_max_cnt, &etd->rep_cosets_cnt);

    cc_cosets_to_positions(etd->inf_cosets, etd->inf_cosets_cnt, etd->inf_positions, etd->k);
    cc_cosets_to_positions(etd->rep_cosets, etd->rep_cosets_cnt, etd->rep_positions, etd->r);

    ret = _rs_get_syndrome_poly(cnx, rs, inf_symbols, etd->inf_positions, etd->syndrome_poly);
    if (ret != 0) {
        _etd_destroy(cnx, etd);
        return ret;
    }

    ret = _rs_get_rep_symbols_locator_poly(cnx, rs, etd->r, etd->rep_cosets, etd->rep_cosets_cnt, etd->locator_poly,
                                           etd->r + 1);
    if (ret != 0) {
        _etd_destroy(cnx, etd);
        return ret;
    }

    _rs_get_evaluator_poly(rs, etd->syndrome_poly, etd->locator_poly, etd->evaluator_poly);

    ret = _rs_get_repair_symbols(cnx, rs, etd->locator_poly, etd->evaluator_poly, etd->rep_positions, etd->rep_cosets,
                                 etd->rep_cosets_cnt, rep_symbols);

    _etd_destroy(cnx, etd);

    return ret;
}

#endif