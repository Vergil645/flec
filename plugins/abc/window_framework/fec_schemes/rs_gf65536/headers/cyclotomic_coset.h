/**
 * @file cyclotomic_coset.h
 * @author Matvey Kolesov (kolesov645@gmail.com)
 * @brief Contains functions that uses cyclotomic cosets over GF(2) modulo N (65535).
 * @date 2024-01-17
 *
 * @copyright Copyright (c) 2024
 */

#ifndef __RS_GF65536_CYCLOTOMIC_COSET_H__
#define __RS_GF65536_CYCLOTOMIC_COSET_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../../../../helpers.h"
#include "prelude.h"
#include "util.h"

/**
 * @brief Number of different sizes of cyclotomic cosets.
 */
#define CC_COSET_SIZES_CNT 5

/**
 * @brief Number of different cyclotomic cosets.
 */
#define CC_COSETS_CNT 4115

/**
 * @brief Maximum cyclotomic coset size.
 */
#define CC_MAX_COSET_SIZE 16

/**
 * @brief Number of leaders of cyclotomic cosets of size 1.
 */
#define CC_LEADERS_1_CNT 1

/**
 * @brief Number of leaders of cyclotomic cosets of size 2.
 */
#define CC_LEADERS_2_CNT 1

/**
 * @brief Number of leaders of cyclotomic cosets of size 4.
 */
#define CC_LEADERS_4_CNT 3

/**
 * @brief Number of leaders of cyclotomic cosets of size 8.
 */
#define CC_LEADERS_8_CNT 30

/**
 * @brief Number of leaders of cyclotomic cosets of size 16.
 */
#define CC_LEADERS_16_CNT 4080

/**
 * @brief If (r > *value*), we have to use cyclotomic cosets of size 1.
 */
#define CC_THRESHOLD_1 0

/**
 * @brief If (r > *value*), we have to use cyclotomic cosets of size 2.
 */
#define CC_THRESHOLD_2 1

/**
 * @brief If (r > *value*), we have to use cyclotomic cosets of size 4.
 */
#define CC_THRESHOLD_4 3

/**
 * @brief If (r > *value*), we have to use cyclotomic cosets of size 8.
 */
#define CC_THRESHOLD_8 15

/**
 * @brief If (r > *value*), we have to use cyclotomic cosets of size 16.
 */
#define CC_THRESHOLD_16 255

/**
 * @brief Next cyclotomic coset element.
 * @details \f$s_{i+1} = \equiv s_i * 2 \mod N\f$
 *
 * @param _s current coset element.
 * @return cyclotomic coset element following _s.
 */
#define NEXT_COSET_ELEMENT(_s) ((uint16_t)(((uint32_t)(_s) << 1) % 65535))

/**
 * @brief Initialize cyclotomic coset by given values.
 *
 * @param _coset cyclotomic coset structure to be initialized.
 * @param _leader cyclotomic coset leader.
 * @param _size cyclotomic coset size.
 *
 * @warning Macro defines local variable with name "__coset_tmp__" in the inner
 * scope.
 */
#define INIT_COSET(_coset, _leader, _size)                                                                             \
    do {                                                                                                               \
        coset_t __coset_tmp__ = {                                                                                      \
            .leader = (_leader),                                                                                       \
            .size = (_size),                                                                                           \
        };                                                                                                             \
        (_coset) = __coset_tmp__;                                                                                      \
    } while (0)

/**
 * @brief  Number of leaders of cyclotomic cosets of a certain size.
 */
static const uint16_t g_leaders_cnt[CC_COSET_SIZES_CNT] = {[0] = CC_LEADERS_1_CNT,
                                                           [1] = CC_LEADERS_2_CNT,
                                                           [2] = CC_LEADERS_4_CNT,
                                                           [3] = CC_LEADERS_8_CNT,
                                                           [4] = CC_LEADERS_16_CNT};

/**
 * @brief Thresholds for using cyclotomic cosets of a certain size.
 */
static const uint16_t g_thresholds[CC_COSET_SIZES_CNT] = {
    [0] = CC_THRESHOLD_1, [1] = CC_THRESHOLD_2, [2] = CC_THRESHOLD_4, [3] = CC_THRESHOLD_8, [4] = CC_THRESHOLD_16};

/**
 * @brief Cyclotomic coset over GF(2) modulo N.
 */
typedef struct coset {
    uint16_t leader;
    uint8_t size;
} coset_t;

/**
 * @brief Cyclotomic cosets over GF(2) modulo N pre-computed data.
 */
typedef struct CC {
    /**
     * @brief Leaders of cyclotomic cosets.
     * @details leaders[i] - leaders of cyclotomic cosets of size \f$2^i\f$.\n
     * All members are pointers to different parts of one allocated memory fragment.\n
     * leaders[0] - pointer to the beginning of this fragment.
     */
    uint16_t* leaders[CC_COSET_SIZES_CNT];

    /**
     * @brief .leaders memory.
     */
    uint16_t _leaders_memory[CC_COSETS_CNT];
} CC_t;

/**
 * @brief Create cyclotomic cosets data structure.
 *
 * @param cnx context object.
 * @return pointer to created cyclotomic cosets data structure on success and NULL otherwise.
 */
static inline __attribute__((always_inline)) CC_t* cc_create(picoquic_cnx_t* cnx) {
    CC_t* cc;

    cc = (CC_t*)my_malloc(cnx, sizeof(CC_t));
    if (!cc)
        return NULL;
    my_memset((void*)cc, 0, sizeof(CC_t));

    cc->leaders[0] = cc->_leaders_memory;
    for (uint8_t i = 1; i < CC_COSET_SIZES_CNT; ++i)
        cc->leaders[i] = cc->leaders[i - 1] + g_leaders_cnt[i - 1];

    bool* processed;
    uint16_t** leaders = cc->leaders;
    uint16_t idx[CC_COSET_SIZES_CNT] = {0}; // idx[i] - index in leaders[i]
    uint16_t cur_coset_elem;
    uint8_t coset_size;
    uint8_t i;

    processed = (bool*)my_calloc(cnx, N, sizeof(bool));
    if (!processed) {
        my_free(cnx, cc);
        return NULL;
    }

    for (uint16_t s = 0; s < N; ++s) {
        if (processed[s])
            continue;
        processed[s] = true;

        cur_coset_elem = NEXT_COSET_ELEMENT(s);
        for (coset_size = 1; coset_size <= CC_MAX_COSET_SIZE; ++coset_size) {
            if (cur_coset_elem == s)
                break;
            processed[cur_coset_elem] = true;
            cur_coset_elem = NEXT_COSET_ELEMENT(cur_coset_elem);
        }

        i = 0;
        while (i < CC_COSET_SIZES_CNT && (1 << i) != coset_size)
            ++i;

        assert(i < CC_COSET_SIZES_CNT);
        assert(idx[i] < g_leaders_cnt[i]);

        leaders[i][idx[i]++] = s;
    }

    assert(idx[0] == CC_LEADERS_1_CNT);
    assert(idx[1] == CC_LEADERS_2_CNT);
    assert(idx[2] == CC_LEADERS_4_CNT);
    assert(idx[3] == CC_LEADERS_8_CNT);
    assert(idx[4] == CC_LEADERS_16_CNT);

    my_free(cnx, processed);

    return cc;
}

/**
 * @brief Destroy cyclotomic cosets data structure.
 *
 * @param cnx context object.
 * @param cc cyclotomic cosets data.
 */
static inline __attribute__((always_inline)) void cc_destroy(picoquic_cnx_t* cnx, CC_t* cc) {
    assert(cc != NULL);

    my_free(cnx, cc);
}

/**
 * @brief Return size of the cyclotomic coset with the given leader.
 *
 * @param leader cyclotomic coset leader.
 * @return cyclotomic coset size.
 */
static inline __attribute__((always_inline)) uint8_t cc_get_coset_size(uint16_t leader) {
    uint8_t m = 1; // cylotomic coset size

    while (leader != (uint16_t)(((uint32_t)leader << m) % N))
        m <<= 1;
    assert(m <= CC_MAX_COSET_SIZE);

    return m;
}

/**
 * @brief Compute a number of cyclotomic cosets the union of which has a given size.
 *
 * @param r union size.
 * @return number of cyclotomic cosets.
 */
static inline __attribute__((always_inline)) uint16_t _cc_get_cosets_cnt(uint16_t r) {
    uint16_t cosets_cnt = 0;
    uint16_t cosets_cnt_inc;

    for (uint8_t i = CC_COSET_SIZES_CNT - 1; r != 0; --i) {
        if (r > g_thresholds[i]) {
            cosets_cnt_inc = (r - g_thresholds[i] + (1 << i) - 1) >> i;
            cosets_cnt += cosets_cnt_inc;
            r -= cosets_cnt_inc << i;
        }
        if (i == 0)
            break;
    }

    assert(r == 0);

    return cosets_cnt;
}

/**
 * @brief Estimate upper limits on the number of cyclotomic cosets that will be selected by cc_select_cosets(...).
 *
 * @param k number of information symbols.
 * @param r number of repair symbols.
 * @param inf_max_cnt upper limit on the number of information symbol cosets.
 * @param rep_max_cnt upper limit on the number of repair symbol cosets.
 */
static inline __attribute__((always_inline)) void cc_estimate_cosets_cnt(uint16_t k, uint16_t r, uint16_t* inf_max_cnt,
                                                                         uint16_t* rep_max_cnt) {
    *inf_max_cnt = _cc_get_cosets_cnt(k); // upper limit
    *rep_max_cnt = _cc_get_cosets_cnt(r); // accurate estimation
}

/**
 * @brief Select cyclotomic cosets over GF(2) modulo N that form information and repair symbol positions in virtual
 * codeword.
 *
 * @param cc cyclotomic cosets data.
 * @param k number of information symbols.
 * @param r number of repair symbols.
 * @param inf_cosets where to place information symbol cosets.
 * @param inf_max_cnt max number of elements that can be written to inf_leaders.
 * @param inf_cosets_cnt where to place number of written information symbol cosets.
 * @param rep_cosets where to place repair symbol cosets.
 * @param rep_max_cnt max number of elements that can be written to rep_leaders.
 * @param rep_cosets_cnt where to place number of written repair symbol cosets.
 */
static inline __attribute__((always_inline)) void cc_select_cosets(CC_t* cc, uint16_t k, uint16_t r,
                                                                   coset_t* inf_cosets, uint16_t inf_max_cnt,
                                                                   uint16_t* inf_cosets_cnt, coset_t* rep_cosets,
                                                                   uint16_t rep_max_cnt, uint16_t* rep_cosets_cnt) {
    assert(cc != NULL);
    assert(k + r <= N);
    assert(inf_cosets != NULL);
    assert(inf_cosets_cnt != NULL);
    assert(rep_cosets != NULL);
    assert(rep_cosets_cnt != NULL);

    uint16_t idx[CC_COSET_SIZES_CNT] = {0}; // idx[i] - index in leaders[i]

    { // repair symbol positions
        uint16_t rep_idx = 0;

        for (uint8_t i = CC_COSET_SIZES_CNT - 1; r != 0; --i) {
            while (r > g_thresholds[i] && rep_idx < rep_max_cnt) {
                assert(idx[i] < g_leaders_cnt[i]);

                INIT_COSET(rep_cosets[rep_idx++], cc->leaders[i][idx[i]++], 1 << i);
                r -= 1 << i;
            }
            if (i == 0)
                break;
        }

        assert(r == 0);

        *rep_cosets_cnt = rep_idx;
    }

    { // source symbol positions
        uint16_t inf_idx = 0;
        uint16_t inf_thresholds[CC_COSET_SIZES_CNT] = {0}; // inf_threshold[i] - threshold for
                                                           // using cyclotomic cosets of size
                                                           // 2^i for information symbols

        for (uint8_t i = 0; i < CC_COSET_SIZES_CNT; ++i)
            inf_thresholds[i] = g_thresholds[i];
        for (uint8_t i = 0; i < CC_COSET_SIZES_CNT - 1; ++i) {
            for (uint8_t j = i + 1; j < CC_COSET_SIZES_CNT; ++j)
                inf_thresholds[j] -= idx[i] << i;
        }

        for (uint8_t i = CC_COSET_SIZES_CNT - 1; k != 0; --i) {
            while (k > inf_thresholds[i] && inf_idx < inf_max_cnt) {
                assert(idx[i] < g_leaders_cnt[i]);

                INIT_COSET(inf_cosets[inf_idx++], cc->leaders[i][idx[i]++], 1 << i);
                k -= MIN(k, 1 << i);
            }
            if (i == 0)
                break;
        }

        assert(k == 0);

        *inf_cosets_cnt = inf_idx;
    }
}

/**
 * @brief Convert list of cyclotomic cosets to list of symbol positions.
 *
 * @param cosets cyclotomic cosets.
 * @param cosets_cnt number of cyclotomic cosets.
 * @param positions where to place symbol positions.
 * @param positions_cnt required number of positions to be written.
 */
static inline __attribute__((always_inline)) void cc_cosets_to_positions(const coset_t* cosets, uint16_t cosets_cnt,
                                                                         uint16_t* positions, uint16_t positions_cnt) {
    assert(cosets != NULL);
    assert(positions != NULL);

    uint16_t positions_idx = 0;

    for (uint16_t i = 0; i < cosets_cnt && positions_idx < positions_cnt; ++i) {
        uint16_t s;
        uint16_t cur_coset_elem;

        s = cosets[i].leader;
        positions[positions_idx++] = s;

        cur_coset_elem = NEXT_COSET_ELEMENT(s);
        while (cur_coset_elem != s && positions_idx < positions_cnt) {
            positions[positions_idx++] = cur_coset_elem;
            cur_coset_elem = NEXT_COSET_ELEMENT(cur_coset_elem);
        }
    }

    assert(positions_idx == positions_cnt);
}

#endif