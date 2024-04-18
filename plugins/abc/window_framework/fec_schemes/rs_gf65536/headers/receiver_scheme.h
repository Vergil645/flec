#ifndef __RS_GF65536_RECEIVER_FEC_SCHEME_H__
#define __RS_GF65536_RECEIVER_FEC_SCHEME_H__

#include <red_black_tree.h>

#include "../../../../../helpers.h"
#include "../../../types.h"
#include "../../../util/arraylist.h"
#include "../../../window_receive_buffers.h"
#include "code_data.h"
#include "reed_solomon.h"

#define _RS_DATA_BUFFER_DEFAULT_MAX_SYMBOLS 2000
#define _ARRAYLIST_INITIAL_SIZE 50

typedef struct {
    RS_t* rsd;

    size_t max_symbols;
    size_t n_symbols;
    /**
     * key: last_protected_id
     * value: code_data_t*
     */
    red_black_tree_t code_data_buffer;

    arraylist_t unknown_ids;
    arraylist_t unknown_recovered; // the IDs contained MUST be ordered !
    arraylist_t temp_arraylist;
    arraylist_t recovered_source_symbols_arraylist;
} receiver_scheme_t;

static inline __attribute__((always_inline)) receiver_scheme_t* receiver_scheme_create(picoquic_cnx_t* cnx, RS_t* rsd) {
    receiver_scheme_t* scheme_receiver = (receiver_scheme_t*)my_malloc(cnx, sizeof(receiver_scheme_t));
    if (!scheme_receiver)
        return NULL;
    my_memset((void*)scheme_receiver, 0, sizeof(receiver_scheme_t));

    int err;

    scheme_receiver->rsd = rsd;
    scheme_receiver->max_symbols = _RS_DATA_BUFFER_DEFAULT_MAX_SYMBOLS;

    rbt_init(cnx, &scheme_receiver->code_data_buffer);

    err = arraylist_init(cnx, &scheme_receiver->unknown_ids, _ARRAYLIST_INITIAL_SIZE);
    if (err) {
        my_free(cnx, scheme_receiver);
        return NULL;
    }

    err = arraylist_init(cnx, &scheme_receiver->unknown_recovered, _ARRAYLIST_INITIAL_SIZE);
    if (err) {
        arraylist_destroy(cnx, &scheme_receiver->unknown_ids);
        my_free(cnx, scheme_receiver);
        return NULL;
    }

    err = arraylist_init(cnx, &scheme_receiver->temp_arraylist, _ARRAYLIST_INITIAL_SIZE);
    if (err) {
        arraylist_destroy(cnx, &scheme_receiver->unknown_recovered);
        arraylist_destroy(cnx, &scheme_receiver->unknown_ids);
        my_free(cnx, scheme_receiver);
        return NULL;
    }

    err = arraylist_init(cnx, &scheme_receiver->recovered_source_symbols_arraylist, _ARRAYLIST_INITIAL_SIZE);
    if (err) {
        arraylist_destroy(cnx, &scheme_receiver->temp_arraylist);
        arraylist_destroy(cnx, &scheme_receiver->unknown_recovered);
        arraylist_destroy(cnx, &scheme_receiver->unknown_ids);
        my_free(cnx, scheme_receiver);
        return NULL;
    }

    return scheme_receiver;
}

static inline __attribute__((always_inline)) void receiver_scheme_destroy(picoquic_cnx_t* cnx,
                                                                          receiver_scheme_t* scheme_receiver) {
    arraylist_destroy(cnx, &scheme_receiver->recovered_source_symbols_arraylist);
    arraylist_destroy(cnx, &scheme_receiver->temp_arraylist);
    arraylist_destroy(cnx, &scheme_receiver->unknown_recovered);
    arraylist_destroy(cnx, &scheme_receiver->unknown_ids);

    while (!rbt_is_empty(cnx, &scheme_receiver->code_data_buffer))
        rbt_delete_min(cnx, &scheme_receiver->code_data_buffer);

    my_free(cnx, scheme_receiver);
}

static inline __attribute__((always_inline)) int
_receiver_scheme_extend_unknown_ids(picoquic_cnx_t* cnx, receiver_scheme_t* scheme_receiver,
                                    ring_based_received_source_symbols_buffer_t* source_symbols) {
    for (int i = 0; i < arraylist_size(&scheme_receiver->temp_arraylist); ++i) {
        source_symbol_id_t current_id = arraylist_get(&scheme_receiver->temp_arraylist, i);
        if (arraylist_is_empty(&scheme_receiver->unknown_ids) ||
            arraylist_get_last(&scheme_receiver->unknown_ids) < current_id) {
            // add all the missing source symbols from the last considered in our system to the current index
            window_source_symbol_id_t initial_id =
                MAX(arraylist_get_last(&scheme_receiver->unknown_ids) + 1,
                    ring_based_source_symbols_buffer_get_first_source_symbol_id(cnx, source_symbols));
            for (source_symbol_id_t id = initial_id; id <= current_id; ++id) {
                if (!ring_based_source_symbols_buffer_contains(cnx, source_symbols, id)) {
                    // the symbol is missing
                    arraylist_push(cnx, &scheme_receiver->unknown_ids, id);
                    arraylist_push(cnx, &scheme_receiver->unknown_recovered, false);
                }
            }
        } else if (current_id < arraylist_get_first(&scheme_receiver->unknown_ids)) {
            PROTOOP_PRINTF(cnx, "should not happen: _receiver_scheme_extend_unknown_ids\n");
            return -1;
        }
    }

    return 0;
}

static inline __attribute__((always_inline)) int
_receiver_scheme_receive_source_data(picoquic_cnx_t* cnx, receiver_scheme_t* scheme_receiver, size_t symbol_size,
                                     window_source_symbol_id_t id, uint8_t* source_data) {
    int index = -1;

    if (arraylist_is_empty(&scheme_receiver->unknown_ids) || id < arraylist_get_first(&scheme_receiver->unknown_ids)) {
        // should be ignored, because unknowns_ids[0] - 1 is the highest contiguously recieved
    } else if (arraylist_is_empty(&scheme_receiver->unknown_ids) ||
               id > arraylist_get_last(&scheme_receiver->unknown_ids)) {
        // should be ignored, nothing to do right now
        // avoid adding if already recovered
    } else if (!arraylist_is_empty(&scheme_receiver->unknown_ids) &&
               (index = arraylist_index(&scheme_receiver->unknown_ids, id)) != -1 &&
               !arraylist_get(&scheme_receiver->unknown_recovered, index)) {
        // has been detected as lost and not recovered yet

        if (rbt_is_empty(cnx, &scheme_receiver->code_data_buffer)) {
            PROTOOP_PRINTF(cnx, "should not happen: _receiver_scheme_receive_source_data 1\n");
            return PICOQUIC_ERROR_UNEXPECTED_ERROR;
        }

        code_data_t* data = NULL;
        window_source_symbol_id_t last_protected_id = 0;

        if (!rbt_ceiling(cnx, &scheme_receiver->code_data_buffer, (rbt_key)id, (rbt_key*)&last_protected_id,
                         (rbt_val*)&data)) {
            PROTOOP_PRINTF(cnx, "should not happen: _receiver_scheme_receive_source_data 2\n");
            return PICOQUIC_ERROR_UNEXPECTED_ERROR;
        }

        if (id < data->first_id || id > last_protected_id) {
            PROTOOP_PRINTF(cnx, "should not happen: _receiver_scheme_receive_source_data 3\n");
            return PICOQUIC_ERROR_UNEXPECTED_ERROR;
        }

        { // add symbol and try to recover
            uint16_t i = (uint16_t)(id - data->first_id);
            code_data_add_symbol_and_try_to_recover(cnx, scheme_receiver->rsd, data, i, source_data,
                                                    &scheme_receiver->recovered_source_symbols_arraylist);
        }

        arraylist_set(&scheme_receiver->unknown_recovered, index, true);
    }

    return 0;
}

static inline __attribute__((always_inline)) int
receiver_scheme_receive_source_symbol(picoquic_cnx_t* cnx, receiver_scheme_t* scheme_receiver,
                                      window_source_symbol_t* ss) {
    size_t symbol_size = ss->source_symbol.chunk_size + 1;
    window_source_symbol_id_t id = ss->id;

    return _receiver_scheme_receive_source_data(cnx, scheme_receiver, symbol_size, id, ss->source_symbol._whole_data);
}

static inline __attribute__((always_inline)) int
_receiver_scheme_receive_fb_fec(picoquic_cnx_t* cnx, receiver_scheme_t* scheme_receiver,
                                ring_based_received_source_symbols_buffer_t* source_symbols,
                                window_repair_symbol_t* rs) {
    size_t symbol_size = rs->repair_symbol.payload_length;
    uint16_t id = rs->metadata.first_id;
    uint16_t k = rs->metadata.n_protected_symbols;
    uint16_t r = decode_u16(rs->metadata.fss.val);
    uint16_t rs_idx = decode_u16(rs->metadata.fss.val + 2);
    int ret = 0;

    PROTOOP_PRINTF(cnx, "START PROCESSING FB_FEC\n");

    if (k != 1 || r != 1 || rs_idx != 0) {
        PROTOOP_PRINTF(cnx, "should not happen: _receiver_scheme_receive_fb_fec\n");
        return PICOQUIC_ERROR_UNEXPECTED_ERROR;
    }

    if (!ring_based_source_symbols_buffer_contains(cnx, source_symbols, id)) {
        window_source_symbol_t* ss = create_window_source_symbol(cnx, symbol_size);
        if (!ss)
            return PICOQUIC_ERROR_MEMORY;

        ss->id = id;
        my_memcpy(ss->source_symbol._whole_data, rs->repair_symbol.repair_payload, symbol_size);

        {
            uint64_t pn = decode_u64(ss->source_symbol.chunk_data);
            PROTOOP_PRINTF(cnx, "FB FEC RECOVERED id=%u, pn=%u\n", id, pn);
        }

        arraylist_push(cnx, &scheme_receiver->recovered_source_symbols_arraylist, (uintptr_t)ss);
    }

    ret = _receiver_scheme_receive_source_data(cnx, scheme_receiver, symbol_size, id, rs->repair_symbol.repair_payload);

    return ret;
}

static inline __attribute__((always_inline)) int
receiver_scheme_receive_repair_symbol(picoquic_cnx_t* cnx, receiver_scheme_t* scheme_receiver,
                                      ring_based_received_source_symbols_buffer_t* source_symbols,
                                      window_repair_symbol_t* rs) {
    arraylist_reset(&scheme_receiver->temp_arraylist);

    uint16_t first_id = rs->metadata.first_id;
    uint16_t k = rs->metadata.n_protected_symbols;
    uint16_t r = decode_u16(rs->metadata.fss.val);

    if (rs->repair_symbol.is_fb_fec || (k == 1 && r == 1))
        return _receiver_scheme_receive_fb_fec(cnx, scheme_receiver, source_symbols, rs);

    code_data_t* data = NULL;
    { // get or create data
        if (!rbt_get(cnx, &scheme_receiver->code_data_buffer, (rbt_key)(first_id + k - 1), (rbt_val*)&data)) {
            if (scheme_receiver->n_symbols + r > scheme_receiver->max_symbols) {
                return 0; // not enough free space in a buffer
            }

            for (window_source_symbol_id_t i = first_id; i < first_id + k; ++i) {
                if (!ring_based_source_symbols_buffer_contains(cnx, source_symbols, i))
                    arraylist_push(cnx, &scheme_receiver->temp_arraylist, i);
            }

            if (arraylist_is_empty(&scheme_receiver->temp_arraylist)) {
                return 0; // no unknown ids in a window, ignore repair symbol
            }

            int err = _receiver_scheme_extend_unknown_ids(cnx, scheme_receiver, source_symbols);
            if (err)
                return err;

            data = code_data_create(cnx, scheme_receiver->rsd, rs->repair_symbol.payload_length, first_id, k, r,
                                    source_symbols);
            if (!data)
                return 0;

            rbt_put(cnx, &scheme_receiver->code_data_buffer, (rbt_key)(first_id + k - 1), (rbt_val)data);
            scheme_receiver->n_symbols += r;
        }
    }

    { // add symbol and try to recover
        uint16_t i = k + decode_u16(rs->metadata.fss.val + 2);
        code_data_add_symbol_and_try_to_recover(cnx, scheme_receiver->rsd, data, i, rs->repair_symbol.repair_payload,
                                                &scheme_receiver->recovered_source_symbols_arraylist);
    }

    return 0;
}

static __attribute__((always_inline)) void receiver_scheme_recover_lost_symbols(picoquic_cnx_t* cnx,
                                                                                receiver_scheme_t* scheme_receiver,
                                                                                arraylist_t* recovered_symbols,
                                                                                protoop_arg_t* recovered) {
    *recovered = 0;

    for (size_t i = 0; i < arraylist_size(&scheme_receiver->recovered_source_symbols_arraylist); ++i) {
        window_source_symbol_t* ss =
            (window_source_symbol_t*)arraylist_get(&scheme_receiver->recovered_source_symbols_arraylist, i);

        { // log recovering
            uint64_t pn = decode_u64(ss->source_symbol.chunk_data);
            PROTOOP_PRINTF(cnx, "FEC RECOVERED ID=%u PN=%u MD=%u\n", ss->id, pn, ss->source_symbol._whole_data[0]);
        }

        arraylist_push(cnx, recovered_symbols, (uintptr_t)ss);
        *recovered = 1;
    }

    // now, empty the arraylist
    arraylist_reset(&scheme_receiver->recovered_source_symbols_arraylist);
}

static __attribute__((always_inline)) int
receiver_scheme_remove_unused(picoquic_cnx_t* cnx, receiver_scheme_t* scheme_receiver,
                              window_source_symbol_id_t highest_contiguously_received_id) {
    if (arraylist_is_empty(&scheme_receiver->unknown_ids) ||
        highest_contiguously_received_id < arraylist_get_first(&scheme_receiver->unknown_ids))
        return 0;

    code_data_t* data = NULL;
    window_source_symbol_id_t last_protected_id = 0;

    while (!rbt_is_empty(cnx, &scheme_receiver->code_data_buffer) &&
           rbt_min(cnx, &scheme_receiver->code_data_buffer, (rbt_key*)&last_protected_id, (rbt_val*)&data)) {
        if (highest_contiguously_received_id < last_protected_id)
            break;

        rbt_delete(cnx, &scheme_receiver->code_data_buffer, (rbt_key)last_protected_id);
        scheme_receiver->n_symbols -= data->r;

        code_data_destroy(cnx, data);
    }

    if (rbt_is_empty(cnx, &scheme_receiver->code_data_buffer)) {
        // the buffer has been emptied
        arraylist_reset(&scheme_receiver->unknown_ids);
        arraylist_reset(&scheme_receiver->unknown_recovered);
    } else {
        size_t n_removed_unknowns = 0;
        for (size_t i = 0; i < arraylist_size(&scheme_receiver->unknown_ids); ++i) {
            if (arraylist_get(&scheme_receiver->unknown_ids, i) > highest_contiguously_received_id)
                break;
            ++n_removed_unknowns;
        }
        arraylist_shift_left(cnx, &scheme_receiver->unknown_ids, n_removed_unknowns);
        arraylist_shift_left(cnx, &scheme_receiver->unknown_recovered, n_removed_unknowns);
    }

    return 0;
}

static __attribute__((always_inline)) int
receiver_scheme_set_max_buffer_size(picoquic_cnx_t* cnx, receiver_scheme_t* scheme_receiver, uint64_t new_max_symbols) {
    scheme_receiver->max_symbols = new_max_symbols;
    return 0;
}

#endif /* __RS_GF65536_RECEIVER_FEC_SCHEME_H__ */