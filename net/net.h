#ifndef REXUS_NET_H
#define REXUS_NET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Maximum packet size
#define NET_MAX_PACKET_SIZE 1518
#define NET_MIN_PACKET_SIZE 64

// Maximum number of network interfaces
#define NET_MAX_INTERFACES 4

// Protocol types
typedef enum {
    NET_PROTO_NONE = 0,
    NET_PROTO_IPV4,
    NET_PROTO_IPV6,
    NET_PROTO_ARP,
    NET_PROTO_ICMP,
    NET_PROTO_TCP,
    NET_PROTO_UDP,
    NET_PROTO_CAN,
    NET_PROTO_LIN
} net_protocol_t;

// Interface types
typedef enum {
    NET_IF_TYPE_NONE = 0,
    NET_IF_TYPE_ETHERNET,
    NET_IF_TYPE_WIFI,
    NET_IF_TYPE_CAN,
    NET_IF_TYPE_LIN,
    NET_IF_TYPE_LOOPBACK
} net_if_type_t;

// Interface flags
#define NET_IF_FLAG_UP        0x01
#define NET_IF_FLAG_RUNNING   0x02
#define NET_IF_FLAG_PROMISC   0x04
#define NET_IF_FLAG_MULTICAST 0x08
#define NET_IF_FLAG_BROADCAST 0x10

// Network packet structure
typedef struct {
    uint8_t* data;
    size_t length;
    net_protocol_t protocol;
    uint8_t priority;
    void* private_data;
} net_packet_t;

// Network interface statistics
typedef struct {
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_errors;
    uint64_t tx_errors;
    uint64_t rx_dropped;
    uint64_t tx_dropped;
    uint64_t collisions;
} net_stats_t;

// Network interface structure
typedef struct net_interface {
    char name[16];
    net_if_type_t type;
    uint32_t flags;
    uint8_t mac[6];
    uint32_t mtu;
    net_stats_t stats;
    
    // Interface operations
    bool (*init)(struct net_interface* iface);
    void (*cleanup)(struct net_interface* iface);
    bool (*start)(struct net_interface* iface);
    void (*stop)(struct net_interface* iface);
    bool (*send)(struct net_interface* iface, net_packet_t* packet);
    net_packet_t* (*receive)(struct net_interface* iface);
    bool (*set_mac)(struct net_interface* iface, const uint8_t* mac);
    bool (*set_flags)(struct net_interface* iface, uint32_t flags);
    bool (*clear_flags)(struct net_interface* iface, uint32_t flags);
    
    // Private driver data
    void* driver_data;
    
    // Link to next interface
    struct net_interface* next;
} net_interface_t;

// Network subsystem functions
void net_init(void);
void net_cleanup(void);
bool net_register_interface(net_interface_t* iface);
void net_unregister_interface(net_interface_t* iface);
net_interface_t* net_get_interface(const char* name);
net_interface_t* net_get_interface_by_index(uint32_t index);
uint32_t net_get_interface_count(void);

// Packet management
net_packet_t* net_alloc_packet(size_t size);
void net_free_packet(net_packet_t* packet);
bool net_send_packet(net_interface_t* iface, net_packet_t* packet);
net_packet_t* net_receive_packet(net_interface_t* iface);

// Protocol handlers
typedef void (*net_protocol_handler_t)(net_interface_t* iface, net_packet_t* packet);
bool net_register_protocol_handler(net_protocol_t proto, net_protocol_handler_t handler);
void net_unregister_protocol_handler(net_protocol_t proto);

// Network stack processing
void net_process_rx_queue(void);
void net_process_tx_queue(void);
void net_update(void);

#endif /* REXUS_NET_H */ 