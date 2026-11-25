/* Compatibility API for the unchanged F103 application and HAL sources.
 * Registers and core definitions always come from the STM32F411 CMSIS device.
 * Only the SPL subset used by this project is supported; this is not an F1 SDK.
 */
#ifndef BSP_STM32F10X_COMPAT_H
#define BSP_STM32F10X_COMPAT_H

#include "stm32f4xx.h"
#include "stm32f10x_conf.h"

typedef enum { GPIO_Speed_10MHz = 1, GPIO_Speed_2MHz, GPIO_Speed_50MHz } GPIOSpeed_TypeDef;
typedef enum {
    GPIO_Mode_AIN = 0, GPIO_Mode_IN_FLOATING = 0x04,
    GPIO_Mode_IPD = 0x28, GPIO_Mode_IPU = 0x48,
    GPIO_Mode_Out_OD = 0x14, GPIO_Mode_Out_PP = 0x10,
    GPIO_Mode_AF_OD = 0x1c, GPIO_Mode_AF_PP = 0x18
} GPIOMode_TypeDef;
typedef struct {
    uint16_t GPIO_Pin;
    GPIOSpeed_TypeDef GPIO_Speed;
    GPIOMode_TypeDef GPIO_Mode;
} GPIO_InitTypeDef;

#define GPIO_Pin_6   (1U << 6)
#define GPIO_Pin_7   (1U << 7)
#define GPIO_Pin_9   (1U << 9)
#define GPIO_Pin_13  (1U << 13)

/* Legacy F1 masks: translated to F4 AHB1/APB2 registers by the BSP. */
#define RCC_APB2Periph_AFIO    (1UL << 0)
#define RCC_APB2Periph_GPIOA   (1UL << 2)
#define RCC_APB2Periph_GPIOB   (1UL << 3)
#define RCC_APB2Periph_GPIOC   (1UL << 4)
#define RCC_APB2Periph_USART1  (1UL << 14)
#define RCC_APB1Periph_I2C1    RCC_APB1ENR_I2C1EN
/* main.c requests the old three-bit group. F411 uses four preemption bits. */
#define NVIC_PriorityGroup_3   0x400UL

void NVIC_PriorityGroupConfig(uint32_t group);
void RCC_APB2PeriphClockCmd(uint32_t mask, FunctionalState state);
void RCC_APB1PeriphClockCmd(uint32_t mask, FunctionalState state);
void GPIO_StructInit(GPIO_InitTypeDef *init);
void GPIO_Init(GPIO_TypeDef *gpio, GPIO_InitTypeDef *init);
void GPIO_SetBits(GPIO_TypeDef *gpio, uint16_t pins);
void GPIO_ResetBits(GPIO_TypeDef *gpio, uint16_t pins);

typedef struct {
    uint32_t USART_BaudRate;
    uint16_t USART_WordLength, USART_StopBits, USART_Parity;
    uint16_t USART_Mode, USART_HardwareFlowControl;
} USART_InitTypeDef;
#define USART_WordLength_8b              0U
#define USART_StopBits_1                 0U
#define USART_Parity_No                  0U
#define USART_HardwareFlowControl_None   0U
#define USART_Mode_Tx                    USART_CR1_TE
#define USART_FLAG_TXE                   USART_SR_TXE
#define USART_FLAG_TC                    USART_SR_TC
void USART_DeInit(USART_TypeDef *uart);
void USART_StructInit(USART_InitTypeDef *init);
void USART_Init(USART_TypeDef *uart, USART_InitTypeDef *init);
void USART_Cmd(USART_TypeDef *uart, FunctionalState state);
FlagStatus USART_GetFlagStatus(USART_TypeDef *uart, uint16_t flag);
void USART_SendData(USART_TypeDef *uart, uint16_t data);

typedef struct {
    uint32_t I2C_ClockSpeed;
    uint16_t I2C_Mode, I2C_DutyCycle, I2C_OwnAddress1;
    uint16_t I2C_Ack, I2C_AcknowledgedAddress;
} I2C_InitTypeDef;
#define I2C_Mode_I2C                     0U
#define I2C_DutyCycle_2                  0U
#define I2C_Ack_Enable                   I2C_CR1_ACK
#define I2C_AcknowledgedAddress_7bit      (1U << 14)
#define I2C_Direction_Transmitter         0U
#define I2C_Direction_Receiver            1U
/* SPL event encoding: SR2 in bits 23:16, SR1 in bits 15:0. */
#define I2C_FLAG_BUSY                                 0x00020000UL
#define I2C_EVENT_MASTER_MODE_SELECT                  0x00030001UL
#define I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED     0x00070082UL
#define I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED        0x00030002UL
#define I2C_EVENT_MASTER_BYTE_TRANSMITTED              0x00070084UL
#define I2C_EVENT_MASTER_BYTE_RECEIVED                 0x00030040UL
void I2C_DeInit(I2C_TypeDef *i2c);
void I2C_StructInit(I2C_InitTypeDef *init);
void I2C_Init(I2C_TypeDef *i2c, I2C_InitTypeDef *init);
void I2C_Cmd(I2C_TypeDef *i2c, FunctionalState state);
void I2C_SoftwareResetCmd(I2C_TypeDef *i2c, FunctionalState state);
void I2C_GenerateSTART(I2C_TypeDef *i2c, FunctionalState state);
void I2C_GenerateSTOP(I2C_TypeDef *i2c, FunctionalState state);
void I2C_AcknowledgeConfig(I2C_TypeDef *i2c, FunctionalState state);
void I2C_Send7bitAddress(I2C_TypeDef *i2c, uint8_t address, uint8_t direction);
void I2C_SendData(I2C_TypeDef *i2c, uint8_t data);
uint8_t I2C_ReceiveData(I2C_TypeDef *i2c);
FlagStatus I2C_GetFlagStatus(I2C_TypeDef *i2c, uint32_t flag);
ErrorStatus I2C_CheckEvent(I2C_TypeDef *i2c, uint32_t event);

#endif
