#include "can.h"
#include "../arch/x86/io.h"
#include "../arch/x86/isr.h"
#include <string.h>

#ifdef REXUS_ARCH_ARM
#include "stm32f4xx.h"
#endif

static struct {
    can_frame_t rx_buffer[CAN_RX_BUFFER_SIZE];
    uint32_t rx_head;
    uint32_t rx_tail;
    can_frame_t tx_buffer[CAN_TX_BUFFER_SIZE];
    uint32_t tx_head;
    uint32_t tx_tail;
    can_rx_callback_t rx_callback;
    can_filter_t filters[CAN_MAX_FILTERS];
    bool filter_used[CAN_MAX_FILTERS];
    can_mode_t current_mode;
} can_state;

static void can_process_rx_interrupt(void);
static void can_process_tx_interrupt(void);
static void can_process_error_interrupt(void);
static bool can_frame_matches_filter(const can_frame_t* frame, const can_filter_t* filter);

#ifdef REXUS_ARCH_ARM
static void can_configure_timing(can_speed_t speed) {
    uint32_t prescaler;
    switch (speed) {
        case CAN_SPEED_1M:
            prescaler = 2;
            break;
        case CAN_SPEED_500K:
            prescaler = 4;
            break;
        case CAN_SPEED_250K:
            prescaler = 8;
            break;
        case CAN_SPEED_125K:
            prescaler = 16;
            break;
        default:
            prescaler = 4;
    }
    
    CAN1->BTR = (prescaler - 1) | (3 << 16) | (2 << 20) | (3 << 24);
}

void CAN1_RX0_IRQHandler(void) {
    can_process_rx_interrupt();
}

void CAN1_TX_IRQHandler(void) {
    can_process_tx_interrupt();
}

void CAN1_SCE_IRQHandler(void) {
    can_process_error_interrupt();
}
#endif

void can_init(can_speed_t speed, can_mode_t mode) {
    memset(&can_state, 0, sizeof(can_state));
    can_state.current_mode = mode;
    
#ifdef REXUS_ARCH_ARM
    RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;
    
    CAN1->MCR = CAN_MCR_INRQ;
    while (!(CAN1->MSR & CAN_MSR_INAK));
    
    can_configure_timing(speed);
    
    switch (mode) {
        case CAN_MODE_LOOPBACK:
            CAN1->BTR |= CAN_BTR_LBKM;
            break;
        case CAN_MODE_SILENT:
            CAN1->BTR |= CAN_BTR_SILM;
            break;
        case CAN_MODE_SILENT_LOOPBACK:
            CAN1->BTR |= CAN_BTR_LBKM | CAN_BTR_SILM;
            break;
        default:
            break;
    }
    
    CAN1->MCR &= ~CAN_MCR_INRQ;
    while (CAN1->MSR & CAN_MSR_INAK);
    
    CAN1->IER = CAN_IER_FMPIE0 | CAN_IER_TMEIE | CAN_IER_ERRIE | CAN_IER_BOFIE;
    
    NVIC_EnableIRQ(CAN1_RX0_IRQn);
    NVIC_EnableIRQ(CAN1_TX_IRQn);
    NVIC_EnableIRQ(CAN1_SCE_IRQn);
#endif
}

bool can_send_frame(const can_frame_t* frame) {
    if (!frame || frame->dlc > CAN_MAX_DLC) {
        return false;
    }
    
    uint32_t next_head = (can_state.tx_head + 1) % CAN_TX_BUFFER_SIZE;
    if (next_head == can_state.tx_tail) {
        return false;
    }
    
    memcpy(&can_state.tx_buffer[can_state.tx_head], frame, sizeof(can_frame_t));
    can_state.tx_head = next_head;
    
#ifdef REXUS_ARCH_ARM
    if (CAN1->TSR & CAN_TSR_TME0) {
        can_process_tx_interrupt();
    }
#endif
    
    return true;
}

bool can_receive_frame(can_frame_t* frame) {
    if (!frame || can_state.rx_head == can_state.rx_tail) {
        return false;
    }
    
    memcpy(frame, &can_state.rx_buffer[can_state.rx_tail], sizeof(can_frame_t));
    can_state.rx_tail = (can_state.rx_tail + 1) % CAN_RX_BUFFER_SIZE;
    
    return true;
}

bool can_add_filter(const can_filter_t* filter) {
    if (!filter) {
        return false;
    }
    
    for (int i = 0; i < CAN_MAX_FILTERS; i++) {
        if (!can_state.filter_used[i]) {
            memcpy(&can_state.filters[i], filter, sizeof(can_filter_t));
            can_state.filter_used[i] = true;
            
#ifdef REXUS_ARCH_ARM
            CAN1->FMR |= CAN_FMR_FINIT;
            
            uint32_t filter_number = i / 2;
            uint32_t scale_bit = (i % 2) * 16;
            
            CAN1->FA1R &= ~(1 << filter_number);
            
            if (filter->extended) {
                CAN1->FS1R |= (1 << filter_number);
                CAN1->sFilterRegister[filter_number].FR1 = (filter->id_filter << scale_bit);
                CAN1->sFilterRegister[filter_number].FR2 = (filter->id_mask << scale_bit);
            } else {
                CAN1->FS1R &= ~(1 << filter_number);
                CAN1->sFilterRegister[filter_number].FR1 &= ~(0xFFFF << scale_bit);
                CAN1->sFilterRegister[filter_number].FR1 |= ((filter->id_filter & 0x7FF) << scale_bit);
                CAN1->sFilterRegister[filter_number].FR2 &= ~(0xFFFF << scale_bit);
                CAN1->sFilterRegister[filter_number].FR2 |= ((filter->id_mask & 0x7FF) << scale_bit);
            }
            
            CAN1->FA1R |= (1 << filter_number);
            CAN1->FMR &= ~CAN_FMR_FINIT;
#endif
            return true;
        }
    }
    
    return false;
}

void can_remove_filter(uint8_t filter_num) {
    if (filter_num >= CAN_MAX_FILTERS) {
        return;
    }
    
    can_state.filter_used[filter_num] = false;
    
#ifdef REXUS_ARCH_ARM
    uint32_t filter_number = filter_num / 2;
    CAN1->FMR |= CAN_FMR_FINIT;
    CAN1->FA1R &= ~(1 << filter_number);
    CAN1->FMR &= ~CAN_FMR_FINIT;
#endif
}

void can_register_rx_callback(can_rx_callback_t callback) {
    can_state.rx_callback = callback;
}

void can_set_mode(can_mode_t mode) {
    if (mode == can_state.current_mode) {
        return;
    }
    
#ifdef REXUS_ARCH_ARM
    CAN1->MCR |= CAN_MCR_INRQ;
    while (!(CAN1->MSR & CAN_MSR_INAK));
    
    CAN1->BTR &= ~(CAN_BTR_SILM | CAN_BTR_LBKM);
    
    switch (mode) {
        case CAN_MODE_LOOPBACK:
            CAN1->BTR |= CAN_BTR_LBKM;
            break;
        case CAN_MODE_SILENT:
            CAN1->BTR |= CAN_BTR_SILM;
            break;
        case CAN_MODE_SILENT_LOOPBACK:
            CAN1->BTR |= CAN_BTR_LBKM | CAN_BTR_SILM;
            break;
        default:
            break;
    }
    
    CAN1->MCR &= ~CAN_MCR_INRQ;
    while (CAN1->MSR & CAN_MSR_INAK);
#endif
    
    can_state.current_mode = mode;
}

uint32_t can_get_error_status(void) {
#ifdef REXUS_ARCH_ARM
    return CAN1->ESR;
#else
    return 0;
#endif
}

void can_clear_error_status(void) {
#ifdef REXUS_ARCH_ARM
    CAN1->ESR &= CAN_ESR_LEC;
#endif
}

bool can_is_bus_off(void) {
#ifdef REXUS_ARCH_ARM
    return (CAN1->ESR & CAN_ESR_BOFF) != 0;
#else
    return false;
#endif
}

void can_recover_from_bus_off(void) {
#ifdef REXUS_ARCH_ARM
    CAN1->MCR &= ~CAN_MCR_INRQ;
    while (CAN1->MSR & CAN_MSR_INAK);
#endif
}

uint32_t can_get_rx_error_counter(void) {
#ifdef REXUS_ARCH_ARM
    return (CAN1->ESR & CAN_ESR_REC) >> CAN_ESR_REC_Pos;
#else
    return 0;
#endif
}

uint32_t can_get_tx_error_counter(void) {
#ifdef REXUS_ARCH_ARM
    return (CAN1->ESR & CAN_ESR_TEC) >> CAN_ESR_TEC_Pos;
#else
    return 0;
#endif
}

void can_enable_interrupts(void) {
#ifdef REXUS_ARCH_ARM
    CAN1->IER = CAN_IER_FMPIE0 | CAN_IER_TMEIE | CAN_IER_ERRIE | CAN_IER_BOFIE;
#endif
}

void can_disable_interrupts(void) {
#ifdef REXUS_ARCH_ARM
    CAN1->IER = 0;
#endif
}

static void can_process_rx_interrupt(void) {
#ifdef REXUS_ARCH_ARM
    while (CAN1->RF0R & CAN_RF0R_FMP0) {
        can_frame_t frame;
        uint32_t rir = CAN1->sFIFOMailBox[0].RIR;
        
        frame.extended = (rir & CAN_RI0R_IDE) != 0;
        frame.rtr = (rir & CAN_RI0R_RTR) != 0;
        frame.id = frame.extended ? (rir >> 3) : (rir >> 21);
        
        frame.dlc = CAN1->sFIFOMailBox[0].RDTR & CAN_RDT0R_DLC;
        
        uint32_t rdlr = CAN1->sFIFOMailBox[0].RDLR;
        uint32_t rdhr = CAN1->sFIFOMailBox[0].RDHR;
        
        frame.data[0] = rdlr;
        frame.data[1] = rdlr >> 8;
        frame.data[2] = rdlr >> 16;
        frame.data[3] = rdlr >> 24;
        frame.data[4] = rdhr;
        frame.data[5] = rdhr >> 8;
        frame.data[6] = rdhr >> 16;
        frame.data[7] = rdhr >> 24;
        
        CAN1->RF0R |= CAN_RF0R_RFOM0;
        
        bool frame_accepted = false;
        for (int i = 0; i < CAN_MAX_FILTERS && !frame_accepted; i++) {
            if (can_state.filter_used[i] && can_frame_matches_filter(&frame, &can_state.filters[i])) {
                frame_accepted = true;
            }
        }
        
        if (frame_accepted) {
            uint32_t next_head = (can_state.rx_head + 1) % CAN_RX_BUFFER_SIZE;
            if (next_head != can_state.rx_tail) {
                memcpy(&can_state.rx_buffer[can_state.rx_head], &frame, sizeof(can_frame_t));
                can_state.rx_head = next_head;
                
                if (can_state.rx_callback) {
                    can_state.rx_callback(&frame);
                }
            }
        }
    }
#endif
}

static void can_process_tx_interrupt(void) {
#ifdef REXUS_ARCH_ARM
    while (can_state.tx_head != can_state.tx_tail && (CAN1->TSR & CAN_TSR_TME0)) {
        const can_frame_t* frame = &can_state.tx_buffer[can_state.tx_tail];
        
        uint32_t rir = frame->extended ? (frame->id << 3) : (frame->id << 21);
        rir |= frame->extended ? CAN_TI0R_IDE : 0;
        rir |= frame->rtr ? CAN_TI0R_RTR : 0;
        
        CAN1->sTxMailBox[0].TIR = rir;
        CAN1->sTxMailBox[0].TDTR = frame->dlc;
        
        CAN1->sTxMailBox[0].TDLR = frame->data[0] |
                                  (frame->data[1] << 8) |
                                  (frame->data[2] << 16) |
                                  (frame->data[3] << 24);
        
        CAN1->sTxMailBox[0].TDHR = frame->data[4] |
                                  (frame->data[5] << 8) |
                                  (frame->data[6] << 16) |
                                  (frame->data[7] << 24);
        
        CAN1->sTxMailBox[0].TIR |= CAN_TI0R_TXRQ;
        
        can_state.tx_tail = (can_state.tx_tail + 1) % CAN_TX_BUFFER_SIZE;
    }
#endif
}

static void can_process_error_interrupt(void) {
#ifdef REXUS_ARCH_ARM
    if (CAN1->ESR & CAN_ESR_BOFF) {
        CAN1->MCR |= CAN_MCR_INRQ;
    }
#endif
}

static bool can_frame_matches_filter(const can_frame_t* frame, const can_filter_t* filter) {
    if (!frame || !filter || frame->extended != filter->extended) {
        return false;
    }
    
    uint32_t frame_id = frame->id;
    uint32_t filter_id = filter->id_filter;
    uint32_t mask = filter->id_mask;
    
    return (frame_id & mask) == (filter_id & mask);
} 