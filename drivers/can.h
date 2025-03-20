#ifndef REXUS_CAN_H
#define REXUS_CAN_H

#include <stdint.h>
#include <stdbool.h>

#define CAN_MAX_FILTERS 14
#define CAN_MAX_DLC 8
#define CAN_RX_BUFFER_SIZE 64
#define CAN_TX_BUFFER_SIZE 32

typedef enum {
    CAN_SPEED_125K = 125000,
    CAN_SPEED_250K = 250000,
    CAN_SPEED_500K = 500000,
    CAN_SPEED_1M = 1000000
} can_speed_t;

typedef enum {
    CAN_MODE_NORMAL,
    CAN_MODE_LOOPBACK,
    CAN_MODE_SILENT,
    CAN_MODE_SILENT_LOOPBACK
} can_mode_t;

typedef struct {
    uint32_t id;
    bool extended;
    bool rtr;
    uint8_t dlc;
    uint8_t data[CAN_MAX_DLC];
} can_frame_t;

typedef struct {
    uint32_t id_mask;
    uint32_t id_filter;
    bool extended;
} can_filter_t;

typedef void (*can_rx_callback_t)(const can_frame_t* frame);

void can_init(can_speed_t speed, can_mode_t mode);
bool can_send_frame(const can_frame_t* frame);
bool can_receive_frame(can_frame_t* frame);
bool can_add_filter(const can_filter_t* filter);
void can_remove_filter(uint8_t filter_num);
void can_register_rx_callback(can_rx_callback_t callback);
void can_set_mode(can_mode_t mode);
uint32_t can_get_error_status(void);
void can_clear_error_status(void);
bool can_is_bus_off(void);
void can_recover_from_bus_off(void);
uint32_t can_get_rx_error_counter(void);
uint32_t can_get_tx_error_counter(void);
void can_enable_interrupts(void);
void can_disable_interrupts(void);

#endif /* REXUS_CAN_H */ 