#include "i2c_soft.h"

#define I2C_PIN_INPUT            GPIO_MODE_INPUT
#define I2C_PIN_OUTPUT           GPIO_MODE_OUTPUT_OD

#define I2C_PIN_LOW              0
#define I2C_PIN_HIGH             1

#define I2C_CYCLE_TIME_OUT       5U
#define I2C_CLOCK_STRETCH_TIMEOUT_US 10000U

static uint32_t I2C_GetTicksPerUs(void)
{
  uint32_t hclk = HAL_RCC_GetHCLKFreq();
  uint32_t ticks_per_us = (hclk + 999999U) / 1000000U;

  if (ticks_per_us == 0U)
  {
    ticks_per_us = 1U;
  }

  return ticks_per_us;
}

static void I2C_DelayUs(uint32_t delay_us)
{
  uint32_t ticks_per_us;
  uint32_t ticks_to_wait;
  uint32_t reload;
  uint32_t last_tick;

  if (delay_us == 0U)
  {
    return;
  }

  ticks_per_us = I2C_GetTicksPerUs();
  if (delay_us > (UINT32_MAX / ticks_per_us))
  {
    ticks_to_wait = UINT32_MAX;
  }
  else
  {
    ticks_to_wait = delay_us * ticks_per_us;
  }

  reload = SysTick->LOAD + 1U;
  if (((SysTick->CTRL & SysTick_CTRL_ENABLE_Msk) == 0U) || (reload == 0U))
  {
    while (ticks_to_wait-- > 0U)
    {
      __NOP();
    }

    return;
  }

  last_tick = SysTick->VAL;

  while (ticks_to_wait > 0U)
  {
    uint32_t current_tick = SysTick->VAL;
    uint32_t elapsed_ticks;

    if (last_tick >= current_tick)
    {
      elapsed_ticks = last_tick - current_tick;
    }
    else
    {
      elapsed_ticks = last_tick + reload - current_tick;
    }

    if (elapsed_ticks > 0U)
    {
      if (elapsed_ticks >= ticks_to_wait)
      {
        break;
      }

      ticks_to_wait -= elapsed_ticks;
      last_tick = current_tick;
    }
  }
}

static Std_ReturnType I2C_WaitSclReleased(void)
{
  uint32_t timeout_us = I2C_CLOCK_STRETCH_TIMEOUT_US;

  /* BQ76952 can stretch SCL while it prepares an ACK or response. */
  while (I2C_SCL_READ == I2C_PIN_LOW)
  {
    if (timeout_us == 0U)
    {
      return E_NOT_OK;
    }

    I2C_DelayUs(I2C_CYCLE_TIME_OUT);
    if (timeout_us > I2C_CYCLE_TIME_OUT)
    {
      timeout_us -= I2C_CYCLE_TIME_OUT;
    }
    else
    {
      timeout_us = 0U;
    }
  }

  return E_OK;
}

static Std_ReturnType I2C_ClockHigh(void)
{
  I2C_SCL_ON;
  if (I2C_WaitSclReleased() != E_OK)
  {
    return E_NOT_OK;
  }

  I2C_DelayUs(I2C_TIME_DELAY);
  return E_OK;
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
  (void)I2C_ClockHigh();
  I2C_DelayUs(I2C_TIME_DELAY);
  
  I2C_SDA_OFF;
  I2C_DelayUs(I2C_TIME_DELAY);
  
  I2C_SCL_OFF;
  I2C_DelayUs(I2C_TIME_DELAY);
}

void I2C_Soft_Stop(void) {
  
  I2C_SDA_Setup(I2C_PIN_OUTPUT);
  I2C_SCL_Setup(I2C_PIN_OUTPUT);
  
  I2C_SCL_OFF;
  I2C_DelayUs(I2C_TIME_DELAY);

  I2C_SDA_OFF;
  I2C_DelayUs(I2C_TIME_DELAY);
  
  (void)I2C_ClockHigh();
  I2C_SDA_ON;
  I2C_DelayUs(I2C_TIME_DELAY);
}

uint8_t I2C_Soft_CheckAck(void)
{
  uint8_t ack = 0;

  I2C_SDA_ON;
  I2C_SDA_Setup(I2C_PIN_INPUT);
  
  if (I2C_ClockHigh() != E_OK)
  {
    I2C_SCL_OFF;
    I2C_SDA_Setup(I2C_PIN_OUTPUT);
    I2C_SDA_ON;
    return 0U;
  }
 
  ack = I2C_SDA_READ;
  
  I2C_SCL_OFF;
  I2C_DelayUs(I2C_TIME_DELAY);
  I2C_SDA_Setup(I2C_PIN_OUTPUT);
  I2C_SDA_ON;
 
  return (~ack)&0x01;
}

Std_ReturnType I2C_Soft_WriteByte(uint8_t byte)
{
  uint8_t ack;

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
    
    I2C_DelayUs(I2C_TIME_DELAY);
    if (I2C_ClockHigh() != E_OK)
    {
      I2C_SCL_OFF;
      I2C_Soft_Stop();
      return E_NOT_OK;
    }
    
    I2C_SCL_OFF;
    byte <<= 1;
    I2C_DelayUs(I2C_TIME_DELAY);
  }
  
  I2C_SDA_ON;
  I2C_SDA_Setup(I2C_PIN_INPUT);
  I2C_DelayUs(I2C_TIME_DELAY);

  if (I2C_ClockHigh() != E_OK)
  {
    I2C_SCL_OFF;
    I2C_SDA_Setup(I2C_PIN_OUTPUT);
    I2C_Soft_Stop();
    return E_NOT_OK;
  }

  ack = (I2C_SDA_READ == I2C_PIN_LOW) ? 1U : 0U;
  
  I2C_SCL_OFF;
  I2C_DelayUs(I2C_TIME_DELAY);
  I2C_SDA_Setup(I2C_PIN_OUTPUT);
  I2C_SDA_ON;

  if (ack == 0U)
  {
    I2C_Soft_Stop();
    return E_NOT_OK;
  }
  
   return E_OK;
}

uint8_t I2C_Soft_ReadByte(uint8_t ack)
{
  uint8_t value = 0;

  I2C_SDA_Setup(I2C_PIN_INPUT);
  for(uint8_t CountBits = 0; CountBits < 8; CountBits++)
  {
    value <<= 1;

    if (I2C_ClockHigh() != E_OK)
    {
      I2C_SCL_OFF;
      return value;
    }

    value |= I2C_SDA_READ;

    I2C_SCL_OFF;
    I2C_DelayUs(I2C_TIME_DELAY);
  }

  I2C_SDA_Setup(I2C_PIN_OUTPUT);
  if (ack != 0U)
  {
    I2C_SDA_OFF;
  }
  else
  {
    I2C_SDA_ON;
  }

  I2C_DelayUs(I2C_TIME_DELAY);
  (void)I2C_ClockHigh();
  I2C_SCL_OFF;
  I2C_DelayUs(I2C_TIME_DELAY);
  I2C_SDA_ON;

  return value;
}

Std_ReturnType I2C_Soft_ReadByteRegister(uint8_t address,uint8_t add_reg, uint8_t *byte) 
{
  if (byte == NULL)
  {
    return E_NOT_OK;
  }

  *byte = 0;
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

  *byte = I2C_Soft_ReadByte(0);
  I2C_Soft_Stop();
  return E_OK;
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
  if (data == NULL)
  {
    return E_NOT_OK;
  }

  I2C_Soft_Start();
  if (E_NOT_OK == I2C_Soft_WriteByte((address << 1) | 0x01))
  {
    return E_NOT_OK;
  }

  for (uint16_t countdata = 0; countdata < len; countdata++)
  {
    data[countdata] = I2C_Soft_ReadByte((countdata + 1U) < len ? 1U : 0U);
  }
  I2C_DelayUs(I2C_TIME_DELAY);
  
  I2C_Soft_Stop();
  return E_OK;
}

Std_ReturnType I2C_Soft_ReadDataFromAddress(uint8_t address,uint8_t add_reg, uint8_t *data, uint16_t len) 
{
  if (data == NULL)
  {
    return E_NOT_OK;
  }

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
      data[countdata] = I2C_Soft_ReadByte((countdata + 1U) < len ? 1U : 0U);
   }
  
  I2C_Soft_Stop();
  return E_OK;
}
