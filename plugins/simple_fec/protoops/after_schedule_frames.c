#include <picoquic.h>
#include <getset.h>
#include "../../helpers.h"
#include "../fec_constants.h"
#include "../fec.h"


/**
 * Schedule frames and provide a packet with the path it should be sent on when connection is ready
 * \param[in] packet \b picoquic_packet_t* The packet to be sent
 * \param[in] send_buffer_max \b size_t The maximum amount of bytes that can be written on the packet
 * \param[in] current_time \b uint64_t Time of the scheduling
 * \param[in] retransmit_p \b picoquic_packet_t* A candidate packet for retransmission
 * \param[in] from_path \b picoquic_path_t* The path on which the candidate packet was sent
 * \param[in] reason \b char* A description of the reason for which the candidate packet is proposed
 *
 * \return \b int 0 if everything is ok
 * \param[out] path_x \b picoquic_path_t* The path on which the packet should be sent
 * \param[out] length \b uint32_t The length of the packet to be sent
 * \param[out] header_length \b uint32_t The length of the header of the packet to be sent
 */
protoop_arg_t schedule_frames_on_path(picoquic_cnx_t *cnx)
{
    picoquic_packet_t *packet = (picoquic_packet_t *) get_cnx(cnx, AK_CNX_INPUT, 0);
    picoquic_packet_t *retransmit_p = (picoquic_packet_t *) get_cnx(cnx, AK_CNX_INPUT, 3);
    uint32_t length = get_cnx(cnx, AK_CNX_OUTPUT, 1);
    // set the current fpid
    plugin_state_t *state = get_plugin_state(cnx);

    PROTOOP_PRINTF(cnx, "AFTER SCHEDULE FRAMES, CURRENT TIME = %lu\n", picoquic_current_time());

    // protect the source symbol
    uint8_t *data = (uint8_t *) get_pkt(packet, AK_PKT_BYTES);
    picoquic_packet_type_enum packet_type = retransmit_p ? get_pkt(retransmit_p, AK_PKT_TYPE) : get_pkt(packet, AK_PKT_TYPE);
    uint32_t header_length = get_pkt(packet, AK_PKT_OFFSET);

    protoop_arg_t packet_flags = get_pkt_metadata(cnx, packet, FEC_PKT_METADATA_FLAGS);

    if (state->has_written_fpi_frame && (packet_type == picoquic_packet_1rtt_protected_phi0 || packet_type == picoquic_packet_1rtt_protected_phi1)){
        // copy the packet payload without the header and put it 8 bytes after the start of the buffer

        uint16_t n_symbols = 0;
        source_symbol_id_t id = 0;
        int err = protect_packet_payload(cnx, data + header_length, length - header_length, get_pkt(packet, AK_PKT_SEQUENCE_NUMBER), &id, &n_symbols);

        if (err != 0 && err != 1)
            return err;

        if (err == 0) {
            PROTOOP_PRINTF(cnx, "THE PACKET HAS BEEN SUCCESSFULLY PROTECTED, CURRENT TIME = %lu\n", picoquic_current_time());
            // something has been protected
            set_pkt_metadata(cnx, packet, FEC_PKT_METADATA_SENT_SLOT, state->current_slot++);
            set_pkt_metadata(cnx, packet, FEC_PKT_METADATA_FLAGS, packet_flags | FEC_PKT_METADATA_FLAG_IS_FEC_PROTECTED);
            set_pkt_metadata(cnx, packet, FEC_PKT_METADATA_FIRST_SOURCE_SYMBOL_ID, id);
            set_pkt_metadata(cnx, packet, FEC_PKT_METADATA_NUMBER_OF_SOURCE_SYMBOLS, n_symbols);
            fec_sent_packet(cnx, packet, true, false, false);
            PROTOOP_PRINTF(cnx, "SENT PACKET DONE\n");
        }

    } else if (state->has_written_repair_frame || state->has_written_fb_fec_repair_frame) {
        set_pkt_metadata(cnx, packet, FEC_PKT_METADATA_SENT_SLOT, state->current_slot++);
        set_pkt_metadata(cnx, packet, FEC_PKT_METADATA_FLAGS, packet_flags | FEC_PKT_METADATA_FLAG_CONTAINS_REPAIR_FRAME);

        set_pkt_metadata(cnx, packet, FEC_PKT_METADATA_FIRST_PROTECTED_SYMBOL_ID, state->current_repair_frame_first_protected_id);
        set_pkt_metadata(cnx, packet, FEC_PKT_METADATA_NUMBER_OF_PROTECTED_SYMBOLS, state->current_repair_frame_n_protected_symbols);
        set_pkt_metadata(cnx, packet, FEC_PKT_METADATA_NUMBER_OF_REPAIR_SYMBOLS, state->current_repair_frame_n_repair_symbols);
        if (state->has_written_fb_fec_repair_frame)
            set_pkt_metadata(cnx, packet, FEC_PKT_METADATA_FLAGS, packet_flags | FEC_PKT_METADATA_FLAG_IS_FB_FEC);
        fec_sent_packet(cnx, packet, false, true, state->has_written_fb_fec_repair_frame);
    }
    packet_flags = get_pkt_metadata(cnx, packet, FEC_PKT_METADATA_FLAGS);
    if (state->has_written_recovered_frame) {
        PROTOOP_PRINTF(cnx, "THIS PACKET CONTAINS A RECOVERED FRAME !\n");
        set_pkt_metadata(cnx, packet, FEC_PKT_METADATA_FLAGS, packet_flags | FEC_PKT_METADATA_FLAG_CONTAINS_RECOVERED_FRAME);
    }
    state->has_written_fpi_frame = false;
    state->has_written_repair_frame = false;
    state->has_written_fb_fec_repair_frame = false;
    state->has_written_recovered_frame = false;

    // maybe wake now if slots are still available
    picoquic_path_t *path = (picoquic_path_t *) get_cnx(cnx, AK_CNX_PATH, 0);
    bool slot_available = get_path(path, AK_PATH_CWIN, 0) > get_path(path, AK_PATH_BYTES_IN_TRANSIT, 0);

    picoquic_state_enum cnx_state = get_cnx(cnx, AK_CNX_STATE, 0);
    // TODO: remove first part of if
    if (cnx_state == picoquic_state_server_ready && !run_noparam(cnx, FEC_PROTOOP_HAS_PROTECTED_DATA_TO_SEND, 0, NULL, NULL) && slot_available && state->n_reserved_id_or_repair_frames == 0) {
        reserve_repair_frames(cnx, state->framework_sender, DEFAULT_SLOT_SIZE, state->symbol_size, false, false, 0, 0);
        PROTOOP_PRINTF(cnx, "FAIR RESERVE THE FLUSHED FRAME\n");
        picoquic_frame_fair_reserve(cnx, path, NULL, get_path(path, AK_PATH_SEND_MTU, 0) - 35);
    }
    return 0;
}