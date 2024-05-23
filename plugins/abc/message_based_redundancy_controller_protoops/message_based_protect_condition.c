#include "../fec.h"
#include "../static_redundancy_controller_protoops/static_redundancy_controller.h"
#include "message_based.h"
#include <getset.h>
#include <picoquic.h>

protoop_arg_t message_based_protect_condition(picoquic_cnx_t* cnx) {
    picoquic_path_t* path = (picoquic_path_t*)get_cnx(cnx, AK_CNX_INPUT, 0);
    static_redundancy_controller_t* controller = (static_redundancy_controller_t*)get_cnx(cnx, AK_CNX_INPUT, 1);
    uint64_t granularity = (protoop_arg_t)get_cnx(cnx, AK_CNX_INPUT, 2);
    fec_window_t* current_window = (fec_window_t*)get_cnx(cnx, AK_CNX_INPUT, 3);
    uint64_t current_time = (protoop_arg_t)get_cnx(cnx, AK_CNX_INPUT, 4);

    plugin_state_t* state = get_plugin_state(cnx);
    if (!state)
        return PICOQUIC_ERROR_MEMORY;
    window_fec_framework_t* wff = (window_fec_framework_t*)state->framework_sender;

    message_based_addon_t* addon = get_message_based_addon_state(cnx, controller);
    if (!addon) {
        PROTOOP_PRINTF(cnx, "ERROR: COULD NOT ALLOCATE THE PLUGIN ADDON STATE\n");
        return false;
    }
    addon->window_first_unprotected_id = MAX(addon->window_first_unprotected_id, current_window->start);

    uint16_t k = (uint16_t)(current_window->end - addon->window_first_unprotected_id);
    uint16_t r = 0;
    if (k == 0)
        return false;

    // obtaining loss parameters
    // TODO: think better about Gilbert model
    uint64_t lr_gran = 0;
    uint64_t gemodel_r_gran = granularity;
    get_loss_parameters(cnx, path, current_time, granularity, &lr_gran, NULL, &gemodel_r_gran);

    // obtaining one-way delay
    uint64_t smoothed_rtt_microsec = get_path(path, AK_PATH_SMOOTHED_RTT, 0);
    uint64_t owd_microsec = smoothed_rtt_microsec >> 1;

    // looking for the soonest deadline
    rbt_key soonest_deadline_microsec_key = 0;
    rbt_val soonest_deadline_first_id_val = NULL;
    bool found_ceiling = rbt_ceiling(cnx, wff->symbols_from_deadlines,
                                     MAX((addon->last_fully_protected_message_deadline != UNDEFINED_SYMBOL_DEADLINE)
                                             ? (addon->last_fully_protected_message_deadline + 1)
                                             : 0,
                                         current_time + owd_microsec),
                                     &soonest_deadline_microsec_key, &soonest_deadline_first_id_val);
    symbol_deadline_t soonest_deadline_microsec =
        found_ceiling ? ((symbol_deadline_t)soonest_deadline_microsec_key) : UNDEFINED_SYMBOL_DEADLINE;

    // obtaining time before next message
    uint64_t next_message_time_to_wait_microsec = 0;
    if (wff->next_message_timestamp_microsec != UNDEFINED_SYMBOL_DEADLINE &&
        wff->next_message_timestamp_microsec > current_time) {
        next_message_time_to_wait_microsec = wff->next_message_timestamp_microsec - current_time;
    }

    // checking that we will be able to protect all corresponding symbols according to CWIN
    // FIXME: improve
    bandwidth_t available_bandwidth_bytes_per_second =
        get_path(path, AK_PATH_CWIN, 0) * SECOND_IN_MICROSEC / smoothed_rtt_microsec;
    bandwidth_t used_bandwidth_bytes_per_second =
        get_path(path, AK_PATH_BYTES_IN_TRANSIT, 0) * SECOND_IN_MICROSEC / smoothed_rtt_microsec;
    int64_t bw_ratio_times_granularity =
        (used_bandwidth_bytes_per_second > 0)
            ? ((granularity * available_bandwidth_bytes_per_second) / used_bandwidth_bytes_per_second)
            : 0;
    bool ew = (!state->has_fec_protected_data_to_send && bw_ratio_times_granularity > (granularity + granularity / 10));

    // if false, that means that we can wait a bit before sending FEC
    bool allowed_to_send_fec_given_deadlines =
        (soonest_deadline_microsec == UNDEFINED_SYMBOL_DEADLINE ||
         wff->next_message_timestamp_microsec == UNDEFINED_SYMBOL_DEADLINE
         // FIXME:: we cross the fingers here so that there will be no overflow
         || current_time + next_message_time_to_wait_microsec + owd_microsec + DEADLINE_CRITICAL_THRESHOLD_MICROSEC >=
                soonest_deadline_microsec);

    bool protect = ew && allowed_to_send_fec_given_deadlines;

    r = MAX((gemodel_r_gran == granularity) ? 1 : 1 + (granularity / gemodel_r_gran),
            (uint16_t)div_ceil((uint64_t)k * lr_gran, granularity - lr_gran));

    if (protect) {
        PROTOOP_PRINTF(cnx, "MESSAGE BASED PC: lr_gran=%lu, k=%u, r=%u, window_first_unprotected_id=%u\n", lr_gran, k,
                       r, addon->window_first_unprotected_id);
        // PROTOOP_PRINTF(
        //     cnx, "MESSAGE BASED PC: lr_gran=%lu, p_gran=%lu, r_gran=%lu, k=%u, r=%u,
        //     window_first_unprotected_id=%u\n", lr_gran, gemodel_p_gran, gemodel_r_gran, k, r,
        //     addon->window_first_unprotected_id);

        set_cnx(cnx, AK_CNX_OUTPUT, 0, addon->window_first_unprotected_id); // first_id_to_protect
        set_cnx(cnx, AK_CNX_OUTPUT, 1, k);                                  // n_symbols_to_protect
        set_cnx(cnx, AK_CNX_OUTPUT, 2, r);                                  // n_repair_symbols

        addon->window_first_unprotected_id += k;
        if (!rbt_is_empty(cnx, wff->symbols_from_deadlines)) {
            // seems correct because we protect symbols until the end of the window
            addon->last_fully_protected_message_deadline = rbt_max_key(cnx, wff->symbols_from_deadlines);
        }
    }

    // // FIXME: do we need this?
    // if (soonest_deadline_microsec != UNDEFINED_SYMBOL_DEADLINE &&
    //     wff->next_message_timestamp_microsec != UNDEFINED_SYMBOL_DEADLINE) {
    //     protoop_arg_t args[2];
    //     args[0] = current_time;
    //     args[1] = 1 + soonest_deadline_microsec - (owd_microsec + DEADLINE_CRITICAL_THRESHOLD_MICROSEC);
    //     run_noparam(cnx, "request_waking_at_last_at", 2, args, NULL);
    // }

    return protect;
}