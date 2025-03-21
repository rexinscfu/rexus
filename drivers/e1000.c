#include "e1000.h"
#include "../arch/x86/io.h"
#include "../mem/pmm.h"
#include "../drivers/vga.h"
#include <string.h>

#define E1000_NUM_RX_DESC 32
#define E1000_NUM_TX_DESC 32
#define E1000_RX_BUFFER_SIZE 2048
#define E1000_TX_BUFFER_SIZE 2048

// Helper functions for MMIO access
static inline void e1000_write_reg(e1000_device_t* dev, uint32_t reg, uint32_t value) {
    *(volatile uint32_t*)(dev->mmio_base + reg) = value;
}

static inline uint32_t e1000_read_reg(e1000_device_t* dev, uint32_t reg) {
    return *(volatile uint32_t*)(dev->mmio_base + reg);
}

// Read MAC address from EEPROM
static bool e1000_read_mac_from_eeprom(e1000_device_t* dev) {
    uint32_t val;
    uint16_t mac_low, mac_high;
    
    // Read MAC address from EEPROM
    e1000_write_reg(dev, E1000_EERD, 0x0 | (0 << 8) | 0x1);
    while (!((val = e1000_read_reg(dev, E1000_EERD)) & (1 << 4)));
    mac_low = (val >> 16) & 0xFFFF;
    
    e1000_write_reg(dev, E1000_EERD, 0x1 | (1 << 8) | 0x1);
    while (!((val = e1000_read_reg(dev, E1000_EERD)) & (1 << 4)));
    mac_high = (val >> 16) & 0xFFFF;
    
    dev->mac_addr[0] = mac_low & 0xFF;
    dev->mac_addr[1] = (mac_low >> 8) & 0xFF;
    dev->mac_addr[2] = mac_high & 0xFF;
    dev->mac_addr[3] = (mac_high >> 8) & 0xFF;
    
    e1000_write_reg(dev, E1000_EERD, 0x2 | (2 << 8) | 0x1);
    while (!((val = e1000_read_reg(dev, E1000_EERD)) & (1 << 4)));
    mac_high = (val >> 16) & 0xFFFF;
    
    dev->mac_addr[4] = mac_high & 0xFF;
    dev->mac_addr[5] = (mac_high >> 8) & 0xFF;
    
    return true;
}

// Initialize receive descriptors
static bool e1000_init_rx(e1000_device_t* dev) {
    // Allocate descriptor array
    dev->rx_descs = pmm_alloc_blocks(
        (sizeof(e1000_rx_desc_t) * E1000_NUM_RX_DESC + PAGE_SIZE - 1) / PAGE_SIZE);
    if (!dev->rx_descs) {
        return false;
    }
    
    // Allocate receive buffers
    dev->rx_buffers = pmm_alloc_blocks(
        (E1000_RX_BUFFER_SIZE * E1000_NUM_RX_DESC + PAGE_SIZE - 1) / PAGE_SIZE);
    if (!dev->rx_buffers) {
        pmm_free_blocks(dev->rx_descs, 
            (sizeof(e1000_rx_desc_t) * E1000_NUM_RX_DESC + PAGE_SIZE - 1) / PAGE_SIZE);
        return false;
    }
    
    // Initialize descriptors
    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        dev->rx_descs[i].addr = (uint64_t)(uintptr_t)(dev->rx_buffers + i * E1000_RX_BUFFER_SIZE);
        dev->rx_descs[i].status = 0;
    }
    
    // Setup receive descriptor registers
    e1000_write_reg(dev, E1000_RDBAL, (uint32_t)(uintptr_t)dev->rx_descs);
    e1000_write_reg(dev, E1000_RDBAH, 0);
    e1000_write_reg(dev, E1000_RDLEN, E1000_NUM_RX_DESC * sizeof(e1000_rx_desc_t));
    e1000_write_reg(dev, E1000_RDH, 0);
    e1000_write_reg(dev, E1000_RDT, E1000_NUM_RX_DESC - 1);
    
    // Enable receiver
    uint32_t rctl = e1000_read_reg(dev, E1000_RCTL);
    rctl |= E1000_RCTL_EN | E1000_RCTL_SBP | E1000_RCTL_UPE | E1000_RCTL_MPE;
    rctl &= ~E1000_RCTL_BSIZE;  // Set buffer size to 2048
    rctl |= E1000_RCTL_SECRC;   // Strip CRC
    e1000_write_reg(dev, E1000_RCTL, rctl);
    
    dev->rx_cur = 0;
    return true;
}

// Initialize transmit descriptors
static bool e1000_init_tx(e1000_device_t* dev) {
    // Allocate descriptor array
    dev->tx_descs = pmm_alloc_blocks(
        (sizeof(e1000_tx_desc_t) * E1000_NUM_TX_DESC + PAGE_SIZE - 1) / PAGE_SIZE);
    if (!dev->tx_descs) {
        return false;
    }
    
    // Allocate transmit buffers
    dev->tx_buffers = pmm_alloc_blocks(
        (E1000_TX_BUFFER_SIZE * E1000_NUM_TX_DESC + PAGE_SIZE - 1) / PAGE_SIZE);
    if (!dev->tx_buffers) {
        pmm_free_blocks(dev->tx_descs, 
            (sizeof(e1000_tx_desc_t) * E1000_NUM_TX_DESC + PAGE_SIZE - 1) / PAGE_SIZE);
        return false;
    }
    
    // Initialize descriptors
    memset(dev->tx_descs, 0, sizeof(e1000_tx_desc_t) * E1000_NUM_TX_DESC);
    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        dev->tx_descs[i].addr = (uint64_t)(uintptr_t)(dev->tx_buffers + i * E1000_TX_BUFFER_SIZE);
        dev->tx_descs[i].cmd = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;
    }
    
    // Setup transmit descriptor registers
    e1000_write_reg(dev, E1000_TDBAL, (uint32_t)(uintptr_t)dev->tx_descs);
    e1000_write_reg(dev, E1000_TDBAH, 0);
    e1000_write_reg(dev, E1000_TDLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t));
    e1000_write_reg(dev, E1000_TDH, 0);
    e1000_write_reg(dev, E1000_TDT, 0);
    
    // Enable transmitter
    uint32_t tctl = e1000_read_reg(dev, E1000_TCTL);
    tctl |= E1000_TCTL_EN | E1000_TCTL_PSP;
    tctl |= (15 << 4);  // Collision threshold
    tctl |= (64 << 12); // Collision distance
    e1000_write_reg(dev, E1000_TCTL, tctl);
    
    dev->tx_cur = 0;
    return true;
}

// Initialize the e1000 device
bool e1000_init(net_interface_t* iface, uint8_t* mmio_base, uint32_t io_base) {
    e1000_device_t* dev = (e1000_device_t*)pmm_alloc_blocks(
        (sizeof(e1000_device_t) + PAGE_SIZE - 1) / PAGE_SIZE);
    if (!dev) {
        return false;
    }
    
    memset(dev, 0, sizeof(e1000_device_t));
    dev->mmio_base = mmio_base;
    dev->io_base = io_base;
    
    // Reset the device
    e1000_write_reg(dev, E1000_CTRL, e1000_read_reg(dev, E1000_CTRL) | E1000_CTRL_RST);
    for (int i = 0; i < 1000; i++) {
        io_wait();
    }
    
    // Wait for reset to complete
    while (e1000_read_reg(dev, E1000_CTRL) & E1000_CTRL_RST);
    
    // Disable interrupts
    e1000_write_reg(dev, E1000_IMC, 0xFFFFFFFF);
    e1000_read_reg(dev, E1000_ICR);
    
    // Read MAC address
    if (!e1000_read_mac_from_eeprom(dev)) {
        pmm_free_blocks(dev, (sizeof(e1000_device_t) + PAGE_SIZE - 1) / PAGE_SIZE);
        return false;
    }
    
    // Initialize receive and transmit descriptors
    if (!e1000_init_rx(dev) || !e1000_init_tx(dev)) {
        pmm_free_blocks(dev, (sizeof(e1000_device_t) + PAGE_SIZE - 1) / PAGE_SIZE);
        return false;
    }
    
    // Setup link
    uint32_t ctrl = e1000_read_reg(dev, E1000_CTRL);
    ctrl |= E1000_CTRL_SLU | E1000_CTRL_ASDE;
    e1000_write_reg(dev, E1000_CTRL, ctrl);
    
    // Store device in interface
    iface->driver_data = dev;
    
    // Set interface MAC address
    memcpy(iface->mac, dev->mac_addr, ETH_ADDR_LEN);
    
    return true;
}

// Clean up the e1000 device
void e1000_cleanup(net_interface_t* iface) {
    e1000_device_t* dev = (e1000_device_t*)iface->driver_data;
    if (!dev) {
        return;
    }
    
    // Stop device
    e1000_stop(iface);
    
    // Free receive and transmit resources
    if (dev->rx_descs) {
        pmm_free_blocks(dev->rx_descs, 
            (sizeof(e1000_rx_desc_t) * E1000_NUM_RX_DESC + PAGE_SIZE - 1) / PAGE_SIZE);
    }
    if (dev->rx_buffers) {
        pmm_free_blocks(dev->rx_buffers,
            (E1000_RX_BUFFER_SIZE * E1000_NUM_RX_DESC + PAGE_SIZE - 1) / PAGE_SIZE);
    }
    if (dev->tx_descs) {
        pmm_free_blocks(dev->tx_descs,
            (sizeof(e1000_tx_desc_t) * E1000_NUM_TX_DESC + PAGE_SIZE - 1) / PAGE_SIZE);
    }
    if (dev->tx_buffers) {
        pmm_free_blocks(dev->tx_buffers,
            (E1000_TX_BUFFER_SIZE * E1000_NUM_TX_DESC + PAGE_SIZE - 1) / PAGE_SIZE);
    }
    
    // Free device structure
    pmm_free_blocks(dev, (sizeof(e1000_device_t) + PAGE_SIZE - 1) / PAGE_SIZE);
    iface->driver_data = NULL;
}

// Start the e1000 device
bool e1000_start(net_interface_t* iface) {
    e1000_device_t* dev = (e1000_device_t*)iface->driver_data;
    if (!dev) {
        return false;
    }
    
    // Enable receiver and transmitter
    uint32_t rctl = e1000_read_reg(dev, E1000_RCTL);
    rctl |= E1000_RCTL_EN;
    e1000_write_reg(dev, E1000_RCTL, rctl);
    
    uint32_t tctl = e1000_read_reg(dev, E1000_TCTL);
    tctl |= E1000_TCTL_EN;
    e1000_write_reg(dev, E1000_TCTL, tctl);
    
    // Enable interrupts
    e1000_enable_interrupts(dev);
    
    return true;
}

// Stop the e1000 device
void e1000_stop(net_interface_t* iface) {
    e1000_device_t* dev = (e1000_device_t*)iface->driver_data;
    if (!dev) {
        return;
    }
    
    // Disable interrupts
    e1000_disable_interrupts(dev);
    
    // Disable receiver and transmitter
    uint32_t rctl = e1000_read_reg(dev, E1000_RCTL);
    rctl &= ~E1000_RCTL_EN;
    e1000_write_reg(dev, E1000_RCTL, rctl);
    
    uint32_t tctl = e1000_read_reg(dev, E1000_TCTL);
    tctl &= ~E1000_TCTL_EN;
    e1000_write_reg(dev, E1000_TCTL, tctl);
}

// Send a packet
bool e1000_send_packet(net_interface_t* iface, net_packet_t* packet) {
    e1000_device_t* dev = (e1000_device_t*)iface->driver_data;
    if (!dev || !packet || packet->length > E1000_TX_BUFFER_SIZE) {
        return false;
    }
    
    // Wait for descriptor to be available
    while (!(dev->tx_descs[dev->tx_cur].status & 0xFF));
    
    // Copy packet to buffer
    memcpy((void*)(uintptr_t)dev->tx_descs[dev->tx_cur].addr, packet->data, packet->length);
    
    // Setup descriptor
    dev->tx_descs[dev->tx_cur].length = packet->length;
    dev->tx_descs[dev->tx_cur].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
    dev->tx_descs[dev->tx_cur].status = 0;
    
    // Update statistics
    dev->tx_packets++;
    dev->tx_bytes += packet->length;
    
    // Advance tail pointer
    uint32_t old_cur = dev->tx_cur;
    dev->tx_cur = (dev->tx_cur + 1) % E1000_NUM_TX_DESC;
    e1000_write_reg(dev, E1000_TDT, dev->tx_cur);
    
    return true;
}

// Receive a packet
net_packet_t* e1000_receive_packet(net_interface_t* iface) {
    e1000_device_t* dev = (e1000_device_t*)iface->driver_data;
    if (!dev) {
        return NULL;
    }
    
    // Check if packet is available
    if (!(dev->rx_descs[dev->rx_cur].status & E1000_RXD_STAT_DD)) {
        return NULL;
    }
    
    // Get packet length
    uint16_t length = dev->rx_descs[dev->rx_cur].length;
    if (length > E1000_RX_BUFFER_SIZE) {
        dev->rx_errors++;
        return NULL;
    }
    
    // Allocate packet
    net_packet_t* packet = net_alloc_packet(length);
    if (!packet) {
        dev->rx_errors++;
        return NULL;
    }
    
    // Copy data from buffer
    memcpy(packet->data, (void*)(uintptr_t)dev->rx_descs[dev->rx_cur].addr, length);
    packet->length = length;
    
    // Update statistics
    dev->rx_packets++;
    dev->rx_bytes += length;
    
    // Reset descriptor
    dev->rx_descs[dev->rx_cur].status = 0;
    
    // Advance tail pointer
    uint32_t old_cur = dev->rx_cur;
    dev->rx_cur = (dev->rx_cur + 1) % E1000_NUM_RX_DESC;
    e1000_write_reg(dev, E1000_RDT, old_cur);
    
    return packet;
}

// Set MAC address
bool e1000_set_mac(net_interface_t* iface, const uint8_t* mac) {
    e1000_device_t* dev = (e1000_device_t*)iface->driver_data;
    if (!dev || !mac) {
        return false;
    }
    
    // Store MAC address
    memcpy(dev->mac_addr, mac, ETH_ADDR_LEN);
    memcpy(iface->mac, mac, ETH_ADDR_LEN);
    
    // Update hardware registers
    uint32_t low = ((uint32_t)mac[0] |
                    ((uint32_t)mac[1] << 8) |
                    ((uint32_t)mac[2] << 16) |
                    ((uint32_t)mac[3] << 24));
    uint32_t high = ((uint32_t)mac[4] |
                     ((uint32_t)mac[5] << 8));
    
    e1000_write_reg(dev, E1000_RAL, low);
    e1000_write_reg(dev, E1000_RAH, high);
    
    return true;
}

// Get link status
bool e1000_get_link_status(net_interface_t* iface) {
    e1000_device_t* dev = (e1000_device_t*)iface->driver_data;
    if (!dev) {
        return false;
    }
    
    return (e1000_read_reg(dev, E1000_STATUS) & E1000_STATUS_LU) != 0;
}

// Enable interrupts
void e1000_enable_interrupts(e1000_device_t* dev) {
    if (!dev) {
        return;
    }
    
    // Enable desired interrupts
    e1000_write_reg(dev, E1000_IMS,
        E1000_ICR_LSC |    // Link Status Change
        E1000_ICR_RXT0 |   // Receiver Timer
        E1000_ICR_RXDMT0 | // Receive Descriptor Minimum
        E1000_ICR_RXO |    // Receiver Overrun
        E1000_ICR_TXQE     // Transmit Queue Empty
    );
}

// Disable interrupts
void e1000_disable_interrupts(e1000_device_t* dev) {
    if (!dev) {
        return;
    }
    
    e1000_write_reg(dev, E1000_IMC, 0xFFFFFFFF);
    e1000_read_reg(dev, E1000_ICR);
}

// Handle interrupt
void e1000_handle_interrupt(net_interface_t* iface) {
    e1000_device_t* dev = (e1000_device_t*)iface->driver_data;
    if (!dev) {
        return;
    }
    
    // Read and clear interrupt status
    uint32_t icr = e1000_read_reg(dev, E1000_ICR);
    
    // Handle link status change
    if (icr & E1000_ICR_LSC) {
        bool link_up = e1000_get_link_status(iface);
        vga_puts(link_up ? "e1000: Link is up\n" : "e1000: Link is down\n");
    }
    
    // Handle receive interrupts
    if (icr & (E1000_ICR_RXT0 | E1000_ICR_RXDMT0)) {
        net_packet_t* packet;
        while ((packet = e1000_receive_packet(iface)) != NULL) {
            // Process received packet
            if (iface->receive) {
                iface->receive(iface, packet);
            } else {
                net_free_packet(packet);
            }
        }
    }
    
    // Handle receive overrun
    if (icr & E1000_ICR_RXO) {
        dev->rx_errors++;
        vga_puts("e1000: Receive overrun\n");
    }
} 