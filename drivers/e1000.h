#ifndef REXUS_E1000_H
#define REXUS_E1000_H

#include "../net/ethernet.h"
#include <stdint.h>
#include <stdbool.h>

// PCI Device IDs for Intel e1000
#define E1000_VENDOR_ID       0x8086
#define E1000_DEVICE_ID_82540 0x100E
#define E1000_DEVICE_ID_82541 0x1013
#define E1000_DEVICE_ID_82545 0x100F
#define E1000_DEVICE_ID_82546 0x1010
#define E1000_DEVICE_ID_82547 0x1019

// e1000 Register Definitions
#define E1000_CTRL     0x0000  // Device Control
#define E1000_STATUS   0x0008  // Device Status
#define E1000_EECD     0x0010  // EEPROM/Flash Control/Data
#define E1000_EERD     0x0014  // EEPROM Read
#define E1000_ICR      0x00C0  // Interrupt Cause Read
#define E1000_IMS      0x00D0  // Interrupt Mask Set
#define E1000_IMC      0x00D8  // Interrupt Mask Clear
#define E1000_RCTL     0x0100  // Receive Control
#define E1000_TCTL     0x0400  // Transmit Control
#define E1000_RDBAL    0x2800  // Rx Descriptor Base Low
#define E1000_RDBAH    0x2804  // Rx Descriptor Base High
#define E1000_RDLEN    0x2808  // Rx Descriptor Length
#define E1000_RDH      0x2810  // Rx Descriptor Head
#define E1000_RDT      0x2818  // Rx Descriptor Tail
#define E1000_TDBAL    0x3800  // Tx Descriptor Base Low
#define E1000_TDBAH    0x3804  // Tx Descriptor Base High
#define E1000_TDLEN    0x3808  // Tx Descriptor Length
#define E1000_TDH      0x3810  // Tx Descriptor Head
#define E1000_TDT      0x3818  // Tx Descriptor Tail
#define E1000_RAL      0x5400  // Receive Address Low
#define E1000_RAH      0x5404  // Receive Address High

// Control Register Bits
#define E1000_CTRL_FD       0x00000001  // Full Duplex
#define E1000_CTRL_ASDE     0x00000020  // Auto-Speed Detection Enable
#define E1000_CTRL_SLU      0x00000040  // Set Link Up
#define E1000_CTRL_ILOS     0x00000080  // Invert Loss of Signal
#define E1000_CTRL_RST      0x04000000  // Device Reset
#define E1000_CTRL_VME      0x40000000  // VLAN Mode Enable

// Status Register Bits
#define E1000_STATUS_FD     0x00000001  // Full Duplex
#define E1000_STATUS_LU     0x00000002  // Link Up
#define E1000_STATUS_SPEED  0x000000C0  // Speed Setting
#define E1000_STATUS_ASDV   0x00000300  // Auto Speed Detection Value
#define E1000_STATUS_TXOFF  0x00000100  // Transmission Paused
#define E1000_STATUS_SPEED_10   0x00000000
#define E1000_STATUS_SPEED_100  0x00000040
#define E1000_STATUS_SPEED_1000 0x00000080

// Receive Control Register Bits
#define E1000_RCTL_EN       0x00000002  // Receiver Enable
#define E1000_RCTL_SBP      0x00000004  // Store Bad Packets
#define E1000_RCTL_UPE      0x00000008  // Unicast Promiscuous Enable
#define E1000_RCTL_MPE      0x00000010  // Multicast Promiscuous Enable
#define E1000_RCTL_LBM      0x00000C00  // Loopback Mode
#define E1000_RCTL_RDMTS    0x00000300  // Rx Descriptor Minimum Threshold Size
#define E1000_RCTL_BSIZE    0x00030000  // Buffer Size
#define E1000_RCTL_BSEX     0x02000000  // Buffer Size Extension
#define E1000_RCTL_SECRC    0x04000000  // Strip Ethernet CRC

// Transmit Control Register Bits
#define E1000_TCTL_EN       0x00000002  // Transmit Enable
#define E1000_TCTL_PSP      0x00000008  // Pad Short Packets
#define E1000_TCTL_CT       0x00000100  // Collision Threshold
#define E1000_TCTL_COLD     0x00040000  // Collision Distance
#define E1000_TCTL_SWXOFF   0x00400000  // Software XOFF Transmission

// Interrupt Bits
#define E1000_ICR_TXDW      0x00000001  // Transmit Descriptor Written Back
#define E1000_ICR_TXQE      0x00000002  // Transmit Queue Empty
#define E1000_ICR_LSC       0x00000004  // Link Status Change
#define E1000_ICR_RXSEQ     0x00000008  // Receive Sequence Error
#define E1000_ICR_RXDMT0    0x00000010  // Receive Descriptor Minimum Threshold
#define E1000_ICR_RXO       0x00000040  // Receiver Overrun
#define E1000_ICR_RXT0      0x00000080  // Receiver Timer Interrupt

// Receive Descriptor Status Bits
#define E1000_RXD_STAT_DD   0x01  // Descriptor Done
#define E1000_RXD_STAT_EOP  0x02  // End of Packet
#define E1000_RXD_STAT_IXSM 0x04  // Ignore Checksum
#define E1000_RXD_STAT_VP   0x08  // VLAN Packet
#define E1000_RXD_STAT_TCPCS 0x20  // TCP/UDP Checksum
#define E1000_RXD_STAT_IPCS 0x40  // IP Checksum

// Transmit Descriptor Command Bits
#define E1000_TXD_CMD_EOP   0x01  // End of Packet
#define E1000_TXD_CMD_IFCS  0x02  // Insert FCS
#define E1000_TXD_CMD_IC    0x04  // Insert Checksum
#define E1000_TXD_CMD_RS    0x08  // Report Status
#define E1000_TXD_CMD_RPS   0x10  // Report Packet Sent
#define E1000_TXD_CMD_DEXT  0x20  // Descriptor Extension
#define E1000_TXD_CMD_VLE   0x40  // VLAN Packet Enable
#define E1000_TXD_CMD_IDE   0x80  // Interrupt Delay Enable

// Receive Descriptor
typedef struct {
    uint64_t addr;     // Buffer Address
    uint16_t length;   // Length
    uint16_t checksum; // Checksum
    uint8_t status;    // Status
    uint8_t errors;    // Errors
    uint16_t special;  // Special
} __attribute__((packed)) e1000_rx_desc_t;

// Transmit Descriptor
typedef struct {
    uint64_t addr;     // Buffer Address
    uint16_t length;   // Length
    uint8_t cso;      // Checksum Offset
    uint8_t cmd;      // Command
    uint8_t status;   // Status
    uint8_t css;      // Checksum Start
    uint16_t special; // Special
} __attribute__((packed)) e1000_tx_desc_t;

// e1000 Device Structure
typedef struct {
    eth_device_t eth_dev;          // Ethernet device structure
    uint8_t* mmio_base;           // Memory-mapped I/O base address
    uint32_t io_base;             // I/O port base address
    uint8_t mac_addr[ETH_ADDR_LEN]; // MAC address
    
    // Receive state
    e1000_rx_desc_t* rx_descs;    // Receive descriptors
    uint8_t* rx_buffers;          // Receive buffers
    uint32_t rx_cur;              // Current receive descriptor
    
    // Transmit state
    e1000_tx_desc_t* tx_descs;    // Transmit descriptors
    uint8_t* tx_buffers;          // Transmit buffers
    uint32_t tx_cur;              // Current transmit descriptor
    
    // Statistics
    uint32_t rx_bytes;
    uint32_t tx_bytes;
    uint32_t rx_packets;
    uint32_t tx_packets;
    uint32_t rx_errors;
    uint32_t tx_errors;
} e1000_device_t;

// Function declarations
bool e1000_init(net_interface_t* iface, uint8_t* mmio_base, uint32_t io_base);
void e1000_cleanup(net_interface_t* iface);
bool e1000_start(net_interface_t* iface);
void e1000_stop(net_interface_t* iface);
bool e1000_send_packet(net_interface_t* iface, net_packet_t* packet);
net_packet_t* e1000_receive_packet(net_interface_t* iface);
bool e1000_set_mac(net_interface_t* iface, const uint8_t* mac);
bool e1000_get_link_status(net_interface_t* iface);
void e1000_enable_interrupts(e1000_device_t* dev);
void e1000_disable_interrupts(e1000_device_t* dev);
void e1000_handle_interrupt(net_interface_t* iface);

#endif /* REXUS_E1000_H */ 