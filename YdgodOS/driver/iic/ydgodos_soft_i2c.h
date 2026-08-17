#ifndef YDGODOS_SOFT_I2C_H
#define YDGODOS_SOFT_I2C_H

#include <stdint.h>

#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* STM32F103 default pins: PB6=SCL and PB7=SDA. */
#ifndef YDGODOS_SOFT_I2C_SCL_PORT
#define YDGODOS_SOFT_I2C_SCL_PORT GPIOB
#define YDGODOS_SOFT_I2C_SCL_PIN  GPIO_PIN_6
#define YDGODOS_SOFT_I2C_SDA_PORT GPIOB
#define YDGODOS_SOFT_I2C_SDA_PIN  GPIO_PIN_7
#endif

typedef enum {
    YDGODOS_SOFT_I2C_OK = 0,
    YDGODOS_SOFT_I2C_ERROR,
    YDGODOS_SOFT_I2C_NACK,
    YDGODOS_SOFT_I2C_BUS_BUSY,
    YDGODOS_SOFT_I2C_TIMEOUT
} ydgodos_soft_i2c_status_t;

void ydgodos_soft_i2c_init(uint32_t speed_hz);
ydgodos_soft_i2c_status_t ydgodos_soft_i2c_write(uint8_t address,
                                                 const uint8_t *data,
                                                 uint16_t size);
ydgodos_soft_i2c_status_t ydgodos_soft_i2c_read(uint8_t address,
                                                uint8_t *data,
                                                uint16_t size);
ydgodos_soft_i2c_status_t ydgodos_soft_i2c_reg_write(uint8_t address,
                                                     uint8_t reg,
                                                     const uint8_t *data,
                                                     uint16_t size);
ydgodos_soft_i2c_status_t ydgodos_soft_i2c_reg_read(uint8_t address,
                                                    uint8_t reg,
                                                    uint8_t *data,
                                                    uint16_t size);
uint8_t ydgodos_soft_i2c_check_device(uint8_t address);

/* Compatibility aliases for the misspelled API shipped in the old file. */
typedef ydgodos_soft_i2c_status_t soft_i2c_err_t;
#define SOFT_I2C_OK                  YDGODOS_SOFT_I2C_OK
#define SOFT_I2C_ERR                 YDGODOS_SOFT_I2C_ERROR
#define SOFT_I2C_NACK                YDGODOS_SOFT_I2C_NACK
#define ydfly_soft_i2c_init          ydgodos_soft_i2c_init
#define ydfly_soft_i2c_write         ydgodos_soft_i2c_write
#define ydfly_soft_i2c_read          ydgodos_soft_i2c_read
#define ydfly_soft_i2c_reg_write     ydgodos_soft_i2c_reg_write
#define ydfly_soft_i2c_reg_read      ydgodos_soft_i2c_reg_read
#define ydfly_soft_i2c_check_device  ydgodos_soft_i2c_check_device

#ifdef __cplusplus
}
#endif

#endif /* YDGODOS_SOFT_I2C_H */
