#ifndef bulk_H
#define bulk_H

#include <picoquic.h>
#include <getset.h>
#include "../fec.h"
#include "../static_redundancy_controller_protoops/static_redundancy_controller.h"

typedef struct {
    window_source_symbol_id_t window_first_unprotected_id;
} bulk_addon_t;

static __attribute__((always_inline)) bulk_addon_t*
get_bulk_addon_state(picoquic_cnx_t* cnx, static_redundancy_controller_t* controller, uint64_t current_time) {
    bulk_addon_t* addon_state = (bulk_addon_t*)controller->addons_states[0];

    if (!addon_state) {
        addon_state = my_malloc(cnx, sizeof(bulk_addon_t));
        if (!addon_state) {
            PROTOOP_PRINTF(cnx, "ERROR: COULD NOT ALLOCATE ADDON STATE\n");
            return NULL;
        }
        my_memset(addon_state, 0, sizeof(bulk_addon_t));
        controller->addons_states[0] = (protoop_arg_t)addon_state;
    }

    return addon_state;
}

#endif // bulk_H