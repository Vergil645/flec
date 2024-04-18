
#include <picoquic.h>

#include "../../../../fec.h"
#include "../../../framework_sender.h"
#include "../../../framework_receiver.h"
#include "../headers/equation.h"
#include "../headers/online_gf256_fec_scheme.h"

/**
 *  fec_scheme_receive_source_symbol(picoquic_cnx_t *cnx, online_gf256_fec_scheme_t *fec_scheme, window_source_symbol_t *ss)
 */
protoop_arg_t receive_source_symbol(picoquic_cnx_t *cnx) {
    plugin_state_t *state = get_plugin_state(cnx);
    if (!state) {
        return (protoop_arg_t) NULL;
    }
    window_fec_framework_receiver_t *wff = (window_fec_framework_receiver_t *) state->framework_receiver;
    online_gf256_fec_scheme_t *fec_scheme = (online_gf256_fec_scheme_t *) get_cnx(cnx, AK_CNX_INPUT, 0);
    window_source_symbol_t *ss = (window_source_symbol_t *) get_cnx(cnx, AK_CNX_INPUT, 1);

    PROTOOP_PRINTF(cnx, "BEFORE CREATE FROM ZEROCOPY\n");

    PROTOOP_PRINTF(cnx, "AFTER CREATE FROM ZEROCOPY\n");

    equation_t *removed = NULL;
    PROTOOP_PRINTF(cnx, "BEFORE WRAPPER RECEIVE SOURCE SYMBOL\n");
    int ret = wrapper_receive_source_symbol(cnx, &fec_scheme->wrapper, ss, &removed);
    PROTOOP_PRINTF(cnx, "AFTER WRAPPER RECEIVE SOURCE SYMBOL\n");

    if (removed != NULL) {
        equation_free(cnx, removed);
    }

    return ret;
}