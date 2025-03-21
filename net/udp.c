#include "udp.h"
#include "../mem/pmm.h"
#include "../drivers/vga.h"
#include <string.h>
#include <stdio.h>

// Maximum number of UDP sockets
#define MAX_UDP_SOCKETS 256

// Default UDP configuration values
#define UDP_DEFAULT_BUFFER_SIZE 8192
#define UDP_DEFAULT_TIMEOUT     0

// UDP subsystem state
static struct {
    udp_socket_t* sockets;
    uint32_t socket_count;
} udp_state;

// Initialize UDP subsystem
void udp_init(void) {
    memset(&udp_state, 0, sizeof(udp_state));
    vga_puts("UDP: Protocol initialized\n");
}

// Clean up UDP subsystem
void udp_cleanup(void) {
    // Close all sockets
    udp_socket_t* socket = udp_state.sockets;
    while (socket) {
        udp_socket_t* next = socket->next;
        udp_close_socket(socket);
        socket = next;
    }
}

// Create UDP socket
udp_socket_t* udp_create_socket(const ipv4_addr_t* local_addr, uint16_t local_port,
                               const udp_config_t* config) {
    if (!local_addr) {
        return NULL;
    }
    
    // Check socket limit
    if (udp_state.socket_count >= MAX_UDP_SOCKETS) {
        return NULL;
    }
    
    // Check if port is already in use
    udp_socket_t* socket = udp_state.sockets;
    while (socket) {
        if (socket->local_port == local_port &&
            ipv4_addr_equals(&socket->local_addr, local_addr)) {
            return NULL;
        }
        socket = socket->next;
    }
    
    // Allocate socket structure
    socket = pmm_alloc_blocks((sizeof(udp_socket_t) + PAGE_SIZE - 1) / PAGE_SIZE);
    if (!socket) {
        return NULL;
    }
    
    // Initialize socket
    memset(socket, 0, sizeof(udp_socket_t));
    socket->local_addr = *local_addr;
    socket->local_port = local_port;
    
    // Set configuration
    if (config) {
        socket->config = *config;
    } else {
        // Use default configuration
        socket->config.buffer_size = UDP_DEFAULT_BUFFER_SIZE;
        socket->config.checksum = true;
        socket->config.timeout = UDP_DEFAULT_TIMEOUT;
    }
    
    // Allocate receive buffer
    socket->recv_buf = pmm_alloc_blocks((socket->config.buffer_size + PAGE_SIZE - 1) / PAGE_SIZE);
    if (!socket->recv_buf) {
        pmm_free_blocks(socket, (sizeof(udp_socket_t) + PAGE_SIZE - 1) / PAGE_SIZE);
        return NULL;
    }
    
    // Add to socket list
    socket->next = udp_state.sockets;
    udp_state.sockets = socket;
    udp_state.socket_count++;
    
    return socket;
}

// Close UDP socket
void udp_close_socket(udp_socket_t* socket) {
    if (!socket) {
        return;
    }
    
    // Remove from socket list
    udp_socket_t** ptr = &udp_state.sockets;
    while (*ptr) {
        if (*ptr == socket) {
            *ptr = socket->next;
            udp_state.socket_count--;
            break;
        }
        ptr = &(*ptr)->next;
    }
    
    // Free receive buffer
    if (socket->recv_buf) {
        pmm_free_blocks(socket->recv_buf, (socket->config.buffer_size + PAGE_SIZE - 1) / PAGE_SIZE);
    }
    
    // Free socket structure
    pmm_free_blocks(socket, (sizeof(udp_socket_t) + PAGE_SIZE - 1) / PAGE_SIZE);
}

// Send UDP datagram
bool udp_send(udp_socket_t* socket, const ipv4_addr_t* dest_addr, uint16_t dest_port,
              const void* data, size_t length) {
    if (!socket || !dest_addr || !data || !length) {
        return false;
    }
    
    // Check maximum datagram size
    if (length > UINT16_MAX - sizeof(udp_header_t)) {
        return false;
    }
    
    // Allocate packet
    net_packet_t* packet = net_alloc_packet(sizeof(udp_header_t) + length);
    if (!packet) {
        return false;
    }
    
    // Build UDP header
    udp_header_t* header = (udp_header_t*)packet->data;
    header->src_port = socket->local_port;
    header->dest_port = dest_port;
    header->length = sizeof(udp_header_t) + length;
    header->checksum = 0;
    
    // Copy data
    memcpy(packet->data + sizeof(udp_header_t), data, length);
    
    // Calculate checksum if enabled
    if (socket->config.checksum) {
        header->checksum = udp_checksum(&socket->local_addr, dest_addr,
                                      header, header->length);
    }
    
    // Send packet
    packet->protocol = NET_PROTO_UDP;
    packet->length = header->length;
    bool success = ipv4_send_packet(packet, dest_addr, IPV4_PROTO_UDP, 0);
    
    if (success) {
        // Update statistics
        socket->stats.packets_sent++;
        socket->stats.bytes_sent += length;
    }
    
    return success;
}

// Receive UDP datagram
size_t udp_receive(udp_socket_t* socket, ipv4_addr_t* src_addr, uint16_t* src_port,
                  void* data, size_t max_length) {
    if (!socket || !data || !max_length) {
        return 0;
    }
    
    // Check if there's data in the receive buffer
    if (socket->recv_len == 0) {
        return 0;
    }
    
    // Calculate amount of data to copy
    size_t length = socket->recv_len;
    if (length > max_length) {
        length = max_length;
    }
    
    // Copy data from receive buffer
    memcpy(data, socket->recv_buf + socket->recv_start, length);
    
    // Update buffer state
    socket->recv_start += length;
    socket->recv_len -= length;
    
    // Reset buffer if empty
    if (socket->recv_len == 0) {
        socket->recv_start = 0;
    }
    
    return length;
}

// Process received UDP packet
void udp_receive_packet(net_interface_t* iface, net_packet_t* packet) {
    if (!iface || !packet || packet->length < sizeof(udp_header_t)) {
        return;
    }
    
    // Get UDP header
    udp_header_t* header = (udp_header_t*)packet->data;
    if (header->length > packet->length) {
        return;
    }
    
    // Find matching socket
    udp_socket_t* socket = udp_state.sockets;
    while (socket) {
        if (socket->local_port == header->dest_port &&
            ipv4_addr_equals(&socket->local_addr, &packet->dest_addr)) {
            break;
        }
        socket = socket->next;
    }
    
    if (socket) {
        // Verify checksum if enabled
        if (socket->config.checksum) {
            uint16_t orig_checksum = header->checksum;
            header->checksum = 0;
            if (udp_checksum(&packet->src_addr, &packet->dest_addr,
                           header, header->length) != orig_checksum) {
                socket->stats.checksum_errors++;
                return;
            }
            header->checksum = orig_checksum;
        }
        
        // Calculate data length
        size_t data_len = header->length - sizeof(udp_header_t);
        
        // Check if there's space in the receive buffer
        if (socket->recv_len + data_len > socket->config.buffer_size) {
            socket->stats.buffer_overflows++;
            return;
        }
        
        // Copy data to receive buffer
        memcpy(socket->recv_buf + socket->recv_start + socket->recv_len,
               packet->data + sizeof(udp_header_t), data_len);
        socket->recv_len += data_len;
        
        // Update statistics
        socket->stats.packets_received++;
        socket->stats.bytes_received += data_len;
    } else {
        // No matching socket
        // TODO: Send ICMP Port Unreachable
    }
}

// Get UDP socket statistics
void udp_get_stats(const udp_socket_t* socket, udp_stats_t* stats) {
    if (socket && stats) {
        *stats = socket->stats;
    }
}

// Reset UDP socket statistics
void udp_reset_stats(udp_socket_t* socket) {
    if (socket) {
        memset(&socket->stats, 0, sizeof(udp_stats_t));
    }
}

// Calculate UDP checksum
uint16_t udp_checksum(const ipv4_addr_t* src_addr, const ipv4_addr_t* dest_addr,
                     const void* udp_header, size_t length) {
    if (!src_addr || !dest_addr || !udp_header) {
        return 0;
    }
    
    // Build pseudo-header
    struct {
        uint32_t src_addr;
        uint32_t dest_addr;
        uint8_t  zero;
        uint8_t  protocol;
        uint16_t udp_length;
    } __attribute__((packed)) pseudo_header;
    
    memcpy(&pseudo_header.src_addr, src_addr->addr, 4);
    memcpy(&pseudo_header.dest_addr, dest_addr->addr, 4);
    pseudo_header.zero = 0;
    pseudo_header.protocol = IPV4_PROTO_UDP;
    pseudo_header.udp_length = length;
    
    // Calculate checksum over pseudo-header and UDP datagram
    uint32_t sum = 0;
    
    // Sum pseudo-header
    uint16_t* ptr = (uint16_t*)&pseudo_header;
    for (size_t i = 0; i < sizeof(pseudo_header) / 2; i++) {
        sum += ptr[i];
    }
    
    // Sum UDP datagram
    ptr = (uint16_t*)udp_header;
    size_t words = length / 2;
    for (size_t i = 0; i < words; i++) {
        sum += ptr[i];
    }
    
    // Add last byte if length is odd
    if (length & 1) {
        sum += ((uint8_t*)udp_header)[length - 1];
    }
    
    // Fold 32-bit sum into 16 bits
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~sum;
} 