#include "../../../framework_receiver.h"
#include "../headers/receiver_scheme.h"

/**
 * Process received repair symbol
 * \param[in] scheme_receiver \b receiver_scheme_t* Receiver FEC scheme object
 * \param[in] rs \b window_repair_symbol_t* Received repair symbol
 *
 * \return \b int 0 if everything is ok
 * \param[out] keep_rs \b int Whether to keep repair symbol data (1) or not (0)
 */
protoop_arg_t receive_repair_symbol(picoquic_cnx_t* cnx) {
    receiver_scheme_t* scheme_receiver = (receiver_scheme_t*)get_cnx(cnx, AK_CNX_INPUT, 0);
    window_repair_symbol_t* rs = (window_repair_symbol_t*)get_cnx(cnx, AK_CNX_INPUT, 1);

    ring_based_received_source_symbols_buffer_t* received_source_symbols;
    { // obtain received source symbols buffer
        plugin_state_t* state = get_plugin_state(cnx);
        if (!state)
            return PICOQUIC_ERROR_MEMORY;
        window_fec_framework_receiver_t* wff = (window_fec_framework_receiver_t*)state->framework_receiver;
        received_source_symbols = wff->received_source_symbols;
    }

    int ret = receiver_scheme_receive_repair_symbol(cnx, scheme_receiver, received_source_symbols, rs);

    set_cnx(cnx, AK_CNX_OUTPUT, 0, 0);

    return ret;
}