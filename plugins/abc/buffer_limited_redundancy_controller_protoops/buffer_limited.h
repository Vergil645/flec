#ifndef buffer_limited_H
#define buffer_limited_H

#include <picoquic.h>
#include <getset.h>
#include "../fec.h"
#include "../static_redundancy_controller_protoops/static_redundancy_controller.h"

typedef struct {
    window_source_symbol_id_t window_first_unprotected_id;
} buffer_limited_addon_t;

static __attribute__((always_inline)) buffer_limited_addon_t*
get_buffer_limited_addon_state(picoquic_cnx_t* cnx, static_redundancy_controller_t* controller, uint64_t current_time) {
    buffer_limited_addon_t* addon_state = (buffer_limited_addon_t*)controller->addons_states[0];

    if (!addon_state) {
        addon_state = my_malloc(cnx, sizeof(buffer_limited_addon_t));
        if (!addon_state) {
            PROTOOP_PRINTF(cnx, "ERROR: COULD NOT ALLOCATE ADDON STATE\n");
            return NULL;
        }
        my_memset(addon_state, 0, sizeof(buffer_limited_addon_t));
        controller->addons_states[0] = (protoop_arg_t)addon_state;
    }

    return addon_state;
}

#endif // buffer_limited_H