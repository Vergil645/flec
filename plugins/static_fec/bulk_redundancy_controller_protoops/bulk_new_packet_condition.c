#include "../fec.h"
#include "../static_redundancy_controller_protoops/static_redundancy_controller.h"
#include <getset.h>
#include <picoquic.h>

protoop_arg_t bulk_new_packet_condition(picoquic_cnx_t* cnx) {
    picoquic_path_t* path = (picoquic_path_t*)get_cnx(cnx, AK_CNX_INPUT, 0);
    static_redundancy_controller_t* controller = (static_redundancy_controller_t*)get_cnx(cnx, AK_CNX_INPUT, 1);
    uint64_t granularity = (protoop_arg_t)get_cnx(cnx, AK_CNX_INPUT, 2);
    fec_window_t* current_window = (fec_window_t*)get_cnx(cnx, AK_CNX_INPUT, 3);
    uint64_t current_time = (protoop_arg_t)get_cnx(cnx, AK_CNX_INPUT, 4);

    plugin_state_t* state = get_plugin_state(cnx);
    if (!state) {
        PROTOOP_PRINTF(cnx, "COULD NOT ALLOCATE THE PLUGIN STATE\n");
        return PICOQUIC_ERROR_MEMORY;
    }

    return state->has_fec_protected_data_to_send;
}