#include "ipv4.h"
#include "../mem/pmm.h"
#include "../drivers/vga.h"
#include <string.h>
#include <stdio.h>

// Maximum number of fragments per packet
#define MAX_FRAGMENTS 64

// Fragment timeout in milliseconds
#define FRAGMENT_TIMEOUT 30000

// IPv4 subsystem state
static struct {
    ipv4_route_t* routes;
    ipv4_stats_t stats;
    uint16_t ip_id;
    
    // Fragment reassembly
    struct {
        uint16_t id;
        ipv4_addr_t src;
        ipv4_addr_t dest;
        uint8_t protocol;
        uint32_t timestamp;
        uint16_t total_length;
        uint8_t* data;
        bool fragments[MAX_FRAGMENTS];
        uint8_t fragment_count;
    } reassembly_buffers[MAX_FRAGMENTS];
} ipv4_state;

// Initialize IPv4 subsystem
void ipv4_init(void) {
    memset(&ipv4_state, 0, sizeof(ipv4_state));
    vga_puts("IPv4: Protocol initialized\n");
}

// Clean up IPv4 subsystem
void ipv4_cleanup(void) {
    ipv4_flush_routes();
    
    // Free reassembly buffers
    for (int i = 0; i < MAX_FRAGMENTS; i++) {
        if (ipv4_state.reassembly_buffers[i].data) {
            pmm_free_blocks(ipv4_state.reassembly_buffers[i].data,
                (ipv4_state.reassembly_buffers[i].total_length + PAGE_SIZE - 1) / PAGE_SIZE);
        }
    }
}

// Calculate IPv4 checksum
uint16_t ipv4_checksum(const void* data, size_t length) {
    const uint16_t* ptr = (const uint16_t*)data;
    uint32_t sum = 0;
    
    // Sum up 16-bit words
    while (length > 1) {
        sum += *ptr++;
        length -= 2;
    }
    
    // Add left-over byte if any
    if (length > 0) {
        sum += *(const uint8_t*)ptr;
    }
    
    // Fold 32-bit sum into 16 bits
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~sum;
}

// Calculate IPv4 pseudo header checksum
uint16_t ipv4_pseudo_checksum(const ipv4_pseudo_header_t* pseudo_header,
                            const void* data, size_t length) {
    uint32_t sum = 0;
    
    // Sum up pseudo header
    const uint16_t* ptr = (const uint16_t*)pseudo_header;
    for (size_t i = 0; i < sizeof(ipv4_pseudo_header_t) / 2; i++) {
        sum += ptr[i];
    }
    
    // Add payload
    ptr = (const uint16_t*)data;
    while (length > 1) {
        sum += *ptr++;
        length -= 2;
    }
    
    // Add left-over byte if any
    if (length > 0) {
        sum += *(const uint8_t*)ptr;
    }
    
    // Fold 32-bit sum into 16 bits
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~sum;
}

// Send IPv4 packet
bool ipv4_send_packet(net_packet_t* packet, const ipv4_addr_t* dest_addr,
                     uint8_t protocol, uint8_t ttl) {
    if (!packet || !dest_addr) {
        return false;
    }
    
    // Find route to destination
    ipv4_route_t* route = ipv4_find_route(dest_addr);
    if (!route) {
        ipv4_state.stats.packets_dropped++;
        return false;
    }
    
    // Get interface configuration
    ipv4_config_t config;
    if (!ipv4_get_interface_config(route->iface, &config)) {
        ipv4_state.stats.packets_dropped++;
        return false;
    }
    
    // Prepare IPv4 header
    ipv4_header_t* header = (ipv4_header_t*)packet->data;
    header->version_ihl = (IPV4_VERSION << 4) | IPV4_IHL_MIN;
    header->tos = 0;
    header->total_length = sizeof(ipv4_header_t) + packet->length;
    header->id = ipv4_state.ip_id++;
    header->flags_offset = 0;
    header->ttl = ttl ? ttl : IPV4_TTL_DEFAULT;
    header->protocol = protocol;
    header->checksum = 0;
    header->src_addr = config.addr;
    header->dest_addr = *dest_addr;
    
    // Calculate header checksum
    header->checksum = ipv4_checksum(header, sizeof(ipv4_header_t));
    
    // Fragment packet if necessary
    if (header->total_length > route->iface->mtu) {
        if (!ipv4_fragment_packet(packet, route->iface->mtu)) {
            ipv4_state.stats.fragmentation_failures++;
            return false;
        }
        ipv4_state.stats.fragments_sent++;
    }
    
    // Update statistics
    ipv4_state.stats.packets_sent++;
    ipv4_state.stats.bytes_sent += packet->length;
    
    // Send packet
    return net_send_packet(route->iface, packet);
}

// Process received IPv4 packet
void ipv4_receive_packet(net_interface_t* iface, net_packet_t* packet) {
    if (!iface || !packet || packet->length < sizeof(ipv4_header_t)) {
        ipv4_state.stats.packets_dropped++;
        return;
    }
    
    // Get IPv4 header
    ipv4_header_t* header = (ipv4_header_t*)packet->data;
    
    // Verify header checksum
    uint16_t orig_checksum = header->checksum;
    header->checksum = 0;
    if (ipv4_checksum(header, sizeof(ipv4_header_t)) != orig_checksum) {
        ipv4_state.stats.packets_dropped++;
        return;
    }
    header->checksum = orig_checksum;
    
    // Update statistics
    ipv4_state.stats.packets_received++;
    ipv4_state.stats.bytes_received += packet->length;
    
    // Handle fragments
    if (header->flags_offset & (IPV4_FLAG_MORE_FRAGMENTS | IPV4_FRAGMENT_OFFSET_MASK)) {
        packet = ipv4_reassemble_packet(packet);
        if (!packet) {
            return;  // Packet is incomplete or reassembly failed
        }
        ipv4_state.stats.fragments_reassembled++;
    }
    
    // Forward packet if needed
    if (!ipv4_addr_is_local(&header->dest_addr)) {
        if (ipv4_forward_packet(packet)) {
            ipv4_state.stats.packets_forwarded++;
        } else {
            ipv4_state.stats.packets_dropped++;
        }
        return;
    }
    
    // Deliver to upper layer protocol
    net_protocol_t proto;
    switch (header->protocol) {
        case IPV4_PROTO_ICMP:
            proto = NET_PROTO_ICMP;
            break;
        case IPV4_PROTO_TCP:
            proto = NET_PROTO_TCP;
            break;
        case IPV4_PROTO_UDP:
            proto = NET_PROTO_UDP;
            break;
        default:
            ipv4_state.stats.packets_dropped++;
            return;
    }
    
    // Adjust packet data and length to remove IP header
    packet->data += sizeof(ipv4_header_t);
    packet->length -= sizeof(ipv4_header_t);
    packet->protocol = proto;
    
    // Pass to network stack
    net_receive_packet(iface, packet);
}

// Forward IPv4 packet
bool ipv4_forward_packet(net_packet_t* packet) {
    ipv4_header_t* header = (ipv4_header_t*)packet->data;
    
    // Check TTL
    if (header->ttl <= 1) {
        // Should send ICMP Time Exceeded here
        return false;
    }
    
    // Decrement TTL
    header->ttl--;
    
    // Recalculate checksum
    header->checksum = 0;
    header->checksum = ipv4_checksum(header, sizeof(ipv4_header_t));
    
    // Find route to destination
    ipv4_route_t* route = ipv4_find_route(&header->dest_addr);
    if (!route) {
        return false;
    }
    
    // Fragment packet if necessary
    if (packet->length > route->iface->mtu) {
        if (!ipv4_fragment_packet(packet, route->iface->mtu)) {
            return false;
        }
    }
    
    // Forward packet
    return net_send_packet(route->iface, packet);
}

// Fragment IPv4 packet
bool ipv4_fragment_packet(net_packet_t* packet, uint16_t mtu) {
    ipv4_header_t* orig_header = (ipv4_header_t*)packet->data;
    
    // Check if fragmentation is allowed
    if (orig_header->flags_offset & IPV4_FLAG_DONT_FRAGMENT) {
        return false;
    }
    
    // Calculate fragment size (must be multiple of 8)
    uint16_t max_data = (mtu - sizeof(ipv4_header_t)) & ~7;
    uint16_t data_len = packet->length - sizeof(ipv4_header_t);
    uint16_t num_fragments = (data_len + max_data - 1) / max_data;
    
    // Fragment the packet
    uint16_t offset = 0;
    uint8_t* data = packet->data + sizeof(ipv4_header_t);
    
    for (uint16_t i = 0; i < num_fragments; i++) {
        // Calculate fragment size
        uint16_t frag_size = (i == num_fragments - 1) ?
            data_len - offset : max_data;
        
        // Allocate fragment packet
        net_packet_t* frag = net_alloc_packet(sizeof(ipv4_header_t) + frag_size);
        if (!frag) {
            return false;
        }
        
        // Copy header
        memcpy(frag->data, orig_header, sizeof(ipv4_header_t));
        ipv4_header_t* frag_header = (ipv4_header_t*)frag->data;
        
        // Set fragment flags and offset
        frag_header->flags_offset = (offset / 8);
        if (i < num_fragments - 1) {
            frag_header->flags_offset |= IPV4_FLAG_MORE_FRAGMENTS;
        }
        
        // Copy data
        memcpy(frag->data + sizeof(ipv4_header_t),
               data + offset, frag_size);
        
        // Update total length and checksum
        frag_header->total_length = sizeof(ipv4_header_t) + frag_size;
        frag_header->checksum = 0;
        frag_header->checksum = ipv4_checksum(frag_header, sizeof(ipv4_header_t));
        
        // Send fragment
        if (!net_send_packet(packet->iface, frag)) {
            net_free_packet(frag);
            return false;
        }
        
        offset += frag_size;
    }
    
    return true;
}

// Reassemble IPv4 packet
net_packet_t* ipv4_reassemble_packet(net_packet_t* fragment) {
    ipv4_header_t* header = (ipv4_header_t*)fragment->data;
    uint16_t offset = (header->flags_offset & IPV4_FRAGMENT_OFFSET_MASK) * 8;
    
    // Find or allocate reassembly buffer
    int buf_index = -1;
    for (int i = 0; i < MAX_FRAGMENTS; i++) {
        if (ipv4_state.reassembly_buffers[i].data &&
            ipv4_state.reassembly_buffers[i].id == header->id &&
            ipv4_addr_equals(&ipv4_state.reassembly_buffers[i].src, &header->src_addr) &&
            ipv4_addr_equals(&ipv4_state.reassembly_buffers[i].dest, &header->dest_addr)) {
            buf_index = i;
            break;
        }
    }
    
    // Allocate new buffer if needed
    if (buf_index == -1) {
        for (int i = 0; i < MAX_FRAGMENTS; i++) {
            if (!ipv4_state.reassembly_buffers[i].data) {
                buf_index = i;
                break;
            }
        }
        
        if (buf_index == -1) {
            ipv4_state.stats.reassembly_failures++;
            return NULL;
        }
        
        // Initialize new reassembly buffer
        ipv4_state.reassembly_buffers[buf_index].id = header->id;
        ipv4_state.reassembly_buffers[buf_index].src = header->src_addr;
        ipv4_state.reassembly_buffers[buf_index].dest = header->dest_addr;
        ipv4_state.reassembly_buffers[buf_index].protocol = header->protocol;
        ipv4_state.reassembly_buffers[buf_index].timestamp = 0; // TODO: Get current time
        ipv4_state.reassembly_buffers[buf_index].total_length = 0;
        ipv4_state.reassembly_buffers[buf_index].fragment_count = 0;
        memset(ipv4_state.reassembly_buffers[buf_index].fragments, 0, MAX_FRAGMENTS);
        
        // Allocate data buffer
        ipv4_state.reassembly_buffers[buf_index].data = pmm_alloc_blocks(
            (IPV4_MAX_PACKET_SIZE + PAGE_SIZE - 1) / PAGE_SIZE);
        if (!ipv4_state.reassembly_buffers[buf_index].data) {
            ipv4_state.stats.reassembly_failures++;
            return NULL;
        }
    }
    
    // Copy fragment data
    uint16_t data_len = header->total_length - sizeof(ipv4_header_t);
    memcpy(ipv4_state.reassembly_buffers[buf_index].data + offset,
           fragment->data + sizeof(ipv4_header_t), data_len);
    
    // Mark fragment as received
    ipv4_state.reassembly_buffers[buf_index].fragments[offset / 8] = true;
    ipv4_state.reassembly_buffers[buf_index].fragment_count++;
    
    // Update total length if this is the last fragment
    if (!(header->flags_offset & IPV4_FLAG_MORE_FRAGMENTS)) {
        ipv4_state.reassembly_buffers[buf_index].total_length = offset + data_len;
    }
    
    // Check if packet is complete
    if (ipv4_state.reassembly_buffers[buf_index].total_length > 0) {
        uint16_t num_fragments = (ipv4_state.reassembly_buffers[buf_index].total_length + 7) / 8;
        bool complete = true;
        
        for (int i = 0; i < num_fragments; i++) {
            if (!ipv4_state.reassembly_buffers[buf_index].fragments[i]) {
                complete = false;
                break;
            }
        }
        
        if (complete) {
            // Allocate complete packet
            net_packet_t* packet = net_alloc_packet(
                sizeof(ipv4_header_t) + ipv4_state.reassembly_buffers[buf_index].total_length);
            if (!packet) {
                ipv4_state.stats.reassembly_failures++;
                return NULL;
            }
            
            // Copy header and data
            memcpy(packet->data, header, sizeof(ipv4_header_t));
            memcpy(packet->data + sizeof(ipv4_header_t),
                   ipv4_state.reassembly_buffers[buf_index].data,
                   ipv4_state.reassembly_buffers[buf_index].total_length);
            
            // Update header
            ipv4_header_t* new_header = (ipv4_header_t*)packet->data;
            new_header->flags_offset = 0;
            new_header->total_length = sizeof(ipv4_header_t) +
                ipv4_state.reassembly_buffers[buf_index].total_length;
            new_header->checksum = 0;
            new_header->checksum = ipv4_checksum(new_header, sizeof(ipv4_header_t));
            
            // Free reassembly buffer
            pmm_free_blocks(ipv4_state.reassembly_buffers[buf_index].data,
                (IPV4_MAX_PACKET_SIZE + PAGE_SIZE - 1) / PAGE_SIZE);
            memset(&ipv4_state.reassembly_buffers[buf_index], 0, sizeof(ipv4_state.reassembly_buffers[0]));
            
            return packet;
        }
    }
    
    return NULL;
}

// Route management functions
bool ipv4_add_route(const ipv4_addr_t* network, const ipv4_addr_t* netmask,
                   const ipv4_addr_t* gateway, net_interface_t* iface, uint32_t metric) {
    if (!network || !netmask || !iface) {
        return false;
    }
    
    // Allocate new route
    ipv4_route_t* route = pmm_alloc_blocks((sizeof(ipv4_route_t) + PAGE_SIZE - 1) / PAGE_SIZE);
    if (!route) {
        return false;
    }
    
    // Initialize route
    route->network = *network;
    route->netmask = *netmask;
    if (gateway) {
        route->gateway = *gateway;
    } else {
        memset(&route->gateway, 0, sizeof(ipv4_addr_t));
    }
    route->iface = iface;
    route->metric = metric;
    
    // Insert into routing table (sorted by metric)
    ipv4_route_t** ptr = &ipv4_state.routes;
    while (*ptr && (*ptr)->metric <= metric) {
        ptr = &(*ptr)->next;
    }
    route->next = *ptr;
    *ptr = route;
    
    return true;
}

bool ipv4_remove_route(const ipv4_addr_t* network, const ipv4_addr_t* netmask) {
    if (!network || !netmask) {
        return false;
    }
    
    ipv4_route_t** ptr = &ipv4_state.routes;
    while (*ptr) {
        if (ipv4_addr_equals(&(*ptr)->network, network) &&
            ipv4_addr_equals(&(*ptr)->netmask, netmask)) {
            ipv4_route_t* route = *ptr;
            *ptr = route->next;
            pmm_free_blocks(route, (sizeof(ipv4_route_t) + PAGE_SIZE - 1) / PAGE_SIZE);
            return true;
        }
        ptr = &(*ptr)->next;
    }
    
    return false;
}

ipv4_route_t* ipv4_find_route(const ipv4_addr_t* dest_addr) {
    if (!dest_addr) {
        return NULL;
    }
    
    ipv4_route_t* route = ipv4_state.routes;
    ipv4_route_t* best_route = NULL;
    
    while (route) {
        bool match = true;
        for (int i = 0; i < 4; i++) {
            if ((dest_addr->addr[i] & route->netmask.addr[i]) != route->network.addr[i]) {
                match = false;
                break;
            }
        }
        
        if (match) {
            if (!best_route || route->metric < best_route->metric) {
                best_route = route;
            }
        }
        
        route = route->next;
    }
    
    return best_route;
}

void ipv4_flush_routes(void) {
    while (ipv4_state.routes) {
        ipv4_route_t* route = ipv4_state.routes;
        ipv4_state.routes = route->next;
        pmm_free_blocks(route, (sizeof(ipv4_route_t) + PAGE_SIZE - 1) / PAGE_SIZE);
    }
}

// Interface configuration functions
bool ipv4_configure_interface(net_interface_t* iface, const ipv4_config_t* config) {
    if (!iface || !config) {
        return false;
    }
    
    // Store configuration in interface private data
    ipv4_config_t* iface_config = pmm_alloc_blocks((sizeof(ipv4_config_t) + PAGE_SIZE - 1) / PAGE_SIZE);
    if (!iface_config) {
        return false;
    }
    
    *iface_config = *config;
    iface->driver_data = iface_config;
    
    return true;
}

bool ipv4_get_interface_config(net_interface_t* iface, ipv4_config_t* config) {
    if (!iface || !config) {
        return false;
    }
    
    ipv4_config_t* iface_config = (ipv4_config_t*)iface->driver_data;
    if (!iface_config) {
        return false;
    }
    
    *config = *iface_config;
    return true;
}

// Address manipulation functions
bool ipv4_addr_is_broadcast(const ipv4_addr_t* addr, const ipv4_addr_t* netmask) {
    if (!addr || !netmask) {
        return false;
    }
    
    for (int i = 0; i < 4; i++) {
        if ((addr->addr[i] & netmask->addr[i]) != (0xFF & ~netmask->addr[i])) {
            return false;
        }
    }
    
    return true;
}

bool ipv4_addr_is_multicast(const ipv4_addr_t* addr) {
    return addr && (addr->addr[0] >= 224 && addr->addr[0] <= 239);
}

bool ipv4_addr_is_local(const ipv4_addr_t* addr) {
    if (!addr) {
        return false;
    }
    
    // Check if address matches any interface
    net_interface_t* iface = net_get_interface_by_index(0);
    while (iface) {
        ipv4_config_t config;
        if (ipv4_get_interface_config(iface, &config)) {
            if (ipv4_addr_equals(addr, &config.addr)) {
                return true;
            }
        }
        iface = iface->next;
    }
    
    return false;
}

bool ipv4_addr_equals(const ipv4_addr_t* addr1, const ipv4_addr_t* addr2) {
    if (!addr1 || !addr2) {
        return false;
    }
    
    return memcmp(addr1->addr, addr2->addr, 4) == 0;
}

void ipv4_addr_to_string(const ipv4_addr_t* addr, char* str) {
    if (!addr || !str) {
        return;
    }
    
    sprintf(str, "%d.%d.%d.%d",
            addr->addr[0], addr->addr[1],
            addr->addr[2], addr->addr[3]);
}

bool ipv4_string_to_addr(const char* str, ipv4_addr_t* addr) {
    if (!str || !addr) {
        return false;
    }
    
    unsigned int a, b, c, d;
    if (sscanf(str, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
        return false;
    }
    
    if (a > 255 || b > 255 || c > 255 || d > 255) {
        return false;
    }
    
    addr->addr[0] = a;
    addr->addr[1] = b;
    addr->addr[2] = c;
    addr->addr[3] = d;
    
    return true;
}

// Statistics functions
void ipv4_get_stats(ipv4_stats_t* stats) {
    if (stats) {
        *stats = ipv4_state.stats;
    }
}

void ipv4_reset_stats(void) {
    memset(&ipv4_state.stats, 0, sizeof(ipv4_stats_t));
} 