#include "picoquic_internal.h"
#include <stdlib.h>
#include <string.h>
#include "cc_common.h"
#include "picoquic.h"
#include <getset.h>

#define picoquic_nocc_ID 0x42424F01 /* No congestion control */

static void picoquic_nocc_init(picoquic_cnx_t* cnx, picoquic_path_t* path_x) {
    /* Initialize the state of the congestion control algorithm */
    path_x->congestion_alg_state = NULL;
    path_x->cwin = (uint64_t)get_cnx(cnx, AK_CNX_FIXED_CWIN, 0);
    path_x->cwin = (path_x->cwin == 0) ? 10 * PICOQUIC_MAX_PACKET_SIZE : path_x->cwin;

    printf("picoquic_nocc_init: cwin=%lu\n", path_x->cwin);
}

static void picoquic_nocc_notify(picoquic_path_t* path_x, picoquic_congestion_notification_t notification,
                                 uint64_t rtt_measurement, uint64_t nb_bytes_acknowledged, uint64_t lost_packet_number,
                                 uint64_t current_time) {}

/* Release the state of the congestion control algorithm */
static void picoquic_nocc_delete(picoquic_cnx_t* cnx, picoquic_path_t* path_x) {}

picoquic_congestion_algorithm_t picoquic_nocc_algorithm_struct = {picoquic_nocc_ID, picoquic_nocc_init,
                                                                  picoquic_nocc_notify, picoquic_nocc_delete};

picoquic_congestion_algorithm_t* picoquic_nocc_algorithm = &picoquic_nocc_algorithm_struct;