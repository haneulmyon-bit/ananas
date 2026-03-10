#include "motor_control.h"

#include <string.h>

static int8_t motor_clamp_speed(int8_t speed_pct)
{
  if (speed_pct > 100)
  {
    return 100;
  }
  if (speed_pct < -100)
  {
    return -100;
  }
  return speed_pct;
}

static uint8_t motor_abs_speed(int8_t speed_pct)
{
  if (speed_pct < 0)
  {
    return (uint8_t)(-speed_pct);
  }
  return (uint8_t)speed_pct;
}

static motor_direction_t motor_direction_from_speed(int8_t speed_pct)
{
  if (speed_pct > 0)
  {
    return MOTOR_DIR_FORWARD;
  }
  if (speed_pct < 0)
  {
    return MOTOR_DIR_REVERSE;
  }
  return MOTOR_DIR_STOP;
}

static void motor_apply_direction(motor_control_t *self, motor_direction_t direction)
{
  if ((self == NULL) || (self->cfg.in1_port == NULL) || (self->cfg.in2_port == NULL))
  {
    return;
  }

  switch (direction)
  {
    case MOTOR_DIR_FORWARD:
      HAL_GPIO_WritePin(self->cfg.in1_port, self->cfg.in1_pin, GPIO_PIN_SET);
      HAL_GPIO_WritePin(self->cfg.in2_port, self->cfg.in2_pin, GPIO_PIN_RESET);
      break;

    case MOTOR_DIR_REVERSE:
      HAL_GPIO_WritePin(self->cfg.in1_port, self->cfg.in1_pin, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(self->cfg.in2_port, self->cfg.in2_pin, GPIO_PIN_SET);
      break;

    case MOTOR_DIR_STOP:
    default:
      HAL_GPIO_WritePin(self->cfg.in1_port, self->cfg.in1_pin, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(self->cfg.in2_port, self->cfg.in2_pin, GPIO_PIN_RESET);
      break;
  }
}

static void motor_apply_duty(motor_control_t *self, uint8_t duty_pct)
{
  uint32_t arr;
  uint32_t ccr;

  if ((self == NULL) || (self->cfg.htim_pwm == NULL))
  {
    return;
  }

  if (duty_pct > 100U)
  {
    duty_pct = 100U;
  }

  arr = __HAL_TIM_GET_AUTORELOAD(self->cfg.htim_pwm);
  ccr = ((arr + 1U) * (uint32_t)duty_pct) / 100U;
  if (ccr > arr)
  {
    ccr = arr;
  }

  __HAL_TIM_SET_COMPARE(self->cfg.htim_pwm, self->cfg.tim_channel, ccr);
}

static void motor_update_signed_speed(motor_control_t *self)
{
  if (self == NULL)
  {
    return;
  }

  self->status.duty_pct = self->applied_duty_pct;
  switch (self->status.direction)
  {
    case MOTOR_DIR_FORWARD:
      self->status.signed_speed_pct = (int8_t)self->applied_duty_pct;
      break;

    case MOTOR_DIR_REVERSE:
      self->status.signed_speed_pct = (int8_t)(-(int8_t)self->applied_duty_pct);
      break;

    case MOTOR_DIR_STOP:
    default:
      self->status.signed_speed_pct = 0;
      break;
  }
}

HAL_StatusTypeDef motor_control_init(motor_control_t *self, const motor_control_config_t *cfg)
{
  if ((self == NULL) || (cfg == NULL) || (cfg->htim_pwm == NULL) || (cfg->in1_port == NULL) ||
      (cfg->in2_port == NULL))
  {
    if (self != NULL)
    {
      (void)memset(self, 0, sizeof(*self));
      self->status.fault_flags = MOTOR_FAULT_BAD_CONFIG;
    }
    return HAL_ERROR;
  }

  (void)memset(self, 0, sizeof(*self));
  self->cfg = *cfg;
  if (self->cfg.ramp_step_pct == 0U)
  {
    self->cfg.ramp_step_pct = 5U;
  }
  if (self->cfg.ramp_interval_ms == 0U)
  {
    self->cfg.ramp_interval_ms = 20U;
  }
  if (self->cfg.dir_change_delay_ms == 0U)
  {
    self->cfg.dir_change_delay_ms = 100U;
  }

  self->status.direction = MOTOR_DIR_STOP;
  self->pending_direction = MOTOR_DIR_STOP;

  motor_apply_direction(self, MOTOR_DIR_STOP);
  motor_apply_duty(self, 0U);

  if (HAL_TIM_PWM_Start(self->cfg.htim_pwm, self->cfg.tim_channel) != HAL_OK)
  {
    self->status.fault_flags |= MOTOR_FAULT_PWM_START;
    return HAL_ERROR;
  }

  return HAL_OK;
}

void motor_control_set_speed_pct(motor_control_t *self, int8_t speed_pct)
{
  if (self == NULL)
  {
    return;
  }

  self->requested_speed_pct = motor_clamp_speed(speed_pct);
}

void motor_control_stop(motor_control_t *self)
{
  if (self == NULL)
  {
    return;
  }

  self->requested_speed_pct = 0;
}

void motor_control_tick(motor_control_t *self, uint32_t now_ms)
{
  motor_direction_t requested_dir;
  uint8_t target_mag = 0U;
  uint8_t requested_mag;

  if (self == NULL)
  {
    return;
  }

  requested_dir = motor_direction_from_speed(self->requested_speed_pct);
  requested_mag = motor_abs_speed(self->requested_speed_pct);

  if (self->pending_direction != MOTOR_DIR_STOP)
  {
    if ((int32_t)(now_ms - self->pending_direction_deadline_ms) >= 0)
    {
      motor_apply_direction(self, self->pending_direction);
      self->status.direction = self->pending_direction;
      self->pending_direction = MOTOR_DIR_STOP;
    }
    else
    {
      return;
    }
  }

  if ((requested_dir != MOTOR_DIR_STOP) && (self->status.direction != MOTOR_DIR_STOP) &&
      (requested_dir != self->status.direction))
  {
    if (self->applied_duty_pct > 0U)
    {
      self->applied_duty_pct = 0U;
      motor_apply_duty(self, 0U);
      self->status.direction = MOTOR_DIR_STOP;
      self->pending_direction = requested_dir;
      self->pending_direction_deadline_ms = now_ms + self->cfg.dir_change_delay_ms;
      motor_apply_direction(self, MOTOR_DIR_STOP);
      motor_update_signed_speed(self);
      return;
    }

    self->status.direction = MOTOR_DIR_STOP;
    self->pending_direction = requested_dir;
    self->pending_direction_deadline_ms = now_ms + self->cfg.dir_change_delay_ms;
    motor_apply_direction(self, MOTOR_DIR_STOP);
    motor_update_signed_speed(self);
    return;
  }

  if ((self->status.direction == MOTOR_DIR_STOP) && (requested_dir != MOTOR_DIR_STOP))
  {
    motor_apply_direction(self, requested_dir);
    self->status.direction = requested_dir;
  }

  if (requested_dir == self->status.direction)
  {
    target_mag = requested_mag;
  }

  if ((uint32_t)(now_ms - self->last_ramp_tick_ms) >= self->cfg.ramp_interval_ms)
  {
    self->last_ramp_tick_ms = now_ms;

    if (self->applied_duty_pct < target_mag)
    {
      uint16_t next = (uint16_t)self->applied_duty_pct + self->cfg.ramp_step_pct;
      self->applied_duty_pct = (next > target_mag) ? target_mag : (uint8_t)next;
      motor_apply_duty(self, self->applied_duty_pct);
    }
    else if (self->applied_duty_pct > target_mag)
    {
      if (self->applied_duty_pct > self->cfg.ramp_step_pct)
      {
        self->applied_duty_pct = (uint8_t)(self->applied_duty_pct - self->cfg.ramp_step_pct);
      }
      else
      {
        self->applied_duty_pct = 0U;
      }

      if (self->applied_duty_pct < target_mag)
      {
        self->applied_duty_pct = target_mag;
      }
      motor_apply_duty(self, self->applied_duty_pct);
    }
  }

  if ((self->applied_duty_pct == 0U) && (requested_dir == MOTOR_DIR_STOP) &&
      (self->status.direction != MOTOR_DIR_STOP))
  {
    self->status.direction = MOTOR_DIR_STOP;
    motor_apply_direction(self, MOTOR_DIR_STOP);
  }

  motor_update_signed_speed(self);
}

void motor_control_get_status(const motor_control_t *self, motor_status_t *out_status)
{
  if ((self == NULL) || (out_status == NULL))
  {
    return;
  }

  *out_status = self->status;
}

static void motor_iface_set_speed(void *ctx, int8_t speed_pct)
{
  motor_control_set_speed_pct((motor_control_t *)ctx, speed_pct);
}

static void motor_iface_stop(void *ctx)
{
  motor_control_stop((motor_control_t *)ctx);
}

static void motor_iface_get_status(void *ctx, motor_status_t *out_status)
{
  motor_control_get_status((const motor_control_t *)ctx, out_status);
}

motor_control_iface_t motor_control_get_iface(motor_control_t *self)
{
  motor_control_iface_t iface = {0};

  iface.ctx = self;
  iface.set_speed_pct = motor_iface_set_speed;
  iface.stop = motor_iface_stop;
  iface.get_status = motor_iface_get_status;
  return iface;
}
