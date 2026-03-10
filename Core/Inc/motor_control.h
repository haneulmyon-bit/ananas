#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#include <stdint.h>

typedef enum
{
  MOTOR_DIR_STOP = 0U,
  MOTOR_DIR_FORWARD = 1U,
  MOTOR_DIR_REVERSE = 2U
} motor_direction_t;

enum
{
  MOTOR_FAULT_NONE = 0x0000U,
  MOTOR_FAULT_BAD_CONFIG = 0x0001U,
  MOTOR_FAULT_PWM_START = 0x0002U
};

typedef struct
{
  int8_t signed_speed_pct;
  uint8_t duty_pct;
  motor_direction_t direction;
  uint16_t fault_flags;
} motor_status_t;

typedef struct
{
  TIM_HandleTypeDef *htim_pwm;
  uint32_t tim_channel;
  GPIO_TypeDef *in1_port;
  uint16_t in1_pin;
  GPIO_TypeDef *in2_port;
  uint16_t in2_pin;
  uint8_t ramp_step_pct;
  uint16_t ramp_interval_ms;
  uint16_t dir_change_delay_ms;
} motor_control_config_t;

typedef struct
{
  void *ctx;
  void (*set_speed_pct)(void *ctx, int8_t speed_pct);
  void (*stop)(void *ctx);
  void (*get_status)(void *ctx, motor_status_t *out_status);
} motor_control_iface_t;

typedef struct
{
  motor_control_config_t cfg;
  motor_status_t status;
  int8_t requested_speed_pct;
  uint8_t applied_duty_pct;
  motor_direction_t pending_direction;
  uint32_t pending_direction_deadline_ms;
  uint32_t last_ramp_tick_ms;
} motor_control_t;

HAL_StatusTypeDef motor_control_init(motor_control_t *self, const motor_control_config_t *cfg);

void motor_control_tick(motor_control_t *self, uint32_t now_ms);

void motor_control_set_speed_pct(motor_control_t *self, int8_t speed_pct);

void motor_control_stop(motor_control_t *self);

void motor_control_get_status(const motor_control_t *self, motor_status_t *out_status);

motor_control_iface_t motor_control_get_iface(motor_control_t *self);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_CONTROL_H */
