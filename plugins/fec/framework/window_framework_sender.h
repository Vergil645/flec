#include "../fec.h"
#include "../../helpers.h"
#include "../fec_protoops.h"
#include "../causal_redundancy_controller_protoops/causal_redundancy_controller_only_fb_fec.h"

#define INITIAL_SYMBOL_ID 1
#define MAX_QUEUED_REPAIR_SYMBOLS 6
#define MAX_SLOT_VALUE 0x7FFFFF

#define MIN(a, b) ((a < b) ? a : b)

typedef uint32_t fec_block_number;

typedef struct {
    repair_symbol_t *repair_symbol;
    uint8_t nss;
    uint8_t nrs;
} queue_item;

typedef struct window_slot {
    source_symbol_t *symbol;
    bool received;
} window_slot_t;

typedef struct {
    fec_scheme_t fec_scheme;
    causal_redundancy_controller_t *controller;
    window_slot_t fec_window[RECEIVE_BUFFER_MAX_LENGTH];
    queue_item repair_symbols_queue[MAX_QUEUED_REPAIR_SYMBOLS];
    uint32_t max_id;
    uint32_t min_id;
    uint32_t highest_sent_id;
    uint32_t smallest_in_transit;
    uint32_t highest_in_transit;
    int window_length;
    int repair_symbols_queue_head;
    int repair_symbols_queue_length;
    int queue_byte_offset;  // current byte offset in the current repair symbol
    int queue_piece_offset; // current piece number of the current repair symbol
    int64_t current_slot;
} window_fec_framework_t;

static __attribute__((always_inline)) bool is_fec_window_empty(window_fec_framework_t *wff) {
    return wff->window_length == 0;
}

static __attribute__((always_inline)) window_fec_framework_t *create_framework_sender(picoquic_cnx_t *cnx, fec_redundancy_controller_t controller, fec_scheme_t fs) {
    window_fec_framework_t *wff = (window_fec_framework_t *) my_malloc(cnx, sizeof(window_fec_framework_t));
    if (!wff)
        return NULL;
    my_memset(wff, 0, sizeof(window_fec_framework_t));
    wff->highest_sent_id = INITIAL_SYMBOL_ID-1;
    wff->highest_in_transit = INITIAL_SYMBOL_ID-1;
    wff->smallest_in_transit = INITIAL_SYMBOL_ID-1;
    wff->controller = controller;
    wff->fec_scheme = fs;
    return wff;
}
static __attribute__((always_inline)) bool ready_to_send(picoquic_cnx_t *cnx, window_fec_framework_t *wff) {
    uint8_t k = 0;
    get_redundancy_parameters(cnx, wff->controller, false, NULL, &k);
    return (wff->max_id-wff->highest_sent_id >= k);
}

static __attribute__((always_inline)) bool has_repair_symbol_at_index(window_fec_framework_t *wff, int idx) {
    return wff->repair_symbols_queue[idx].repair_symbol != NULL;
}

static __attribute__((always_inline)) void remove_item_at_index(picoquic_cnx_t *cnx, window_fec_framework_t *wff, int idx) {
    free_repair_symbol(cnx, wff->repair_symbols_queue[idx].repair_symbol);
    wff->repair_symbols_queue[idx].repair_symbol = NULL;
    wff->repair_symbols_queue[idx].nss = 0;
    wff->repair_symbols_queue[idx].nrs = 0;
}

static __attribute__((always_inline)) void put_item_at_index(window_fec_framework_t *wff, int idx, repair_symbol_t *rs, uint8_t nss, uint8_t nrs) {
    wff->repair_symbols_queue[idx].repair_symbol = rs;
    wff->repair_symbols_queue[idx].nss = nss;
    wff->repair_symbols_queue[idx].nrs = nrs;
}

// adds a repair symbol in the queue waiting for the symbol to be sent
static __attribute__((always_inline)) void queue_repair_symbol(picoquic_cnx_t *cnx, window_fec_framework_t *wff, repair_symbol_t *rs, fec_block_t *fb){
    int idx = ((uint32_t) ((uint32_t) wff->repair_symbols_queue_head + wff->repair_symbols_queue_length)) % MAX_QUEUED_REPAIR_SYMBOLS;
    if (has_repair_symbol_at_index(wff, idx)) {
        remove_item_at_index(cnx, wff, idx);
        if (wff->repair_symbols_queue_length > 1 && wff->repair_symbols_queue_head == idx) {
            // the head is the next symbol
            wff->repair_symbols_queue_head = ((uint32_t) ( (uint32_t) wff->repair_symbols_queue_head + 1)) % MAX_QUEUED_REPAIR_SYMBOLS;
            wff->queue_byte_offset = 0;
        }
        wff->repair_symbols_queue_length--;
    }
    put_item_at_index(wff, idx, rs, fb->total_source_symbols, fb->total_repair_symbols);
    if (wff->repair_symbols_queue_length == 0) {
        wff->repair_symbols_queue_head = idx;
        wff->queue_byte_offset = 0;
    }
    wff->repair_symbols_queue_length++;
}

// adds a repair symbol in the queue waiting for the symbol to be sent
static __attribute__((always_inline)) void queue_repair_symbols(picoquic_cnx_t *cnx, window_fec_framework_t *wff, repair_symbol_t *rss[], int number_of_symbols, fec_block_t *fec_block){
    int i;
    for (i = 0 ; i < number_of_symbols ; i++) {
        queue_repair_symbol(cnx, wff, rss[i], fec_block);
    }
}

static __attribute__((always_inline)) size_t get_repair_payload_from_queue(picoquic_cnx_t *cnx, window_fec_framework_t *wff, size_t bytes_max, fec_frame_header_t *ffh, uint8_t *bytes){
    if (wff->repair_symbols_queue_length == 0)
        return 0;
    repair_symbol_t *rs = wff->repair_symbols_queue[wff->repair_symbols_queue_head].repair_symbol;
    // FIXME: temporarily ensure that the repair symbols are not split into multiple frames
    if (bytes_max < rs->data_length) {
        PROTOOP_PRINTF(cnx, "NOT ENOUGH BYTES TO SEND SYMBOL: %u < %u\n", bytes_max, rs->data_length);
        return 0;
    }
    size_t amount = ((rs->data_length - wff->queue_byte_offset) <= bytes_max) ? (rs->data_length - wff->queue_byte_offset) : bytes_max;
    // copy
    my_memcpy(bytes, rs->data + wff->queue_byte_offset, amount);
    // move forward in the symbol's buffer
    wff->queue_byte_offset += amount;
    wff->queue_piece_offset++;

    ffh->repair_fec_payload_id = rs->repair_fec_payload_id;
    PROTOOP_PRINTF(cnx, "GET RS WITH FEC SCHEME SPECIFIC %u\n", ffh->repair_fec_payload_id.fec_scheme_specific);
    ffh->offset = (uint8_t) wff->queue_piece_offset;
    ffh->nss = wff->repair_symbols_queue[wff->repair_symbols_queue_head].nss;
    ffh->nrs = wff->repair_symbols_queue[wff->repair_symbols_queue_head].nrs;
    ffh->data_length = (uint16_t) amount;
    ffh->fin_bit = wff->queue_byte_offset == rs->data_length;
    protoop_arg_t args[2];
    args[0] = amount;
    args[1] = bytes_max;
    if (wff->queue_byte_offset == rs->data_length) {
        // this symbol has been sent: free the symbol and remove it from the queue
        remove_item_at_index(cnx, wff, wff->repair_symbols_queue_head);
        wff->repair_symbols_queue_head = ((uint32_t) wff->repair_symbols_queue_head + 1) % MAX_QUEUED_REPAIR_SYMBOLS;
        wff->queue_byte_offset = 0;
        wff->queue_piece_offset = 0;
        wff->repair_symbols_queue_length--;
        args[0] = (uint32_t) wff->repair_symbols_queue_length;
    }
    return amount;
}

//TODO: currently unprovable
static __attribute__((always_inline)) int reserve_fec_frames(picoquic_cnx_t *cnx, window_fec_framework_t *wff, size_t size_max) {
    if (size_max <= sizeof(fec_frame_header_t))
        return -1;
    while (wff->repair_symbols_queue_length != 0) {
        // FIXME: bourrin
        fec_frame_t *ff = my_malloc(cnx, sizeof(fec_frame_t));
        if (!ff)
            return PICOQUIC_ERROR_MEMORY;
        uint8_t *bytes = my_malloc(cnx, (unsigned int) (size_max - (1 + sizeof(fec_frame_header_t))));
        if (!bytes)
            return PICOQUIC_ERROR_MEMORY;
        // copy the frame payload
        size_t payload_size = get_repair_payload_from_queue(cnx, wff, size_max - sizeof(fec_frame_header_t) - 1, &ff->header, bytes);
        if (!payload_size)
            return PICOQUIC_ERROR_FRAME_BUFFER_TOO_SMALL;
        ff->data = bytes;
        reserve_frame_slot_t *slot = (reserve_frame_slot_t *) my_malloc(cnx, sizeof(reserve_frame_slot_t));
        if (!slot)
            return PICOQUIC_ERROR_MEMORY;
        my_memset(slot, 0, sizeof(reserve_frame_slot_t));
        slot->frame_type = FEC_TYPE;
        slot->nb_bytes = 1 + sizeof(fec_frame_header_t) + payload_size;
        slot->frame_ctx = ff;
        slot->is_congestion_controlled = true;
        PROTOOP_PRINTF(cnx, "RESERVE FEC FRAMES\n");
        size_t reserved_size = reserve_frames(cnx, 1, slot);
        if (reserved_size < slot->nb_bytes) {
            PROTOOP_PRINTF(cnx, "Unable to reserve frame slot\n");
            my_free(cnx, ff->data);
            my_free(cnx, ff);
            my_free(cnx, slot);
            return 1;
        }
    }
    return 0;
}

static __attribute__((always_inline)) int reserve_fec_frame_for_repair_symbol(picoquic_cnx_t *cnx, window_fec_framework_t *wff, size_t size_max, repair_symbol_t *rs, uint8_t nss) {
    // FIXME: bourrin
    fec_frame_t *ff = my_malloc(cnx, sizeof(fec_frame_t));
    if (!ff)
        return PICOQUIC_ERROR_MEMORY;
    if (!rs->data_length || rs->data_length > size_max)
        return PICOQUIC_ERROR_FRAME_BUFFER_TOO_SMALL;
    ff->data = rs->data;
    ff->header.data_length = rs->data_length;
    ff->header.offset = rs->symbol_number;
    ff->header.repair_fec_payload_id = rs->repair_fec_payload_id;
    ff->header.nss = nss;
    ff->header.nrs = 0;
    reserve_frame_slot_t *slot = (reserve_frame_slot_t *) my_malloc(cnx, sizeof(reserve_frame_slot_t));
    if (!slot)
        return PICOQUIC_ERROR_MEMORY;
    my_memset(slot, 0, sizeof(reserve_frame_slot_t));
    slot->frame_type = FEC_TYPE;
    slot->nb_bytes = 1 + sizeof(fec_frame_header_t) + rs->data_length;
    slot->frame_ctx = ff;
    slot->is_congestion_controlled = true;
    PROTOOP_PRINTF(cnx, "RESERVE FEC FRAMES\n");
    size_t reserved_size = reserve_frames(cnx, 1, slot);
    PROTOOP_PRINTF(cnx, "RESERVING REPAIR FRAME\n");
    if (reserved_size < slot->nb_bytes) {
        PROTOOP_PRINTF(cnx, "Unable to reserve frame slot\n");
        my_free(cnx, ff->data);
        my_free(cnx, ff);
        my_free(cnx, slot);
        return 1;
    }
    return 0;
}



static __attribute__((always_inline)) bool _remove_source_symbol_from_window(picoquic_cnx_t *cnx, window_fec_framework_t *wff, source_symbol_t *ss){

    int idx = (int) (ss->source_fec_payload_id.raw % RECEIVE_BUFFER_MAX_LENGTH);
    // wrong symbol ?
    if (!wff->fec_window[idx].symbol || wff->fec_window[idx].symbol->source_fec_payload_id.raw != ss->source_fec_payload_id.raw)
        return false;
    if (wff->fec_window[idx].symbol->source_fec_payload_id.raw == wff->min_id) wff->min_id++;
    free_source_symbol(cnx, wff->fec_window[idx].symbol);
    wff->fec_window[idx].symbol = NULL;
    wff->fec_window[idx].received = false;
    // one less symbol
    wff->window_length--;
    return true;
}

static __attribute__((always_inline)) bool remove_source_symbol_from_window(picoquic_cnx_t *cnx, window_fec_framework_t *wff, source_symbol_t *ss){
    if (ss) {
        if (ss->source_fec_payload_id.raw != wff->smallest_in_transit || is_fec_window_empty(wff)) {
            return false;
        }


        if (!_remove_source_symbol_from_window(cnx, wff, ss))
            return false;
        wff->smallest_in_transit++;

        while(!is_fec_window_empty(wff) && wff->fec_window[wff->smallest_in_transit % RECEIVE_BUFFER_MAX_LENGTH].received) {

            if (!_remove_source_symbol_from_window(cnx, wff, wff->fec_window[wff->smallest_in_transit % RECEIVE_BUFFER_MAX_LENGTH].symbol))
                return false;
            wff->smallest_in_transit++;
        }

        if (is_fec_window_empty(wff)) {
            wff->smallest_in_transit = wff->highest_in_transit = INITIAL_SYMBOL_ID-1;
        }
        return true;
    }
    return false;
}

static __attribute__((always_inline)) bool add_source_symbol_to_window(picoquic_cnx_t *cnx, window_fec_framework_t *wff, source_symbol_t *ss){
    if (ss) {
        PROTOOP_PRINTF(cnx, "SOURCE SYMBOL ADDED\n");
        int idx = (int) (ss->source_fec_payload_id.raw % RECEIVE_BUFFER_MAX_LENGTH);
        if (wff->fec_window[idx].symbol || ss->source_fec_payload_id.raw <= wff->highest_in_transit) {
            PROTOOP_PRINTF(cnx, "ANOTHER SYMBOL IS THERE: %p, RAW = %u, HIGHEST IN TRANSIT = %u\n", (protoop_arg_t) wff->fec_window[idx].symbol, ss->source_fec_payload_id.raw, wff->highest_in_transit);
            // we cannot add a symbol if another one is already there
            PROTOOP_PRINTF(cnx, "SMALLEST IN TRANSIT = %u, HIGHEST IN TRANSIT = %u\n", wff->smallest_in_transit, wff->highest_in_transit);
            return false;
        }
        wff->fec_window[idx].symbol = ss;
        wff->fec_window[idx].received = false;
        if (wff->window_length == 0) {
            wff->min_id = wff->max_id = ss->source_fec_payload_id.raw;
            wff->highest_in_transit = ss->source_fec_payload_id.raw;
            wff->smallest_in_transit = ss->source_fec_payload_id.raw;
        }
        // one more symbol
        wff->window_length++;
        wff->highest_in_transit = ss->source_fec_payload_id.raw;
        return true;
    }
    PROTOOP_PRINTF(cnx, "SS = NULL\n");
    return false;

}

static __attribute__((always_inline)) int sfpid_has_landed(picoquic_cnx_t *cnx, window_fec_framework_t *wff, source_fpid_t sfpid, bool received) {
    PROTOOP_PRINTF(cnx, "SYMBOL %d LANDED, RECEIVED = %d\n", sfpid.raw, received);
    // remove all the needed symbols from the window
    uint32_t idx = sfpid.raw % RECEIVE_BUFFER_MAX_LENGTH;
    if (received && wff->fec_window[idx].symbol) {
        if (wff->fec_window[idx].symbol->source_fec_payload_id.raw == sfpid.raw) {
            wff->fec_window[idx].received = true;
            // if it is the first symbol of the window, let's prune the window
            if (!is_fec_window_empty(wff) && sfpid.raw == wff->smallest_in_transit) {
                if (!remove_source_symbol_from_window(cnx, wff, wff->fec_window[idx].symbol)) {
                    return -1;
                }
            }
        }
    }
    return 0;
}

static __attribute__((always_inline)) void sfpid_takes_off(window_fec_framework_t *wff, source_fpid_t sfpid) {
    wff->highest_in_transit = MAX(wff->highest_in_transit, sfpid.raw);
    wff->smallest_in_transit = MIN(wff->smallest_in_transit, sfpid.raw);
}



static __attribute__((always_inline)) void remove_source_symbols_from_block_and_window(picoquic_cnx_t *cnx, window_fec_framework_t *wff, fec_block_t *fb){
    for (int i = 0 ; i < fb->total_source_symbols ; i++) {
        source_symbol_t *ss = fb->source_symbols[i];
        remove_source_symbol_from_window(cnx, wff, ss);
        fb->source_symbols[i] = NULL;
    }
    fb->total_source_symbols = 0;
    fb->current_source_symbols = 0;
}

static __attribute__((always_inline)) int generate_and_queue_repair_symbols(picoquic_cnx_t *cnx, window_fec_framework_t *wff, bool flush){
    protoop_arg_t args[3];
    protoop_arg_t outs[1];

    // build the block to generate the symbols

    fec_block_t *fb = malloc_fec_block(cnx, 0);
    if (!fb)
        return PICOQUIC_ERROR_MEMORY;

    args[0] = (protoop_arg_t) fb;
    args[1] = (protoop_arg_t) wff;
    args[2] = flush;

    int ret = 0;
    bool should_send_rs = (bool) run_noparam(cnx, "should_send_repair_symbols", 0, NULL, NULL);
    if (should_send_rs) {
        ret = (int) run_noparam(cnx, "window_select_symbols_to_protect", 3, args, outs);
        if (ret) {
            my_free(cnx, fb);
            PROTOOP_PRINTF(cnx, "ERROR WHEN SELECTING THE SYMBOLS TO PROTECT\n");
            return ret;
        }
        if (fb->total_source_symbols > 0) {
            PROTOOP_PRINTF(cnx, "PROTECT FEC BLOCK OF %u INFLIGHT SYMBOLS, SFPID = %u\n", fb->total_source_symbols, fb->source_symbols[0]->source_fec_payload_id.raw);
            args[1] = (protoop_arg_t) wff->fec_scheme;
            ret = (int) run_noparam(cnx, "fec_generate_repair_symbols", 2, args, outs);
            if (!ret) {
                PROTOOP_PRINTF(cnx, "SUCCESSFULLY GENERATED\n");
                uint8_t i = 0;
                for_each_repair_symbol(fb, repair_symbol_t *rs) {
                        rs->fec_block_number = 0;
                        rs->symbol_number = i++;
                }

                queue_repair_symbols(cnx, wff, fb->repair_symbols, fb->total_repair_symbols, fb);
                uint32_t last_id = fb->source_symbols[fb->total_source_symbols-1]->source_fec_payload_id.raw;
                // we don't free the source symbols: they can still be used afterwards
                // we don't free the repair symbols, they are queued and will be free afterwards
                // so we only free the fec block
                wff->highest_sent_id = MAX(last_id, wff->highest_sent_id);
                reserve_fec_frames(cnx, wff, PICOQUIC_MAX_PACKET_SIZE);
            }
        } else {
            PROTOOP_PRINTF(cnx, "NO SYMBOL TO PROTECT\n");
        }
    }


    my_free(cnx, fb);
    return ret;
}


static __attribute__((always_inline)) source_fpid_t get_source_fpid(window_fec_framework_t *wff){
    source_fpid_t s;
    s.raw = wff->max_id + 1;
    return s;
}

static __attribute__((always_inline)) int select_all_inflight_source_symbols(picoquic_cnx_t *cnx, window_fec_framework_t *wff, fec_block_t *fb)
{
    fb->current_source_symbols = 0;
    for (int i = MAX(wff->smallest_in_transit, wff->highest_in_transit - MIN(RECEIVE_BUFFER_MAX_LENGTH, wff->highest_in_transit)) ; i <= wff->highest_in_transit ; i++) {
        source_symbol_t *ss = wff->fec_window[((uint32_t) i) % RECEIVE_BUFFER_MAX_LENGTH].symbol;
        if (!ss || ss->source_fec_payload_id.raw != i)
            return -1;
        fb->source_symbols[fb->current_source_symbols++] = ss;
    }
    fb->total_source_symbols = fb->current_source_symbols;
    fb->total_repair_symbols = 1;

    return 0;
}

static __attribute__((always_inline)) repair_symbol_t *get_one_coded_symbol(picoquic_cnx_t *cnx, window_fec_framework_t *wff, uint8_t *nss, causal_packet_type_t type){
    protoop_arg_t args[3];
    fec_block_t *fb = malloc_fec_block(cnx, 0);
    if (!fb)
        return NULL;

    if (select_all_inflight_source_symbols(cnx, wff, fb)) {
        my_free(cnx, fb);
        return NULL;
    }
    if (wff->window_length == 0)
        return NULL;

    args[0] = (protoop_arg_t) fb;
    args[1] = (protoop_arg_t) wff->fec_scheme;
    args[2] = (protoop_arg_t) wff->current_slot;

    run_noparam(cnx, "rlc_gf256_get_one_coded_symbol", 3, args, NULL);
    if (fb->total_repair_symbols != 1)
        return NULL;
    repair_symbol_t *rs = fb->repair_symbols[0];
    if (!rs)
        return NULL;
    rs->fec_scheme_specific = wff->current_slot;
    if (type == fb_fec_packet)
        rs->fec_scheme_specific |= (1UL << 31U);
    rs->repair_fec_payload_id.source_fpid.raw = fb->source_symbols[0]->source_fec_payload_id.raw;
    *nss = fb->total_source_symbols;
    my_free(cnx, fb);
    return rs;
}

static __attribute__((always_inline)) int protect_source_symbol(picoquic_cnx_t *cnx, window_fec_framework_t *wff, source_symbol_t *ss){
    ss->source_fec_payload_id.raw = ++wff->max_id;
    if (!add_source_symbol_to_window(cnx, wff, ss)) {
        PROTOOP_PRINTF(cnx, "COULDN't ADD\n");
        return -1;
    }

    wff->highest_in_transit = ss->source_fec_payload_id.raw;

    return 0;
}

static __attribute__((always_inline)) int flush_fec_window(picoquic_cnx_t *cnx, window_fec_framework_t *wff) {
    return 0;
}


static __attribute__((always_inline)) uint64_t window_sent_symbol(picoquic_cnx_t *cnx, window_fec_framework_t *wff, causal_packet_type_t type, fec_window_t *window) {
    sent_packet(wff->controller, type, wff->current_slot, window);
    return wff->current_slot++;
}

static __attribute__((always_inline)) fec_window_t get_current_rlnc_window(picoquic_cnx_t *cnx, window_fec_framework_t *wff) {
    fec_window_t window;
    window.start = 0;
    window.end = 0;
    if (!is_fec_window_empty(wff)) {
        window.start = wff->smallest_in_transit;
        window.end = wff->highest_in_transit + 1;
    }
    return window;
}


static __attribute__((always_inline)) void window_slot_acked(picoquic_cnx_t *cnx, window_fec_framework_t *wff, uint64_t slot) {
    fec_window_t window = get_current_rlnc_window(cnx, wff);
    slot_acked(cnx, wff->controller, slot, &window);
    PROTOOP_PRINTF(cnx, "END SLOT ACKED\n");
}

static __attribute__((always_inline)) void window_free_slot_without_feedback(picoquic_cnx_t *cnx, window_fec_framework_t *wff) {
    fec_window_t window = get_current_rlnc_window(cnx, wff);
    free_slot_without_feedback(cnx, wff->controller, &window);
}

static __attribute__((always_inline)) void window_slot_nacked(picoquic_cnx_t *cnx, window_fec_framework_t *wff, uint64_t slot) {
    fec_window_t window = get_current_rlnc_window(cnx, wff);
    slot_nacked(cnx, wff->controller, slot, &window);
}

static __attribute__((always_inline)) causal_packet_type_t window_what_to_send(picoquic_cnx_t *cnx, window_fec_framework_t *wff) {
    if (wff->window_length == MAX_SLOTS)
        return fec_packet;
    causal_packet_type_t what = what_to_send(cnx, wff->controller);
    if (what == nothing) {
        window_free_slot_without_feedback(cnx, wff);
        what = what_to_send(cnx, wff->controller);
        if (what == nothing)
            return new_rlnc_packet;
    }
    // if the window is empty, we cannot send a FEC packet, so just send a new packet
    return is_fec_window_empty(wff) ? new_rlnc_packet : what;
}

static __attribute__((always_inline)) int window_detect_lost_protected_packets(picoquic_cnx_t *cnx, uint64_t current_time, picoquic_packet_context_enum pc) {

    char *reason = NULL;
    int nb_paths = (int) get_cnx(cnx, AK_CNX_NB_PATHS, 0);
    bool stop = false;
    for (int i = 0; i < nb_paths; i++) {
        picoquic_path_t *orig_path = (picoquic_path_t *) get_cnx(cnx, AK_CNX_PATH, i);
        picoquic_packet_context_t *orig_pkt_ctx = (picoquic_packet_context_t *) get_path(orig_path, AK_PATH_PKT_CTX,
                                                                                         pc);
        picoquic_packet_t *p = (picoquic_packet_t *) get_pkt_ctx(orig_pkt_ctx, AK_PKTCTX_RETRANSMIT_OLDEST);
        /* TODO: while packets are pure ACK, drop them from retransmit queue */
        while (p != NULL) {
            int should_retransmit = 0;
            int timer_based_retransmit = 0;
            uint64_t lost_packet_number = (uint64_t) get_pkt(p, AK_PKT_SEQUENCE_NUMBER);
            picoquic_packet_t *p_next = (picoquic_packet_t *) get_pkt(p, AK_PKT_NEXT_PACKET);
            picoquic_packet_type_enum ptype = (picoquic_packet_type_enum) get_pkt(p, AK_PKT_TYPE);

            /* Get the packet type */

            should_retransmit = helper_retransmit_needed_by_packet(cnx, p, current_time, &timer_based_retransmit,
                                                                   &reason);

            if (should_retransmit == 0) {
                /*
                * Always retransmit in order. If not this one, then nothing.
                * But make an exception for 0-RTT packets.
                */
                if (ptype == picoquic_packet_0rtt_protected) {
                    p = p_next;
                    continue;
                } else {
                    stop = true;
                    break;
                }
            } else {
                /* check if this is an ACK only packet */
//                int contains_crypto = (int) get_pkt(p, AK_PKT_CONTAINS_CRYPTO);
                int packet_is_pure_ack = (int) get_pkt(p, AK_PKT_IS_PURE_ACK);
                int do_not_detect_spurious = 1;
                /* TODO: should be the path on which the packet was transmitted */
                picoquic_path_t *old_path = (picoquic_path_t *) get_pkt(p, AK_PKT_SEND_PATH);

                if (should_retransmit != 0) {

                    /* Update the number of bytes in transit and remove old packet from queue */
                    /* If not pure ack, the packet will be placed in the "retransmitted" queue,
                    * in order to enable detection of spurious restransmissions */
                    // if the pc is the application, we want to free the packet: indeed, we disable the retransmissions

                    uint64_t slot = get_pkt_metadata(cnx, p, FEC_PKT_METADATA_SENT_SLOT);
                    bool fec_protected = get_pkt_metadata(cnx, p, FEC_PKT_METADATA_IS_FEC_PROTECTED);
                    bool fec_related = fec_protected || get_pkt_metadata(cnx, p, FEC_PKT_METADATA_CONTAINS_FEC_PACKET);
                    uint32_t sfpid = get_pkt_metadata(cnx, p, FEC_PKT_METADATA_FIRST_SOURCE_SYMBOL_ID);
                    bpf_state *state = NULL;
                    if (fec_related) {
                        state = get_bpf_state(cnx);
                        if (!state)
                            return PICOQUIC_ERROR_MEMORY;
                        state->current_packet_is_lost = true;
                        int err = 0;
                        if (fec_protected &&
                            (err = add_lost_packet(cnx, &state->lost_packets, lost_packet_number, slot, sfpid))) {
                            return err;
                        }
                        helper_dequeue_retransmit_packet(cnx, p, (packet_is_pure_ack & do_not_detect_spurious) ||
                                                                 (pc == picoquic_packet_context_application &&
                                                                  !get_pkt(p, AK_PKT_IS_MTU_PROBE)));

                        state->current_packet_is_lost = false;
                        window_slot_nacked(cnx, state->framework_sender, slot);
                        helper_congestion_algorithm_notify(cnx, old_path,
                                                           (timer_based_retransmit == 0)
                                                           ? picoquic_congestion_notification_repeat
                                                           : picoquic_congestion_notification_timeout,
                                                           0, 0, lost_packet_number, current_time);
                        stop = true;
                        break;
                    }
                }
                /*
                * If the loop is continuing, this means that we need to look
                * at the next candidate packet.
                */
                p = p_next;
            }

            if (stop) {
                break;
            }
        }
    }
    return 0;
}

