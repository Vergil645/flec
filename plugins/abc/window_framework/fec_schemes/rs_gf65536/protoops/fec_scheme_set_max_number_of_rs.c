
#include "../headers/receiver_scheme.h"

/**
 * Remove repair symbols that cannot be used
 * \param[in] scheme_receiver \b receiver_scheme_t* Receiver FEC scheme object
 * \param[in] max_rs \b uint64_t Max number of symbols that can be stored
 *
 * \return \b int 0 if everything is ok
 */
protoop_arg_t set_max_number_of_rs(picoquic_cnx_t* cnx) {
    receiver_scheme_t* scheme_receiver = (receiver_scheme_t*)get_cnx(cnx, AK_CNX_INPUT, 0);
    uint64_t max_rs = (uint64_t)get_cnx(cnx, AK_CNX_INPUT, 1);

    return receiver_scheme_set_max_buffer_size(cnx, scheme_receiver, max_rs);
}