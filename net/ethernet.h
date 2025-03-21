#ifndef REXUS_ETHERNET_H
#define REXUS_ETHERNET_H

#include "net.h"
#include <stdint.h>
#include <stdbool.h>

// Ethernet frame constants
#define ETH_HEADER_SIZE     14
#define ETH_FCS_SIZE        4
#define ETH_MIN_DATA_SIZE   46
#define ETH_MAX_DATA_SIZE   1500
#define ETH_MIN_FRAME_SIZE  (ETH_HEADER_SIZE + ETH_MIN_DATA_SIZE + ETH_FCS_SIZE)
#define ETH_MAX_FRAME_SIZE  (ETH_HEADER_SIZE + ETH_MAX_DATA_SIZE + ETH_FCS_SIZE)

// Ethernet address (MAC) size
#define ETH_ADDR_LEN 6

// Ethernet frame types
#define ETH_TYPE_IPV4 0x0800
#define ETH_TYPE_ARP  0x0806
#define ETH_TYPE_IPV6 0x86DD
#define ETH_TYPE_VLAN 0x8100

// Ethernet frame header
typedef struct {
    uint8_t dest[ETH_ADDR_LEN];
    uint8_t src[ETH_ADDR_LEN];
    uint16_t type;
} __attribute__((packed)) eth_header_t;

// Ethernet frame structure
typedef struct {
    eth_header_t header;
    uint8_t data[ETH_MAX_DATA_SIZE];
    uint32_t fcs;
} __attribute__((packed)) eth_frame_t;

// Ethernet device capabilities
typedef struct {
    bool full_duplex;
    bool auto_negotiate;
    uint32_t speed;        // in Mbps
    bool rx_checksum;      // hardware checksum verification
    bool tx_checksum;      // hardware checksum generation
    bool scatter_gather;   // scatter-gather DMA support
    bool tso;             // TCP segmentation offload
    bool ufo;             // UDP fragmentation offload
    bool rx_vlan;         // VLAN tag extraction
    bool tx_vlan;         // VLAN tag insertion
} eth_capabilities_t;

// Ethernet device configuration
typedef struct {
    bool promiscuous;
    bool all_multicast;
    bool broadcast;
    uint16_t rx_buffer_size;
    uint16_t tx_buffer_size;
    uint16_t rx_descriptors;
    uint16_t tx_descriptors;
    uint32_t rx_interrupt_threshold;
    uint32_t tx_interrupt_threshold;
} eth_config_t;

// Ethernet device operations
typedef struct {
    bool (*init)(net_interface_t* iface);
    void (*cleanup)(net_interface_t* iface);
    bool (*start)(net_interface_t* iface);
    void (*stop)(net_interface_t* iface);
    bool (*send)(net_interface_t* iface, net_packet_t* packet);
    net_packet_t* (*receive)(net_interface_t* iface);
    bool (*set_mac)(net_interface_t* iface, const uint8_t* mac);
    bool (*get_stats)(net_interface_t* iface, net_stats_t* stats);
    bool (*set_promiscuous)(net_interface_t* iface, bool enable);
    bool (*set_multicast)(net_interface_t* iface, bool enable);
} eth_ops_t;

// Ethernet device structure
typedef struct {
    eth_capabilities_t caps;
    eth_config_t config;
    eth_ops_t ops;
    void* priv;           // Private driver data
} eth_device_t;

// Function declarations
bool eth_init_device(net_interface_t* iface, eth_device_t* dev);
void eth_cleanup_device(net_interface_t* iface);
bool eth_start_device(net_interface_t* iface);
void eth_stop_device(net_interface_t* iface);
bool eth_send_frame(net_interface_t* iface, const void* data, size_t length);
bool eth_receive_frame(net_interface_t* iface, void* data, size_t* length);
bool eth_set_mac_address(net_interface_t* iface, const uint8_t* mac);
bool eth_get_mac_address(net_interface_t* iface, uint8_t* mac);
bool eth_set_promiscuous(net_interface_t* iface, bool enable);
bool eth_set_multicast(net_interface_t* iface, bool enable);

// Helper functions
uint32_t eth_calculate_crc32(const void* data, size_t length);
bool eth_is_valid_mac(const uint8_t* mac);
void eth_format_mac(char* str, const uint8_t* mac);
bool eth_parse_mac(const char* str, uint8_t* mac);

#endif /* REXUS_ETHERNET_H */ 