
#include <picoquic.h>
#include <getset.h>
#include "../fec.h"
#include "../static_redundancy_controller_protoops/static_redundancy_controller.h"
#include "bulk.h"

protoop_arg_t bulk_protect_condition(picoquic_cnx_t* cnx) {
    PROTOOP_PRINTF(cnx, "BULK PC!\n");

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

    // uint64_t lr_gran = l_times_granularity(controller);
    // uint16_t k = (uint16_t)window_size(current_window);
    // uint16_t r = MAX(1, (uint16_t)div_ceil((uint64_t)k * lr_gran, GRANULARITY - lr_gran));

    // PROTOOP_PRINTF_1(cnx, "BULK EW: lr_gran=%lu, k=%u, r=%u, first_id_to_protect=%u\n", lr_gran, k, r,
    //                  current_window->start);

    // set_cnx(cnx, AK_CNX_OUTPUT, 0, current_window->start); // first_id_to_protect
    // set_cnx(cnx, AK_CNX_OUTPUT, 1, k);                     // n_symbols_to_protect
    // set_cnx(cnx, AK_CNX_OUTPUT, 2, r);                     // n_repair_symbols

    // return true;

    bulk_addon_t* addon = get_bulk_addon_state(cnx, controller, current_time);
    if (!addon) {
        PROTOOP_PRINTF(cnx, "ERROR: COULD NOT ALLOCATE THE PLUGIN ADDON STATE\n");
        return false;
    }
    addon->window_first_unprotected_id = MAX(addon->window_first_unprotected_id, current_window->start);

    // uint64_t cwin = (uint64_t)get_path(path, AK_PATH_CWIN, 0);
    // uint64_t bytes_in_transit = (uint64_t)get_path(path, AK_PATH_BYTES_IN_TRANSIT, 0);
    // uint32_t send_mtu = (uint32_t)get_path(path, AK_PATH_SEND_MTU, 0);
    // uint32_t p_cwin = (uint32_t)div_ceil(cwin, MIN((uint64_t)send_mtu, PICOQUIC_MAX_PACKET_SIZE));
    uint64_t wsize = (uint64_t)window_size(current_window);
    uint64_t lr_gran = 0;
    uint64_t gemodel_r_gran = 0;

    uint16_t k = (uint16_t)(current_window->end - addon->window_first_unprotected_id);
    uint16_t r_max = 0;
    uint16_t r = 0;

    if (k == 0) {
        return false;
    }

    get_loss_parameters(cnx, path, current_time, granularity, &lr_gran, NULL, &gemodel_r_gran);

    r_max = 1 + MAX(2 * (uint16_t)div_ceil(wsize * lr_gran, granularity - lr_gran), granularity/gemodel_r_gran);

    if (controller->n_fec_in_flight >= r_max) {
        return false;
    }

    r = MAX(1, (uint16_t)div_ceil((uint64_t)k * lr_gran, granularity - lr_gran));
    r = MIN(r, r_max - controller->n_fec_in_flight); // no more than 2 "packs" or repair symbols in a window

    PROTOOP_PRINTF_1(cnx, "BULK PC: lr_gran=%lu, k=%u, r=%u, window_first_unprotected_id=%u\n", lr_gran, k, r,
                     addon->window_first_unprotected_id);

    set_cnx(cnx, AK_CNX_OUTPUT, 0, addon->window_first_unprotected_id); // first_id_to_protect
    set_cnx(cnx, AK_CNX_OUTPUT, 1, k);                                  // n_symbols_to_protect
    set_cnx(cnx, AK_CNX_OUTPUT, 2, r);                                  // n_repair_symbols

    addon->window_first_unprotected_id += k;

    return true;
}