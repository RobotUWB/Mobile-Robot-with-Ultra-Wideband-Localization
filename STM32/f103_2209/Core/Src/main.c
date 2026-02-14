/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : ROS-Style Velocity Control (Differential Drive) - FIXED
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
//Address ของ ICM-20948 (SPI)
#define ICM_REG_BANK_SEL    0x7F
#define ICM_REG_PWR_MGMT_1  0x06
#define ICM_REG_GYRO_ZOUT_H 0x37
#define ICM_REG_WHO_AM_I    0x00
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi2;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
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
uint32_t last_imu_time = 0;
int16_t raw_gyro_z = 0;
float gyro_z_dps = 0;
float gyro_z_offset = 0;

float robot_heading = 0.0f;   // มุมสะสม (Heading)
uint32_t prev_imu_time = 0;   // เวลาล่าสุดที่คำนวณ (ใช้ micros)
<<<<<<< HEAD
=======

uint32_t last_cmd_time = 0;
uint8_t is_timeout = 0;
>>>>>>> parent of 8ab0966 (Revert "Merge branch 'udev' of https://github.com/RobotUWB/Mobile-Robot-with-Ultra-Wideband-Localization into Ice")
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_SPI2_Init(void);
/* USER CODE BEGIN PFP */
void Parse_Command(char *cmd);
void TMC2209_Init(void);

void ICM_Init(void);
void ICM_ReadGyroZ(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void Force_UART3_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	// 1. 🔥 เปิด Clock สำคัญ 2 ตัว (คราวที่แล้วลืมบรรทัดที่ 2 ขอโทษครับ!)
	__HAL_RCC_GPIOB_CLK_ENABLE();// ไฟเลี้ยงขา PB
	__HAL_RCC_USART3_CLK_ENABLE();  // ไฟเลี้ยงสมอง UART3 <--- ตัวนี้สำคัญมาก!

	// 2. ตั้งค่าขา PB10 (TX)
	GPIO_InitStruct.Pin = GPIO_PIN_10;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP; // โหมดส่งข้อมูล
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}
// --- 🕒 TIME KEEPER (จับเวลาละเอียดระดับ us) ---
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

// --- 🏎️ KINEMATICS ENGINE (ทำงานตลอดเวลา) ---
void Run_Motors_Loop(void) {

	// 1. คำนวณความเร็วเป้าหมายของแต่ละล้อ (Differential Drive)
	// V_L = v - w
	// V_R = v + w
	float target_v_L = target_v - target_w;
	float target_v_R = target_v + target_w;

	// 2. Ramp Generator (Soft Start/Stop) - อัปเดตทุก 1ms
	if (micros() - last_update_time > 1000) {
		last_update_time = micros();

		// Soft Start L
		if (current_v_L < target_v_L)
			current_v_L += ACCEL;
		if (current_v_L > target_v_L)
			current_v_L -= ACCEL;
		if (abs(current_v_L - target_v_L) < ACCEL)
			current_v_L = target_v_L;

		// Soft Start R
		if (current_v_R < target_v_R)
			current_v_R += ACCEL;
		if (current_v_R > target_v_R)
			current_v_R -= ACCEL;
		if (abs(current_v_R - target_v_R) < ACCEL)
			current_v_R = target_v_R;
	}

	// 3. Step Generator (Motor L)
	if (abs(current_v_L) > 5.0f) {
		// ตั้งทิศทาง
		HAL_GPIO_WritePin(DIR_L_GPIO_Port, DIR_L_Pin,
				(current_v_L > 0) ? GPIO_PIN_SET : GPIO_PIN_RESET);

		// คำนวณ Delay
		uint32_t step_delay = (uint32_t) (1000000.0f
				/ (abs(current_v_L) * STEPS_PER_MM));

		if (micros() - last_step_time_L >= step_delay) {
			last_step_time_L = micros();
			HAL_GPIO_WritePin(STEP_L_GPIO_Port, STEP_L_Pin, GPIO_PIN_SET);
			for (volatile int i = 0; i < 10; i++)
				__NOP(); // Pulse width
			HAL_GPIO_WritePin(STEP_L_GPIO_Port, STEP_L_Pin, GPIO_PIN_RESET);
		}
	}

	// 4. Step Generator (Motor R)
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

// --- ICM-20948 SPI Functions ---
void ICM_SelectBank(uint8_t bank) {
	uint8_t tx[2] = { ICM_REG_BANK_SEL & 0x7F, (bank << 4) };
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET); // CS Low
	HAL_SPI_Transmit(&hspi2, tx, 2, 10);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);   // CS High
}

uint8_t ICM_ReadByte(uint8_t reg) {
	uint8_t tx_data[2];
	uint8_t rx_data[2];

	// Byte 1: ส่ง Address + Read Bit (0x80)
	tx_data[0] = reg | 0x80;
	// Byte 2: ส่ง Dummy (0x00) เพื่อสร้าง Clock ให้เซนเซอร์ส่งข้อมูลกลับมา
	tx_data[1] = 0x00;

	// 1. ดึง CS ลงเพื่อเริ่มคุย
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);

	// 2. ส่งและรับพร้อมกัน 2 Byte
	// (จังหวะแรกเราส่ง Address เซนเซอร์จะยังไม่ตอบ)
	// (จังหวะสองเราส่ง Dummy เซนเซอร์จะตอบข้อมูลมาใส่ใน rx_data[1])
	HAL_SPI_TransmitReceive(&hspi2, tx_data, rx_data, 2, 100);

	// 3. ยก CS ขึ้นจบการคุย
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);

	// ข้อมูลที่แท้จริงจะอยู่ที่ Byte ที่ 2
	return rx_data[1];
}

void ICM_WriteByte(uint8_t reg, uint8_t data) {
	uint8_t tx[2] = { reg & 0x7F, data };
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET); // CS Low
	HAL_SPI_Transmit(&hspi2, tx, 2, 10);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);   // CS High
}

void ICM_Init(void) {
	// 1. Reset
	ICM_SelectBank(0);
	HAL_Delay(10);
	ICM_WriteByte(ICM_REG_PWR_MGMT_1, 0x80); // Hard Reset
	HAL_Delay(100);

	// 2. Wake Up
	ICM_WriteByte(ICM_REG_PWR_MGMT_1, 0x01); // Auto Clock Select
	HAL_Delay(100);

	// 3. 🔥 เปิด DLPF (Low Pass Filter) เพื่อลด Noise 🔥
	// ต้องสลับไป Bank 2 เพื่อแก้ GYRO_CONFIG_1
	ICM_SelectBank(2);
	// GYRO_CONFIG_1 (Addr 0x01)
	// Bit[0] = 0 (Enable DLPF)
	// Bit[5:3] = 3 (Bandwidth ~50Hz) -> กรองความถี่สูงออก ค่าจะนิ่งขึ้น
	ICM_WriteByte(0x01, 0x19); // 00011001b

	// กลับมา Bank 0
	ICM_SelectBank(0);
	HAL_Delay(100);

	// 4. Debug: Read ID
	uint8_t whoami = ICM_ReadByte(ICM_REG_WHO_AM_I);
	char log[32];
	sprintf(log, "ICM ID: 0x%02X (Filter ON)\n", whoami);
	HAL_UART_Transmit(&huart2, (uint8_t*) log, strlen(log), 100);
}

void ICM_ReadGyroZ(void) {
	ICM_SelectBank(0);
	uint8_t h = ICM_ReadByte(ICM_REG_GYRO_ZOUT_H);
	uint8_t l = ICM_ReadByte(ICM_REG_GYRO_ZOUT_H + 1);

	raw_gyro_z = (int16_t) ((h << 8) | l);

	// แปลงเป็น dps แล้วลบ Offset ออก!
	gyro_z_dps = ((float) raw_gyro_z / 131.0f) - gyro_z_offset;
}
void ICM_Calibrate(void) {
	char msg[] = "Calibrating Gyro... KEEP STILL!\n";
	HAL_UART_Transmit(&huart2, (uint8_t*) msg, strlen(msg), 100);

	float sum = 0;
	int samples = 200; // อ่าน 200 ครั้งเพื่อหาค่าเฉลี่ย

	for (int i = 0; i < samples; i++) {
		ICM_ReadGyroZ(); // อ่านค่า
		sum += gyro_z_dps; // บวกสะสม (ยังไม่ลบ offset)
		HAL_Delay(5);
	}

	gyro_z_offset = sum / samples; // หาค่าเฉลี่ยความผิดพลาด

	char log[64];
	sprintf(log, "Done! Offset: %.2f\n", gyro_z_offset);
	HAL_UART_Transmit(&huart2, (uint8_t*) log, strlen(log), 100);
}
void Update_Heading(void) {
<<<<<<< HEAD
    // 1. อ่านค่า Gyro ปัจจุบัน (และลบ Offset ที่ Calibrate แล้ว)
    ICM_ReadGyroZ();

    // 2. คำนวณเวลาที่ผ่านไป (dt) เป็นวินาที
    uint32_t now = micros();
    float dt = (now - prev_imu_time) / 1000000.0f; // แปลง us เป็น s
    prev_imu_time = now;

    // 3. ป้องกันบั๊กเวลา dt กระโดด (เช่น รอบแรกสุด)
    if (dt > 1.0f) dt = 0;

    // 4. คำนวณมุมสะสม (Integration)
    // ถ้า Gyro นิ่งจริง (น้อยกว่า 0.05 dps) ไม่ต้องบวก (Deadband) เพื่อตัด Noise
    if (fabsf(gyro_z_dps) > 1.5f) {
        robot_heading += gyro_z_dps * dt;
    }
=======
	// 1. อ่านค่า Gyro ปัจจุบัน (และลบ Offset ที่ Calibrate แล้ว)
	ICM_ReadGyroZ();

	// 2. คำนวณเวลาที่ผ่านไป (dt) เป็นวินาที
	uint32_t now = micros();
	float dt = (now - prev_imu_time) / 1000000.0f; // แปลง us เป็น s
	prev_imu_time = now;

	// 3. ป้องกันบั๊กเวลา dt กระโดด (เช่น รอบแรกสุด)
	if (dt > 1.0f)
		dt = 0;

	// 4. คำนวณมุมสะสม (Integration)
	// ถ้า Gyro นิ่งจริง (น้อยกว่า 0.05 dps) ไม่ต้องบวก (Deadband) เพื่อตัด Noise
	if (fabsf(gyro_z_dps) > 1.5f) {
		robot_heading += gyro_z_dps * dt;
	}
>>>>>>> parent of 8ab0966 (Revert "Merge branch 'udev' of https://github.com/RobotUWB/Mobile-Robot-with-Ultra-Wideband-Localization into Ice")
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
	MX_SPI2_Init();
	/* USER CODE BEGIN 2 */
	HAL_Delay(100);
	ICM_Init();
	HAL_Delay(100);
	ICM_Calibrate();

	DWT_Init(); // เริ่มตัวจับเวลาละเอียด
	HAL_Delay(1000);
	TMC2209_Init();

	HAL_GPIO_WritePin(GPIOA, EN_Pin, GPIO_PIN_RESET);

	// Start Receiving
	HAL_UART_Receive_IT(&huart1, &rx_byte, 1);

	char msg[] = "\nROS-STYLE READY\n";
	HAL_UART_Transmit(&huart2, (uint8_t*) msg, strlen(msg), 100);
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
<<<<<<< HEAD
=======
		// 1. รับคำสั่ง
>>>>>>> parent of 8ab0966 (Revert "Merge branch 'udev' of https://github.com/RobotUWB/Mobile-Robot-with-Ultra-Wideband-Localization into Ice")
		if (cmd_ready) {
			cmd_ready = 0;
			Parse_Command((char*) rx_buffer);
		}

<<<<<<< HEAD
		if (HAL_GetTick() - last_imu_time > 10) {
			last_imu_time = HAL_GetTick();

			Update_Heading(); // เรียกฟังก์ชันคำนวณมุม

			//print
			static uint8_t print_count = 0;
			if (++print_count >= 10) {
				print_count = 0;
				char log[64];
				// โชว์ทั้งความเร็ว (GZ) และมุมสะสม (ANG)
				sprintf(log, "GZ: %.2f | ANG: %.2f\n", gyro_z_dps,
						robot_heading);
				HAL_UART_Transmit(&huart2, (uint8_t*) log, strlen(log), 10);
			}
		}

=======
		// 2. ระบบ Watchdog (แยก Block ออกมาให้ชัดเจน)
		if (HAL_GetTick() - last_cmd_time > 200) {
			// ถ้าเกิน 0.5 วิ และหุ่นยังมีความเร็วอยู่
			if ((target_v != 0 || target_w != 0) && is_timeout == 0) {
				// สั่งเบรกฉุกเฉินทันที
				target_v = 0;
				target_w = 0;
				current_v_L = 0;
				current_v_R = 0;

				is_timeout = 1; // ล็อกไว้

				// ส่งข้อความแจ้งเตือนไปที่ Debug
				char timeout_msg[] = "WARNING: Signal Lost! Auto Stopped.\n";
				HAL_UART_Transmit(&huart2, (uint8_t*) timeout_msg,
						strlen(timeout_msg), 10);
			}
		}

		// 3. อัปเดตมุม (ให้ทำงานตลอดเวลา ไม่ต้องรอ Timeout)
		static uint32_t last_imu_time = 0;
		if (HAL_GetTick() - last_imu_time >= 10) { // อ่านทุก 10ms (100Hz)
			last_imu_time = HAL_GetTick();
			Update_Heading();
		}

		// 4. Print Log ออก Debug (ให้ทำงานตลอดเวลา)
		static uint32_t last_print_time = 0;
		if (HAL_GetTick() - last_print_time >= 500) { // ปริ้นทุก 500ms จะชัวร์กว่านับลูป
			last_print_time = HAL_GetTick();
			char log[64];
			sprintf(log, "GZ: %.2f | ANG: %.2f\n", gyro_z_dps, robot_heading);
			HAL_UART_Transmit(&huart2, (uint8_t*) log, strlen(log), 10);
		}

		// 5. ส่ง Telemetry ไป ESP32
>>>>>>> parent of 8ab0966 (Revert "Merge branch 'udev' of https://github.com/RobotUWB/Mobile-Robot-with-Ultra-Wideband-Localization into Ice")
		static uint32_t last_telemetry = 0;
		if (HAL_GetTick() - last_telemetry > 200) {
			last_telemetry = HAL_GetTick();

<<<<<<< HEAD
			char tx_buf[32];
			// ส่งรูปแบบ "A=มุม\n" (เช่น A=90.50)
			sprintf(tx_buf, "A=%.2f\n", robot_heading);
			HAL_UART_Transmit(&huart1, (uint8_t*) tx_buf, strlen(tx_buf), 10);
		}
=======
			// สร้างข้อความตั้งต้น
			char payload[32];
			sprintf(payload, "A=%.2f", robot_heading);

			// คำนวณ Checksum
			uint8_t cs = 0;
			for (int i = 0; i < strlen(payload); i++) {
				cs ^= payload[i];
			}

			// นำข้อความมารวมกับ * และค่า Checksum (แบบ Hex)
			char tx_buf[64];
			sprintf(tx_buf, "%s*%02X\n", payload, cs);
			HAL_UART_Transmit(&huart1, (uint8_t*) tx_buf, strlen(tx_buf), 10);
		}

		// 6. ขับมอเตอร์
>>>>>>> parent of 8ab0966 (Revert "Merge branch 'udev' of https://github.com/RobotUWB/Mobile-Robot-with-Ultra-Wideband-Localization into Ice")
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
 * @brief SPI2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI2_Init(void) {

	/* USER CODE BEGIN SPI2_Init 0 */

	/* USER CODE END SPI2_Init 0 */

	/* USER CODE BEGIN SPI2_Init 1 */

	/* USER CODE END SPI2_Init 1 */
	/* SPI2 parameter configuration*/
	hspi2.Instance = SPI2;
	hspi2.Init.Mode = SPI_MODE_MASTER;
	hspi2.Init.Direction = SPI_DIRECTION_2LINES;
	hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi2.Init.NSS = SPI_NSS_SOFT;
	hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
	hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi2.Init.CRCPolynomial = 10;
	if (HAL_SPI_Init(&hspi2) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN SPI2_Init 2 */

	/* USER CODE END SPI2_Init 2 */

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
	EN_Pin | STEP_R_Pin | DIR_R_Pin | STEP_L_Pin | DIR_L_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);

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

	/*Configure GPIO pin : PB12 */
	GPIO_InitStruct.Pin = GPIO_PIN_12;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/* USER CODE BEGIN MX_GPIO_Init_2 */

	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

// --- TMC Helper Functions (ที่หายไป ผมเติมให้แล้วครับ) ---
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
	// ส่งมันทุก Address เลย (0, 1, 2, 3) กันพลาด!
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
	// 1. 🟢 แก้ความสมูท: เปลี่ยนจาก SpreadCycle เป็น StealthChop
	// ของเดิม: 0xC0 (SpreadCycle = ON) -> เสียงดัง, สั่น
	// ของใหม่: 0x40 (SpreadCycle = OFF) -> StealthChop = ON (นิ่ม, เงียบ)
	TMC_Write(0x00, 0x00000040); // GCONF
	HAL_Delay(5);

	// 2. Microstepping (1/16) - เหมือนเดิม
	TMC_Write(0x6C, 0x14000053); // CHOPCONF
	HAL_Delay(5);

	// 3. 🔥 เพิ่มกระแส (IRUN): แก้ตรงเลขหลักที่ 3 จากท้าย
	// ของเดิม: 0x00061401 (14 hex = 20 dec) -> ประมาณ 60%
	// ของใหม่: 0x00061C01 (1C hex = 28 dec) -> ประมาณ 90% (แรงบิดสูงขึ้น)
	// หมายเหตุ: IHOLD=1 (ตัวท้าย) ยังคงไว้ที่ 1 เพื่อให้ตอนหยุดไม่ร้อน
	TMC_Write(0x10, 0x00061C01); // IHOLD_IRUN
	HAL_Delay(5);
}

// --- 🎮 COMMAND PARSER (New Protocol) ---
// เปลี่ยนจาก M, T เป็น V (Velocity)
// V,Linear_Speed,Turn_Speed
void Parse_Command(char *cmd) {
<<<<<<< HEAD
	char type = cmd[0];

	if (type == 'V') { // Velocity Command
		char *p = strtok(cmd + 2, ",");
		if (p) {
			target_v = atof(p); // ความเร็วทางตรง
			p = strtok(NULL, ",");
			if (p)
				target_w = atof(p); // ความเร็วเลี้ยว (Diff)
=======
	// 1. หาตำแหน่งของตัวดอกจัน (*)
	char *asterisk = strchr(cmd, '*');
	if (asterisk == NULL)
		return; // ถ้าไม่มี * แสดงว่ารูปแบบผิด หรือโดนกวนจนเละ ทิ้งทันที

	// 2. คำนวณ Checksum จากข้อความที่ได้รับ
	uint8_t calc_cs = 0;
	for (char *p = cmd; p < asterisk; p++) {
		calc_cs ^= *p; // สัญลักษณ์ ^ คือ XOR
	}

	// 3. อ่าน Checksum 2 หลักสุดท้ายที่ ESP32 แนบมา
	unsigned int recv_cs;
	if (sscanf(asterisk + 1, "%x", &recv_cs) != 1)
		return;

	// 4. ยืนยันความถูกต้อง
	if (calc_cs != recv_cs) {
		// ถ้าไม่ตรงกัน แสดงว่ามี Noise ในสาย UART
		HAL_UART_Transmit(&huart2, (uint8_t*) "ERR: Noise Detected!\n", 21, 10);
		return;
	}

	// 5. ตัดข้อความตรง * ทิ้ง เพื่อให้โค้ดแยกคำสั่งเดิมทำงานต่อได้ปกติ
	*asterisk = '\0';

	char type = cmd[0];

	// รีเซ็ต Watchdog (เฉพาะคำสั่งที่ผ่าน Checksum มาแล้วเท่านั้น)
	if (type == 'V' || type == 'S' || type == 'H') {
		last_cmd_time = HAL_GetTick();
		is_timeout = 0;
	}

	// ถ้าเป็นคำสั่ง H (Heartbeat) ไม่ต้องทำอะไร ให้จบฟังก์ชันเลย
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
>>>>>>> parent of 8ab0966 (Revert "Merge branch 'udev' of https://github.com/RobotUWB/Mobile-Robot-with-Ultra-Wideband-Localization into Ice")
		}

		char log[64];
		sprintf(log, "CMD: V=%.0f, W=%.0f\n", target_v, target_w);
		HAL_UART_Transmit(&huart2, (uint8_t*) log, strlen(log), 10);
		HAL_UART_Transmit(&huart1, (uint8_t*) "OK\n", 3, 10);
<<<<<<< HEAD
	} else if (type == 'S') { // Stop Command
		target_v = 0;
		target_w = 0;
		// หยุดทันที (Force Stop)
=======
	} else if (type == 'S') {
		target_v = 0;
		target_w = 0;
>>>>>>> parent of 8ab0966 (Revert "Merge branch 'udev' of https://github.com/RobotUWB/Mobile-Robot-with-Ultra-Wideband-Localization into Ice")
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
<<<<<<< HEAD
#endif /* USE_FULL_ASSERT */
=======
#endif /* USE_FULL_ASSERT */
>>>>>>> parent of 8ab0966 (Revert "Merge branch 'udev' of https://github.com/RobotUWB/Mobile-Robot-with-Ultra-Wideband-Localization into Ice")
