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
  spin_delay(us * 24);
}

static SPI_HandleTypeDef spi1;
static DMA_HandleTypeDef dma1_ch1;

#define N_HALF_BUF 512
static uint16_t audio_buf[N_HALF_BUF * 2];

static void refill_buffer(uint16_t *buf)
{
  // 64-sample cycle = 366 Hz tone
if (0) {
  for (int i = 0; i < N_HALF_BUF; i++)
    buf[i] = (i % 64 < 32) ? 0x80 : 0;
} else {
  static uint16_t sine_table[64] = {
/*
from math import *
print(','.join('%d' % round(511.5 + 512 * (0.7 * sin(i / 64 * pi * 2))) for i in range(64)))
*/
512,547,581,616,649,680,711,739,765,789,809,828,843,854,863,868,870,868,863,854,843,828,809,789,765,739,711,680,649,616,581,547,512,476,442,407,374,343,312,284,258,234,214,195,180,169,160,155,153,155,160,169,180,195,214,234,258,284,312,343,374,407,442,476
  };
  static uint16_t tri_table[64] = {
/*
from math import *
print(','.join('%d' % round(511.5 + 512 * 0.7 * (1 - 4 * abs(0.5 - i / 64))) for i in range(64)))
*/
153,176,198,220,243,265,288,310,332,355,377,400,422,444,467,489,512,534,556,579,601,624,646,668,691,713,736,758,780,803,825,848,870,848,825,803,780,758,736,713,691,668,646,624,601,579,556,534,512,489,467,444,422,400,377,355,332,310,288,265,243,220,198,176
  };
  static uint32_t phase = 0;
  static uint32_t n_cycles = 0;
  for (int i = 0; i < N_HALF_BUF; i++) {
    buf[i] = (n_cycles < 366 ? sine_table : tri_table)[phase];
    if (n_cycles >= 50 || 1) buf[i] = 0;
    phase += 2;
    if (phase >= 64) {
      phase -= 64;
      n_cycles++;
      if (n_cycles == 732) n_cycles = 0;
    }
  }
}
}

#pragma GCC push_options
#pragma GCC optimize("O3")
int main()
{
  HAL_Init();

  // ============ Clocks ============ //
{
  HAL_RCC_OscConfig(&(RCC_OscInitTypeDef){
    .OscillatorType = RCC_OSCILLATORTYPE_HSI,
    .HSIState = RCC_HSI_ON,
    .HSICalibrationValue = RCC_HSICALIBRATION_24MHz,
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

while (0) {
  printf("sysclk = %lu Hz\n", HAL_RCC_GetSysClockFreq());
  delay_us(1000000);
}

  // ============ LED ============ //
{
  HAL_GPIO_Init(GPIOA, &(GPIO_InitTypeDef){
    .Mode = GPIO_MODE_AF_PP,
    .Pin = (1 << 4) | (1 << 5),
    .Alternate = 13,  // PA4 = TIM3_CH3, PA5 = TIM3_CH2
    .Speed = GPIO_SPEED_FREQ_HIGH,
  });
  HAL_GPIO_Init(GPIOA, &(GPIO_InitTypeDef){
    .Mode = GPIO_MODE_AF_PP,
    .Pin = (1 << 6),
    .Alternate = 1,   // PA6 = TIM3_CH1
    .Speed = GPIO_SPEED_FREQ_HIGH,
  });

  __HAL_RCC_TIM3_CLK_ENABLE();
  TIM_HandleTypeDef tim3 = {
    .Instance = TIM3,
    .Init = {
      .Prescaler = 1 - 1,   // 24 MHz
      .CounterMode = TIM_COUNTERMODE_DOWN,
      .Period = 4096,       // 6 kHz
      .ClockDivision = TIM_CLOCKDIVISION_DIV1,
    },
  };
  HAL_TIM_PWM_Init(&tim3);
  HAL_TIM_PWM_ConfigChannel(&tim3, &(TIM_OC_InitTypeDef){
    .OCMode = TIM_OCMODE_PWM1,
    .OCPolarity = TIM_OCPOLARITY_HIGH,
  }, TIM_CHANNEL_1);
  HAL_TIM_PWM_ConfigChannel(&tim3, &(TIM_OC_InitTypeDef){
    .OCMode = TIM_OCMODE_PWM1,
    .OCPolarity = TIM_OCPOLARITY_HIGH,
  }, TIM_CHANNEL_2);
  HAL_TIM_PWM_ConfigChannel(&tim3, &(TIM_OC_InitTypeDef){
    .OCMode = TIM_OCMODE_PWM1,
    .OCPolarity = TIM_OCPOLARITY_HIGH,
  }, TIM_CHANNEL_3);
  TIM3->CCR1 = 4096;  // Blue
  TIM3->CCR2 = 4096;  // Green
  TIM3->CCR3 = 4096;  // Red
  HAL_TIM_PWM_Start(&tim3, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&tim3, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&tim3, TIM_CHANNEL_3);

  while (0) {
    for (int i = 0; i < 4096; i += 8) {
      TIM3->CCR3 = i;
      HAL_Delay(4);
    }
  }
}

  // ============ Audio output ============ //
{
  __HAL_RCC_TIM17_CLK_ENABLE();
  TIM_HandleTypeDef tim17 = {
    .Instance = TIM17,
    .Init = {
      .Prescaler = 1 - 1,   // 24 MHz
      .CounterMode = TIM_COUNTERMODE_DOWN,
      .Period = 1024,       // 24 kHz
      .ClockDivision = TIM_CLOCKDIVISION_DIV1,
    },
  };

  HAL_GPIO_Init(GPIOA, &(GPIO_InitTypeDef){
    .Mode = GPIO_MODE_AF_PP,
    .Pin = (1 << 7),
    .Alternate = 5,   // PA7 = TIM17_CH1
    .Speed = GPIO_SPEED_FREQ_HIGH,
  });
  HAL_TIM_PWM_Init(&tim17);
  HAL_TIM_PWM_ConfigChannel(&tim17, &(TIM_OC_InitTypeDef){
    .OCMode = TIM_OCMODE_PWM1,
    .OCPolarity = TIM_OCPOLARITY_HIGH,
  }, TIM_CHANNEL_1);

  __HAL_RCC_DMA_CLK_ENABLE();
  // Reference manual speficies that all channels map to all peripherals
  dma1_ch1 = (DMA_HandleTypeDef){
    .Instance = DMA1_Channel1,
    .Init = {
      .Direction = DMA_MEMORY_TO_PERIPH,
      .PeriphInc = DMA_PINC_DISABLE,
      .MemInc = DMA_MINC_ENABLE,
      .PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD,
      .MemDataAlignment = DMA_MDATAALIGN_HALFWORD,
      .Mode = DMA_CIRCULAR,
      .Priority = DMA_PRIORITY_MEDIUM,
    },
  };
  HAL_DMA_Init(&dma1_ch1);
  HAL_DMA_ChannelMap(&dma1_ch1, DMA_CHANNEL_MAP_TIM17_CH1);
  __HAL_LINKDMA(&tim17, hdma[TIM_DMA_ID_CC1], dma1_ch1);
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 15, 1);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

  void dma_tx_half_cplt()
  {
    refill_buffer(audio_buf);
  }
  void dma_tx_cplt()
  {
    refill_buffer(audio_buf + N_HALF_BUF);
    // 24 kHz / 1024 samples
    static int count = 0;
    if (0 && ++count == 24) { printf("refill\n"); count = 0; }
  }
  HAL_TIM_PWM_Start_DMA(&tim17, TIM_CHANNEL_1, (void *)audio_buf, N_HALF_BUF * 2);
  // Overwrite callbacks and handle the events ourselves
  dma1_ch1.XferHalfCpltCallback = dma_tx_half_cplt;
  dma1_ch1.XferCpltCallback = dma_tx_cplt;
  dma1_ch1.XferErrorCallback = NULL;
  dma1_ch1.XferAbortCallback = NULL;
}

if (0) {
  TIM3->CCR3 = 3072;
  TIM3->CCR2 = 3072;
}

  // ============ ADC ============ //
{
  HAL_GPIO_Init(GPIOA, &(GPIO_InitTypeDef){
    .Mode = GPIO_MODE_ANALOG,
    .Pin = (1 << 0),
  });

  __HAL_RCC_ADC_CLK_ENABLE();
  ADC_HandleTypeDef adc1 = (ADC_HandleTypeDef){
    .Instance = ADC1,
    .Init = {
      .ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2,
      .Resolution = ADC_RESOLUTION_12B,
      .DataAlign = ADC_DATAALIGN_RIGHT,
      .ScanConvMode = ADC_SCAN_DIRECTION_FORWARD,
      .EOCSelection = ADC_EOC_SINGLE_CONV,
      .SamplingTimeCommon = ADC_SAMPLETIME_239CYCLES_5,
    },
  };
  HAL_ADC_Init(&adc1);
  HAL_ADC_Calibration_Start(&adc1);
  HAL_ADC_ConfigChannel(&adc1, &(ADC_ChannelConfTypeDef){
    .Channel = ADC_CHANNEL_0,
    .Rank = ADC_RANK_CHANNEL_NUMBER,
    .SamplingTime = ADC_SAMPLETIME_239CYCLES_5, // Obsolete
  });
  HAL_ADC_Start(&adc1);
  HAL_ADC_PollForConversion(&adc1, HAL_MAX_DELAY);
  uint32_t adc_value = HAL_ADC_GetValue(&adc1);
  HAL_ADC_Stop(&adc1);
  HAL_ADC_DeInit(&adc1);
  __HAL_RCC_ADC_CLK_DISABLE();
  while (0) {
    printf("%u\n", (unsigned)adc_value);
    HAL_Delay(1000);
  }
}

  // ============ IMU SC7A20 ============ //
{
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
}

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

  int abs(int x) { return x < 0 ? -x : x; }
  int max(int a, int b) { return a > b ? a : b; }
  int sqrti(uint32_t x) {
    // TODO: Optimize?
    uint32_t i = 1;
    while (i * i <= x) i++;
    return i - 1;
  }

  int process(uint32_t m) {
    return max(0, (3072 - sqrti(m)) * 3 / 2);
  }
  int process_test(int x) {
    return max(0, abs(x - 17800) - 500);
  }

  while (1) {
    uint8_t count;
    imu_read(0x2F, &count, 1); count &= 0x1F;
    uint8_t a[7];   // SC7A20 asks for a 7-byte read, unlike ST's 6-byte
    for (int i = 0; i < count; i++) {
      imu_read(0x27, a, 7);
      int16_t x = (int16_t)(((uint16_t)a[2] << 8) | (uint16_t)a[1]);
      int16_t y = (int16_t)(((uint16_t)a[4] << 8) | (uint16_t)a[3]);
      int16_t z = (int16_t)(((uint16_t)a[6] << 8) | (uint16_t)a[5]);
      uint32_t m = (int32_t)y * (int32_t)y + (int32_t)z * (int32_t)z;
      printf("%6d %6d %6d\t", (int)x, (int)y, (int)z); printf("%8d\n", sqrti(m));
      TIM3->CCR2 = max(0, 4096 - process_test(x));
    }
    HAL_Delay(10);
  }

  // ============ LEDS ============ //

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
void DMA1_Channel1_IRQHandler()
{
  HAL_DMA_IRQHandler(&dma1_ch1);
}
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
