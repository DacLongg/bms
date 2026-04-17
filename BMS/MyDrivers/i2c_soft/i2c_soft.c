#include "I2C_Soft.h"

#define I2C_PIN_INPUT            GPIO_MODE_INPUT
#define I2C_PIN_OUTPUT           GPIO_MODE_OUTPUT_PP

#define I2C_PIN_LOW              0
#define I2C_PIN_HIGH             1

#define I2C_CYCLE_TIME_OUT       3
#define I2C_COUNT_TIME_OUT       1000

void I2C_Delay(uint32_t)
{
  
}

void I2C_SDA_Setup(uint32_t direct)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = I2C_SDA_PIN;
  GPIO_InitStruct.Mode = direct;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(I2C_PORT, &GPIO_InitStruct);
}
void I2C_SCL_Setup(uint32_t direct)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = I2C_SCL_PIN;
  GPIO_InitStruct.Mode = direct;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(I2C_PORT, &GPIO_InitStruct);
}

void I2C_Soft_Init(void)
{
    I2C_SDA_Setup(I2C_PIN_OUTPUT);
    I2C_SCL_Setup(I2C_PIN_OUTPUT);
    I2C_SDA_ON;
    I2C_SCL_ON;
}

void I2C_Soft_Start(void)
{
  I2C_SDA_Setup(I2C_PIN_OUTPUT);
  I2C_SCL_Setup(I2C_PIN_OUTPUT);
  
  I2C_SDA_ON;
  I2C_SCL_ON;
  I2C_Delay(I2C_TIME_DELAY);
  
  I2C_SDA_OFF;
  I2C_Delay(I2C_TIME_DELAY);
  
  I2C_SCL_OFF;
  I2C_Delay(I2C_TIME_DELAY);
}

void I2C_Soft_Stop() {
  
  I2C_SDA_Setup(I2C_PIN_OUTPUT);
  I2C_SCL_Setup(I2C_PIN_OUTPUT);
  
  I2C_SDA_ON;
  I2C_SCL_OFF;
  I2C_Delay(I2C_TIME_DELAY);
  
  I2C_SDA_OFF;
  I2C_Delay(I2C_TIME_DELAY);
  
  I2C_SCL_ON;
  I2C_SDA_ON;
  I2C_Delay(I2C_TIME_DELAY);
}

uint8_t I2C_Soft_CheckAck(void)
{
  uint8_t ack = 0;
  I2C_SDA_Setup(I2C_PIN_INPUT);
  
  I2C_SCL_ON;
  I2C_Delay(I2C_TIME_DELAY);
 
  ack = I2C_SDA_READ;
  I2C_Delay(I2C_TIME_DELAY);
  
  I2C_SCL_OFF;
  I2C_Delay(I2C_TIME_DELAY);
 
  return (~ack)&0x01;
}

Std_ReturnType I2C_Soft_WriteByte(uint8_t byte)
{
  uint16_t CountTime = 0;
  I2C_SDA_Setup(I2C_PIN_OUTPUT);
  I2C_SCL_Setup(I2C_PIN_OUTPUT);
  
  for(uint8_t CountBits = 0; CountBits < 8; CountBits++)
  {
    if((byte & 0x80))
    {
      I2C_SDA_ON;
    }
    else
    {
      I2C_SDA_OFF;
    }
    
    I2C_SCL_ON;
    I2C_Delay(I2C_TIME_DELAY);
    
    I2C_SCL_OFF;
    byte <<= 1;
    if(CountBits != 7)
    {
        I2C_Delay(I2C_TIME_DELAY);
    }
  }
  
  I2C_SDA_ON;
  I2C_SDA_Setup(I2C_PIN_INPUT);

  while(I2C_SDA_READ == I2C_PIN_HIGH)
  {
      if(++ CountTime == I2C_COUNT_TIME_OUT)
      {
          CountTime = 0;
          I2C_Soft_Stop();
          return E_NOT_OK;
      }

      I2C_Delay(I2C_CYCLE_TIME_OUT);
  }

  I2C_SDA_ON;
  I2C_SCL_ON;
  I2C_Delay(I2C_TIME_DELAY);
  
  I2C_SCL_OFF;
  I2C_SDA_Setup(I2C_PIN_OUTPUT);
  
   return E_OK;
}

Std_ReturnType I2C_Soft_ReadByteRegister(uint8_t address,uint8_t add_reg, uint8_t *byte) 
{
  I2C_Soft_Start();
  
  if (E_NOT_OK == I2C_Soft_WriteByte(address << 1))
  {
    return E_NOT_OK;
  }
  
  if (E_NOT_OK == I2C_Soft_WriteByte(add_reg))
  {
    return E_NOT_OK;
  }
 
  I2C_Soft_Start();
  if (E_NOT_OK ==  I2C_Soft_WriteByte((address << 1) | 0x01))
  {
    return E_NOT_OK;
  }

  I2C_SDA_Setup(I2C_PIN_INPUT);
  for(uint8_t CountBits = 0; CountBits < 8; CountBits++) 
  {
    *byte <<= 1;
    
    I2C_SCL_ON;
    I2C_Delay(I2C_TIME_DELAY/2 + 1);
    
    *byte |= I2C_SDA_READ;
    I2C_Delay(I2C_TIME_DELAY/2 + 1);
    
    I2C_SCL_OFF;
    I2C_Delay(I2C_TIME_DELAY + 1);
  }

  I2C_SDA_Setup(I2C_PIN_OUTPUT);
  
  I2C_SDA_ON;
  I2C_SCL_ON;
  I2C_Delay(I2C_TIME_DELAY);
  
  I2C_SCL_OFF;
  I2C_Delay(I2C_TIME_DELAY);

  I2C_Soft_Stop();
  return *byte;
}

Std_ReturnType I2C_Soft_WriteData(uint8_t address, uint8_t *data, uint16_t len)
{
  I2C_Soft_Start();
  if (E_NOT_OK == I2C_Soft_WriteByte(address << 1))
  {
    return E_NOT_OK;
  }

  for (uint16_t countdata = 0; countdata < len; countdata++)
  {
    if (E_NOT_OK == I2C_Soft_WriteByte(data[countdata]))
    {
      return E_NOT_OK;
    }
  }
  I2C_Soft_Stop();
  return E_OK;
}

Std_ReturnType I2C_Soft_WriteDataFromAddress(uint8_t address, uint8_t add_reg, uint8_t *data, uint16_t len)
{
  I2C_Soft_Start();
  if (E_NOT_OK == I2C_Soft_WriteByte(address << 1))
  {
    return E_NOT_OK;
  }
  
  if (E_NOT_OK == I2C_Soft_WriteByte(add_reg))
  {
    return E_NOT_OK;
  }

  for (uint16_t countdata = 0; countdata < len; countdata++)
  {
    if (E_NOT_OK == I2C_Soft_WriteByte(data[countdata]))
    {
      return E_NOT_OK;
    }
  }
  
  I2C_Soft_Stop();
  return E_OK;
}

Std_ReturnType I2c_Soft_ReadData(uint8_t address, uint8_t *data, uint16_t len)
{
  I2C_Soft_Start();
  if (E_NOT_OK == I2C_Soft_WriteByte((address << 1) | 0x01))
  {
    return E_NOT_OK;
  }

  for (uint16_t countdata = 0; countdata < len; countdata++)
  {
    I2C_SDA_Setup(I2C_PIN_INPUT);
    for (uint8_t CountBits = 0; CountBits < 8; CountBits++)
    {
      data[countdata] <<= 1;
      I2C_SCL_ON;
      I2C_Delay(I2C_TIME_DELAY/2 + 1);

      data[countdata] |= I2C_SDA_READ;
      I2C_Delay(I2C_TIME_DELAY/2 + 1);
      
      I2C_SCL_OFF;
      I2C_Delay(I2C_TIME_DELAY + 1);
    }

    I2C_SDA_Setup(I2C_PIN_OUTPUT);
    if (countdata == (len - 1))
    {
       I2C_SDA_ON;
    }
    else
    {
       I2C_SDA_OFF;
    }

    I2C_SCL_ON;
    I2C_Delay(I2C_TIME_DELAY);
    
    I2C_SCL_OFF;
    I2C_Delay(I2C_TIME_DELAY);
  }
  I2C_Delay(I2C_TIME_DELAY);
  
  I2C_Soft_Stop();
  return E_OK;
}

Std_ReturnType I2C_Soft_ReadDataFromAddress(uint8_t address,uint8_t add_reg, uint8_t *data, uint16_t len) 
{
  I2C_Soft_Start();
  
  if (E_NOT_OK == I2C_Soft_WriteByte(address << 1))
  {
    return E_NOT_OK;
  }
  
  if (E_NOT_OK == I2C_Soft_WriteByte(add_reg))
  {
    return E_NOT_OK;
  }
 
  I2C_Soft_Start();
  if (E_NOT_OK ==  I2C_Soft_WriteByte((address << 1) | 0x01))
  {
    return E_NOT_OK;
  }
  
  for (uint16_t countdata = 0; countdata < len; countdata++)
  {
      I2C_SDA_Setup(I2C_PIN_INPUT);
      for(uint8_t CountBits = 0; CountBits < 8; CountBits++) 
      {
          *data <<= 1;
        
          I2C_SCL_ON;
          I2C_Delay(I2C_TIME_DELAY/2 + 1);
        
          *data |= I2C_SDA_READ;
          I2C_Delay(I2C_TIME_DELAY/2 + 1);
          
          I2C_SCL_OFF;
          I2C_Delay(I2C_TIME_DELAY + 1);
      }
    
      I2C_SDA_Setup(I2C_PIN_OUTPUT);
      if (countdata == (len - 1))
      {
         I2C_SDA_ON;
      }
      else
      {
         I2C_SDA_OFF;
      }
      
      I2C_SCL_ON;
      I2C_Delay(I2C_TIME_DELAY);
      
      I2C_SCL_OFF;
      I2C_Delay(I2C_TIME_DELAY);
   }
  
  I2C_Soft_Stop();
  return E_OK;
}
