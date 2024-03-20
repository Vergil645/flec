
#include <picoquic.h>
#include <getset.h>
#include "../fec.h"
#include "../static_redundancy_controller_protoops/static_redundancy_controller.h"
#include "buffer_limited.h"

protoop_arg_t buffer_limited_protect_condition(picoquic_cnx_t* cnx) {
    PROTOOP_PRINTF(cnx, "BUFFER LIMITED PC!\n");

    picoquic_path_t* path = (picoquic_path_t*)get_cnx(cnx, AK_CNX_INPUT, 0);
    static_redundancy_controller_t* controller = (static_redundancy_controller_t*)get_cnx(cnx, AK_CNX_INPUT, 1);
    uint64_t granularity = (protoop_arg_t)get_cnx(cnx, AK_CNX_INPUT, 2);
    fec_window_t* current_window = (fec_window_t*)get_cnx(cnx, AK_CNX_INPUT, 3);
    uint64_t current_time = (protoop_arg_t)get_cnx(cnx, AK_CNX_INPUT, 4);

    buffer_limited_addon_t* addon = get_buffer_limited_addon_state(cnx, controller, current_time);
    if (!addon) {
        PROTOOP_PRINTF(cnx, "ERROR: COULD NOT ALLOCATE THE PLUGIN ADDON STATE\n");
        return false;
    }
    addon->window_first_unprotected_id = MAX(addon->window_first_unprotected_id, current_window->start);

    bool fc_blocked = !fec_has_protected_data_to_send(cnx);

    uint64_t cwin = (uint64_t)get_path(path, AK_PATH_CWIN, 0);
    uint64_t bytes_in_transit = (uint64_t)get_path(path, AK_PATH_BYTES_IN_TRANSIT, 0);
    uint64_t send_mtu = (uint64_t)get_path(path, AK_PATH_SEND_MTU, 0);
    // uint32_t p_cwin = (uint32_t)div_ceil(cwin, MIN((uint64_t)send_mtu, PICOQUIC_MAX_PACKET_SIZE));
    uint16_t rem_p_cwin = (uint16_t)div_ceil(cwin - bytes_in_transit, MIN(send_mtu, PICOQUIC_MAX_PACKET_SIZE));
    uint64_t wsize = (uint64_t)window_size(current_window);
    uint64_t lr_gran = 0;
    uint64_t gemodel_p_gran = 0;
    uint64_t gemodel_r_gran = granularity;

    uint16_t k = (uint16_t)(current_window->end - addon->window_first_unprotected_id);
    uint16_t r_max = 0;
    uint16_t r = 0;

    if (k == 0) {
        return false;
    }

    get_loss_parameters(cnx, path, current_time, granularity, &lr_gran, &gemodel_p_gran, &gemodel_r_gran);
    lr_gran = MIN(granularity / 2, 2 * lr_gran);

    // r_max = 2 * (1 + (uint16_t)(wsize * lr_gran / (granularity - lr_gran)));
    // r_max = MAX(1, (uint16_t)div_ceil(wsize * lr_gran, granularity - lr_gran));

    // if (controller->n_fec_in_flight >= r_max) {
    //     PROTOOP_PRINTF_1(cnx, "BUFFER LIMITED PC: n_fec_in_flight=%lu, r_max=%lu\n", controller->n_fec_in_flight, r_max);
    //     return false;
    // }

    // bool enough_packets_sent = k >= MIN(wsize, granularity / MAX(1, gemodel_p_gran));
    bool enough_packets_sent = k >= granularity / MAX(1, gemodel_p_gran);
    bool should_send_fec = enough_packets_sent;
    bool protect = fc_blocked || should_send_fec;

    r = MAX((gemodel_r_gran == granularity) ? 1 : 1 + (granularity / gemodel_r_gran),
            (uint16_t)div_ceil((uint64_t)k * lr_gran, granularity - lr_gran));
    // r = MIN(r, r_max - controller->n_fec_in_flight); // no more than 2 "packs" or repair symbols in a window
    // r = MIN(r, r_max);
    r = MIN(r, rem_p_cwin);

    if (protect) {
        PROTOOP_PRINTF_1(cnx, "BUFFER LIMITED PC: lr_gran=%lu, p_gran=%lu, r_gran=%lu k=%u, r=%u, window_first_unprotected_id=%u\n",
                         lr_gran, gemodel_p_gran, gemodel_r_gran, k, r, addon->window_first_unprotected_id);

        set_cnx(cnx, AK_CNX_OUTPUT, 0, addon->window_first_unprotected_id); // first_id_to_protect
        set_cnx(cnx, AK_CNX_OUTPUT, 1, k);                                  // n_symbols_to_protect
        set_cnx(cnx, AK_CNX_OUTPUT, 2, r);                                  // n_repair_symbols

        addon->window_first_unprotected_id += k;
    }

    return protect;
}