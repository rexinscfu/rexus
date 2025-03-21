#ifndef REXUS_UDP_H
#define REXUS_UDP_H

#include "net.h"
#include "ipv4.h"
#include <stdint.h>
#include <stdbool.h>

// UDP header
typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) udp_header_t;

// UDP socket configuration
typedef struct {
    uint16_t buffer_size;    // Size of receive buffer
    bool     checksum;       // Enable/disable checksums
    uint32_t timeout;        // Receive timeout in milliseconds
} udp_config_t;

// UDP socket statistics
typedef struct {
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t checksum_errors;
    uint64_t buffer_overflows;
    uint64_t no_port_errors;
} udp_stats_t;

// UDP socket
typedef struct udp_socket {
    // Socket identification
    ipv4_addr_t local_addr;
    uint16_t local_port;
    
    // Socket configuration
    udp_config_t config;
    udp_stats_t stats;
    
    // Receive buffer
    uint8_t* recv_buf;
    uint32_t recv_len;
    uint32_t recv_start;
    
    // Linked list
    struct udp_socket* next;
} udp_socket_t;

// Initialize UDP subsystem
void udp_init(void);

// Clean up UDP subsystem
void udp_cleanup(void);

// Create UDP socket
udp_socket_t* udp_create_socket(const ipv4_addr_t* local_addr, uint16_t local_port,
                               const udp_config_t* config);

// Close UDP socket
void udp_close_socket(udp_socket_t* socket);

// Send UDP datagram
bool udp_send(udp_socket_t* socket, const ipv4_addr_t* dest_addr, uint16_t dest_port,
              const void* data, size_t length);

// Receive UDP datagram
size_t udp_receive(udp_socket_t* socket, ipv4_addr_t* src_addr, uint16_t* src_port,
                  void* data, size_t max_length);

// Process received UDP packet
void udp_receive_packet(net_interface_t* iface, net_packet_t* packet);

// Get UDP socket statistics
void udp_get_stats(const udp_socket_t* socket, udp_stats_t* stats);

// Reset UDP socket statistics
void udp_reset_stats(udp_socket_t* socket);

// Helper functions
uint16_t udp_checksum(const ipv4_addr_t* src_addr, const ipv4_addr_t* dest_addr,
                     const void* udp_header, size_t length);

#endif // REXUS_UDP_H 