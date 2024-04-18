#include "../headers/receiver_scheme.h"
#include "../headers/sender_scheme.h"

/**
 * Create receiver and sender FEC schemes
 *
 * \return \b int 0 if everything is ok
 * \param[out] scheme_receiver \b receiver_scheme_t* Pointer to receiver scheme object
 * \param[out] scheme_sender \b sender_scheme_t* Pointer to sender scheme object
 */
protoop_arg_t create_fec_scheme(picoquic_cnx_t* cnx) {
    RS_t* rsd = rs_create(cnx);
    if (!rsd) {
        PROTOOP_PRINTF(cnx, "ERROR CREATING REED-SOLOMON DATA\n");
        return PICOQUIC_ERROR_MEMORY;
    }

    receiver_scheme_t* scheme_receiver = receiver_scheme_create(cnx, rsd);
    if (!scheme_receiver) {
        PROTOOP_PRINTF(cnx, "ERROR CREATING RECEIVER SCHEME\n");
        rs_destroy(cnx, rsd);
        return PICOQUIC_ERROR_MEMORY;
    }

    sender_scheme_t* scheme_sender = sender_scheme_create(cnx, rsd);
    if (!scheme_sender) {
        PROTOOP_PRINTF(cnx, "ERROR CREATING SENDER SCHEME\n");
        receiver_scheme_destroy(cnx, scheme_receiver);
        rs_destroy(cnx, rsd);
        return PICOQUIC_ERROR_MEMORY;
    }

    set_cnx(cnx, AK_CNX_OUTPUT, 0, (protoop_arg_t)scheme_receiver);
    set_cnx(cnx, AK_CNX_OUTPUT, 1, (protoop_arg_t)scheme_sender);

    return 0;
}