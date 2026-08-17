#include "ydgodos_soft_i2c.h"

#define I2C_MIN_SPEED_HZ       10000UL
#define I2C_MAX_SPEED_HZ      400000UL
#define I2C_STRETCH_TIMEOUT_US   1000UL

static uint32_t half_period_cycles;
static uint32_t stretch_timeout_cycles;

static void scl_release(void)
{
    YDGODOS_SOFT_I2C_SCL_PORT->BSRR = YDGODOS_SOFT_I2C_SCL_PIN;
}

static void scl_low(void)
{
    YDGODOS_SOFT_I2C_SCL_PORT->BRR = YDGODOS_SOFT_I2C_SCL_PIN;
}

static void sda_release(void)
{
    YDGODOS_SOFT_I2C_SDA_PORT->BSRR = YDGODOS_SOFT_I2C_SDA_PIN;
}

static void sda_low(void)
{
    YDGODOS_SOFT_I2C_SDA_PORT->BRR = YDGODOS_SOFT_I2C_SDA_PIN;
}

static uint8_t scl_read(void)
{
    return ((YDGODOS_SOFT_I2C_SCL_PORT->IDR & YDGODOS_SOFT_I2C_SCL_PIN) != 0U) ? 1U : 0U;
}

static uint8_t sda_read(void)
{
    return ((YDGODOS_SOFT_I2C_SDA_PORT->IDR & YDGODOS_SOFT_I2C_SDA_PIN) != 0U) ? 1U : 0U;
}

static void delay_half_period(void)
{
    uint32_t start = DWT->CYCCNT;
    while ((uint32_t)(DWT->CYCCNT - start) < half_period_cycles) {
        __NOP();
    }
}

static ydgodos_soft_i2c_status_t clock_high(void)
{
    uint32_t start;

    scl_release();
    start = DWT->CYCCNT;
    while (scl_read() == 0U) {
        if ((uint32_t)(DWT->CYCCNT - start) >= stretch_timeout_cycles) {
            return YDGODOS_SOFT_I2C_TIMEOUT;
        }
    }
    delay_half_period();
    return YDGODOS_SOFT_I2C_OK;
}

static void stop_condition(void)
{
    sda_low();
    scl_low();
    delay_half_period();
    (void)clock_high();
    sda_release();
    delay_half_period();
}

static void recover_bus(void)
{
    uint8_t pulse;

    sda_release();
    for (pulse = 0U; (pulse < 9U) && (sda_read() == 0U); pulse++) {
        scl_low();
        delay_half_period();
        (void)clock_high();
    }
    stop_condition();
}

static ydgodos_soft_i2c_status_t start_condition(void)
{
    ydgodos_soft_i2c_status_t status;

    sda_release();
    status = clock_high();
    if (status != YDGODOS_SOFT_I2C_OK) {
        return status;
    }
    if (sda_read() == 0U) {
        recover_bus();
        if (sda_read() == 0U) {
            return YDGODOS_SOFT_I2C_BUS_BUSY;
        }
        status = clock_high();
        if (status != YDGODOS_SOFT_I2C_OK) {
            return status;
        }
    }
    sda_low();
    delay_half_period();
    scl_low();
    delay_half_period();
    return YDGODOS_SOFT_I2C_OK;
}

static ydgodos_soft_i2c_status_t write_byte(uint8_t value)
{
    uint8_t bit;
    ydgodos_soft_i2c_status_t status;

    for (bit = 0U; bit < 8U; bit++) {
        if ((value & 0x80U) != 0U) {
            sda_release();
        } else {
            sda_low();
        }
        delay_half_period();
        status = clock_high();
        if (status != YDGODOS_SOFT_I2C_OK) {
            scl_low();
            return status;
        }
        scl_low();
        value <<= 1;
    }

    sda_release();
    delay_half_period();
    status = clock_high();
    if (status != YDGODOS_SOFT_I2C_OK) {
        scl_low();
        return status;
    }
    status = (sda_read() == 0U) ? YDGODOS_SOFT_I2C_OK : YDGODOS_SOFT_I2C_NACK;
    scl_low();
    delay_half_period();
    return status;
}

static ydgodos_soft_i2c_status_t read_byte(uint8_t *value, uint8_t ack)
{
    uint8_t bit;
    uint8_t received = 0U;
    ydgodos_soft_i2c_status_t status;

    sda_release();
    for (bit = 0U; bit < 8U; bit++) {
        received <<= 1;
        status = clock_high();
        if (status != YDGODOS_SOFT_I2C_OK) {
            scl_low();
            return status;
        }
        if (sda_read() != 0U) {
            received |= 1U;
        }
        scl_low();
        delay_half_period();
    }

    if (ack != 0U) {
        sda_low();
    }
    status = clock_high();
    scl_low();
    sda_release();
    delay_half_period();
    *value = received;
    return status;
}

static ydgodos_soft_i2c_status_t begin_address(uint8_t address, uint8_t read)
{
    ydgodos_soft_i2c_status_t status = start_condition();
    if (status == YDGODOS_SOFT_I2C_OK) {
        status = write_byte((uint8_t)((address << 1) | (read & 1U)));
    }
    return status;
}

void ydgodos_soft_i2c_init(uint32_t speed_hz)
{
    GPIO_InitTypeDef gpio = {0};
    uint32_t core_clock;

    __HAL_RCC_GPIOB_CLK_ENABLE();

    if (speed_hz < I2C_MIN_SPEED_HZ) {
        speed_hz = I2C_MIN_SPEED_HZ;
    } else if (speed_hz > I2C_MAX_SPEED_HZ) {
        speed_hz = I2C_MAX_SPEED_HZ;
    }

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    core_clock = HAL_RCC_GetHCLKFreq();
    half_period_cycles = core_clock / (speed_hz * 2U);
    if (half_period_cycles < 4U) {
        half_period_cycles = 4U;
    }
    stretch_timeout_cycles = (core_clock / 1000000U) * I2C_STRETCH_TIMEOUT_US;

    gpio.Pin = YDGODOS_SOFT_I2C_SCL_PIN | YDGODOS_SOFT_I2C_SDA_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);

    scl_release();
    sda_release();
    recover_bus();
}

ydgodos_soft_i2c_status_t ydgodos_soft_i2c_write(uint8_t address,
                                                 const uint8_t *data,
                                                 uint16_t size)
{
    uint16_t index;
    ydgodos_soft_i2c_status_t status;

    if ((address > 0x7FU) || ((data == NULL) && (size != 0U))) {
        return YDGODOS_SOFT_I2C_ERROR;
    }
    status = begin_address(address, 0U);
    for (index = 0U; (index < size) && (status == YDGODOS_SOFT_I2C_OK); index++) {
        status = write_byte(data[index]);
    }
    stop_condition();
    return status;
}

ydgodos_soft_i2c_status_t ydgodos_soft_i2c_read(uint8_t address,
                                                uint8_t *data,
                                                uint16_t size)
{
    uint16_t index;
    ydgodos_soft_i2c_status_t status;

    if ((address > 0x7FU) || (data == NULL) || (size == 0U)) {
        return YDGODOS_SOFT_I2C_ERROR;
    }
    status = begin_address(address, 1U);
    for (index = 0U; (index < size) && (status == YDGODOS_SOFT_I2C_OK); index++) {
        status = read_byte(&data[index], (index + 1U < size) ? 1U : 0U);
    }
    stop_condition();
    return status;
}

ydgodos_soft_i2c_status_t ydgodos_soft_i2c_reg_write(uint8_t address,
                                                     uint8_t reg,
                                                     const uint8_t *data,
                                                     uint16_t size)
{
    uint16_t index;
    ydgodos_soft_i2c_status_t status;

    if ((address > 0x7FU) || ((data == NULL) && (size != 0U))) {
        return YDGODOS_SOFT_I2C_ERROR;
    }
    status = begin_address(address, 0U);
    if (status == YDGODOS_SOFT_I2C_OK) {
        status = write_byte(reg);
    }
    for (index = 0U; (index < size) && (status == YDGODOS_SOFT_I2C_OK); index++) {
        status = write_byte(data[index]);
    }
    stop_condition();
    return status;
}

ydgodos_soft_i2c_status_t ydgodos_soft_i2c_reg_read(uint8_t address,
                                                    uint8_t reg,
                                                    uint8_t *data,
                                                    uint16_t size)
{
    uint16_t index;
    ydgodos_soft_i2c_status_t status;

    if ((address > 0x7FU) || (data == NULL) || (size == 0U)) {
        return YDGODOS_SOFT_I2C_ERROR;
    }
    status = begin_address(address, 0U);
    if (status == YDGODOS_SOFT_I2C_OK) {
        status = write_byte(reg);
    }
    if (status == YDGODOS_SOFT_I2C_OK) {
        status = begin_address(address, 1U);
    }
    for (index = 0U; (index < size) && (status == YDGODOS_SOFT_I2C_OK); index++) {
        status = read_byte(&data[index], (index + 1U < size) ? 1U : 0U);
    }
    stop_condition();
    return status;
}

uint8_t ydgodos_soft_i2c_check_device(uint8_t address)
{
    ydgodos_soft_i2c_status_t status;

    if (address > 0x7FU) {
        return 0U;
    }
    status = begin_address(address, 0U);
    stop_condition();
    return (status == YDGODOS_SOFT_I2C_OK) ? 1U : 0U;
}
