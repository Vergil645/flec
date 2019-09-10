#include <picoquic.h>
#include "../fec.h"
#include "../gf256/swif_symbol.c"
#include "../../helpers.h"
#include "../prng/tinymt32.c"
#include "rlc_fec_scheme_gf256.h"


static inline void get_coefs(picoquic_cnx_t *cnx, tinymt32_t *prng, uint32_t seed, int n, uint8_t coefs[n]) {
    tinymt32_init(prng, seed);
    int i;
    for (i = 0 ; i < n ; i++) {
        coefs[i] = (uint8_t) tinymt32_generate_uint32(prng);
        if (coefs[i] == 0)
            coefs[i] = 1;
    }
}

/**
 * fec_block_t* fec_block = (fec_block_t *) cnx->protoop_inputv[0];
 *
 * Output: return code (int)
 */
protoop_arg_t get_one_coded_symbol(picoquic_cnx_t *cnx)
{
    tinymt32_t prng;
    prng.mat1 = 0x8f7011ee;
    prng.mat2 = 0xfc78ff1f;
    prng.tmat = 0x3793fdff;
    fec_block_t* fec_block = (fec_block_t *) get_cnx(cnx, AK_CNX_INPUT, 0);
    rlc_gf256_fec_scheme_t *fs = (rlc_gf256_fec_scheme_t *) get_cnx(cnx, AK_CNX_INPUT, 1);
    int64_t seed = (int64_t) get_cnx(cnx, AK_CNX_INPUT, 2);
    uint8_t **mul = fs->table_mul;
    PROTOOP_PRINTF(cnx, "GENERATING SYMBOLS WITH RLC GF256, BLOCK = %p\n", (protoop_arg_t) fec_block);
    if (fec_block->total_source_symbols < 1
        || fec_block->current_source_symbols != fec_block->total_source_symbols) {
        PROTOOP_PRINTF(cnx, "IMPOSSIBLE TO GENERATE\n");
        return 1;
    }

    uint16_t max_length = 0;

    for_each_source_symbol(fec_block, source_symbol_t *source_symbol) {
        max_length = MAX(source_symbol->data_length, max_length);
    }


    uint8_t i, j;
    uint8_t *coefs = my_malloc(cnx, fec_block->total_source_symbols*sizeof(uint8_t));
    uint8_t **knowns = my_malloc(cnx, fec_block->total_source_symbols*sizeof(uint8_t));
    for (i = 0 ; i < fec_block->total_source_symbols ; i++) {
        knowns[i] = my_malloc(cnx, max_length);
        my_memset(knowns[i], 0, max_length);
        my_memcpy(knowns[i], fec_block->source_symbols[i]->data, fec_block->source_symbols[i]->data_length);
    }

    // generate one symbol
    repair_fpid_t rfpid;
    rfpid.raw = 0;
    rfpid.fec_block_number = fec_block->fec_block_number;
    rfpid.symbol_number = 0;
    rfpid.fec_scheme_specific = seed;
    get_coefs(cnx, &prng, seed, fec_block->total_source_symbols, coefs);
    repair_symbol_t *rs = malloc_repair_symbol(cnx, rfpid, max_length);
    for (j = 0 ; j < fec_block->total_source_symbols ; j++) {
        symbol_add_scaled(rs->data, coefs[j], knowns[j], max_length, mul);
    }
    fec_block->repair_symbols[0] = rs;
    fec_block->total_repair_symbols = 1;
    // done

    for (i = 0 ; i < fec_block->total_source_symbols ; i++) {
        my_free(cnx, knowns[i]);
    }
    my_free(cnx, coefs);
    my_free(cnx, knowns);
    return 0;
}