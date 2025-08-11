#include "py32f0xx_hal.h"
#include "py32f0xx_ll_rcc.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

// #define RELEASE

#include "debug_printf.h"

static void spin_delay(uint32_t cycles)
{
  __asm__ volatile (
    "   cmp %[cycles], #5\n"
    "   ble 2f\n"
    "   sub %[cycles], #5\n"
    "   lsr %[cycles], #2\n"
    "1: sub %[cycles], #1\n"
    "   nop\n"
    "   bne 1b\n"   // 2 cycles if taken
    "2: \n"
    : [cycles] "+l" (cycles)
    : // No output
    : "cc"
  );
}
// __attribute__ ((section(".RamFunc")))
static inline void delay_us(uint32_t us)
{
  spin_delay(us * 8);
}

static SPI_HandleTypeDef spi1;

#pragma GCC push_options
#pragma GCC optimize("O3")
int main()
{
  HAL_Init();

  // ============ Clocks ============ //
{
  HAL_RCC_OscConfig(&(RCC_OscInitTypeDef){
    .OscillatorType = RCC_OSCILLATORTYPE_HSE,
    .HSIState = RCC_HSI_ON,
  });
}

{
  HAL_RCC_ClockConfig(&(RCC_ClkInitTypeDef){
    .ClockType =
      RCC_CLOCKTYPE_SYSCLK |
      RCC_CLOCKTYPE_HCLK |
      RCC_CLOCKTYPE_PCLK1,
    .SYSCLKSource = RCC_SYSCLKSOURCE_HSI, // 24 MHz
    .AHBCLKDivider = RCC_SYSCLK_DIV1,
    .APB1CLKDivider = RCC_HCLK_DIV1,
  }, FLASH_LATENCY_0);
}

  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);

  __HAL_RCC_GPIOA_CLK_ENABLE();

  printf("sysclk = %lu Hz\n", HAL_RCC_GetSysClockFreq());

  // ============ ACT LED ============ //
{
  GPIOA->BSRR = (1 << 4) << 16;
  HAL_GPIO_Init(GPIOA, &(GPIO_InitTypeDef){
    .Mode = GPIO_MODE_OUTPUT_PP,
    .Pin = (1 << 4),
    .Pull = GPIO_NOPULL,
    .Speed = GPIO_SPEED_FREQ_LOW,
  });
}

  // ============ SPI1: IMU SC7A20 ============ //
  __HAL_RCC_SPI1_CLK_ENABLE();
  spi1 = (SPI_HandleTypeDef){
    .Instance = SPI1,
    .Init = {
      .Mode = SPI_MODE_MASTER,
      .Direction = SPI_DIRECTION_1LINE,
      .DataSize = SPI_DATASIZE_8BIT,
      .CLKPolarity = SPI_POLARITY_HIGH, // CPOL = 1
      .CLKPhase = SPI_PHASE_2EDGE,      // CPHA = 1 -- SC7A20 requires mode 3
      .NSS = SPI_NSS_SOFT,
      .BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4, // 6 MHz
      .FirstBit = SPI_FIRSTBIT_MSB,
    },
  };
  HAL_SPI_Init(&spi1);
  GPIOA->BSRR = (1 << 3);   // IMU_CS high
  HAL_GPIO_Init(GPIOA, &(GPIO_InitTypeDef){
    .Mode = GPIO_MODE_OUTPUT_PP,
    .Pin = (1 << 3),
  });
  HAL_GPIO_Init(GPIOA, &(GPIO_InitTypeDef){
    .Mode = GPIO_MODE_AF_PP,
    .Pin = (1 << 1) | (1 << 2),
    .Alternate = 10,  // PA1 = SPI1_MOSI, PA2 = SPI1_SCK
  });

  void imu_write(uint8_t reg, uint8_t byte)
  {
    delay_us(2);
    GPIOA->BSRR = (1 << 3) << 16;
    HAL_SPI_Transmit(&spi1, (uint8_t []){reg, byte}, 2, HAL_MAX_DELAY);
    GPIOA->BSRR = (1 << 3);
  }
  void imu_read(uint8_t reg, uint8_t *data, size_t n)
  {
    delay_us(2);
    GPIOA->BSRR = (1 << 3) << 16;
    HAL_SPI_Transmit(&spi1, (uint8_t []){0xC0 | reg}, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(&spi1, data, n, HAL_MAX_DELAY);
    GPIOA->BSRR = (1 << 3);
  }

  imu_write(0x24, 0b10000000);  // CTRL_REG5: BOOT = 1
  HAL_Delay(10);

  imu_write(0x23, 0b00000001);  // CTRL_REG4: SIM = 1

  // Check IC identifier
  while (1) {
    uint8_t b;
    imu_read(0x0F, &b, 1);
    printf("WHO_AM_I = %02x\n", (unsigned)b);
    if (b == 0x11) break;
    HAL_Delay(1000);
  }

  imu_write(0x20, 0b01010111);  // CTRL_REG1: ODR = 100 Hz, Z/Y/Xen = 1
  imu_write(0x23, 0b10001001);  // CTRL_REG4: BDU = 1, HR = 1
  imu_write(0x24, 0b01000000);  // CTRL_REG5: FIFO_EN = 1
  imu_write(0x2E, 0b11000000);  // FIFO_CTRL_REG: FM = FIFO mode

  int total = 0;
  while (1) {
    // https://github.com/STMicroelectronics/STMems_Standard_C_drivers/blob/8e3777b/lis3dh_STdC/examples/lis3dh_multi_read_fifo.c#L164
    // https://github.com/STMicroelectronics/lis3dh-pid/blob/4cd1e4a/lis3dh_reg.c#L2111
    uint8_t s;
    imu_read(0x2F, &s, 1);
    uint8_t count;
    imu_read(0x2F, &count, 1); count &= 0x1F;
    uint16_t a[3] = {0, 0, 0};
    for (int i = 0; i < count; i++) {
      imu_read(0x28, (uint8_t *)&a[0], 6);
    }
    if ((total += count + 1) >= 200 * 4) {
      printf("!! %02x %d %04x %04x %04x\n", (int)s, (int)count, (int)a[0], (int)a[1], (int)a[2]);
      total -= 200 * 4;
    }
  }

  while (1) { printf("!!\n"); delay_us(1000000); }
}

void NMI_Handler() { while (1) { } }
void HardFault_Handler() { while (1) { } }
void SVC_Handler() { while (1) { } }
void PendSV_Handler() { while (1) { } }
void SysTick_Handler()
{
  HAL_IncTick();
}
void WWDG_IRQHandler() { while (1) { } }
void PVD_IRQHandler() { while (1) { } }
void RTC_IRQHandler() { while (1) { } }
void FLASH_IRQHandler() { while (1) { } }
void RCC_IRQHandler() { while (1) { } }
void EXTI0_1_IRQHandler() { while (1) { } }
void EXTI2_3_IRQHandler() { while (1) { } }
void EXTI4_15_IRQHandler() { while (1) { } }
void DMA1_Channel1_IRQHandler() { while (1) { } }
void DMA1_Channel2_3_IRQHandler() { while (1) { } }
void ADC_COMP_IRQHandler() { while (1) { } }
void TIM1_BRK_UP_TRG_COM_IRQHandler() { while (1) { } }
void TIM1_CC_IRQHandler() { while (1) { } }
void TIM3_IRQHandler() { while (1) { } }
void LPTIM1_IRQHandler() { while (1) { } }
void TIM14_IRQHandler() { while (1) { } }
void TIM16_IRQHandler() { while (1) { } }
void TIM17_IRQHandler() { while (1) { } }
void I2C1_IRQHandler() { while (1) { } }
void SPI1_IRQHandler() { while (1) { } }
void USART1_IRQHandler() { while (1) { } }
void USART2_IRQHandler() { while (1) { } }
