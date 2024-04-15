#include "../headers/receiver_scheme.h"

/**
 * Process received source symbol
 * \param[in] scheme_receiver \b receiver_scheme_t* Receiver FEC scheme object
 * \param[in] ss \b window_source_symbol_t* Received source symbol
 *
 * \return \b int 0 if everything is ok
 */
protoop_arg_t receive_source_symbol(picoquic_cnx_t* cnx) {
    receiver_scheme_t* scheme_receiver = (receiver_scheme_t*)get_cnx(cnx, AK_CNX_INPUT, 0);
    window_source_symbol_t* ss = (window_source_symbol_t*)get_cnx(cnx, AK_CNX_INPUT, 1);

    return receiver_scheme_receive_source_symbol(cnx, scheme_receiver, ss);
}