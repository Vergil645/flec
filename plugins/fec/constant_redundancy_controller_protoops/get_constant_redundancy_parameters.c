#include "picoquic.h"
#include "../bpf.h"
#include "constant_redundancy_controller.h"

// sets as output:
// Input  0: the redundancy controller state
// Output 0: the size of a block
// Output 1: the number of source symbols in a block
protoop_arg_t get_constant_redundancy_parameters(picoquic_cnx_t *cnx)
{
    set_cnx(cnx, AK_CNX_OUTPUT, 0, DEFAULT_N);
    set_cnx(cnx, AK_CNX_OUTPUT, 1, DEFAULT_K);
    return 0;
}