/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : ROS-Style Velocity Control (Differential Drive) + BNO055
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
// --- ⚙️ ROBOT CONFIG ---
#define STEPS_PER_MM        10      // ค่าที่คุณจูนแล้ว
#define WHEEL_BASE_MM       295.0f  // ความกว้างฐานล้อ (mm)
#define MAX_SPEED_MM_S      600.0f  // จำกัดความเร็วสูงสุด
#define ACCEL               2.0f    // ความเร่ง (Ramp) ยิ่งน้อยยิ่งนิ่ม (0.1 - 5.0)

// UART Buffer
#define RX_BUFFER_SIZE      64

// --- BNO055 Config ---
#define BNO055_I2C_ADDR         (0x28 << 1)
#define BNO055_CONFIG_MODE      0x00
#define BNO055_IMU_MODE         0x08  // โหมด IMU (Gyro + Accel) ตัดแม่เหล็กออกกันกวน
#define BNO055_OPR_MODE_ADDR    0x3D
#define BNO055_EULER_H_LSB_ADDR 0x1A
#define BNO055_CALIB_DATA_ADDR  0x55
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2; //debug
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
uint8_t rx_buffer[RX_BUFFER_SIZE];
uint8_t rx_byte;
uint8_t rx_index = 0;
volatile uint8_t cmd_ready = 0;

// --- ROS-Style Variables ---
float target_v = 0;      // Linear Velocity (mm/s)  [เดินหน้า-ถอยหลัง]
float target_w = 0;      // Angular Velocity (diff) [เลี้ยวซ้าย-ขวา]

float current_v_L = 0;   // ความเร็วปัจจุบันล้อซ้าย
float current_v_R = 0;   // ความเร็วปัจจุบันล้อขวา

// Timing Variables
uint32_t last_step_time_L = 0;
uint32_t last_step_time_R = 0;
uint32_t last_update_time = 0;

// IMU Variables
float robot_heading = 0.0f;   // มุมสะสม (Heading) จาก BNO055

uint32_t last_cmd_time = 0;
uint8_t is_timeout = 0;

// 🔥 เอาค่า 22 ตัวที่ดูดมาได้ มาใส่แทนที่ตรงนี้นะครับ 🔥
uint8_t saved_calib_profile[22] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00 };
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */
void Parse_Command(char *cmd);
void TMC2209_Init(void);
void BNO055_Init(void);
void BNO055_ReadEuler(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void Force_UART3_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_USART3_CLK_ENABLE();

	GPIO_InitStruct.Pin = GPIO_PIN_10;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

// --- 🕒 TIME KEEPER ---
void DWT_Init(void) {
	if (!(CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk)) {
		CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
		DWT->CYCCNT = 0;
		DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
	}
}

uint32_t micros(void) {
	return DWT->CYCCNT / 72; // 72MHz
}

// --- 🏎️ KINEMATICS ENGINE ---
void Run_Motors_Loop(void) {
	float target_v_L = target_v - target_w;
	float target_v_R = target_v + target_w;

	if (micros() - last_update_time > 1000) {
		last_update_time = micros();

		if (current_v_L < target_v_L)
			current_v_L += ACCEL;
		if (current_v_L > target_v_L)
			current_v_L -= ACCEL;
		if (abs(current_v_L - target_v_L) < ACCEL)
			current_v_L = target_v_L;

		if (current_v_R < target_v_R)
			current_v_R += ACCEL;
		if (current_v_R > target_v_R)
			current_v_R -= ACCEL;
		if (abs(current_v_R - target_v_R) < ACCEL)
			current_v_R = target_v_R;
	}

	if (abs(current_v_L) > 5.0f) {
		HAL_GPIO_WritePin(DIR_L_GPIO_Port, DIR_L_Pin,
				(current_v_L > 0) ? GPIO_PIN_SET : GPIO_PIN_RESET);
		uint32_t step_delay = (uint32_t) (1000000.0f
				/ (abs(current_v_L) * STEPS_PER_MM));
		if (micros() - last_step_time_L >= step_delay) {
			last_step_time_L = micros();
			HAL_GPIO_WritePin(STEP_L_GPIO_Port, STEP_L_Pin, GPIO_PIN_SET);
			for (volatile int i = 0; i < 10; i++)
				__NOP();
			HAL_GPIO_WritePin(STEP_L_GPIO_Port, STEP_L_Pin, GPIO_PIN_RESET);
		}
	}

	if (abs(current_v_R) > 5.0f) {
		HAL_GPIO_WritePin(DIR_R_GPIO_Port, DIR_R_Pin,
				(current_v_R > 0) ? GPIO_PIN_SET : GPIO_PIN_RESET);
		uint32_t step_delay = (uint32_t) (1000000.0f
				/ (abs(current_v_R) * STEPS_PER_MM));
		if (micros() - last_step_time_R >= step_delay) {
			last_step_time_R = micros();
			HAL_GPIO_WritePin(STEP_R_GPIO_Port, STEP_R_Pin, GPIO_PIN_SET);
			for (volatile int i = 0; i < 10; i++)
				__NOP();
			HAL_GPIO_WritePin(STEP_R_GPIO_Port, STEP_R_Pin, GPIO_PIN_RESET);
		}
	}
}

void BNO055_Init(void) {
	uint8_t mode = BNO055_CONFIG_MODE;

	// 1. เข้า Config Mode (เปลี่ยนเป็นวนลูปรอจนกว่าจะตื่นและตอบรับ)
	while(HAL_I2C_Mem_Write(&hi2c1, BNO055_I2C_ADDR, BNO055_OPR_MODE_ADDR, 1, &mode,
			1, 100) != HAL_OK) {
		char msg[] = "WAITING FOR BNO055 TO WAKE UP...\n";
		HAL_UART_Transmit(&huart2, (uint8_t*) msg, strlen(msg), 10);
		HAL_Delay(100);
	}
	HAL_Delay(50);

	// 2. โหลดค่า Calibration Profile
	HAL_I2C_Mem_Write(&hi2c1, BNO055_I2C_ADDR, BNO055_CALIB_DATA_ADDR, 1,
			saved_calib_profile, 22, 100);
	HAL_Delay(50);

	// 3. เข้า IMU Mode
	mode = BNO055_IMU_MODE;
	HAL_I2C_Mem_Write(&hi2c1, BNO055_I2C_ADDR, BNO055_OPR_MODE_ADDR, 1, &mode,
			1, 100);
	HAL_Delay(50);

	char msg[] = "BNO055 READY (IMU MODE & CALIBRATED)\n";
	HAL_UART_Transmit(&huart2, (uint8_t*) msg, strlen(msg), 100);
}

void I2C_ClearBus(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	__HAL_RCC_GPIOB_CLK_ENABLE();

	// เซ็ตขา SCL (PB6) และ SDA (PB7) เป็นโหมดส่งออก (Output Open-Drain) ชั่วคราว
	GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET); // ปล่อย SDA ให้เป็น High

	// กระตุ้น Clock (SCL) 9 ครั้ง ทิ้งบิตค้างในเซ็นเซอร์ให้ชิปคายคลื่นออกมา
	for (int i = 0; i < 9; i++) {
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
		for(volatile int j=0; j<1000; j++) __NOP(); // หน่วงเวลาเล็กน้อย
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
		for(volatile int j=0; j<1000; j++) __NOP();
	}

	// สร้างฉากจบ (STOP condition: ให้ SDA ขึ้นตอนที่ SCL เป็น High)
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
	for(volatile int j=0; j<1000; j++) __NOP();
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
	for(volatile int j=0; j<1000; j++) __NOP();
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
	for(volatile int j=0; j<1000; j++) __NOP();
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
	for(volatile int j=0; j<1000; j++) __NOP();
}

void BNO055_ReadEuler(void) {
	uint8_t rx_data[2];
	static uint8_t error_count = 0; // สำหรับนับจำนวนครั้งที่ Error

	if (HAL_I2C_Mem_Read(&hi2c1, BNO055_I2C_ADDR, BNO055_EULER_H_LSB_ADDR,
			I2C_MEMADD_SIZE_8BIT, rx_data, 2, 10) == HAL_OK) {
		int16_t raw_heading = (int16_t) ((rx_data[1] << 8) | rx_data[0]);
		robot_heading = raw_heading / 16.0f;
		error_count = 0; // ถ้าระบบอ่านสำเร็จให้รีเซ็ตตัวนับ
	} else {
		error_count++;
	}

	// ถ้าระบบ I2C ค้าง อ่านไม่ได้ติดต่อกัน 50 รอบ -> สั่งกู้คืนระบบใหม่ (Recovery)
	if (error_count > 50) {
		char msg[] = "I2C ERROR: Hard Resetting I2C Bus...\n";
		HAL_UART_Transmit(&huart2, (uint8_t*) msg, strlen(msg), 10);
		
		HAL_I2C_DeInit(&hi2c1);  // ปิด I2C เพื่อเคลียร์สถานะเออเร่อ
		HAL_Delay(5);
		I2C_ClearBus();         // เคลียร์สายให้ Slave ปล่อยขา SDA
		HAL_Delay(5);
		MX_I2C1_Init();         // สั่งตั้งค่า I2C ใหม่ (ฟังก์ชันเดิมที่แอตทริคสร้างไว้)
		HAL_Delay(10);
		BNO055_Init();          // สั่ง Config ค่าเริ่มต้นของ BNO055 ใหม่อีกครั้ง
		error_count = 0;
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
	MX_USART2_UART_Init();
	MX_TIM2_Init();
	MX_I2C1_Init();
	/* USER CODE BEGIN 2 */
	HAL_Delay(800); // 💥 เพิ่มเวลาหน่วงให้ BNO055 บูตเสร็จชัวร์ๆ ตอนเปิดเครื่อง

	// เรียกใช้ BNO055 แทน ICM
	BNO055_Init();

	DWT_Init();
	HAL_Delay(1000);

	// เติมบรรทัดนี้เพื่อบังคับเปิดพิน UART3 ให้คุยกับมอเตอร์รู้เรื่อง
	Force_UART3_GPIO_Init();
	TMC2209_Init();

	HAL_GPIO_WritePin(GPIOA, EN_Pin, GPIO_PIN_RESET);

	// Start Receiving
	HAL_UART_Receive_IT(&huart1, &rx_byte, 1);

	char msg[] = "\nROS-STYLE READY (BNO055)\n";
	HAL_UART_Transmit(&huart2, (uint8_t*) msg, strlen(msg), 100);
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		if (cmd_ready) {
			cmd_ready = 0;
			Parse_Command((char*) rx_buffer);
		}

		// Watchdog 200ms
		if (HAL_GetTick() - last_cmd_time > 200) {
			if ((target_v != 0 || target_w != 0) && is_timeout == 0) {
				target_v = 0;
				target_w = 0;
				current_v_L = 0;
				current_v_R = 0;
				is_timeout = 1;

				char timeout_msg[] = "WARNING: Signal Lost! Auto Stopped.\n";
				HAL_UART_Transmit(&huart2, (uint8_t*) timeout_msg,
						strlen(timeout_msg), 10);
			}
		}

		// อ่านค่า BNO055 ทุกๆ 10ms
		static uint32_t last_imu_time_loop = 0;
		if (HAL_GetTick() - last_imu_time_loop >= 10) {
			last_imu_time_loop = HAL_GetTick();
			BNO055_ReadEuler();
		}

		// Print Log ออก Debug
		static uint32_t last_print_time = 0;
		if (HAL_GetTick() - last_print_time >= 500) {
			last_print_time = HAL_GetTick();
			char log[64];
			sprintf(log, "Heading: %.2f\n", robot_heading);
			HAL_UART_Transmit(&huart2, (uint8_t*) log, strlen(log), 10);
		}

		// ส่ง Telemetry ไป ESP32 พร้อม Checksum
		static uint32_t last_telemetry = 0;
		if (HAL_GetTick() - last_telemetry > 200) {
			last_telemetry = HAL_GetTick();

			char payload[32];
			sprintf(payload, "A=%.2f", robot_heading);

			uint8_t cs = 0;
			for (int i = 0; i < strlen(payload); i++) {
				cs ^= payload[i];
			}

			char tx_buf[64];
			sprintf(tx_buf, "%s*%02X\n", payload, cs);
			HAL_UART_Transmit(&huart1, (uint8_t*) tx_buf, strlen(tx_buf), 10);
		}

		// ขับมอเตอร์
		Run_Motors_Loop();
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
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
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void) {

	/* USER CODE BEGIN I2C1_Init 0 */

	/* USER CODE END I2C1_Init 0 */

	/* USER CODE BEGIN I2C1_Init 1 */

	/* USER CODE END I2C1_Init 1 */
	hi2c1.Instance = I2C1;
	hi2c1.Init.ClockSpeed = 400000;
	hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_16_9;
	hi2c1.Init.OwnAddress1 = 0;
	hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.OwnAddress2 = 0;
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN I2C1_Init 2 */

	/* USER CODE END I2C1_Init 2 */

}

/**
 * @brief TIM2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM2_Init(void) {

	/* USER CODE BEGIN TIM2_Init 0 */

	/* USER CODE END TIM2_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };

	/* USER CODE BEGIN TIM2_Init 1 */

	/* USER CODE END TIM2_Init 1 */
	htim2.Instance = TIM2;
	htim2.Init.Prescaler = 71;
	htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim2.Init.Period = 100;
	htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM2_Init 2 */

	/* USER CODE END TIM2_Init 2 */

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
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void) {

	/* USER CODE BEGIN USART2_Init 0 */

	/* USER CODE END USART2_Init 0 */

	/* USER CODE BEGIN USART2_Init 1 */

	/* USER CODE END USART2_Init 1 */
	huart2.Instance = USART2;
	huart2.Init.BaudRate = 115200;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart2) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART2_Init 2 */

	/* USER CODE END USART2_Init 2 */

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
			EN_Pin | STEP_R_Pin | DIR_R_Pin | STEP_L_Pin | DIR_L_Pin,
			GPIO_PIN_RESET);

	/*Configure GPIO pins : EN_Pin DIR_R_Pin DIR_L_Pin */
	GPIO_InitStruct.Pin = EN_Pin | DIR_R_Pin | DIR_L_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pins : STEP_R_Pin STEP_L_Pin */
	GPIO_InitStruct.Pin = STEP_R_Pin | STEP_L_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* USER CODE BEGIN MX_GPIO_Init_2 */

	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

// --- TMC Helper Functions ---
uint8_t TMC_CRC8(uint8_t *data, size_t length) {
	uint8_t crc = 0, currentByte;
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

void TMC_Write_To_Addr(uint8_t addr, uint8_t reg, uint32_t data) {
	uint8_t tx[8] = { 0x05, addr, reg | 0x80, (data >> 24) & 0xFF, (data >> 16)
			& 0xFF, (data >> 8) & 0xFF, data & 0xFF, 0 };
	tx[7] = TMC_CRC8(&tx[1], 6);
	HAL_UART_Transmit(&huart3, tx, 8, 100);
}

void TMC_Write(uint8_t reg, uint32_t data) {
	TMC_Write_To_Addr(0x00, reg, data);
	HAL_Delay(1);
	TMC_Write_To_Addr(0x01, reg, data);
	HAL_Delay(1);
	TMC_Write_To_Addr(0x02, reg, data);
	HAL_Delay(1);
	TMC_Write_To_Addr(0x03, reg, data);
	HAL_Delay(1);
}
void TMC2209_Init(void) {
	TMC_Write(0x00, 0x00000040); // GCONF (StealthChop)
	HAL_Delay(5);
	TMC_Write(0x6C, 0x14000053); // CHOPCONF
	HAL_Delay(5);
	TMC_Write(0x10, 0x00061C01); // IHOLD_IRUN
	HAL_Delay(5);
}

// --- 🎮 COMMAND PARSER ---
void Parse_Command(char *cmd) {
	char *asterisk = strchr(cmd, '*');
	if (asterisk == NULL)
		return;

	uint8_t calc_cs = 0;
	for (char *p = cmd; p < asterisk; p++) {
		calc_cs ^= *p;
	}

	unsigned int recv_cs;
	if (sscanf(asterisk + 1, "%x", &recv_cs) != 1)
		return;

	if (calc_cs != recv_cs) {
		HAL_UART_Transmit(&huart2, (uint8_t*) "ERR: Noise Detected!\n", 21, 10);
		return;
	}

	*asterisk = '\0';
	char type = cmd[0];

	if (type == 'V' || type == 'S' || type == 'H') {
		last_cmd_time = HAL_GetTick();
		is_timeout = 0;
	}

	if (type == 'H') {
		return;
	}
	if (type == 'V') {
		char *p = strtok(cmd + 2, ",");
		if (p) {
			target_v = atof(p);
			p = strtok(NULL, ",");
			if (p)
				target_w = atof(p);
		}

		char log[64];
		sprintf(log, "CMD: V=%.0f, W=%.0f\n", target_v, target_w);
		HAL_UART_Transmit(&huart2, (uint8_t*) log, strlen(log), 10);
		HAL_UART_Transmit(&huart1, (uint8_t*) "OK\n", 3, 10);
	} else if (type == 'S') {
		target_v = 0;
		target_w = 0;
		current_v_L = 0;
		current_v_R = 0;
		HAL_UART_Transmit(&huart1, (uint8_t*) "STOP\n", 5, 10);
	}
}

// --- UART Interrupt ---
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance == USART1) {
		if (rx_byte == '\n' || rx_byte == '\r') {
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

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	__disable_irq();
	while (1) {
	}
	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  * where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
