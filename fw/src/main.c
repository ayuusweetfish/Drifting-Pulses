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

  while (1) { printf("!!\n"); delay_us(1000000); }

  // ============ ACT LED ============ //
{
  GPIOA->BSRR = (1 << 4);
  HAL_GPIO_Init(GPIOA, &(GPIO_InitTypeDef){
    .Mode = GPIO_MODE_OUTPUT_PP,
    .Pin = (1 << 4),
    .Pull = GPIO_NOPULL,
    .Speed = GPIO_SPEED_FREQ_LOW,
  });
}
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
