#include "net.h"
#include "../mem/pmm.h"
#include "../drivers/vga.h"
#include <string.h>

// Network subsystem state
static struct {
    net_interface_t* interfaces;
    uint32_t interface_count;
    net_protocol_handler_t protocol_handlers[16];
    uint8_t* packet_pool;
    uint32_t packet_pool_size;
} net_state;

// Initialize network subsystem
void net_init(void) {
    memset(&net_state, 0, sizeof(net_state));
    
    // Allocate packet pool (64KB initially)
    net_state.packet_pool_size = 65536;
    net_state.packet_pool = pmm_alloc_blocks(net_state.packet_pool_size / PAGE_SIZE);
    
    if (!net_state.packet_pool) {
        vga_puts("NET: Failed to allocate packet pool\n");
        return;
    }
    
    vga_puts("NET: Network subsystem initialized\n");
}

// Clean up network subsystem
void net_cleanup(void) {
    // Free all interfaces
    net_interface_t* iface = net_state.interfaces;
    while (iface) {
        net_interface_t* next = iface->next;
        if (iface->cleanup) {
            iface->cleanup(iface);
        }
        iface = next;
    }
    
    // Free packet pool
    if (net_state.packet_pool) {
        pmm_free_blocks(net_state.packet_pool, net_state.packet_pool_size / PAGE_SIZE);
    }
    
    memset(&net_state, 0, sizeof(net_state));
}

// Register a network interface
bool net_register_interface(net_interface_t* iface) {
    if (!iface || !iface->init || !iface->send || !iface->receive) {
        return false;
    }
    
    // Initialize the interface
    if (!iface->init(iface)) {
        return false;
    }
    
    // Add to interface list
    if (!net_state.interfaces) {
        net_state.interfaces = iface;
    } else {
        net_interface_t* last = net_state.interfaces;
        while (last->next) {
            last = last->next;
        }
        last->next = iface;
    }
    
    net_state.interface_count++;
    
    vga_puts("NET: Registered interface ");
    vga_puts(iface->name);
    vga_puts("\n");
    
    return true;
}

// Unregister a network interface
void net_unregister_interface(net_interface_t* iface) {
    if (!iface) {
        return;
    }
    
    // Remove from interface list
    if (net_state.interfaces == iface) {
        net_state.interfaces = iface->next;
    } else {
        net_interface_t* prev = net_state.interfaces;
        while (prev && prev->next != iface) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = iface->next;
        }
    }
    
    // Clean up the interface
    if (iface->cleanup) {
        iface->cleanup(iface);
    }
    
    net_state.interface_count--;
}

// Get interface by name
net_interface_t* net_get_interface(const char* name) {
    net_interface_t* iface = net_state.interfaces;
    while (iface) {
        if (strcmp(iface->name, name) == 0) {
            return iface;
        }
        iface = iface->next;
    }
    return NULL;
}

// Get interface by index
net_interface_t* net_get_interface_by_index(uint32_t index) {
    if (index >= net_state.interface_count) {
        return NULL;
    }
    
    net_interface_t* iface = net_state.interfaces;
    while (index-- > 0 && iface) {
        iface = iface->next;
    }
    return iface;
}

// Get number of registered interfaces
uint32_t net_get_interface_count(void) {
    return net_state.interface_count;
}

// Allocate a network packet
net_packet_t* net_alloc_packet(size_t size) {
    if (size > NET_MAX_PACKET_SIZE) {
        return NULL;
    }
    
    // Allocate packet structure
    net_packet_t* packet = pmm_alloc_block();
    if (!packet) {
        return NULL;
    }
    
    // Allocate packet data
    packet->data = pmm_alloc_blocks((size + PAGE_SIZE - 1) / PAGE_SIZE);
    if (!packet->data) {
        pmm_free_block(packet);
        return NULL;
    }
    
    packet->length = size;
    packet->protocol = NET_PROTO_NONE;
    packet->priority = 0;
    packet->private_data = NULL;
    
    return packet;
}

// Free a network packet
void net_free_packet(net_packet_t* packet) {
    if (!packet) {
        return;
    }
    
    if (packet->data) {
        pmm_free_blocks(packet->data, (packet->length + PAGE_SIZE - 1) / PAGE_SIZE);
    }
    
    pmm_free_block(packet);
}

// Send a packet through an interface
bool net_send_packet(net_interface_t* iface, net_packet_t* packet) {
    if (!iface || !packet || !iface->send) {
        return false;
    }
    
    // Update statistics
    iface->stats.tx_packets++;
    iface->stats.tx_bytes += packet->length;
    
    // Send the packet
    bool success = iface->send(iface, packet);
    if (!success) {
        iface->stats.tx_errors++;
    }
    
    return success;
}

// Receive a packet from an interface
net_packet_t* net_receive_packet(net_interface_t* iface) {
    if (!iface || !iface->receive) {
        return NULL;
    }
    
    // Receive the packet
    net_packet_t* packet = iface->receive(iface);
    if (packet) {
        // Update statistics
        iface->stats.rx_packets++;
        iface->stats.rx_bytes += packet->length;
    }
    
    return packet;
}

// Register a protocol handler
bool net_register_protocol_handler(net_protocol_t proto, net_protocol_handler_t handler) {
    if (proto >= sizeof(net_state.protocol_handlers) / sizeof(net_protocol_handler_t)) {
        return false;
    }
    
    net_state.protocol_handlers[proto] = handler;
    return true;
}

// Unregister a protocol handler
void net_unregister_protocol_handler(net_protocol_t proto) {
    if (proto < sizeof(net_state.protocol_handlers) / sizeof(net_protocol_handler_t)) {
        net_state.protocol_handlers[proto] = NULL;
    }
}

// Process received packets
void net_process_rx_queue(void) {
    net_interface_t* iface = net_state.interfaces;
    while (iface) {
        net_packet_t* packet = net_receive_packet(iface);
        if (packet) {
            // Handle the packet based on protocol
            if (packet->protocol < sizeof(net_state.protocol_handlers) / sizeof(net_protocol_handler_t) &&
                net_state.protocol_handlers[packet->protocol]) {
                net_state.protocol_handlers[packet->protocol](iface, packet);
            } else {
                // No handler for this protocol
                net_free_packet(packet);
            }
        }
        iface = iface->next;
    }
}

// Process transmit queue
void net_process_tx_queue(void) {
    // To be implemented when we add queuing support
}

// Update network subsystem
void net_update(void) {
    net_process_rx_queue();
    net_process_tx_queue();
} 