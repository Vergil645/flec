#include "../headers/receiver_scheme.h"

/**
 * Recover lost symbols if possible
 * \param[in] scheme_receiver \b receiver_scheme_t* Receiver FEC scheme object
 * \param[in] symbol_size \b uint16_t Symbol size
 * \param[in] recovered_symbols \b arraylist_t* Where to place recovered source symbols
 *
 * \return \b int 0 if everything is ok
 * \param[out] recovered \b protoop_arg_t Whether some symbols has been recovered or not
 */
protoop_arg_t recover_lost_symbols(picoquic_cnx_t* cnx) {
    receiver_scheme_t* scheme_receiver = (receiver_scheme_t*)get_cnx(cnx, AK_CNX_INPUT, 0);
    uint16_t _symbol_size = (uint16_t)get_cnx(cnx, AK_CNX_INPUT, 1);
    arraylist_t* recovered_symbols = (arraylist_t*)get_cnx(cnx, AK_CNX_INPUT, 2);

    protoop_arg_t recovered = 0;
    receiver_scheme_recover_lost_symbols(cnx, scheme_receiver, recovered_symbols, &recovered);

    set_cnx(cnx, AK_CNX_OUTPUT, 0, recovered);

    return 0;
}