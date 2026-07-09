/*
 * Power management task header for DJM-V10
 */
#ifndef __POWER_TASK_H__
#define __POWER_TASK_H__

#include "bsp_sys.h"

/* Power state machine */
typedef enum {
    POWER_STATE_OFF = 0,
    POWER_STATE_BOOTING,
    POWER_STATE_ON,
    POWER_STATE_SHUTTING_DOWN
} power_state_t;

/* Public API */
int power_task_init(void);
power_state_t power_get_state(void);

/* Called by KEY_Scan() in bsp_key.c */
void power_request_boot(void);
void power_request_shutdown_by_key(void);
void power_force_shutdown(void);

/* Called by protocol module when shutdown ACK received */
void power_shutdown_confirm(void);

#endif /* __POWER_TASK_H__ */
