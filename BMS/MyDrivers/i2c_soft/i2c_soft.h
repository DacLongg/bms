#ifndef __I2C_Soft_H__
#define __I2C_Soft_H__

#ifdef __cplusplus
extern "C" {
#endif
  
#include "stm32l0xx_hal.h"
typedef uint8_t Std_ReturnType;

#ifndef E_OK
#define E_OK ((Std_ReturnType)0U)
#endif

#ifndef E_NOT_OK
#define E_NOT_OK ((Std_ReturnType)1U)
#endif

#define I2C_PORT                GPIOB
#define I2C_SDA_PIN             GPIO_PIN_7
#define I2C_SCL_PIN             GPIO_PIN_6

#define  I2C_SDA_ON             HAL_GPIO_WritePin(I2C_PORT, I2C_SDA_PIN, GPIO_PIN_SET);
#define  I2C_SDA_OFF            HAL_GPIO_WritePin(I2C_PORT, I2C_SDA_PIN, GPIO_PIN_RESET);
#define  I2C_SDA_READ           HAL_GPIO_ReadPin(I2C_PORT, I2C_SDA_PIN)

#define  I2C_SCL_ON             HAL_GPIO_WritePin(I2C_PORT, I2C_SCL_PIN, GPIO_PIN_SET);
#define  I2C_SCL_OFF            HAL_GPIO_WritePin(I2C_PORT, I2C_SCL_PIN, GPIO_PIN_RESET);

#define  I2C_TIME_DELAY         10


void I2C_Soft_Init(void);
void I2C_Soft_Start(void);
void I2C_Soft_Stop(void);

Std_ReturnType I2C_Soft_WriteByte(uint8_t byte);  
Std_ReturnType I2C_Soft_ReadByteRegister(uint8_t address, uint8_t add_reg, uint8_t *byte);
uint8_t I2C_Soft_ReadByte(uint8_t ack);

Std_ReturnType I2C_Soft_WriteData(uint8_t address, uint8_t *data, uint16_t len);
Std_ReturnType I2c_Soft_ReadData(uint8_t address, uint8_t *data, uint16_t len);

Std_ReturnType I2C_Soft_WriteDataFromAddress(uint8_t address, uint8_t add_reg, uint8_t *data, uint16_t len);
Std_ReturnType I2C_Soft_ReadDataFromAddress(uint8_t address, uint8_t add_reg, uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif
#endif
