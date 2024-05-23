#ifndef message_based_H
#define message_based_H

#include "../fec.h"
#include "../static_redundancy_controller_protoops/static_redundancy_controller.h"
#include <picoquic.h>

typedef struct {
    window_source_symbol_id_t window_first_unprotected_id;
    uint64_t last_fully_protected_message_deadline;
} message_based_addon_t;

static __attribute__((always_inline)) message_based_addon_t*
get_message_based_addon_state(picoquic_cnx_t* cnx, static_redundancy_controller_t* controller) {
    message_based_addon_t* addon_state = (message_based_addon_t*)controller->addons_states[0];

    if (!addon_state) {
        addon_state = my_malloc(cnx, sizeof(message_based_addon_t));
        if (!addon_state) {
            PROTOOP_PRINTF(cnx, "ERROR: COULD NOT ALLOCATE ADDON STATE\n");
            return NULL;
        }
        my_memset(addon_state, 0, sizeof(message_based_addon_t));
        addon_state->last_fully_protected_message_deadline = UNDEFINED_SYMBOL_DEADLINE;
        controller->addons_states[0] = (protoop_arg_t)addon_state;
    }

    return addon_state;
}

#endif // message_based_H