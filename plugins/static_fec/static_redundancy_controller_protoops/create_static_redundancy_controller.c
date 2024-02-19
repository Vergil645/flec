#include "picoquic.h"
#include "static_redundancy_controller.h"

// sets as output the pointer towards the controller's state
protoop_arg_t create_window_controller(picoquic_cnx_t *cnx)
{
    static_redundancy_controller_t *c = create_static_redundancy_controller(cnx, 1);
    set_cnx(cnx, AK_CNX_OUTPUT, 0, (protoop_arg_t) c);
    return 0;
}