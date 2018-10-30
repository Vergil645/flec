#ifndef HELPERS_H
#define HELPERS_H

#include "picoquic_internal.h"
#include "plugin.h"
#include "memcpy.h"

#define PROTOOP_NUMARGS(...)  (sizeof((protoop_arg_t[]){__VA_ARGS__})/sizeof(protoop_arg_t))
#define PROTOOP_PRINTF(cnx, fmt, ...)   helper_protoop_printf(cnx, fmt, (protoop_arg_t[]){__VA_ARGS__}, PROTOOP_NUMARGS(__VA_ARGS__))

static inline protoop_params_t get_pp_noparam(protoop_id_t pid, int inputc, protoop_arg_t *inputv, protoop_arg_t *outputv) {
    protoop_params_t pp;
    pp.pid = pid;
    pp.param = NO_PARAM;
    pp.caller_is_intern = true;
    pp.inputc = inputc;
    pp.inputv = inputv;
    pp.outputv = outputv;
    return pp;
}

static inline protoop_params_t get_pp_param(protoop_id_t pid, param_id_t param, int inputc, protoop_arg_t *inputv, protoop_arg_t *outputv) {
    protoop_params_t pp;
    pp.pid = pid;
    pp.param = param;
    pp.caller_is_intern = true;
    pp.inputc = inputc;
    pp.inputv = inputv;
    pp.outputv = outputv;
    return pp;
}

static uint32_t helper_get_checksum_length(picoquic_cnx_t* cnx, int is_cleartext_mode)
{
    protoop_arg_t args[1];
    args[0] = (protoop_arg_t) is_cleartext_mode;
    protoop_params_t pp = get_pp_noparam(PROTOOP_NOPARAM_GET_CHECKSUM_LENGTH, 1, args, NULL);
    return (uint32_t) plugin_run_protoop(cnx, &pp);
}

static void helper_protoop_printf(picoquic_cnx_t *cnx, const char *fmt, protoop_arg_t *fmt_args, size_t args_len)
{
    protoop_arg_t args[3];
    args[0] = (protoop_arg_t) fmt;
    args[1] = (protoop_arg_t) fmt_args;
    args[2] = (protoop_arg_t) args_len;
    protoop_params_t pp = get_pp_noparam(PROTOOP_NOPARAM_PRINTF, 3, args, NULL);
    plugin_run_protoop(cnx, &pp);
}

static int helper_retransmit_needed_by_packet(picoquic_cnx_t *cnx, picoquic_packet_t *p, uint64_t current_time, int *timer_based_retransmit, protoop_id_t *reason)
{
    protoop_arg_t outs[PROTOOPARGS_MAX], args[3];
    args[0] = (protoop_arg_t) p;
    args[1] = (protoop_arg_t) current_time;
    args[2] = (protoop_arg_t) *timer_based_retransmit;
    protoop_params_t pp = get_pp_noparam(PROTOOP_NOPARAM_RETRANSMIT_NEEDED_BY_PACKET, 3, args, outs);
    int ret = (int) plugin_run_protoop(cnx, &pp);
    *timer_based_retransmit = (int) outs[0];
    if (reason != NULL) {
        *reason = (protoop_id_t) outs[1];
    }
    return ret;
}

static void helper_congestion_algorithm_notify(picoquic_cnx_t *cnx, picoquic_path_t* path_x,
    picoquic_congestion_notification_t notification, uint64_t rtt_measurement, uint64_t nb_bytes_acknowledged,
    uint64_t lost_packet_number, uint64_t current_time)
{
    protoop_arg_t args[6];
    args[0] = (protoop_arg_t) path_x;
    args[1] = (protoop_arg_t) notification;
    args[2] = (protoop_arg_t) rtt_measurement;
    args[3] = (protoop_arg_t) nb_bytes_acknowledged;
    args[4] = (protoop_arg_t) lost_packet_number;
    args[5] = (protoop_arg_t) current_time;
    protoop_params_t pp = get_pp_noparam(PROTOOP_NOPARAM_CONGESTION_ALGORITHM_NOTIFY, 6, args, NULL);
    plugin_run_protoop(cnx, &pp);
}

static void helper_callback_function(picoquic_cnx_t* cnx, uint64_t stream_id, uint8_t* bytes,
    size_t length, picoquic_call_back_event_t fin_or_event, void* callback_ctx)
{
    protoop_arg_t args[5];
    args[0] = (protoop_arg_t) stream_id;
    args[1] = (protoop_arg_t) bytes;
    args[2] = (protoop_arg_t) length;
    args[3] = (protoop_arg_t) fin_or_event;
    args[4] = (protoop_arg_t) callback_ctx;
    protoop_params_t pp = get_pp_noparam("callback_function", 5, args, NULL);
    plugin_run_protoop(cnx, &pp);
}

static int helper_skip_frame(picoquic_cnx_t *cnx, uint8_t* bytes, size_t bytes_max, size_t* consumed, int* pure_ack)
{
    protoop_arg_t args[4], outs[PROTOOPARGS_MAX];
    args[0] = (protoop_arg_t) bytes;
    args[1] = (protoop_arg_t) bytes_max;
    args[2] = (protoop_arg_t) *consumed;
    args[3] = (protoop_arg_t) *pure_ack;
    protoop_params_t pp = get_pp_noparam("skip_frame", 4, args, outs);
    int ret = (int) plugin_run_protoop(cnx, &pp);
    *consumed = (size_t) outs[0];
    *pure_ack = (int) outs[1];
    return ret;
}

static int helper_check_stream_frame_already_acked(picoquic_cnx_t* cnx, uint8_t* bytes,
    size_t bytes_max, int* no_need_to_repeat)
{
    protoop_arg_t args[3], outs[PROTOOPARGS_MAX];
    args[0] = (protoop_arg_t) bytes;
    args[1] = (protoop_arg_t) bytes_max;
    args[2] = (protoop_arg_t) *no_need_to_repeat;
    protoop_params_t pp = get_pp_noparam(PROTOOP_NOPARAM_CHECK_STREAM_FRAME_ALREADY_ACKED, 3, args, outs);
    int ret = (int) plugin_run_protoop(cnx, &pp);
    *no_need_to_repeat = (int) outs[0];
    return ret;
}

static uint32_t helper_predict_packet_header_length(picoquic_cnx_t *cnx, picoquic_packet_type_enum packet_type, picoquic_path_t* path_x)
{
    protoop_arg_t args[2];
    args[0] = (protoop_arg_t) packet_type;
    args[1] = (protoop_arg_t) path_x;
    protoop_params_t pp = get_pp_noparam(PROTOOP_NOPARAM_PREDICT_PACKET_HEADER_LENGTH, 2, args, NULL);
    return (uint32_t) plugin_run_protoop(cnx, &pp);
}

static int helper_is_stream_frame_unlimited(const uint8_t* bytes)
{
    return PICOQUIC_BITS_CLEAR_IN_RANGE(bytes[0], picoquic_frame_type_stream_range_min, picoquic_frame_type_stream_range_max, 0x02);
}

static void helper_dequeue_retransmit_packet(picoquic_cnx_t* cnx, picoquic_packet_t* p, int should_free)
{
    protoop_arg_t args[2];
    args[0] = (protoop_arg_t) p;
    args[1] = (protoop_arg_t) should_free;
    protoop_params_t pp = get_pp_noparam(PROTOOP_NOPARAM_DEQUEUE_RETRANSMIT_PACKET, 2, args, NULL);
    plugin_run_protoop(cnx, &pp);
}

/* Decide whether MAX data need to be sent or not */
static int helper_should_send_max_data(picoquic_cnx_t* cnx)
{
    int ret = 0;

    if (2 * cnx->data_received > cnx->maxdata_local)
        ret = 1;

    return ret;
}

/* Decide whether to send an MTU probe */
static int helper_is_mtu_probe_needed(picoquic_cnx_t* cnx, picoquic_path_t * path_x)
{
    int ret = 0;

    if ((cnx->cnx_state == picoquic_state_client_ready || cnx->cnx_state == picoquic_state_server_ready) && path_x->mtu_probe_sent == 0 && (path_x->send_mtu_max_tried == 0 || (path_x->send_mtu + 10) < path_x->send_mtu_max_tried)) {
        ret = 1;
    }

    return ret;
}

static picoquic_stream_head *helper_find_ready_stream(picoquic_cnx_t *cnx)
{
    protoop_params_t pp = get_pp_noparam(PROTOOP_NOPARAM_FIND_READY_STREAM, 0, NULL, NULL);
    return (picoquic_stream_head *) plugin_run_protoop(cnx, &pp);
}

static int helper_is_ack_needed(picoquic_cnx_t *cnx, uint64_t current_time, picoquic_packet_context_enum pc,
    picoquic_path_t* path_x)
{
    protoop_arg_t args[3];
    args[0] = (protoop_arg_t) current_time;
    args[1] = (protoop_arg_t) pc;
    args[2] = (protoop_arg_t) path_x;
    protoop_params_t pp = get_pp_noparam(PROTOOP_NOPARAM_IS_ACK_NEEDED, 3, args, NULL);
    return (int) plugin_run_protoop(cnx, &pp);
}

static int helper_is_tls_stream_ready(picoquic_cnx_t *cnx)
{
    protoop_params_t pp = get_pp_noparam(PROTOOP_NOPARAM_IS_TLS_STREAM_READY, 0, NULL, NULL);
    return (int) plugin_run_protoop(cnx, &pp);
}

static uint32_t helper_prepare_packet_old_context(picoquic_cnx_t *cnx, picoquic_packet_context_enum pc,
    picoquic_path_t * path_x, picoquic_packet_t* packet, size_t send_buffer_max,
    uint64_t current_time, uint32_t * header_length)
{
    protoop_arg_t outs[1];
    protoop_arg_t args[6];
    args[0] = (protoop_arg_t) pc;
    args[1] = (protoop_arg_t) path_x;
    args[2] = (protoop_arg_t) packet;
    args[3] = (protoop_arg_t) send_buffer_max;
    args[4] = (protoop_arg_t) current_time;
    args[5] = (protoop_arg_t) *header_length;
    protoop_params_t pp = get_pp_noparam(PROTOOP_NOPARAM_PREPARE_PACKET_OLD_CONTEXT, 6, args, outs);
    uint32_t length = (uint32_t) plugin_run_protoop(cnx, &pp);
    *header_length = (uint32_t) outs[0];
    return length;
}

static int helper_retransmit_needed(picoquic_cnx_t* cnx,
    picoquic_packet_context_enum pc,
    picoquic_path_t * path_x, uint64_t current_time,
    picoquic_packet_t* packet, size_t send_buffer_max, int* is_cleartext_mode, uint32_t* header_length, protoop_id_t *reason)
{
    protoop_arg_t outs[2];
    protoop_arg_t args[7];
    args[0] = (protoop_arg_t) pc;
    args[1] = (protoop_arg_t) path_x;
    args[2] = (protoop_arg_t) current_time;
    args[3] = (protoop_arg_t) packet;
    args[4] = (protoop_arg_t) send_buffer_max;
    args[5] = (protoop_arg_t) *is_cleartext_mode;
    args[6] = (protoop_arg_t) *header_length;
    protoop_params_t pp = get_pp_noparam(PROTOOP_NOPARAM_RETRANSMIT_NEEDED, 7, args, outs);
    int ret = (int) plugin_run_protoop(cnx, &pp);
    *is_cleartext_mode = (int) outs[0];
    *header_length = (uint32_t) outs[1];
    *reason = (protoop_id_t) outs[2];
    return ret;
}

static uint32_t helper_prepare_mtu_probe(picoquic_cnx_t* cnx,
    picoquic_path_t * path_x,
    uint32_t header_length, uint32_t checksum_length,
    uint8_t* bytes)
{
    protoop_arg_t args[4];
    args[0] = (protoop_arg_t) path_x;
    args[1] = (protoop_arg_t) header_length;
    args[2] = (protoop_arg_t) checksum_length;
    args[3] = (protoop_arg_t) bytes;
    protoop_params_t pp = get_pp_noparam(PROTOOP_NOPARAM_PREPARE_MTU_PROBE, 4, args, NULL);
    return (uint32_t) plugin_run_protoop(cnx, &pp);
}

static int helper_prepare_path_challenge_frame(picoquic_cnx_t *cnx, uint8_t* bytes,
    size_t bytes_max, size_t* consumed, picoquic_path_t * path)
{
    protoop_arg_t outs[1];
    protoop_arg_t args[4];
    args[0] = (protoop_arg_t) bytes;
    args[1] = (protoop_arg_t) bytes_max;
    args[2] = (protoop_arg_t) *consumed;
    args[3] = (protoop_arg_t) path;
    protoop_params_t pp = get_pp_noparam("prepare_path_challenge_frame", 4, args, outs);
    int ret = (int) plugin_run_protoop(cnx, &pp);
    *consumed = (size_t) outs[0];
    return ret;
}

static int helper_prepare_ack_frame(picoquic_cnx_t* cnx, uint64_t current_time,
    picoquic_packet_context_enum pc,
    uint8_t* bytes, size_t bytes_max, size_t* consumed)
{
    protoop_arg_t outs[1];
    protoop_arg_t args[5];
    args[0] = (protoop_arg_t) current_time;
    args[1] = (protoop_arg_t) pc;
    args[2] = (protoop_arg_t) bytes;
    args[3] = (protoop_arg_t) bytes_max;
    args[4] = (protoop_arg_t) *consumed;
    protoop_params_t pp = get_pp_noparam("prepare_ack_frame", 5, args, outs);
    int ret = (int) plugin_run_protoop(cnx, &pp);
    *consumed = (size_t) outs[0];
    return ret;
}

static int helper_prepare_crypto_hs_frame(picoquic_cnx_t* cnx, int epoch,
    uint8_t* bytes, size_t bytes_max, size_t* consumed)
{
    protoop_arg_t outs[1];
    protoop_arg_t args[4];
    args[0] = (protoop_arg_t) epoch;
    args[1] = (protoop_arg_t) bytes;
    args[2] = (protoop_arg_t) bytes_max;
    args[3] = (protoop_arg_t) *consumed;
    protoop_params_t pp = get_pp_noparam("prepare_crypto_hs_frame", 4, args, outs);
    int ret = (int) plugin_run_protoop(cnx, &pp);
    *consumed = (size_t) outs[0];
    return ret;
}

static int helper_prepare_first_misc_frame(picoquic_cnx_t* cnx, uint8_t* bytes,
                                      size_t bytes_max, size_t* consumed)
{
    protoop_arg_t outs[1];
    protoop_arg_t args[3];
    args[0] = (protoop_arg_t) bytes;
    args[1] = (protoop_arg_t) bytes_max;
    args[2] = (protoop_arg_t) *consumed;
    protoop_params_t pp = get_pp_noparam("prepare_first_misc_frame", 3, args, outs);
    int ret = (int) plugin_run_protoop(cnx, &pp);
    *consumed = (size_t) outs[0];
    return ret;
}

static int helper_prepare_max_data_frame(picoquic_cnx_t* cnx, uint64_t maxdata_increase,
    uint8_t* bytes, size_t bytes_max, size_t* consumed)
{
    protoop_arg_t outs[1];
    protoop_arg_t args[4];
    args[0] = (protoop_arg_t) maxdata_increase;
    args[1] = (protoop_arg_t) bytes;
    args[2] = (protoop_arg_t) bytes_max;
    args[3] = (protoop_arg_t) *consumed;
    protoop_params_t pp = get_pp_noparam("prepare_max_data_frame", 4, args, outs);
    int ret = (int) plugin_run_protoop(cnx, &pp);
    *consumed = (size_t) outs[0];
    return ret;
}

static int helper_prepare_required_max_stream_data_frames(picoquic_cnx_t* cnx,
    uint8_t* bytes, size_t bytes_max, size_t* consumed)
{
    protoop_arg_t outs[1];
    protoop_arg_t args[3];
    args[0] = (protoop_arg_t) bytes;
    args[1] = (protoop_arg_t) bytes_max;
    args[2] = (protoop_arg_t) *consumed;
    protoop_params_t pp = get_pp_noparam("prepare_required_max_stream_data_frames", 3, args, outs);
    int ret = (int) plugin_run_protoop(cnx, &pp);
    *consumed = (size_t)outs[0];
    return ret;
}

static int helper_prepare_stream_frame(picoquic_cnx_t* cnx, picoquic_stream_head* stream,
    uint8_t* bytes, size_t bytes_max, size_t* consumed)
{
    protoop_arg_t outs[1];
    protoop_arg_t args[4];
    args[0] = (protoop_arg_t) stream;
    args[1] = (protoop_arg_t) bytes;
    args[2] = (protoop_arg_t) bytes_max;
    args[3] = (protoop_arg_t) *consumed;
    protoop_params_t pp = get_pp_noparam("prepare_stream_frame", 3, args, outs);
    int ret = (int) plugin_run_protoop(cnx, &pp);
    *consumed = (protoop_arg_t) outs[0];
    return ret;
}

static void helper_finalize_and_protect_packet(picoquic_cnx_t *cnx, picoquic_packet_t * packet, int ret, 
    uint32_t length, uint32_t header_length, uint32_t checksum_overhead,
    size_t * send_length, uint8_t * send_buffer, uint32_t send_buffer_max, 
    picoquic_path_t * path_x, uint64_t current_time)
{
    protoop_arg_t args[10];
    args[0] = (protoop_arg_t) packet;
    args[1] = (protoop_arg_t) ret;
    args[2] = (protoop_arg_t) length;
    args[3] = (protoop_arg_t) header_length;
    args[4] = (protoop_arg_t) checksum_overhead;
    args[5] = (protoop_arg_t) *send_length;
    args[6] = (protoop_arg_t) send_buffer;
    args[7] = (protoop_arg_t) send_buffer_max;
    args[8] = (protoop_arg_t) path_x;
    args[9] = (protoop_arg_t) current_time;
    protoop_params_t pp = get_pp_noparam(PROTOOP_NOPARAM_FINALIZE_AND_PROTECT_PACKET, 10, args, NULL);
    *send_length = (size_t) plugin_run_protoop(cnx, &pp);
}

/* TODO: tie with per path scheduling */
static void helper_cnx_set_next_wake_time(picoquic_cnx_t* cnx, uint64_t current_time)
{
    protoop_arg_t args[1];
    args[0] = (protoop_arg_t) current_time;
    protoop_params_t pp = get_pp_noparam(PROTOOP_NOPARAM_SET_NEXT_WAKE_TIME, 1, args, NULL);
    plugin_run_protoop(cnx, &pp);
}

static picoquic_packet_context_enum helper_context_from_epoch(int epoch)
{
    picoquic_packet_context_enum pc[4];
    pc[0] = picoquic_packet_context_initial;
    pc[1] = picoquic_packet_context_application;
    pc[2] = picoquic_packet_context_handshake;
    pc[3] = picoquic_packet_context_application;

    /* 5 to 4, bug in picoquic... */
    return (epoch >= 0 && epoch < 4) ? pc[epoch] : 0;
}

static int helper_connection_error(picoquic_cnx_t* cnx, uint16_t local_error, uint64_t frame_type)
{
    protoop_arg_t args[2];
    args[0] = (protoop_arg_t) local_error;
    args[1] = (protoop_arg_t) frame_type;
    protoop_params_t pp = get_pp_noparam(PROTOOP_NOPARAM_CONNECTION_ERROR, 2, args, NULL);
    return (int) plugin_run_protoop(cnx, &pp);
}

static uint8_t* helper_decode_stream_frame(picoquic_cnx_t* cnx, uint8_t* bytes, const uint8_t* bytes_max, uint64_t current_time)
{
    protoop_arg_t args[3];
    args[0] = (protoop_arg_t) bytes;
    args[1] = (protoop_arg_t) bytes_max;
    args[2] = (protoop_arg_t) current_time;
    protoop_params_t pp = get_pp_noparam(PROTOOP_NOPARAM_DECODE_STREAM_FRAME, 3, args, NULL);
    return (uint8_t *) plugin_run_protoop(cnx, &pp);
}

#define VARINT_LEN(bytes) (1U << (((bytes)[0] & 0xC0) >> 6))

/* Integer parsing macros */
#define PICOPARSE_16(b) ((((uint16_t)(b)[0]) << 8) | (b)[1])
#define PICOPARSE_24(b) ((((uint32_t)PICOPARSE_16(b)) << 16) | ((b)[2]))
#define PICOPARSE_32(b) ((((uint32_t)PICOPARSE_16(b)) << 16) | PICOPARSE_16((b) + 2))
#define PICOPARSE_64(b) ((((uint64_t)PICOPARSE_32(b)) << 32) | PICOPARSE_32((b) + 4))

/* Parse a varint. In case of an error, *n64 is unchanged, and NULL is returned */
static uint8_t* helper_frames_varint_decode(uint8_t* bytes, const uint8_t* bytes_max, uint64_t* n64)
{
    uint8_t length;

    if (bytes < bytes_max && bytes + (length=VARINT_LEN(bytes)) <= bytes_max) {
        uint64_t v = *bytes++ & 0x3F;

        while (--length > 0) {
            v <<= 8;
            v += *bytes++;
        }

        *n64 = v;
    } else {
        bytes = NULL;
    }

    return bytes;
}

static uint8_t* helper_frames_uint8_decode(uint8_t* bytes, const uint8_t* bytes_max, uint8_t* n)
{
    if (bytes < bytes_max) {
        *n = *bytes++;
    } else {
        bytes = NULL;
    }
    return bytes;
}


static uint8_t* helper_frames_uint16_decode(uint8_t* bytes, const uint8_t* bytes_max, uint16_t* n)
{
    if (bytes + sizeof(*n) <= bytes_max) {
        *n = PICOPARSE_16(bytes);
        bytes += sizeof(*n);
    } else {
        bytes = NULL;
    }
    return bytes;
}


static __attribute__((always_inline)) uint8_t* helper_frames_uint64_decode(uint8_t* bytes, const uint8_t* bytes_max, uint64_t* n)
{
    if (bytes + sizeof(*n) <= bytes_max) {
        *n = PICOPARSE_64(bytes);
        bytes += sizeof(*n);
    } else {
        bytes = NULL;
    }
    return bytes;
}

static uint8_t *helper_parse_frame(picoquic_cnx_t *cnx, uint8_t frame_type, uint8_t *bytes, const uint8_t *bytes_max,
    void **frame, int *ack_needed, int *is_retransmittable)
{
    protoop_arg_t args[2], outs[3];
    args[0] = (protoop_arg_t) bytes;
    args[1] = (protoop_arg_t) bytes_max;
    protoop_params_t pp = get_pp_param(PROTOOP_PARAM_PARSE_FRAME, frame_type, 2, args, outs);
    uint8_t *ret_bytes = (uint8_t *) plugin_run_protoop(cnx, &pp);
    *frame = (void *) outs[0];
    *ack_needed = (int) outs[1];
    *is_retransmittable = (int) outs[2];
    return ret_bytes;
}

static int helper_parse_ack_header(uint8_t const* bytes, size_t bytes_max,
    uint64_t* num_block, uint64_t* nb_ecnx3,
    uint64_t* largest, uint64_t* ack_delay, size_t* consumed,
    uint8_t ack_delay_exponent)
{
    int ret = 0;
    size_t byte_index = 1;
    size_t l_largest = 0;
    size_t l_delay = 0;
    size_t l_blocks = 0;

    if (bytes_max > byte_index) {
        l_largest = picoquic_varint_decode(bytes + byte_index, bytes_max - byte_index, largest);
        byte_index += l_largest;
    }

    if (bytes_max > byte_index) {
        l_delay = picoquic_varint_decode(bytes + byte_index, bytes_max - byte_index, ack_delay);
        *ack_delay <<= ack_delay_exponent;
        byte_index += l_delay;
    }

    if (nb_ecnx3 != NULL) {
        for (int ecnx = 0; ecnx < 3; ecnx++) {
            size_t l_ecnx = picoquic_varint_decode(bytes + byte_index, bytes_max - byte_index, &nb_ecnx3[ecnx]);

            if (l_ecnx == 0) {
                byte_index = bytes_max;
            }
            else {
                byte_index += l_ecnx;
            }
        }
    }

    if (bytes_max > byte_index) {
        l_blocks = picoquic_varint_decode(bytes + byte_index, bytes_max - byte_index, num_block);
        byte_index += l_blocks;
    }

    if (l_largest == 0 || l_delay == 0 || l_blocks == 0 || bytes_max < byte_index) {
        // DBG_PRINTF("ack frame fixed header too large: first_byte=0x%02x, bytes_max=%" PRIst,
        //     bytes[0], bytes_max);
        byte_index = bytes_max;
        ret = -1;
    }

    *consumed = byte_index;
    return ret;
}

static picoquic_packet_t* helper_update_rtt(picoquic_cnx_t* cnx, uint64_t largest,
    uint64_t current_time, uint64_t ack_delay, picoquic_packet_context_enum pc,
    picoquic_path_t* path_x)
{
    protoop_arg_t args[5];
    args[0] = (protoop_arg_t) largest;
    args[1] = (protoop_arg_t) current_time;
    args[2] = (protoop_arg_t) ack_delay;
    args[3] = (protoop_arg_t) pc;
    args[4] = (protoop_arg_t) path_x;
    protoop_params_t pp = get_pp_noparam(PROTOOP_NOPARAM_UPDATE_RTT, 5, args, NULL);
    return (picoquic_packet_t *) plugin_run_protoop(cnx, &pp);
}

static int helper_process_ack_range(
    picoquic_cnx_t* cnx, picoquic_packet_context_enum pc, uint64_t highest, uint64_t range, picoquic_packet_t** ppacket,
    uint64_t current_time)
{
    protoop_arg_t args[5], outs[1];
    args[0] = (protoop_arg_t) pc;
    args[1] = (protoop_arg_t) highest;
    args[2] = (protoop_arg_t) range;
    args[3] = (protoop_arg_t) *ppacket;
    args[4] = (protoop_arg_t) current_time;
    protoop_params_t pp = get_pp_noparam(PROTOOP_NOPARAM_PROCESS_ACK_RANGE, 5, args, outs);
    int ret = (int) plugin_run_protoop(cnx, &pp);
    *ppacket = (picoquic_packet_t*) outs[0];
    return ret;
}

static void helper_check_spurious_retransmission(picoquic_cnx_t* cnx,
    uint64_t start_of_range, uint64_t end_of_range, uint64_t current_time,
    picoquic_packet_context_enum pc, picoquic_path_t* path_x)
{
    protoop_arg_t args[5];
    args[0] = (protoop_arg_t) start_of_range;
    args[1] = (protoop_arg_t) end_of_range;
    args[2] = (protoop_arg_t) current_time;
    args[3] = (protoop_arg_t) pc;
    args[4] = (protoop_arg_t) path_x;
    protoop_params_t pp = get_pp_noparam(PROTOOP_NOPARAM_CHECK_SPURIOUS_RETRANSMISSION, 5, args, NULL);
    plugin_run_protoop(cnx, &pp);
}

static void helper_process_possible_ack_of_ack_frame(picoquic_cnx_t* cnx, picoquic_packet_t* p)
{
    protoop_arg_t args[1];
    args[0] = (protoop_arg_t) p;
    protoop_params_t pp = get_pp_noparam(PROTOOP_NOPARAM_PROCESS_POSSIBLE_ACK_OF_ACK_FRAME, 1, args, NULL);
    plugin_run_protoop(cnx, &pp);
}

static int helper_process_ack_of_stream_frame(picoquic_cnx_t* cnx, uint8_t* bytes,
    size_t bytes_max, size_t* consumed)
{
    protoop_arg_t args[3], outs[1];
    args[0] = (protoop_arg_t) bytes;
    args[1] = (protoop_arg_t) bytes_max;
    args[2] = (protoop_arg_t) *consumed;
    protoop_params_t pp = get_pp_noparam(PROTOOP_NOPARAM_PROCESS_ACK_OF_STREAM_FRAME, 3, args, outs);
    int ret = (int) plugin_run_protoop(cnx, &pp);
    *consumed = (size_t) outs[0];
    return ret;
}

/**
 *  return_values must contain 5 pointers to:
 *
 *  uint64_t* stream_id
 *  uint64_t* offset
 *  size_t* data_length
 *  int* fin
    size_t* consumed
 */
static int helper_parse_stream_header(const uint8_t* bytes, size_t bytes_max, protoop_arg_t** return_values) {
    int ret = 0;
    int len = bytes[0] & 2;
    int off = bytes[0] & 4;
    uint64_t length = 0;
    size_t l_stream = 0;
    size_t l_len = 0;
    size_t l_off = 0;
    size_t byte_index = 1;

    uint64_t* stream_id = *(return_values);
    uint64_t* offset = *(return_values + 1);
    size_t* data_length = *(return_values + 2);
    int* fin = (int *) *(return_values + 3);
    size_t* consumed = *(return_values + 4);

    *fin = bytes[0] & 1;

    if (bytes_max > byte_index) {
        l_stream = picoquic_varint_decode(bytes + byte_index, bytes_max - byte_index, stream_id);
        byte_index += l_stream;
    }

    if (off == 0) {
        *offset = 0;
    } else if (bytes_max > byte_index) {
        l_off = picoquic_varint_decode(bytes + byte_index, bytes_max - byte_index, offset);
        byte_index += l_off;
    }

    if (bytes_max < byte_index || l_stream == 0 || (off != 0 && l_off == 0)) {
        //DBG_PRINTF("stream frame header too large: first_byte=0x%02x, bytes_max=%" PRIst, bytes[0], bytes_max);
        *data_length = 0;
        byte_index = bytes_max;
        ret = -1;
    } else if (len == 0) {
        *data_length = bytes_max - byte_index;
    } else {
        if (bytes_max > byte_index) {
            l_len = picoquic_varint_decode(bytes + byte_index, bytes_max - byte_index, &length);
            byte_index += l_len;
            *data_length = (size_t)length;
        }

        if (l_len == 0 || bytes_max < byte_index) {
            //DBG_PRINTF("stream frame header too large: first_byte=0x%02x, bytes_max=%" PRIst, bytes[0], bytes_max);
            byte_index = bytes_max;
            ret = -1;
        } else if (byte_index + length > bytes_max) {
            //DBG_PRINTF("stream data past the end of the packet: first_byte=0x%02x, data_length=%" PRIst ", max_bytes=%" PRIst, bytes[0], *data_length, bytes_max);
            ret = -1;
        }
    }

    *consumed = byte_index;
    return ret;
}

static int helper_packet_was_retransmitted(picoquic_cnx_t* cnx, protoop_id_t reason, picoquic_packet_t *p)
{
    protoop_arg_t args[1], outs[0];
    args[0] = (protoop_arg_t) p;
    protoop_params_t pp = get_pp_noparam(reason, 1, args, outs);
    int ret = (int) plugin_run_protoop(cnx, &pp);
    return ret;
}

#endif