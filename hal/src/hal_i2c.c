/* Layer 1: Hardware Abstraction Layer - Cấu hình và giao tiếp I2C1 (SPL) */
#include "hal_i2c.h"
#include "stm32f10x.h"

#define I2C_TIMEOUT_CYCLES  50000UL

static uint32_t s_clock_speed = 100000UL;

/* Tạo độ trễ ngắn cho bus recovery */
static void i2c_delay(void)
{
    for (volatile uint32_t i = 0; i < 50; i++)
    {
        __NOP();
    }
}

void hal_i2c_bus_reset(void)
{
    GPIO_InitTypeDef gpio;

    /* Tắt I2C1 tạm thời */
    I2C_Cmd(I2C1, DISABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, DISABLE);

    /* Cấu hình PB6, PB7 thành GPIO Open-Drain để nhả bus thủ công */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    GPIO_SetBits(GPIOB, GPIO_Pin_6 | GPIO_Pin_7);
    i2c_delay();

    /* Nhấp nháy SCL 9 xung để kích hoạt slave nhả SDA nếu đang bị kẹt */
    for (int i = 0; i < 9; i++)
    {
        GPIO_ResetBits(GPIOB, GPIO_Pin_6);
        i2c_delay();
        GPIO_SetBits(GPIOB, GPIO_Pin_6);
        i2c_delay();
    }

    /* Tạo STOP condition giả lập */
    GPIO_ResetBits(GPIOB, GPIO_Pin_7);
    i2c_delay();
    GPIO_SetBits(GPIOB, GPIO_Pin_6);
    i2c_delay();
    GPIO_SetBits(GPIOB, GPIO_Pin_7);
    i2c_delay();

    /* Khởi tạo lại I2C1 */
    hal_i2c_init(s_clock_speed);
}

hal_i2c_status_t hal_i2c_init(uint32_t clock_speed)
{
    GPIO_InitTypeDef gpio;
    I2C_InitTypeDef i2c;

    if (clock_speed == 0U || clock_speed > 400000UL)
    {
        return HAL_I2C_INVALID_ARGUMENT;
    }
    s_clock_speed = clock_speed;

    /* Bật clock cho GPIOB và I2C1 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);

    /* Cấu hình PB6 (SCL) và PB7 (SDA): Alternate Function Open Drain */
    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    gpio.GPIO_Mode = GPIO_Mode_AF_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    /* Reset I2C peripheral */
    I2C_SoftwareResetCmd(I2C1, ENABLE);
    I2C_SoftwareResetCmd(I2C1, DISABLE);

    I2C_DeInit(I2C1);
    I2C_StructInit(&i2c);
    i2c.I2C_Mode = I2C_Mode_I2C;
    i2c.I2C_DutyCycle = I2C_DutyCycle_2;
    i2c.I2C_OwnAddress1 = 0x00;
    i2c.I2C_Ack = I2C_Ack_Enable;
    i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    i2c.I2C_ClockSpeed = clock_speed;

    I2C_Init(I2C1, &i2c);
    I2C_Cmd(I2C1, ENABLE);

    return HAL_I2C_OK;
}

hal_i2c_status_t hal_i2c_write(uint8_t dev_addr, uint8_t reg_addr, const uint8_t *data, uint16_t len)
{
    uint32_t timeout = I2C_TIMEOUT_CYCLES;
    uint8_t raw_addr = (uint8_t)(dev_addr << 1);

    /* 1. Chờ bus rảnh */
    while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY))
    {
        if (--timeout == 0U)
        {
            hal_i2c_bus_reset();
            return HAL_I2C_TIMEOUT;
        }
    }

    /* 2. Gửi START bit */
    I2C_GenerateSTART(I2C1, ENABLE);
    timeout = I2C_TIMEOUT_CYCLES;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT))
    {
        if (--timeout == 0U)
        {
            hal_i2c_bus_reset();
            return HAL_I2C_TIMEOUT;
        }
    }

    /* 3. Gửi địa chỉ thiết bị + cờ ghi (Transmitter) */
    I2C_Send7bitAddress(I2C1, raw_addr, I2C_Direction_Transmitter);
    timeout = I2C_TIMEOUT_CYCLES;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
    {
        if (--timeout == 0U)
        {
            I2C_GenerateSTOP(I2C1, ENABLE);
            return HAL_I2C_TIMEOUT;
        }
    }

    /* 4. Gửi địa chỉ thanh ghi */
    I2C_SendData(I2C1, reg_addr);
    timeout = I2C_TIMEOUT_CYCLES;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
    {
        if (--timeout == 0U)
        {
            I2C_GenerateSTOP(I2C1, ENABLE);
            return HAL_I2C_TIMEOUT;
        }
    }

    /* 5. Gửi dữ liệu */
    for (uint16_t i = 0; i < len; i++)
    {
        I2C_SendData(I2C1, data[i]);
        timeout = I2C_TIMEOUT_CYCLES;
        while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
        {
            if (--timeout == 0U)
            {
                I2C_GenerateSTOP(I2C1, ENABLE);
                return HAL_I2C_TIMEOUT;
            }
        }
    }

    /* 6. Gửi STOP */
    I2C_GenerateSTOP(I2C1, ENABLE);
    return HAL_I2C_OK;
}

hal_i2c_status_t hal_i2c_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    if (data == 0 || len == 0U)
    {
        return HAL_I2C_INVALID_ARGUMENT;
    }

    uint32_t timeout = I2C_TIMEOUT_CYCLES;
    uint8_t raw_addr = (uint8_t)(dev_addr << 1);

    /* 1. Chờ bus rảnh */
    while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY))
    {
        if (--timeout == 0U)
        {
            hal_i2c_bus_reset();
            return HAL_I2C_TIMEOUT;
        }
    }

    /* 2. Gửi START để ghi thanh ghi muốn đọc */
    I2C_GenerateSTART(I2C1, ENABLE);
    timeout = I2C_TIMEOUT_CYCLES;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT))
    {
        if (--timeout == 0U)
        {
            hal_i2c_bus_reset();
            return HAL_I2C_TIMEOUT;
        }
    }

    /* 3. Gửi địa chỉ slave + write */
    I2C_Send7bitAddress(I2C1, raw_addr, I2C_Direction_Transmitter);
    timeout = I2C_TIMEOUT_CYCLES;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
    {
        if (--timeout == 0U)
        {
            I2C_GenerateSTOP(I2C1, ENABLE);
            return HAL_I2C_TIMEOUT;
        }
    }

    /* 4. Gửi thanh ghi cần đọc */
    I2C_SendData(I2C1, reg_addr);
    timeout = I2C_TIMEOUT_CYCLES;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
    {
        if (--timeout == 0U)
        {
            I2C_GenerateSTOP(I2C1, ENABLE);
            return HAL_I2C_TIMEOUT;
        }
    }

    /* 5. Gửi Repeated START để chuyển sang đọc */
    I2C_GenerateSTART(I2C1, ENABLE);
    timeout = I2C_TIMEOUT_CYCLES;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT))
    {
        if (--timeout == 0U)
        {
            hal_i2c_bus_reset();
            return HAL_I2C_TIMEOUT;
        }
    }

    /* 6. Gửi địa chỉ slave + read */
    I2C_Send7bitAddress(I2C1, raw_addr, I2C_Direction_Receiver);
    timeout = I2C_TIMEOUT_CYCLES;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED))
    {
        if (--timeout == 0U)
        {
            I2C_GenerateSTOP(I2C1, ENABLE);
            return HAL_I2C_TIMEOUT;
        }
    }

    /* 7. Đọc dữ liệu */
    for (uint16_t i = 0; i < len; i++)
    {
        if (i == (len - 1U))
        {
            /* Byte cuối cùng: Tắt ACK và phát tín hiệu STOP */
            I2C_AcknowledgeConfig(I2C1, DISABLE);
            I2C_GenerateSTOP(I2C1, ENABLE);
        }

        timeout = I2C_TIMEOUT_CYCLES;
        while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_RECEIVED))
        {
            if (--timeout == 0U)
            {
                I2C_AcknowledgeConfig(I2C1, ENABLE);
                return HAL_I2C_TIMEOUT;
            }
        }
        data[i] = I2C_ReceiveData(I2C1);
    }

    /* Bật lại ACK cho các lần sau */
    I2C_AcknowledgeConfig(I2C1, ENABLE);
    return HAL_I2C_OK;
}

hal_i2c_status_t hal_i2c_write_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t val)
{
    return hal_i2c_write(dev_addr, reg_addr, &val, 1U);
}

hal_i2c_status_t hal_i2c_read_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t *val)
{
    return hal_i2c_read(dev_addr, reg_addr, val, 1U);
}
