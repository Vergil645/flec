/**
 * @file code_data.h
 * @author Matvey Kolesov (kolesov645@gmail.com)
 * @brief Contains code_data_t definition and operations
 * @date 2024-04-05
 *
 * @copyright Copyright (c) 2024
 */

#ifndef __RS_GF65536_CODE_DATA_H__
#define __RS_GF65536_CODE_DATA_H__

#include "../../../util/arraylist.h"
#include "../../../window_receive_buffers.h"
#include "reed_solomon.h"

/**
 * @brief Contains data for on-the-filght Reed-Solomon decoding
 */
typedef struct {
    window_source_symbol_id_t first_id; // first protected id
    uint16_t k;                         // number of source symbols
    uint16_t r;                         // number of repair symbols
    uint16_t t;                         // number of erased symbols

    uint16_t* positions; // symbol positions
    bool* is_erased;     // whether each symbol is erased or not

    union {
        symbol_seq_t* syndrome;
        symbol_seq_t* evaluator;
    } poly;
} code_data_t;

static int _code_data_init(picoquic_cnx_t* cnx, RS_t* rsd, code_data_t* data, size_t symbol_size,
                           window_source_symbol_id_t first_id, uint16_t k, uint16_t r,
                           ring_based_received_source_symbols_buffer_t* source_symbols);

static inline __attribute__((always_inline)) code_data_t*
code_data_create(picoquic_cnx_t* cnx, RS_t* rsd, size_t symbol_size, window_source_symbol_id_t first_id, uint16_t k,
                 uint16_t r, ring_based_received_source_symbols_buffer_t* source_symbols) {

    /* ===== ALLOCATION ===== */

    code_data_t* data = (code_data_t*)my_malloc(cnx, sizeof(code_data_t));
    if (!data)
        return NULL;
    my_memset(data, 0, sizeof(code_data_t));

    data->first_id = first_id;
    data->k = k;
    data->r = r;
    data->t = 0;

    data->positions = (uint16_t*)my_calloc(cnx, k + r, sizeof(uint16_t));
    if (!data->positions) {
        my_free(cnx, data);
        return NULL;
    }

    data->is_erased = (bool*)my_calloc(cnx, k + r, sizeof(bool));
    if (!data->is_erased) {
        my_free(cnx, data->positions);
        my_free(cnx, data);
        return NULL;
    }

    data->poly.syndrome = seq_create(cnx, r, symbol_size);
    if (!data->poly.syndrome) {
        my_free(cnx, data->is_erased);
        my_free(cnx, data->positions);
        my_free(cnx, data);
        return NULL;
    }

    /* ===== INITIALIZATION ===== */

    int err = _code_data_init(cnx, rsd, data, symbol_size, first_id, k, r, source_symbols);
    if (err) {
        seq_destroy(cnx, data->poly.syndrome);
        my_free(cnx, data->is_erased);
        my_free(cnx, data->positions);
        my_free(cnx, data);
        return NULL;
    }

    return data;
}

static inline __attribute__((always_inline)) int
_code_data_init(picoquic_cnx_t* cnx, RS_t* rsd, code_data_t* data, size_t symbol_size,
                window_source_symbol_id_t first_id, uint16_t k, uint16_t r,
                ring_based_received_source_symbols_buffer_t* source_symbols) {
    { // calculate positions
        uint16_t inf_max_cnt = 0;
        uint16_t rep_max_cnt = 0;
        uint16_t inf_cosets_cnt = 0;
        uint16_t rep_cosets_cnt = 0;

        cc_estimate_cosets_cnt(k, r, &inf_max_cnt, &rep_max_cnt);

        coset_t* _cosets = (coset_t*)my_calloc(cnx, inf_max_cnt + rep_max_cnt, sizeof(coset_t));
        if (!_cosets)
            return PICOQUIC_ERROR_MEMORY;

        cc_select_cosets(rsd->cc, k, r, _cosets, inf_max_cnt, &inf_cosets_cnt, _cosets + inf_max_cnt, rep_max_cnt,
                         &rep_cosets_cnt);

        cc_cosets_to_positions(_cosets, inf_cosets_cnt, data->positions, k);
        cc_cosets_to_positions(_cosets + inf_max_cnt, rep_cosets_cnt, data->positions + k, r);

        my_free(cnx, _cosets);
    }

    { // calculate source part of a syndrome
        source_symbol_t** source_symbols_array = (source_symbol_t**)my_calloc(cnx, k, sizeof(source_symbol_t*));
        if (!source_symbols_array)
            return PICOQUIC_ERROR_MEMORY;

        for (window_source_symbol_id_t i = first_id; i < first_id + k; ++i) {
            uint16_t idx = (uint16_t)(i - first_id);

            if (ring_based_source_symbols_buffer_contains(cnx, source_symbols, i)) {
                source_symbols_array[idx] =
                    &ring_based_source_symbols_buffer_get(cnx, source_symbols, i)->source_symbol;
                data->is_erased[idx] = false;
            } else {
                source_symbols_array[idx] = NULL;
                data->is_erased[idx] = true;
                ++data->t;
            }
        }

        for (uint16_t idx = k; idx < k + r; ++idx) {
            data->is_erased[idx] = true;
            ++data->t;
        }

        symbol_seq_t* inf_symbols = seq_create_from_source_symbols(cnx, symbol_size, source_symbols_array, k);
        my_free(cnx, source_symbols_array);
        if (!inf_symbols)
            return PICOQUIC_ERROR_MEMORY;

        int err = fft_transform_cycl(cnx, rsd->gf, inf_symbols, data->positions, data->poly.syndrome);
        seq_destroy_conditionally(cnx, inf_symbols);
        if (err)
            return PICOQUIC_ERROR_MEMORY;
    }

    return 0;
}

static inline __attribute__((always_inline)) void code_data_destroy(picoquic_cnx_t* cnx, code_data_t* data) {
    seq_destroy(cnx, data->poly.syndrome);
    my_free(cnx, data->is_erased);
    my_free(cnx, data->positions);
    my_free(cnx, data);
}

static int _code_data_try_to_recover(picoquic_cnx_t* cnx, RS_t* rsd, code_data_t* data,
                                     arraylist_t* recovered_source_symbols_arraylist);

static inline __attribute__((always_inline)) int
code_data_add_symbol_and_try_to_recover(picoquic_cnx_t* cnx, RS_t* rsd, code_data_t* data, uint16_t i,
                                        uint8_t* symbol_data, arraylist_t* recovered_source_symbols_arraylist) {
    if (data->t <= data->r || !data->is_erased[i])
        return 0;
    data->is_erased[i] = false;
    --data->t;

    { // add to syndome
        GF_t* gf = rsd->gf;
        size_t symbol_size = data->poly.syndrome->symbol_size;
        uint16_t r = (uint16_t)data->poly.syndrome->length;

        for (uint16_t j = 0; j < r; ++j) {
            element_t coef = gf->pow_table[(data->positions[i] * j) % N];
            gf_madd(gf, (void*)data->poly.syndrome->symbols[j]->data, coef, (void*)symbol_data, symbol_size);
        }
    }

    return _code_data_try_to_recover(cnx, rsd, data, recovered_source_symbols_arraylist);
}

static inline __attribute__((always_inline)) int
_code_data_try_to_recover(picoquic_cnx_t* cnx, RS_t* rsd, code_data_t* data,
                          arraylist_t* recovered_source_symbols_arraylist) {
    if (data->t > data->r)
        return 1;

    element_t* locator_poly = (element_t*)my_calloc(cnx, data->t + 1, sizeof(element_t));
    if (!locator_poly)
        return PICOQUIC_ERROR_MEMORY;

    { // calculate locator polynomial
        uint16_t* erased_positions = (uint16_t*)my_calloc(cnx, data->t, sizeof(uint16_t));
        if (!erased_positions) {
            my_free(cnx, locator_poly);
            return PICOQUIC_ERROR_MEMORY;
        }

        { // fill erased_positions
            uint16_t idx = 0;
            for (uint16_t i = 0; i < data->k + data->r; ++i) {
                if (!data->is_erased[i])
                    continue;
                erased_positions[idx++] = data->positions[i];
            }
            assert(idx == data->t);
        }

        _rs_get_locator_poly(rsd, erased_positions, data->t, locator_poly, data->t + 1);

        my_free(cnx, erased_positions);
    }

    GF_t* gf = rsd->gf;
    size_t symbol_size = data->poly.syndrome->symbol_size;

    { // compute evaluator poly as syndrome*locator
        for (uint16_t i = data->t - 1;; --i) {
            for (uint16_t j = 1; j < data->t - i; ++j)
                gf_madd(gf, (void*)data->poly.evaluator->symbols[i + j]->data, locator_poly[j],
                        (void*)data->poly.syndrome->symbols[i]->data, symbol_size);

            gf_mul(gf, (void*)data->poly.evaluator->symbols[i]->data, locator_poly[0], symbol_size);

            if (i == 0)
                break;
        }
    }

    { // retore erased source symbols
        for (uint16_t pos_idx = 0; pos_idx < data->k; ++pos_idx) {
            if (!data->is_erased[pos_idx])
                continue;

            window_source_symbol_t* ss = create_window_source_symbol(cnx, symbol_size);
            if (!ss) {
                my_free(cnx, locator_poly);
                return PICOQUIC_ERROR_MEMORY;
            }

            { // recovering
                element_t forney_coef = _rs_get_forney_coef(rsd, locator_poly, data->t, data->positions[pos_idx]);

                ss->id = data->first_id + pos_idx;
                my_memset((void*)ss->source_symbol._whole_data, 0, symbol_size);
                for (uint16_t i = 0; i < data->t; ++i) {
                    element_t coef =
                        gf_mul_ee(gf, forney_coef, gf->pow_table[(i * (N - data->positions[pos_idx])) % N]);
                    gf_madd(gf, (void*)ss->source_symbol._whole_data, coef,
                            (void*)data->poly.evaluator->symbols[i]->data, symbol_size);
                }
            }

            arraylist_push(cnx, recovered_source_symbols_arraylist, (uintptr_t)ss);
        }
    }

    my_free(cnx, locator_poly);

    return 0;
}

#endif /* __RS_GF65536_CODE_DATA_H__ */