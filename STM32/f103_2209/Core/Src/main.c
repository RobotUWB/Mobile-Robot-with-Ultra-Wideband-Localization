/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body (Optimized Physics Motion)
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

// --- ⚙️ ROBOT CONFIGURATION (ปรับจูนตรงนี้) ---
#define WHEEL_DIAMETER      0.125f   // เมตร (125mm)
#define WHEEL_BASE          0.295f   // เมตร (295mm)
#define PI                  3.14159265f

// ค่า Steps ที่ Calibrate แล้ว (จากที่คุณเทสต์ว่า 10 เวิร์ค)
#define STEPS_PER_MM        10

// --- 🏎️ PHYSICS SETTINGS (จูนความนิ่มนวลตรงนี้) ---
#define MAX_SPEED_MM_S      400.0f   // ความเร็วสูงสุด (mm/s)
#define ACCEL_MM_S2         300.0f   // ความเร่ง (mm/s^2) ยิ่งน้อยยิ่งนิ่ม
#define START_SPEED_MM_S    50.0f    // ความเร็วตอนเริ่มออกตัว

// UART Buffer
#define RX_BUFFER_SIZE      64

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
uint8_t rx_buffer[RX_BUFFER_SIZE];
uint8_t rx_byte;
uint8_t rx_index = 0;
volatile uint8_t cmd_ready = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
void Move_With_Physics(long total_steps, int dir_mode);
void Execute_Move(int mm);
void Execute_Turn(int degrees);
void Send_Response(const char *msg);
void Log_Debug(const char *msg);
void Parse_Command(char *cmd);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// --- TMC2209 Helper Functions (Optional for Setup) ---
uint8_t TMC_CRC8(uint8_t *data, size_t length) {
	uint8_t crc = 0;
	uint8_t currentByte;
	for (size_t i = 0; i < length; i++) {
		currentByte = data[i];
		for (int j = 0; j < 8; j++) {
			if ((crc >> 7) ^ (currentByte >> 7))
				crc = (crc << 1) ^ 0x07;
			else
				crc = (crc << 1);
			currentByte <<= 1;
		}
	}
	return crc;
}

void TMC2209_Write(uint8_t addr, uint8_t reg, uint32_t data) {
	uint8_t txBuffer[8];
	txBuffer[0] = 0x05;
	txBuffer[1] = addr;
	txBuffer[2] = reg | 0x80;
	txBuffer[3] = (data >> 24) & 0xFF;
	txBuffer[4] = (data >> 16) & 0xFF;
	txBuffer[5] = (data >> 8) & 0xFF;
	txBuffer[6] = data & 0xFF;
	txBuffer[7] = TMC_CRC8(&txBuffer[1], 6);
	HAL_UART_Transmit(&huart3, txBuffer, 8, 100);
}

// --- 🧠 CORE PHYSICS MOTION ENGINE ---
// dir_mode: 0=Straight, 1=Spin
/* USER CODE BEGIN 0 */

// --- ฟังก์ชันหน่วงเวลาหน่วยไมโครวินาที (จูนสำหรับ 72MHz) ---
// ใช้แทน NOP Loop แบบเดิมที่กะกณฑ์ยาก
void delay_us_tuned(uint32_t us) {
	// 72MHz: 1 us ≈ 72 cycles
	// Loop C ทั่วไปใช้ประมาณ 4-5 cycles ต่อรอบ -> คูณ 12-15 กำลังดี
	us *= 12;
	while (us--) {
		__NOP();
	}
}

// --- ฟังก์ชันการเคลื่อนที่แบบ Physics (แก้ไขแล้ว) ---
void Move_Engine(long steps_L, long steps_R) {
	char log_msg[64];

	// 1. หาว่าล้อไหนต้องวิ่งเยอะกว่า (Master Axis)
	long max_steps = (steps_L > steps_R) ? steps_L : steps_R;

	// Safety: ถ้าไม่มีการเคลื่อนที่ ก็ออกเลย
	if (max_steps == 0)
		return;

	// 2. ตั้งค่า Physics (อิงตามล้อที่วิ่งเยอะสุด)
	float v_start = START_SPEED_MM_S;
	float v_max = MAX_SPEED_MM_S;
	float accel = ACCEL_MM_S2;

	// คำนวณระยะ Ramp (s = (v^2 - u^2) / 2a)
	float dist_accel_mm = ((v_max * v_max) - (v_start * v_start)) / (2 * accel);
	long accel_steps = (long) (dist_accel_mm * STEPS_PER_MM);

	if (accel_steps > max_steps / 2) {
		accel_steps = max_steps / 2;
	}
	long decel_start = max_steps - accel_steps;

	sprintf(log_msg, "ENGINE: L=%ld, R=%ld, Ramp=%ld\n", steps_L, steps_R,
			accel_steps);
	Log_Debug(log_msg);

	// 3. ตัวแปรสำหรับ Bresenham Algorithm
	long acc_L = 0;
	long acc_R = 0;

	// Masks (PA6=Left, PA4=Right)
	uint32_t mask_L = (1 << 6);
	uint32_t mask_R = (1 << 4);

	// ตัวแปรความเร็ว
	float current_v = v_start;
	float speed_inc = 0.5f;

	// --- 4. START LOOP ---
	for (long i = 0; i < max_steps; i++) {

		// A. คำนวณ Mask ว่ารอบนี้ล้อไหนต้องก้าวบ้าง?
		uint32_t step_mask = 0;

		acc_L += steps_L;
		if (acc_L >= max_steps) {
			step_mask |= mask_L; // ล้อซ้ายถึงคิวก้าว
			acc_L -= max_steps;
		}

		acc_R += steps_R;
		if (acc_R >= max_steps) {
			step_mask |= mask_R; // ล้อขวาถึงคิวก้าว
			acc_R -= max_steps;
		}

		// B. คำนวณความเร็ว (Ramp Up/Down)
		if (i < accel_steps) {
			if (current_v < v_max)
				current_v += speed_inc;
		} else if (i >= decel_start) {
			if (current_v > v_start)
				current_v -= speed_inc;
		}

		// C. แปลงความเร็วเป็น Delay
		float steps_per_sec = current_v * STEPS_PER_MM;
		if (steps_per_sec < 10.0f)
			steps_per_sec = 10.0f;
		uint32_t period_us = (uint32_t) (1000000.0f / steps_per_sec);
		uint32_t half_period = period_us / 2;

		// D. ยิง Pulse (เฉพาะล้อที่ถึงคิว)
		if (step_mask != 0) {
			GPIOA->BSRR = step_mask;          // High
			delay_us_tuned(half_period);
			GPIOA->BSRR = (step_mask << 16);  // Low
			delay_us_tuned(half_period);
		} else {
			// ถ้ารอบนี้ไม่มีใครก้าวเลย (กรณี Ratio ต่ำมากๆ) ก็ต้องหน่วงเวลาอยู่ดีเพื่อรักษา Timing
			delay_us_tuned(period_us);
		}
	}
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
	/* MCU Configuration */
	HAL_Init();
	SystemClock_Config();
	MX_GPIO_Init();
	MX_USART1_UART_Init();
	MX_USART3_UART_Init();
	MX_USART2_UART_Init();

	/* USER CODE BEGIN 2 */
	HAL_Delay(1000);

	// Setup TMC2209 (เหมือนเดิม)
	TMC2209_Write(0, 0x00, 0x000000C0); // GCONF
	HAL_Delay(10);
	TMC2209_Write(0, 0x6C, 0x14000053); // CHOPCONF
	HAL_Delay(10);
	TMC2209_Write(0, 0x10, 0x00061401); // IHOLD_IRUN
	HAL_Delay(10);

	// Enable Driver
	HAL_GPIO_WritePin(GPIOA, EN_Pin, GPIO_PIN_RESET);

	// Start Receive
	HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
	Log_Debug("\nREADY (PHYSICS MODE)\n");
	/* USER CODE END 2 */

	/* Infinite loop */
	while (1) {
		if (cmd_ready) {
			cmd_ready = 0;
			Parse_Command((char*) rx_buffer);
		}
	}
}

/* USER CODE BEGIN 4 */

void Log_Debug(const char *msg) {
	HAL_UART_Transmit(&huart2, (uint8_t*) msg, strlen(msg), 100);
}

void Send_Response(const char *msg) {
	HAL_UART_Transmit(&huart1, (uint8_t*) msg, strlen(msg), 100);
}

// --- ฟังก์ชันสั่งเดิน (เรียกใช้ Physics Engine) ---
void Execute_Move(int mm) {
	// ตั้งทิศทาง (เหมือนเดิม)
	if (mm >= 0) {
		HAL_GPIO_WritePin(DIR_L_GPIO_Port, DIR_L_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(DIR_R_GPIO_Port, DIR_R_Pin, GPIO_PIN_SET);
	} else {
		HAL_GPIO_WritePin(DIR_L_GPIO_Port, DIR_L_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(DIR_R_GPIO_Port, DIR_R_Pin, GPIO_PIN_RESET);
		mm = -mm;
	}

	long steps = (long) mm * STEPS_PER_MM;

	// สั่ง L และ R เท่ากัน (วิ่งตรง)
	Move_Engine(steps, steps);

	Send_Response("DONE\n");
}

// --- ฟังก์ชันสั่งเลี้ยว (เรียกใช้ Physics Engine) ---
void Execute_Turn(int degrees) {
	// 1. ตั้งทิศทาง "เดินหน้าทั้งคู่" (เพื่อให้เป็นโค้ง ไม่ใช่หมุน)
	// หมายเหตุ: ถ้าอยากให้เลี้ยวถอยหลัง ต้องแก้ Logic นี้
	HAL_GPIO_WritePin(DIR_L_GPIO_Port, DIR_L_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(DIR_R_GPIO_Port, DIR_R_Pin, GPIO_PIN_SET);

	// 2. กำหนดอัตราทด (Turn Ratio)
	// 1.0 = วิ่งเต็ม, 0.5 = วิ่งครึ่งเดียว, 0.0 = หยุดนิ่ง (จุดหมุนอยู่ที่ล้อ)
	// แนะนำ 0.2 ถึง 0.4 สำหรับการเลี้ยวโค้งแคบๆ
	float turn_ratio = 0.3f;

	// คำนวณระยะทางของล้อนอกโค้ง (Outer Wheel)
	// สมมติว่าอยากให้ล้อนอกวิ่งระยะเท่ากับส่วนโค้งที่ต้องการ
	// Arc = (deg/360) * PI * Base * 1.5 (คูณเผื่อวงเลี้ยวหน่อย)
	float arc_mm = ((float) abs(degrees) / 360.0f) * PI * (WHEEL_BASE * 1000.0f)
			* 1.5f;
	long outer_steps = (long) (arc_mm * STEPS_PER_MM);
	long inner_steps = (long) (outer_steps * turn_ratio);

	if (degrees >= 0) {
		// เลี้ยวขวา -> ล้อซ้าย(นอก)วิ่งเยอะ, ล้อขวา(ใน)วิ่งน้อย
		Log_Debug("TURN RIGHT (Curve)\n");
		Move_Engine(outer_steps, inner_steps);
	} else {
		// เลี้ยวซ้าย -> ล้อขวา(นอก)วิ่งเยอะ, ล้อซ้าย(ใน)วิ่งน้อย
		Log_Debug("TURN LEFT (Curve)\n");
		Move_Engine(inner_steps, outer_steps);
	}

	Send_Response("DONE\n");
}

void Execute_Stop(void) {
	HAL_GPIO_WritePin(GPIOA, EN_Pin, GPIO_PIN_SET); // Disable Motor
	Send_Response("STOP\n");
}

void Parse_Command(char *cmd) {
	char debug_msg[64];
	sprintf(debug_msg, "CMD: %s\n", cmd);
	Log_Debug(debug_msg);

	char type = cmd[0];
	int value = 0;
	char *comma = strchr(cmd, ',');
	if (comma)
		value = atoi(comma + 1);

	switch (type) {
	case 'M':
		Execute_Move(value);
		break;
	case 'T':
		Execute_Turn(value);
		break;
	case 'S':
		Execute_Stop();
		break;
	default:
		Send_Response("ERR\n");
		break;
	}
}

// --- 🔥 FIX: RACE CONDITION SOLVED ---
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance == USART1) {
		if (rx_byte == '\n' || rx_byte == '\r') {
			// รับเฉพาะตอนที่มีข้อมูลแล้วเท่านั้น (กัน \n เบิ้ล)
			if (rx_index > 0) {
				rx_buffer[rx_index] = '\0';
				cmd_ready = 1;
				rx_index = 0;
			}
		} else if (rx_index < RX_BUFFER_SIZE - 1) {
			rx_buffer[rx_index++] = rx_byte;
		}
		HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
	}
}
/* USER CODE END 4 */

// --- System Config (Standard CubeMX) ---
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
	HAL_RCC_OscConfig(&RCC_OscInitStruct);
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
	HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

static void MX_USART1_UART_Init(void) {
	huart1.Instance = USART1;
	huart1.Init.BaudRate = 115200;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	HAL_UART_Init(&huart1);
}
static void MX_USART2_UART_Init(void) {
	huart2.Instance = USART2;
	huart2.Init.BaudRate = 115200;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	HAL_UART_Init(&huart2);
}
static void MX_USART3_UART_Init(void) {
	huart3.Instance = USART3;
	huart3.Init.BaudRate = 115200;
	huart3.Init.WordLength = UART_WORDLENGTH_8B;
	huart3.Init.StopBits = UART_STOPBITS_1;
	huart3.Init.Parity = UART_PARITY_NONE;
	huart3.Init.Mode = UART_MODE_TX_RX;
	HAL_UART_Init(&huart3);
}
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	HAL_GPIO_WritePin(GPIOA,
	EN_Pin | STEP_R_Pin | DIR_R_Pin | STEP_L_Pin | DIR_L_Pin, GPIO_PIN_RESET);
	GPIO_InitStruct.Pin = EN_Pin | DIR_R_Pin | DIR_L_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	GPIO_InitStruct.Pin = STEP_R_Pin | STEP_L_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}
void Error_Handler(void) {
	__disable_irq();
	while (1) {
	}
}
