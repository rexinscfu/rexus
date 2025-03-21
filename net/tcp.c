#include "tcp.h"
#include "../mem/pmm.h"
#include "../drivers/vga.h"
#include <string.h>
#include <stdio.h>

// Maximum number of TCP connections
#define MAX_TCP_CONNECTIONS 256

// Default TCP configuration values
#define TCP_DEFAULT_MSS          1460
#define TCP_DEFAULT_WINDOW       65535
#define TCP_DEFAULT_RETRANS_TIME 1000
#define TCP_DEFAULT_KEEPALIVE    7200000

// TCP subsystem state
static struct {
    tcp_conn_t* connections;
    uint32_t connection_count;
    uint32_t current_time;  // TODO: Get from system timer
} tcp_state;

// Initialize TCP subsystem
void tcp_init(void) {
    memset(&tcp_state, 0, sizeof(tcp_state));
    vga_puts("TCP: Protocol initialized\n");
}

// Clean up TCP subsystem
void tcp_cleanup(void) {
    // Close all connections
    tcp_conn_t* conn = tcp_state.connections;
    while (conn) {
        tcp_conn_t* next = conn->next;
        tcp_close_connection(conn);
        conn = next;
    }
}

// Create TCP connection
tcp_conn_t* tcp_create_connection(const ipv4_addr_t* local_addr, uint16_t local_port,
                                const ipv4_addr_t* remote_addr, uint16_t remote_port,
                                const tcp_config_t* config) {
    if (!local_addr || !remote_addr) {
        return NULL;
    }
    
    // Check connection limit
    if (tcp_state.connection_count >= MAX_TCP_CONNECTIONS) {
        return NULL;
    }
    
    // Allocate connection structure
    tcp_conn_t* conn = pmm_alloc_blocks((sizeof(tcp_conn_t) + PAGE_SIZE - 1) / PAGE_SIZE);
    if (!conn) {
        return NULL;
    }
    
    // Initialize connection
    memset(conn, 0, sizeof(tcp_conn_t));
    conn->local_addr = *local_addr;
    conn->remote_addr = *remote_addr;
    conn->local_port = local_port;
    conn->remote_port = remote_port;
    conn->state = TCP_STATE_CLOSED;
    
    // Set configuration
    if (config) {
        conn->config = *config;
    } else {
        // Use default configuration
        conn->config.mss = TCP_DEFAULT_MSS;
        conn->config.window_scale = 0;
        conn->config.sack_permitted = false;
        conn->config.timestamps = false;
        conn->config.initial_seq = 0;  // TODO: Generate random ISN
        conn->config.window_size = TCP_DEFAULT_WINDOW;
        conn->config.retransmit_time = TCP_DEFAULT_RETRANS_TIME;
        conn->config.keepalive_time = TCP_DEFAULT_KEEPALIVE;
    }
    
    // Allocate buffers
    conn->send_buf = pmm_alloc_blocks((conn->config.window_size + PAGE_SIZE - 1) / PAGE_SIZE);
    conn->recv_buf = pmm_alloc_blocks((conn->config.window_size + PAGE_SIZE - 1) / PAGE_SIZE);
    if (!conn->send_buf || !conn->recv_buf) {
        if (conn->send_buf) pmm_free_blocks(conn->send_buf, (conn->config.window_size + PAGE_SIZE - 1) / PAGE_SIZE);
        if (conn->recv_buf) pmm_free_blocks(conn->recv_buf, (conn->config.window_size + PAGE_SIZE - 1) / PAGE_SIZE);
        pmm_free_blocks(conn, (sizeof(tcp_conn_t) + PAGE_SIZE - 1) / PAGE_SIZE);
        return NULL;
    }
    
    // Initialize sequence numbers
    conn->snd_una = conn->config.initial_seq;
    conn->snd_nxt = conn->config.initial_seq;
    conn->snd_wnd = conn->config.window_size;
    conn->rcv_wnd = conn->config.window_size;
    
    // Initialize timers
    conn->rto = conn->config.retransmit_time;
    conn->keepalive = tcp_state.current_time + conn->config.keepalive_time;
    
    // Add to connection list
    conn->next = tcp_state.connections;
    tcp_state.connections = conn;
    tcp_state.connection_count++;
    
    return conn;
}

// Close TCP connection
void tcp_close_connection(tcp_conn_t* conn) {
    if (!conn) {
        return;
    }
    
    // Remove from connection list
    tcp_conn_t** ptr = &tcp_state.connections;
    while (*ptr) {
        if (*ptr == conn) {
            *ptr = conn->next;
            tcp_state.connection_count--;
            break;
        }
        ptr = &(*ptr)->next;
    }
    
    // Free buffers
    if (conn->send_buf) {
        pmm_free_blocks(conn->send_buf, (conn->config.window_size + PAGE_SIZE - 1) / PAGE_SIZE);
    }
    if (conn->recv_buf) {
        pmm_free_blocks(conn->recv_buf, (conn->config.window_size + PAGE_SIZE - 1) / PAGE_SIZE);
    }
    
    // Free connection structure
    pmm_free_blocks(conn, (sizeof(tcp_conn_t) + PAGE_SIZE - 1) / PAGE_SIZE);
}

// Send data over TCP connection
bool tcp_send(tcp_conn_t* conn, const void* data, size_t length) {
    if (!conn || !data || !length) {
        return false;
    }
    
    // Check connection state
    if (conn->state != TCP_STATE_ESTABLISHED) {
        return false;
    }
    
    // Check if there's space in the send buffer
    if (conn->send_len + length > conn->config.window_size) {
        return false;
    }
    
    // Copy data to send buffer
    memcpy(conn->send_buf + conn->send_len, data, length);
    conn->send_len += length;
    
    // Update statistics
    conn->stats.bytes_sent += length;
    
    // TODO: Schedule transmission
    
    return true;
}

// Receive data from TCP connection
size_t tcp_receive(tcp_conn_t* conn, void* data, size_t max_length) {
    if (!conn || !data || !max_length) {
        return 0;
    }
    
    // Check connection state
    if (conn->state != TCP_STATE_ESTABLISHED) {
        return 0;
    }
    
    // Calculate amount of data to copy
    size_t length = (max_length < conn->recv_len) ? max_length : conn->recv_len;
    if (!length) {
        return 0;
    }
    
    // Copy data from receive buffer
    memcpy(data, conn->recv_buf, length);
    
    // Remove copied data from buffer
    memmove(conn->recv_buf, conn->recv_buf + length, conn->recv_len - length);
    conn->recv_len -= length;
    
    // Update receive window
    conn->rcv_wnd += length;
    
    return length;
}

// Process received TCP packet
void tcp_receive_packet(net_interface_t* iface, net_packet_t* packet) {
    if (!iface || !packet || packet->length < sizeof(tcp_header_t)) {
        return;
    }
    
    // Get TCP header
    tcp_header_t* header = (tcp_header_t*)packet->data;
    size_t header_len = (header->data_offset >> 4) * 4;
    if (header_len < sizeof(tcp_header_t) || header_len > packet->length) {
        return;
    }
    
    // Find matching connection
    tcp_conn_t* conn = tcp_state.connections;
    while (conn) {
        if (conn->local_port == header->dest_port &&
            conn->remote_port == header->src_port &&
            ipv4_addr_equals(&conn->local_addr, &packet->dest_addr) &&
            ipv4_addr_equals(&conn->remote_addr, &packet->src_addr)) {
            break;
        }
        conn = conn->next;
    }
    
    // Handle packet based on connection state
    if (conn) {
        switch (conn->state) {
            case TCP_STATE_LISTEN:
                if (header->flags & TCP_FLAG_SYN) {
                    // TODO: Handle incoming connection
                }
                break;
                
            case TCP_STATE_SYN_SENT:
                if ((header->flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) ==
                    (TCP_FLAG_SYN | TCP_FLAG_ACK)) {
                    // TODO: Handle connection establishment
                }
                break;
                
            case TCP_STATE_ESTABLISHED:
                // Update receive window
                conn->snd_wnd = header->window;
                
                // Process acknowledgment
                if (header->flags & TCP_FLAG_ACK) {
                    uint32_t acked = header->ack_num - conn->snd_una;
                    if (acked > 0 && acked <= conn->send_len) {
                        // Remove acknowledged data from send buffer
                        memmove(conn->send_buf, conn->send_buf + acked,
                               conn->send_len - acked);
                        conn->send_len -= acked;
                        conn->snd_una = header->ack_num;
                        
                        // Update RTT estimates
                        // TODO: Implement RTT estimation
                    }
                }
                
                // Process received data
                size_t data_len = packet->length - header_len;
                if (data_len > 0) {
                    // Check sequence number
                    uint32_t seq = header->seq_num;
                    if (seq == conn->rcv_nxt &&
                        conn->recv_len + data_len <= conn->config.window_size) {
                        // Copy data to receive buffer
                        memcpy(conn->recv_buf + conn->recv_len,
                               packet->data + header_len, data_len);
                        conn->recv_len += data_len;
                        conn->rcv_nxt += data_len;
                        conn->rcv_wnd -= data_len;
                        
                        // Update statistics
                        conn->stats.bytes_received += data_len;
                    } else {
                        // Out of order segment
                        conn->stats.out_of_order++;
                    }
                }
                
                // Handle connection termination
                if (header->flags & TCP_FLAG_FIN) {
                    // TODO: Handle connection termination
                }
                break;
                
            default:
                // TODO: Handle other states
                break;
        }
        
        // Update statistics
        conn->stats.packets_received++;
    } else {
        // No matching connection
        if (!(header->flags & TCP_FLAG_RST)) {
            // TODO: Send RST
        }
    }
}

// Get TCP connection statistics
void tcp_get_stats(const tcp_conn_t* conn, tcp_stats_t* stats) {
    if (conn && stats) {
        *stats = conn->stats;
    }
}

// Reset TCP connection statistics
void tcp_reset_stats(tcp_conn_t* conn) {
    if (conn) {
        memset(&conn->stats, 0, sizeof(tcp_stats_t));
    }
}

// Calculate TCP checksum
uint16_t tcp_checksum(const ipv4_addr_t* src_addr, const ipv4_addr_t* dest_addr,
                     const void* tcp_header, size_t length) {
    if (!src_addr || !dest_addr || !tcp_header) {
        return 0;
    }
    
    // Build pseudo-header
    struct {
        uint32_t src_addr;
        uint32_t dest_addr;
        uint8_t  zero;
        uint8_t  protocol;
        uint16_t tcp_length;
    } __attribute__((packed)) pseudo_header;
    
    memcpy(&pseudo_header.src_addr, src_addr->addr, 4);
    memcpy(&pseudo_header.dest_addr, dest_addr->addr, 4);
    pseudo_header.zero = 0;
    pseudo_header.protocol = IPV4_PROTO_TCP;
    pseudo_header.tcp_length = length;
    
    // Calculate checksum over pseudo-header and TCP segment
    uint32_t sum = 0;
    
    // Sum pseudo-header
    uint16_t* ptr = (uint16_t*)&pseudo_header;
    for (size_t i = 0; i < sizeof(pseudo_header) / 2; i++) {
        sum += ptr[i];
    }
    
    // Sum TCP segment
    ptr = (uint16_t*)tcp_header;
    size_t words = length / 2;
    for (size_t i = 0; i < words; i++) {
        sum += ptr[i];
    }
    
    // Add last byte if length is odd
    if (length & 1) {
        sum += ((uint8_t*)tcp_header)[length - 1];
    }
    
    // Fold 32-bit sum into 16 bits
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~sum;
}

// Convert TCP state to string
const char* tcp_state_to_string(tcp_state_t state) {
    switch (state) {
        case TCP_STATE_CLOSED:      return "CLOSED";
        case TCP_STATE_LISTEN:      return "LISTEN";
        case TCP_STATE_SYN_SENT:    return "SYN_SENT";
        case TCP_STATE_SYN_RECEIVED: return "SYN_RECEIVED";
        case TCP_STATE_ESTABLISHED: return "ESTABLISHED";
        case TCP_STATE_FIN_WAIT_1:  return "FIN_WAIT_1";
        case TCP_STATE_FIN_WAIT_2:  return "FIN_WAIT_2";
        case TCP_STATE_CLOSE_WAIT:  return "CLOSE_WAIT";
        case TCP_STATE_CLOSING:     return "CLOSING";
        case TCP_STATE_LAST_ACK:    return "LAST_ACK";
        case TCP_STATE_TIME_WAIT:   return "TIME_WAIT";
        default:                    return "UNKNOWN";
    }
}

// Parse TCP options
bool tcp_parse_options(const tcp_header_t* header, tcp_config_t* config) {
    if (!header || !config) {
        return false;
    }
    
    size_t header_len = (header->data_offset >> 4) * 4;
    size_t options_len = header_len - sizeof(tcp_header_t);
    const uint8_t* options = header->options;
    
    while (options_len > 0) {
        // Get option kind
        uint8_t kind = options[0];
        
        // Handle single-byte options
        if (kind == TCP_OPT_END) {
            break;
        }
        if (kind == TCP_OPT_NOP) {
            options++;
            options_len--;
            continue;
        }
        
        // Check for option length
        if (options_len < 2) {
            return false;
        }
        uint8_t length = options[1];
        if (length < 2 || length > options_len) {
            return false;
        }
        
        // Parse option based on kind
        switch (kind) {
            case TCP_OPT_MSS:
                if (length == sizeof(tcp_opt_mss_t)) {
                    const tcp_opt_mss_t* opt = (const tcp_opt_mss_t*)options;
                    config->mss = opt->mss;
                }
                break;
                
            case TCP_OPT_WSCALE:
                if (length == sizeof(tcp_opt_wscale_t)) {
                    const tcp_opt_wscale_t* opt = (const tcp_opt_wscale_t*)options;
                    config->window_scale = opt->shift_count;
                }
                break;
                
            case TCP_OPT_SACK_PERM:
                if (length == 2) {
                    config->sack_permitted = true;
                }
                break;
                
            case TCP_OPT_TIMESTAMP:
                if (length == sizeof(tcp_opt_timestamp_t)) {
                    config->timestamps = true;
                }
                break;
        }
        
        options += length;
        options_len -= length;
    }
    
    return true;
}

// Build TCP options
void tcp_build_options(tcp_header_t* header, const tcp_config_t* config) {
    if (!header || !config) {
        return;
    }
    
    uint8_t* options = header->options;
    size_t offset = 0;
    
    // Add MSS option
    tcp_opt_mss_t* mss = (tcp_opt_mss_t*)(options + offset);
    mss->kind = TCP_OPT_MSS;
    mss->length = sizeof(tcp_opt_mss_t);
    mss->mss = config->mss;
    offset += sizeof(tcp_opt_mss_t);
    
    // Add window scale option
    if (config->window_scale > 0) {
        tcp_opt_wscale_t* wscale = (tcp_opt_wscale_t*)(options + offset);
        wscale->kind = TCP_OPT_WSCALE;
        wscale->length = sizeof(tcp_opt_wscale_t);
        wscale->shift_count = config->window_scale;
        offset += sizeof(tcp_opt_wscale_t);
    }
    
    // Add SACK permitted option
    if (config->sack_permitted) {
        options[offset++] = TCP_OPT_SACK_PERM;
        options[offset++] = 2;
    }
    
    // Add timestamp option
    if (config->timestamps) {
        tcp_opt_timestamp_t* ts = (tcp_opt_timestamp_t*)(options + offset);
        ts->kind = TCP_OPT_TIMESTAMP;
        ts->length = sizeof(tcp_opt_timestamp_t);
        ts->timestamp = 0;  // TODO: Get current timestamp
        ts->echo_reply = 0;
        offset += sizeof(tcp_opt_timestamp_t);
    }
    
    // Add padding if necessary
    while (offset & 3) {
        options[offset++] = TCP_OPT_NOP;
    }
    
    // Update data offset
    header->data_offset = ((sizeof(tcp_header_t) + offset) / 4) << 4;
} 