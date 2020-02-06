
#include <picoquic.h>
#include <getset.h>
#include "../../../fec.h"
#include "../../framework_receiver.h"

/**
 * MUST BE CALLED BEFORE REPAIR SYMBOLS ARE ACTUALLY RECEIVED !!
 */
protoop_arg_t set_repair_rwin_size(picoquic_cnx_t *cnx)
{
    plugin_state_t *state = get_plugin_state(cnx);
    if (!state)
        return PICOQUIC_ERROR_MEMORY;
    size_t buffer_size = (size_t) get_cnx(cnx, AK_CNX_INPUT, 0);
    window_fec_framework_receiver_t *wff = (window_fec_framework_receiver_t *) state->framework_receiver;
    release_repair_symbols_buffer(cnx, wff->received_repair_symbols);
    int margin = 64;    // to be sure we can store metadata etc.
    wff->received_repair_symbols = new_repair_symbols_buffer(cnx, buffer_size/(state->symbol_size + margin));
    PROTOOP_PRINTF(cnx, "REPAIR RWIN SIZE SET TO %lu\n", wff->received_repair_symbols->pq->max_size);
    return 0;
}