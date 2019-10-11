#include "../../../helpers.h"


protoop_arg_t notify_fec_frame(picoquic_cnx_t *cnx)
{
    reserve_frame_slot_t *rfs = (reserve_frame_slot_t *) get_cnx(cnx, AK_CNX_INPUT, 0);
    my_free(cnx, rfs);
    return 0;
}