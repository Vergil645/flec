
#include "../headers/receiver_scheme.h"

/**
 * Remove repair symbols that cannot be used
 * \param[in] scheme_receiver \b receiver_scheme_t* Receiver FEC scheme object
 * \param[in] highest_contiguously_received \b window_source_symbol_id_t Highest contiguously received source symbol ID
 *
 * \return \b int 0 if everything is ok
 */
protoop_arg_t remove_unused_repair_symbols(picoquic_cnx_t* cnx) {
    receiver_scheme_t* scheme_receiver = (receiver_scheme_t*)get_cnx(cnx, AK_CNX_INPUT, 0);
    window_source_symbol_id_t highest_contiguously_received = (window_source_symbol_id_t)get_cnx(cnx, AK_CNX_INPUT, 1);

    return receiver_scheme_remove_unused(cnx, scheme_receiver, highest_contiguously_received);
}