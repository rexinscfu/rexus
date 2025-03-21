#ifndef REXUS_IPV4_H
#define REXUS_IPV4_H

#include "net.h"
#include <stdint.h>
#include <stdbool.h>

// IPv4 header constants
#define IPV4_VERSION 4
#define IPV4_IHL_MIN 5
#define IPV4_TTL_DEFAULT 64
#define IPV4_HEADER_MIN_LEN 20
#define IPV4_HEADER_MAX_LEN 60
#define IPV4_MAX_PACKET_SIZE 65535

// IPv4 flags
#define IPV4_FLAG_RESERVED 0x8000
#define IPV4_FLAG_DONT_FRAGMENT 0x4000
#define IPV4_FLAG_MORE_FRAGMENTS 0x2000
#define IPV4_FRAGMENT_OFFSET_MASK 0x1FFF

// IPv4 protocols
#define IPV4_PROTO_ICMP 1
#define IPV4_PROTO_TCP 6
#define IPV4_PROTO_UDP 17

// IPv4 address structure
typedef struct {
    uint8_t addr[4];
} ipv4_addr_t;

// IPv4 header structure
typedef struct {
    uint8_t version_ihl;      // Version (4 bits) + IHL (4 bits)
    uint8_t tos;              // Type of Service
    uint16_t total_length;    // Total Length
    uint16_t id;              // Identification
    uint16_t flags_offset;    // Flags (3 bits) + Fragment Offset (13 bits)
    uint8_t ttl;              // Time to Live
    uint8_t protocol;         // Protocol
    uint16_t checksum;        // Header Checksum
    ipv4_addr_t src_addr;     // Source Address
    ipv4_addr_t dest_addr;    // Destination Address
    uint8_t options[];        // Options (if IHL > 5)
} __attribute__((packed)) ipv4_header_t;

// IPv4 pseudo header for checksum calculation
typedef struct {
    ipv4_addr_t src_addr;     // Source Address
    ipv4_addr_t dest_addr;    // Destination Address
    uint8_t zero;             // Always 0
    uint8_t protocol;         // Protocol
    uint16_t length;          // TCP/UDP Length
} __attribute__((packed)) ipv4_pseudo_header_t;

// IPv4 route entry
typedef struct ipv4_route {
    ipv4_addr_t network;      // Network address
    ipv4_addr_t netmask;      // Network mask
    ipv4_addr_t gateway;      // Gateway address
    net_interface_t* iface;   // Network interface
    uint32_t metric;          // Route metric
    struct ipv4_route* next;  // Next route in list
} ipv4_route_t;

// IPv4 interface configuration
typedef struct {
    ipv4_addr_t addr;         // Interface IPv4 address
    ipv4_addr_t netmask;      // Interface network mask
    ipv4_addr_t broadcast;    // Interface broadcast address
    ipv4_addr_t gateway;      // Default gateway
    bool dhcp_enabled;        // DHCP enabled flag
    uint32_t dhcp_lease_time; // DHCP lease time
} ipv4_config_t;

// IPv4 statistics
typedef struct {
    uint64_t packets_received;
    uint64_t packets_sent;
    uint64_t bytes_received;
    uint64_t bytes_sent;
    uint64_t packets_forwarded;
    uint64_t packets_dropped;
    uint64_t fragments_received;
    uint64_t fragments_reassembled;
    uint64_t reassembly_failures;
    uint64_t fragments_sent;
    uint64_t fragmentation_failures;
} ipv4_stats_t;

// Function declarations
void ipv4_init(void);
void ipv4_cleanup(void);

// Packet handling
bool ipv4_send_packet(net_packet_t* packet, const ipv4_addr_t* dest_addr, 
                     uint8_t protocol, uint8_t ttl);
void ipv4_receive_packet(net_interface_t* iface, net_packet_t* packet);
bool ipv4_forward_packet(net_packet_t* packet);

// Route management
bool ipv4_add_route(const ipv4_addr_t* network, const ipv4_addr_t* netmask,
                   const ipv4_addr_t* gateway, net_interface_t* iface, uint32_t metric);
bool ipv4_remove_route(const ipv4_addr_t* network, const ipv4_addr_t* netmask);
ipv4_route_t* ipv4_find_route(const ipv4_addr_t* dest_addr);
void ipv4_flush_routes(void);

// Interface configuration
bool ipv4_configure_interface(net_interface_t* iface, const ipv4_config_t* config);
bool ipv4_get_interface_config(net_interface_t* iface, ipv4_config_t* config);
bool ipv4_enable_dhcp(net_interface_t* iface);
bool ipv4_disable_dhcp(net_interface_t* iface);

// Address manipulation
bool ipv4_addr_is_broadcast(const ipv4_addr_t* addr, const ipv4_addr_t* netmask);
bool ipv4_addr_is_multicast(const ipv4_addr_t* addr);
bool ipv4_addr_is_local(const ipv4_addr_t* addr);
bool ipv4_addr_equals(const ipv4_addr_t* addr1, const ipv4_addr_t* addr2);
void ipv4_addr_to_string(const ipv4_addr_t* addr, char* str);
bool ipv4_string_to_addr(const char* str, ipv4_addr_t* addr);

// Checksum calculation
uint16_t ipv4_checksum(const void* data, size_t length);
uint16_t ipv4_pseudo_checksum(const ipv4_pseudo_header_t* pseudo_header, 
                             const void* data, size_t length);

// Fragment handling
bool ipv4_fragment_packet(net_packet_t* packet, uint16_t mtu);
net_packet_t* ipv4_reassemble_packet(net_packet_t* fragment);

// Statistics
void ipv4_get_stats(ipv4_stats_t* stats);
void ipv4_reset_stats(void);

#endif /* REXUS_IPV4_H */ 