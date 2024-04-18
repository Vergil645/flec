#ifndef __RS_GF65536_SENDER_FEC_SCHEME_H__
#define __RS_GF65536_SENDER_FEC_SCHEME_H__

#include "../../../../../helpers.h"
#include "../../../types.h"
#include "reed_solomon.h"

typedef struct {
    RS_t* rsd;

    window_repair_symbol_t** repair_symbols;
    uint16_t cur_repair_symbol_idx;
    struct {
        window_source_symbol_id_t first_id_to_protect;
        uint16_t n_symbols_to_protect;
        uint16_t n_repair_symbols;
    } fec_params;
} sender_scheme_t;

static inline __attribute__((always_inline)) sender_scheme_t* sender_scheme_create(picoquic_cnx_t* cnx, RS_t* rsd) {
    sender_scheme_t* scheme_sender = (sender_scheme_t*)my_malloc(cnx, sizeof(sender_scheme_t));
    if (!scheme_sender)
        return NULL;
    my_memset((void*)scheme_sender, 0, sizeof(sender_scheme_t));

    scheme_sender->rsd = rsd;
    scheme_sender->repair_symbols = NULL; // allocated and destroyed in an encoding process

    return scheme_sender;
}

static inline __attribute__((always_inline)) void sender_scheme_destroy(picoquic_cnx_t* cnx,
                                                                        sender_scheme_t* scheme_sender) {
    if (scheme_sender->repair_symbols != NULL) {
        my_free(cnx, scheme_sender->repair_symbols);
        scheme_sender->repair_symbols = NULL;
    }

    my_free(cnx, scheme_sender);
}

// Create and encode retransmission symbol.
// pre: n_source_symbols == 1 && n_repair_symbols == 1
static inline __attribute__((always_inline)) int
_sender_scheme_retransmit(picoquic_cnx_t* cnx, sender_scheme_t* scheme_sender, source_symbol_t** source_symbols,
                          uint16_t n_source_symbols, window_repair_symbol_t** repair_symbols, uint16_t symbol_size,
                          window_source_symbol_id_t first_protected_id, uint16_t n_repair_symbols) {
    if (n_source_symbols != 1 || n_repair_symbols != 1) {
        PROTOOP_PRINTF(cnx, "ERROR: INCORRECT RETRANSMITION PARAMETERS!\n");
        return PICOQUIC_ERROR_UNEXPECTED_ERROR;
    }

    window_repair_symbol_t* rs = create_window_repair_symbol(cnx, symbol_size);
    if (!rs)
        return PICOQUIC_ERROR_MEMORY;

    my_memcpy(rs->repair_symbol.repair_payload, source_symbols[0]->_whole_data, symbol_size);

    rs->metadata.n_protected_symbols = 1;
    rs->metadata.first_id = first_protected_id;
    rs->repair_symbol.is_fb_fec = true;
    encode_u16(n_repair_symbols, rs->metadata.fss.val);
    encode_u16(0, rs->metadata.fss.val + 2);
    repair_symbols[0] = rs;

    return 0;
}

// Create repair symbols of a code with given parameters.
static inline __attribute__((always_inline)) int
sender_scheme_encode(picoquic_cnx_t* cnx, sender_scheme_t* scheme_sender, source_symbol_t** source_symbols,
                     uint16_t n_source_symbols, window_repair_symbol_t** repair_symbols, uint16_t symbol_size,
                     window_source_symbol_id_t first_protected_id, uint16_t n_repair_symbols) {
    if (n_source_symbols == 1 && n_repair_symbols == 1)
        return _sender_scheme_retransmit(cnx, scheme_sender, source_symbols, n_source_symbols, repair_symbols,
                                         symbol_size, first_protected_id, n_repair_symbols);

    // we don't protect same window twice
    if (first_protected_id + n_source_symbols !=
        scheme_sender->fec_params.first_id_to_protect + scheme_sender->fec_params.n_symbols_to_protect) {
        if (scheme_sender->repair_symbols != NULL) {
            my_free(cnx, scheme_sender->repair_symbols);
            scheme_sender->repair_symbols = NULL;
        }

        scheme_sender->repair_symbols =
            (window_repair_symbol_t**)my_calloc(cnx, n_repair_symbols, sizeof(window_repair_symbol_t*));
        if (!scheme_sender->repair_symbols)
            return PICOQUIC_ERROR_MEMORY;

        for (uint16_t i = 0; i < n_repair_symbols; ++i) {
            scheme_sender->repair_symbols[i] = create_window_repair_symbol(cnx, symbol_size);
            if (!scheme_sender->repair_symbols[i]) {
                for (uint16_t j = 0; j < i; ++j)
                    delete_window_repair_symbol(cnx, scheme_sender->repair_symbols[j]);
                my_free(cnx, scheme_sender->repair_symbols);
                scheme_sender->repair_symbols = NULL;
                return PICOQUIC_ERROR_MEMORY;
            }
        }

        symbol_seq_t* src_seq = seq_create_from_source_symbols(cnx, symbol_size, source_symbols, n_source_symbols);
        if (!src_seq) {
            for (uint16_t j = 0; j < n_repair_symbols; ++j)
                delete_window_repair_symbol(cnx, scheme_sender->repair_symbols[j]);
            my_free(cnx, scheme_sender->repair_symbols);
            scheme_sender->repair_symbols = NULL;
            return PICOQUIC_ERROR_MEMORY;
        }

        symbol_seq_t* rep_seq =
            seq_create_from_repair_symbols(cnx, symbol_size, scheme_sender->repair_symbols, n_repair_symbols);
        if (!rep_seq) {
            seq_destroy_keep_symbols_data(cnx, src_seq); // source_symbols[i] != NULL
            for (uint16_t j = 0; j < n_repair_symbols; ++j)
                delete_window_repair_symbol(cnx, scheme_sender->repair_symbols[j]);
            my_free(cnx, scheme_sender->repair_symbols);
            scheme_sender->repair_symbols = NULL;
            return PICOQUIC_ERROR_MEMORY;
        }

        int err = rs_generate_repair_symbols(cnx, scheme_sender->rsd, src_seq, rep_seq);
        seq_destroy_keep_symbols_data(cnx, rep_seq);
        seq_destroy_keep_symbols_data(cnx, src_seq); // source_symbols[i] != NULL
        if (err) {
            PROTOOP_PRINTF(cnx, "ERROR: ENCODING FAILED WITH CODE %d!\n", err);
            for (uint16_t j = 0; j < n_repair_symbols; ++j)
                delete_window_repair_symbol(cnx, scheme_sender->repair_symbols[j]);
            my_free(cnx, scheme_sender->repair_symbols);
            scheme_sender->repair_symbols = NULL;
            return PICOQUIC_ERROR_UNEXPECTED_ERROR;
        }

        scheme_sender->fec_params.first_id_to_protect = first_protected_id;
        scheme_sender->fec_params.n_symbols_to_protect = n_source_symbols;
        scheme_sender->fec_params.n_repair_symbols = n_repair_symbols;
        scheme_sender->cur_repair_symbol_idx = 0;
    }

    if (n_repair_symbols != scheme_sender->fec_params.n_repair_symbols) {
        PROTOOP_PRINTF(cnx, "ERROR: N REPAIR SYMBOLS CHANGED!\n");
        return PICOQUIC_ERROR_UNEXPECTED_ERROR;
    }

    uint16_t rs_idx = scheme_sender->cur_repair_symbol_idx++;
    if (rs_idx >= n_repair_symbols) {
        PROTOOP_PRINTF(cnx, "ERROR: NOT ENOUGH REPAIR SYMBOLS!\n");
        return PICOQUIC_ERROR_UNEXPECTED_ERROR;
    }

    window_repair_symbol_t* rs = scheme_sender->repair_symbols[rs_idx];

    rs->metadata.n_protected_symbols = scheme_sender->fec_params.n_symbols_to_protect;
    rs->metadata.first_id = scheme_sender->fec_params.first_id_to_protect;
    rs->repair_symbol.is_fb_fec = false;
    encode_u16(scheme_sender->fec_params.n_repair_symbols, rs->metadata.fss.val);
    encode_u16(rs_idx, rs->metadata.fss.val + 2);
    repair_symbols[0] = rs;

    return 0;
}

#endif /* __RS_GF65536_SENDER_FEC_SCHEME_H__ */