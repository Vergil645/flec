
#include "../fec.h"
#include "../static_redundancy_controller_protoops/static_redundancy_controller.h"
#include "bulk.h"
#include <getset.h>
#include <picoquic.h>

protoop_arg_t bulk_protect_condition(picoquic_cnx_t* cnx) {
    picoquic_path_t* path = (picoquic_path_t*)get_cnx(cnx, AK_CNX_INPUT, 0);
    static_redundancy_controller_t* controller = (static_redundancy_controller_t*)get_cnx(cnx, AK_CNX_INPUT, 1);
    uint64_t granularity = (protoop_arg_t)get_cnx(cnx, AK_CNX_INPUT, 2);
    fec_window_t* current_window = (fec_window_t*)get_cnx(cnx, AK_CNX_INPUT, 3);
    uint64_t current_time = (protoop_arg_t)get_cnx(cnx, AK_CNX_INPUT, 4);

    if (fec_has_protected_data_to_send(cnx)) {
        return false;
    }

    uint64_t time_threshold = (uint64_t)get_path(path, AK_PATH_SMOOTHED_RTT, 0) >> 3;
    if (current_time < controller->last_sent_id_timestamp + time_threshold) {
        protoop_arg_t args[2];
        args[0] = current_time;
        args[1] = controller->last_sent_id_timestamp + time_threshold;
        run_noparam(cnx, "request_waking_at_last_at", 2, args, NULL);
        return false;
    }

    bulk_addon_t* addon = get_bulk_addon_state(cnx, controller, current_time);
    if (!addon) {
        PROTOOP_PRINTF(cnx, "ERROR: COULD NOT ALLOCATE THE PLUGIN ADDON STATE\n");
        return false;
    }
    addon->window_first_unprotected_id = MAX(addon->window_first_unprotected_id, current_window->start);

    uint64_t cwin = (uint64_t)get_path(path, AK_PATH_CWIN, 0);
    uint64_t bytes_in_transit = (uint64_t)get_path(path, AK_PATH_BYTES_IN_TRANSIT, 0);
    uint64_t send_mtu = (uint64_t)get_path(path, AK_PATH_SEND_MTU, 0);
    // uint32_t p_cwin = (uint32_t)div_ceil(cwin, MIN((uint64_t)send_mtu, PICOQUIC_MAX_PACKET_SIZE));
    uint16_t rem_p_cwin = (uint16_t)div_ceil(cwin - bytes_in_transit, MIN(send_mtu, PICOQUIC_MAX_PACKET_SIZE));
    uint64_t lr_gran = 0;
    // uint64_t gemodel_r_gran = 0;

    uint16_t k = (uint16_t)(current_window->end - addon->window_first_unprotected_id);
    uint16_t r = 0;

    if (k == 0) {
        return false;
    }

    // get_loss_parameters(cnx, path, current_time, granularity, &lr_gran, NULL, &gemodel_r_gran);
    get_loss_parameters(cnx, path, current_time, granularity, &lr_gran, NULL, NULL);

    // r = 1 + (uint16_t)(((uint64_t)k * lr_gran) / (granularity - lr_gran));
    r = rem_p_cwin;
    if (lr_gran != 0) {
        uint64_t lr_gran_1 = MIN(granularity / 5, lr_gran * 2);
        r = MIN(r, 1 + (uint16_t)(((uint64_t)k * lr_gran_1) / (granularity - lr_gran_1)));
    }

    set_cnx(cnx, AK_CNX_OUTPUT, 0, addon->window_first_unprotected_id); // first_id_to_protect
    set_cnx(cnx, AK_CNX_OUTPUT, 1, k);                                  // n_symbols_to_protect
    set_cnx(cnx, AK_CNX_OUTPUT, 2, r);                                  // n_repair_symbols

    addon->window_first_unprotected_id += k;

    return true;
}