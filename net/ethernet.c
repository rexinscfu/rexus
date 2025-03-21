#include "ethernet.h"
#include "../drivers/vga.h"
#include <string.h>
#include <stdio.h>

// CRC-32 lookup table for Ethernet FCS calculation
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
    0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    // ... (truncated for brevity, full table would be here)
};

// Initialize Ethernet device
bool eth_init_device(net_interface_t* iface, eth_device_t* dev) {
    if (!iface || !dev) {
        return false;
    }
    
    // Set up interface operations
    iface->init = dev->ops.init;
    iface->cleanup = dev->ops.cleanup;
    iface->start = dev->ops.start;
    iface->stop = dev->ops.stop;
    iface->send = dev->ops.send;
    iface->receive = dev->ops.receive;
    iface->set_mac = dev->ops.set_mac;
    
    // Set interface type
    iface->type = NET_IF_TYPE_ETHERNET;
    iface->mtu = ETH_MAX_DATA_SIZE;
    
    // Store device structure in interface
    iface->driver_data = dev;
    
    // Initialize the device
    if (!dev->ops.init(iface)) {
        return false;
    }
    
    return true;
}

// Clean up Ethernet device
void eth_cleanup_device(net_interface_t* iface) {
    if (!iface || !iface->driver_data) {
        return;
    }
    
    eth_device_t* dev = (eth_device_t*)iface->driver_data;
    if (dev->ops.cleanup) {
        dev->ops.cleanup(iface);
    }
}

// Start Ethernet device
bool eth_start_device(net_interface_t* iface) {
    if (!iface || !iface->driver_data) {
        return false;
    }
    
    eth_device_t* dev = (eth_device_t*)iface->driver_data;
    if (!dev->ops.start) {
        return false;
    }
    
    return dev->ops.start(iface);
}

// Stop Ethernet device
void eth_stop_device(net_interface_t* iface) {
    if (!iface || !iface->driver_data) {
        return;
    }
    
    eth_device_t* dev = (eth_device_t*)iface->driver_data;
    if (dev->ops.stop) {
        dev->ops.stop(iface);
    }
}

// Send an Ethernet frame
bool eth_send_frame(net_interface_t* iface, const void* data, size_t length) {
    if (!iface || !data || length < ETH_MIN_FRAME_SIZE || length > ETH_MAX_FRAME_SIZE) {
        return false;
    }
    
    // Allocate packet
    net_packet_t* packet = net_alloc_packet(length);
    if (!packet) {
        return false;
    }
    
    // Copy frame data
    memcpy(packet->data, data, length);
    packet->length = length;
    
    // Send packet
    bool success = net_send_packet(iface, packet);
    
    // Free packet
    net_free_packet(packet);
    
    return success;
}

// Receive an Ethernet frame
bool eth_receive_frame(net_interface_t* iface, void* data, size_t* length) {
    if (!iface || !data || !length || *length < ETH_MIN_FRAME_SIZE) {
        return false;
    }
    
    // Receive packet
    net_packet_t* packet = net_receive_packet(iface);
    if (!packet) {
        return false;
    }
    
    // Check length
    if (packet->length > *length) {
        net_free_packet(packet);
        return false;
    }
    
    // Copy frame data
    memcpy(data, packet->data, packet->length);
    *length = packet->length;
    
    // Free packet
    net_free_packet(packet);
    
    return true;
}

// Set MAC address
bool eth_set_mac_address(net_interface_t* iface, const uint8_t* mac) {
    if (!iface || !mac || !eth_is_valid_mac(mac)) {
        return false;
    }
    
    eth_device_t* dev = (eth_device_t*)iface->driver_data;
    if (!dev || !dev->ops.set_mac) {
        return false;
    }
    
    if (!dev->ops.set_mac(iface, mac)) {
        return false;
    }
    
    memcpy(iface->mac, mac, ETH_ADDR_LEN);
    return true;
}

// Get MAC address
bool eth_get_mac_address(net_interface_t* iface, uint8_t* mac) {
    if (!iface || !mac) {
        return false;
    }
    
    memcpy(mac, iface->mac, ETH_ADDR_LEN);
    return true;
}

// Set promiscuous mode
bool eth_set_promiscuous(net_interface_t* iface, bool enable) {
    if (!iface || !iface->driver_data) {
        return false;
    }
    
    eth_device_t* dev = (eth_device_t*)iface->driver_data;
    if (!dev->ops.set_promiscuous) {
        return false;
    }
    
    return dev->ops.set_promiscuous(iface, enable);
}

// Set multicast mode
bool eth_set_multicast(net_interface_t* iface, bool enable) {
    if (!iface || !iface->driver_data) {
        return false;
    }
    
    eth_device_t* dev = (eth_device_t*)iface->driver_data;
    if (!dev->ops.set_multicast) {
        return false;
    }
    
    return dev->ops.set_multicast(iface, enable);
}

// Calculate Ethernet frame CRC32 (FCS)
uint32_t eth_calculate_crc32(const void* data, size_t length) {
    const uint8_t* buf = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc & 0xFF) ^ buf[i]];
    }
    
    return ~crc;
}

// Check if MAC address is valid
bool eth_is_valid_mac(const uint8_t* mac) {
    if (!mac) {
        return false;
    }
    
    // Check for null address
    bool all_zero = true;
    for (int i = 0; i < ETH_ADDR_LEN; i++) {
        if (mac[i] != 0) {
            all_zero = false;
            break;
        }
    }
    if (all_zero) {
        return false;
    }
    
    // Check for broadcast address
    bool all_one = true;
    for (int i = 0; i < ETH_ADDR_LEN; i++) {
        if (mac[i] != 0xFF) {
            all_one = false;
            break;
        }
    }
    if (all_one) {
        return false;
    }
    
    // Check for multicast bit
    if (mac[0] & 0x01) {
        return false;
    }
    
    return true;
}

// Format MAC address as string
void eth_format_mac(char* str, const uint8_t* mac) {
    if (!str || !mac) {
        return;
    }
    
    snprintf(str, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// Parse MAC address from string
bool eth_parse_mac(const char* str, uint8_t* mac) {
    if (!str || !mac) {
        return false;
    }
    
    int values[6];
    int count = sscanf(str, "%x:%x:%x:%x:%x:%x",
                      &values[0], &values[1], &values[2],
                      &values[3], &values[4], &values[5]);
    
    if (count != 6) {
        return false;
    }
    
    for (int i = 0; i < 6; i++) {
        if (values[i] < 0 || values[i] > 255) {
            return false;
        }
        mac[i] = (uint8_t)values[i];
    }
    
    return eth_is_valid_mac(mac);
} 