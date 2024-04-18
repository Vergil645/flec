#include <getset.h>
#include <picoquic.h>

#include "../headers/sender_scheme.h"

/**
 * Generate repair symbol
 * \param[in] scheme_sender \b sender_scheme_t* Sender FEC scheme object
 * \param[in] source_symbols \b source_symbol_t** Source symbols to protect from losses
 * \param[in] n_source_symbols \b uint16_t Number of source symbols
 * \param[in] repair_symbols \b window_repair_symbol_t** Where to place generated repair symbols
 * \param[in] n_symbols_to_generate \b uint16_t Number of repair symbols to be generated (must be 1)
 * \param[in] symbol_size \b uint16_t Symbol size
 * \param[in] first_protected_id \b window_source_symbol_id_t ID of the first protected source symbol
 * \param[in] n_repair_symbols \b uint16_t Number of repair symbols that can be generated for these source symbols
 *
 * \return \b int 0 if everything is ok
 * \param[out] fec_scheme_specific \b uint32_t FEC scheme specific big endian value
 * \param[out] n_symbols_generated \b uint16_t Number of generated repair symbols
 */
protoop_arg_t generate(picoquic_cnx_t* cnx) {
    sender_scheme_t* scheme_sender = (sender_scheme_t*)get_cnx(cnx, AK_CNX_INPUT, 0);
    source_symbol_t** source_symbols = (source_symbol_t**)get_cnx(cnx, AK_CNX_INPUT, 1);
    uint16_t n_source_symbols = (uint16_t)get_cnx(cnx, AK_CNX_INPUT, 2);
    window_repair_symbol_t** repair_symbols = (window_repair_symbol_t**)get_cnx(cnx, AK_CNX_INPUT, 3);
    uint16_t n_symbols_to_generate = (uint16_t)get_cnx(cnx, AK_CNX_INPUT, 4);
    uint16_t symbol_size = (uint16_t)get_cnx(cnx, AK_CNX_INPUT, 5);
    window_source_symbol_id_t first_protected_id = (window_source_symbol_id_t)get_cnx(cnx, AK_CNX_INPUT, 6);
    uint16_t n_repair_symbols = (uint16_t)get_cnx(cnx, AK_CNX_INPUT, 7);

    int ret = 0;

    if (n_symbols_to_generate != 1) {
        PROTOOP_PRINTF(cnx, "ERROR: N SYMBOLS TO GENERATE MUST BE 1!\n");
        ret = PICOQUIC_ERROR_UNEXPECTED_ERROR;
    } else {
        ret = sender_scheme_encode(cnx, scheme_sender, source_symbols, n_source_symbols, repair_symbols, symbol_size,
                                   first_protected_id, n_repair_symbols);
    }

    if (ret == 0) {
        set_cnx(cnx, AK_CNX_OUTPUT, 0, repair_symbols[0]->metadata.fss.val_big_endian);
        set_cnx(cnx, AK_CNX_OUTPUT, 1, n_symbols_to_generate);
    } else {
        set_cnx(cnx, AK_CNX_OUTPUT, 0, 0);
        set_cnx(cnx, AK_CNX_OUTPUT, 1, 0);
    }

    return ret;
}