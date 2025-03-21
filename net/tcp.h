#ifndef REXUS_TCP_H
#define REXUS_TCP_H

#include "net.h"
#include "ipv4.h"
#include <stdint.h>
#include <stdbool.h>

// TCP header flags
#define TCP_FLAG_FIN  0x01
#define TCP_FLAG_SYN  0x02
#define TCP_FLAG_RST  0x04
#define TCP_FLAG_PSH  0x08
#define TCP_FLAG_ACK  0x10
#define TCP_FLAG_URG  0x20
#define TCP_FLAG_ECE  0x40
#define TCP_FLAG_CWR  0x80

// TCP options
#define TCP_OPT_END          0
#define TCP_OPT_NOP         1
#define TCP_OPT_MSS         2
#define TCP_OPT_WSCALE      3
#define TCP_OPT_SACK_PERM   4
#define TCP_OPT_SACK        5
#define TCP_OPT_TIMESTAMP   8

// TCP states
typedef enum {
    TCP_STATE_CLOSED,
    TCP_STATE_LISTEN,
    TCP_STATE_SYN_SENT,
    TCP_STATE_SYN_RECEIVED,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_FIN_WAIT_1,
    TCP_STATE_FIN_WAIT_2,
    TCP_STATE_CLOSE_WAIT,
    TCP_STATE_CLOSING,
    TCP_STATE_LAST_ACK,
    TCP_STATE_TIME_WAIT
} tcp_state_t;

// TCP header
typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset;  // Upper 4 bits
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;
    uint8_t  options[];    // Variable length
} __attribute__((packed)) tcp_header_t;

// TCP option
typedef struct {
    uint8_t kind;
    uint8_t length;
    uint8_t data[];
} __attribute__((packed)) tcp_option_t;

// TCP Maximum Segment Size option
typedef struct {
    uint8_t kind;
    uint8_t length;
    uint16_t mss;
} __attribute__((packed)) tcp_opt_mss_t;

// TCP Window Scale option
typedef struct {
    uint8_t kind;
    uint8_t length;
    uint8_t shift_count;
} __attribute__((packed)) tcp_opt_wscale_t;

// TCP Timestamp option
typedef struct {
    uint8_t kind;
    uint8_t length;
    uint32_t timestamp;
    uint32_t echo_reply;
} __attribute__((packed)) tcp_opt_timestamp_t;

// TCP connection configuration
typedef struct {
    uint16_t mss;              // Maximum Segment Size
    uint8_t  window_scale;     // Window Scale factor
    bool     sack_permitted;   // Selective ACK permitted
    bool     timestamps;       // Timestamp support
    uint32_t initial_seq;      // Initial sequence number
    uint16_t window_size;      // Initial window size
    uint32_t retransmit_time; // Retransmission timeout (ms)
    uint32_t keepalive_time;  // Keepalive timeout (ms)
} tcp_config_t;

// TCP connection statistics
typedef struct {
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t retransmissions;
    uint64_t duplicate_acks;
    uint64_t out_of_order;
    uint64_t window_probes;
    uint64_t keepalives_sent;
    uint64_t keepalives_received;
    uint64_t resets_sent;
    uint64_t resets_received;
    uint64_t segments_dropped;
} tcp_stats_t;

// TCP connection
typedef struct tcp_conn {
    // Connection identification
    ipv4_addr_t local_addr;
    ipv4_addr_t remote_addr;
    uint16_t local_port;
    uint16_t remote_port;
    
    // Connection state
    tcp_state_t state;
    tcp_config_t config;
    tcp_stats_t stats;
    
    // Sequence numbers
    uint32_t snd_una;    // Oldest unacknowledged sequence number
    uint32_t snd_nxt;    // Next sequence number to send
    uint32_t snd_wnd;    // Send window
    uint32_t rcv_nxt;    // Next sequence number expected
    uint32_t rcv_wnd;    // Receive window
    
    // Timers and timeouts
    uint32_t rto;        // Retransmission timeout
    uint32_t srtt;       // Smoothed round-trip time
    uint32_t rttvar;     // Round-trip time variation
    uint32_t last_ack;   // Time of last acknowledgment
    uint32_t keepalive;  // Keepalive timer
    
    // Buffers
    uint8_t* send_buf;   // Send buffer
    uint32_t send_len;   // Send buffer length
    uint8_t* recv_buf;   // Receive buffer
    uint32_t recv_len;   // Receive buffer length
    
    // Linked list
    struct tcp_conn* next;
} tcp_conn_t;

// Initialize TCP subsystem
void tcp_init(void);

// Clean up TCP subsystem
void tcp_cleanup(void);

// Create TCP connection
tcp_conn_t* tcp_create_connection(const ipv4_addr_t* local_addr, uint16_t local_port,
                                const ipv4_addr_t* remote_addr, uint16_t remote_port,
                                const tcp_config_t* config);

// Close TCP connection
void tcp_close_connection(tcp_conn_t* conn);

// Send data over TCP connection
bool tcp_send(tcp_conn_t* conn, const void* data, size_t length);

// Receive data from TCP connection
size_t tcp_receive(tcp_conn_t* conn, void* data, size_t max_length);

// Process received TCP packet
void tcp_receive_packet(net_interface_t* iface, net_packet_t* packet);

// Get TCP connection statistics
void tcp_get_stats(const tcp_conn_t* conn, tcp_stats_t* stats);

// Reset TCP connection statistics
void tcp_reset_stats(tcp_conn_t* conn);

// Helper functions
uint16_t tcp_checksum(const ipv4_addr_t* src_addr, const ipv4_addr_t* dest_addr,
                     const void* tcp_header, size_t length);
const char* tcp_state_to_string(tcp_state_t state);
bool tcp_parse_options(const tcp_header_t* header, tcp_config_t* config);
void tcp_build_options(tcp_header_t* header, const tcp_config_t* config);

#endif // REXUS_TCP_H 