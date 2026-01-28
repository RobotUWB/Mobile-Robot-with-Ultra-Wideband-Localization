/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART3_UART_Init(void);
/* USER CODE BEGIN PFP */
void TMC2209_Write(uint8_t addr, uint8_t reg, uint32_t data);
void Move_Trapezoid(long total_steps, int min_delay, int start_delay);
uint8_t TMC_CRC8(uint8_t *data, size_t length);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// TMC2209 UART Constants
#define TMC2209_ADDR     0x00  // Default Address
#define TMC2209_SYNC     0x05
#define REG_IHOLD_IRUN   0x10
#define REG_GCONF        0x00
#define REG_CHOPCONF     0x6C

// ฟังก์ชันคำนวณ CRC สำหรับ TMC2209
uint8_t TMC_CRC8(uint8_t *data, size_t length) {
	uint8_t crc = 0;
	uint8_t currentByte;
	for (size_t i = 0; i < length; i++) {
		currentByte = data[i];
		for (int j = 0; j < 8; j++) {
			if ((crc >> 7) ^ (currentByte >> 7)) {
				crc = (crc << 1) ^ 0x07;
			} else {
				crc = (crc << 1);
			}
			currentByte <<= 1;
		}
	}
	return crc;
}

// ฟังก์ชันส่งข้อมูลผ่าน UART3 ไปยัง TMC2209
void TMC2209_Write(uint8_t addr, uint8_t reg, uint32_t data) {
	uint8_t txBuffer[8];

	txBuffer[0] = 0x05; // Sync Byte
	txBuffer[1] = addr; // <--- ใช้ Address ที่ส่งเข้ามา (0 หรือ 1)
	txBuffer[2] = reg | 0x80;

	txBuffer[3] = (data >> 24) & 0xFF;
	txBuffer[4] = (data >> 16) & 0xFF;
	txBuffer[5] = (data >> 8) & 0xFF;
	txBuffer[6] = data & 0xFF;

	txBuffer[7] = TMC_CRC8(&txBuffer[1], 6);

	HAL_UART_Transmit(&huart3, txBuffer, 8, 100);
}
void Move_Trapezoid_Perf(long total_steps, int min_delay, int start_delay) {

	int current_delay = start_delay;
	int accel_steps = 1000; // ระยะทางในการเร่ง (จูนตามชอบ)

	// Safety Check: ถ้าระยะทางสั้นเกินไป ให้ลดระยะเร่งลง
	if (total_steps < (accel_steps * 2)) {
		accel_steps = total_steps / 2;
	}
	long decel_start = total_steps - accel_steps;

	// --- เตรียม Mask สำหรับยิงพร้อมกัน ---
	// STEP_L_Pin (PA2) | STEP_R_Pin (PA4)
	// การเอามา OR กัน คือการเตรียมสั่งงานทั้งคู่ในคำสั่งเดียว
	uint32_t step_mask = STEP_L_Pin | STEP_R_Pin;

	for (long i = 0; i < total_steps; i++) {

		// --- 1. สั่ง HIGH พร้อมกัน (Set Bits) ---
		// เขียนลง BSRR ทีเดียว ไฟวิ่งออกทั้ง PA2 และ PA4 พร้อมกันทันที!
		GPIOA->BSRR = step_mask;

		// Delay High
		for (int d = 0; d < current_delay; d++)
			__NOP();

		// --- 2. สั่ง LOW พร้อมกัน (Reset Bits) ---
		// การ Shift << 16 คือการย้ายไปสั่งที่ปุ่ม Reset ของ Register
		GPIOA->BSRR = (step_mask << 16);

		// Delay Low
		for (int d = 0; d < current_delay; d++)
			__NOP();

		// --- 3. คำนวณความเร็ว (Trapezoid Logic เหมือนเดิม) ---
		if (i < accel_steps) {
			// ช่วงเร่ง: ลด Delay ลง
			if (current_delay > min_delay) {
				current_delay -= (start_delay - min_delay) / accel_steps;
			}
		} else if (i >= decel_start) {
			// ช่วงเบรก: เพิ่ม Delay ขึ้น
			if (current_delay < start_delay) {
				current_delay += (start_delay - min_delay) / accel_steps;
			}
		} else {
			// ช่วงวิ่งคงที่
			current_delay = min_delay;
		}
	}
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_USART1_UART_Init();
	MX_USART3_UART_Init();
	/* USER CODE BEGIN 2 */
	HAL_Delay(1000); // รอไฟนิ่งสำคัญมาก

	// --- ส่งไปที่ Address 0 (มันจะเข้าทั้ง L และ R พร้อมกัน) ---

	// 1. ปิด VR (0xC0)
	TMC2209_Write(0, REG_GCONF, 0x000000C0);
	HAL_Delay(10);

	// 2. ตั้ง Microstep 1/16 (0x14000053)
	// ถ้า UART ติด -> ทั้งคู่จะวิ่งเนียนขึ้น (1/16)
	// ถ้า UART ดับ -> ทั้งคู่จะวิ่งเท่ากัน (1/8) -> อย่างน้อยก็วิ่งตรง!
	TMC2209_Write(0, REG_CHOPCONF, 0x14000053);
	HAL_Delay(10);

	// 3. ตั้งกระแส (Current)
	TMC2209_Write(0, REG_IHOLD_IRUN, 0x0006140A);
	HAL_Delay(10);

	// Enable & Dir
	HAL_GPIO_WritePin(GPIOA, EN_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(DIR_L_GPIO_Port, DIR_L_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(DIR_R_GPIO_Port, DIR_R_Pin, GPIO_PIN_SET);
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
		// เดินหน้า 6400 ก้าว (ประมาณ 2 รอบวงล้อ ถ้าใช้ 1/16 step)
		// Top Speed = 500 (เร็ว)
		// Start Speed = 2500 (ช้า)
		Move_Trapezoid_Perf(6400, 500, 2500);

		HAL_Delay(500);

		// กลับหลังหัน
		HAL_GPIO_TogglePin(DIR_L_GPIO_Port, DIR_L_Pin);
		HAL_GPIO_TogglePin(DIR_R_GPIO_Port, DIR_R_Pin);
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void) {

	/* USER CODE BEGIN USART1_Init 0 */

	/* USER CODE END USART1_Init 0 */

	/* USER CODE BEGIN USART1_Init 1 */

	/* USER CODE END USART1_Init 1 */
	huart1.Instance = USART1;
	huart1.Init.BaudRate = 115200;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART1_Init 2 */

	/* USER CODE END USART1_Init 2 */

}

/**
 * @brief USART3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART3_UART_Init(void) {

	/* USER CODE BEGIN USART3_Init 0 */

	/* USER CODE END USART3_Init 0 */

	/* USER CODE BEGIN USART3_Init 1 */

	/* USER CODE END USART3_Init 1 */
	huart3.Instance = USART3;
	huart3.Init.BaudRate = 115200;
	huart3.Init.WordLength = UART_WORDLENGTH_8B;
	huart3.Init.StopBits = UART_STOPBITS_1;
	huart3.Init.Parity = UART_PARITY_NONE;
	huart3.Init.Mode = UART_MODE_TX_RX;
	huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart3.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart3) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART3_Init 2 */

	/* USER CODE END USART3_Init 2 */

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	/* USER CODE BEGIN MX_GPIO_Init_1 */

	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOA,
	EN_Pin | STEP_L_Pin | DIR_L_Pin | STEP_R_Pin | DIR_R_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pins : EN_Pin DIR_L_Pin DIR_R_Pin */
	GPIO_InitStruct.Pin = EN_Pin | DIR_L_Pin | DIR_R_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pins : STEP_L_Pin STEP_R_Pin */
	GPIO_InitStruct.Pin = STEP_L_Pin | STEP_R_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* USER CODE BEGIN MX_GPIO_Init_2 */

	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
