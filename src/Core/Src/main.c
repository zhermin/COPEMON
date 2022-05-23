  /******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  * (c) EE2028 Teaching Team
  ******************************************************************************/

// TAM ZHER MIN (A0206262N)
// ANGELINA GRACE (A0201165W)

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stdio.h"
#include "string.h"
#include "math.h"
#include "../../Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01_accelero.h"
#include "../../Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01_gyro.h"
#include "../../Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01_magneto.h"
#include "../../Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01_psensor.h"
#include "../../Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01_tsensor.h"
#include "../../Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01_hsensor.h"
#include "../../Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01.h"
#include "../../Drivers/BSP/Components/lsm6dsl/lsm6dsl.h"

//extern void initialise_monitor_handles(void);	// for semi-hosting support (printf)
static void LSM6DSL_Interrupt_Init(void);
static void LSM6DSL_Init(void);
static void UART1_Init(void);
UART_HandleTypeDef huart1;

// CORE VARIABLES DEFINITIONS & FOR EXTI ACCESS
volatile int icu_mode = 0;
int btn_count = 0;
int counter = 0;
int icu_timer = 0;
int msg_mode_flag = 0;

// GLOBAL WARNING FLAGS & FOR EXTI ACCESS
int temp_warn = 0;
int accel_warn = 0;
int gyro_warn = 0;
int mag_warn = 0;
int breath_warn = 0;

// ACCELEROMETER GLOBAL MESSAGE DEFINITION & FOR EXTI ACCESS
char msg_accel[] = "Fall detected!\r\n";

// SYSTICK AND DOUBLE PRESS DETECTION DURING EXTI
uint32_t tickstart;
uint32_t btn_wait = 1000;

int main(void)
{
//	initialise_monitor_handles(); // for semi-hosting support (printf)

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* Peripheral initializations */
	BSP_TSENSOR_Init();
	BSP_ACCELERO_Init();
	BSP_GYRO_Init();
	BSP_MAGNETO_Init();
	BSP_HSENSOR_Init();
	BSP_PSENSOR_Init();

	BSP_LED_Init(LED2);
	BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

	LSM6DSL_Interrupt_Init();
	LSM6DSL_Init();

	UART1_Init();

	// TEMPERATURE DEFINITIONS
	float temp_data;
	float TEMP_THRESHOLD = 37.8;
	char msg_temp[] = "Fever detected\r\n";
	int temp_initial_warn = 0;

	// ACCELEROMETER DEFINITIONS
	float accel_data[3];
	int16_t accel_data_i16[3];

	// GYROSCOPE DEFINITIONS
	float gyro_twitch;
	int GYRO_THRESHOLD = 1000;
	char msg_gyro[] = "Patient in pain\r\n";

	// MAGNETOMETER DEFINITIONS
	float original_orientation;
	float original_mag[3];
	int16_t original_mag_raw[3];
	float new_mag[3];
	int MAG_THRESHOLD = 200;
	char msg_mag[] = "Check patient's abnormal orientation\r\n";

	// SAVE ORIGINAL MAGNOMETER READINGS FOR REFERENCE LATER
	BSP_MAGNETO_GetXYZ(original_mag_raw);
	for (int i = 0; i < 3; i++)	original_mag[i] = (float)original_mag_raw[i];
	original_orientation = sqrt(pow((float)original_mag[0], 2) + pow((float)original_mag[1], 2) + pow((float)original_mag[2], 2));

	// HUMIDITY SENSOR / BAROMETER DEFINITIONS
	float humidity_data, pressure_data;
	int HUMIDITY_THRESHOLD = 50, PRESSURE_THRESHOLD = 1100;
	char msg_breath[] = "Check patient's breath\r\n";

	while (1) {

		if (temp_warn || accel_warn || gyro_warn || mag_warn || breath_warn) HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);

		if (!icu_mode) {

			char msg_mode[] = "Entering Healthy Mode\r\n\n";
			if (!msg_mode_flag) HAL_UART_Transmit(&huart1, (uint8_t*)msg_mode, strlen(msg_mode), 0xFFFF);
			msg_mode_flag = 1;

		} else {

			char msg_mode[] = "Entering Intensive Care Mode\r\n\n";
			if (msg_mode_flag) HAL_UART_Transmit(&huart1, (uint8_t*)msg_mode, strlen(msg_mode), 0xFFFF);
			msg_mode_flag = 0;

			if (counter % 4 == 0) { // check sensors every 1s

				// GYROSCOPE (PAIN)
				float gyro_data[3];
				BSP_GYRO_GetXYZ(gyro_data);
				gyro_twitch = sqrt(pow(gyro_data[0]/100, 2) + pow(gyro_data[1]/100, 2) + pow(gyro_data[2]/100, 2));
				if (gyro_twitch > GYRO_THRESHOLD) gyro_warn = 1;

				// MAGNETOMETER (ABNORMAL POSITION)
				float new_orientation, change_orientation;
				int16_t new_mag_raw[3];
				BSP_MAGNETO_GetXYZ(new_mag_raw);
				for (int i = 0; i < 3; i++) new_mag[i] = (float)new_mag_raw[i];
				new_orientation = sqrt(pow((float)new_mag[0], 2) + pow((float)new_mag[1], 2) + pow((float)new_mag[2], 2));
				change_orientation = new_orientation - original_orientation;
				if (change_orientation > MAG_THRESHOLD || change_orientation < -MAG_THRESHOLD) mag_warn = 1;

				// HUMIDITY & PRESSURE (RESPIRATORY)
				humidity_data = BSP_HSENSOR_ReadHumidity(); // normal: ~70%
				pressure_data = BSP_PSENSOR_ReadPressure(); // normal: 1015 hPa
				if (humidity_data < HUMIDITY_THRESHOLD || pressure_data > PRESSURE_THRESHOLD) breath_warn = 1;

			}

		}

		if (counter % 4 == 0) { // always check temp and accel every 1s regardless of mode
			// TEMPERATURE (FEVER)
			temp_data = BSP_TSENSOR_ReadTemp();
			if (temp_data > TEMP_THRESHOLD) {
				if (!temp_initial_warn) HAL_UART_Transmit(&huart1, (uint8_t*)msg_temp, strlen(msg_temp), 0xFFFF);
				temp_initial_warn = 1;
				temp_warn = 1;
			}

			// ACCELEROMETER (FALL)
			BSP_ACCELERO_AccGetXYZ(accel_data_i16); // returns 16 bit integers which are 100 * acceleration_in_m/s2.
			for (int i = 0; i < 3; i++) accel_data[i] = (float)accel_data_i16[i] / 100.0f;
		}

		if (counter == 40) {  // send telemetry msg every 10s (40)

			if (temp_warn) HAL_UART_Transmit(&huart1, (uint8_t*)msg_temp, strlen(msg_temp), 0xFFFF);
			if (accel_warn) HAL_UART_Transmit(&huart1, (uint8_t*)msg_accel, strlen(msg_accel), 0xFFFF);
			if (gyro_warn) HAL_UART_Transmit(&huart1, (uint8_t*)msg_gyro, strlen(msg_gyro), 0xFFFF);
			if (mag_warn) HAL_UART_Transmit(&huart1, (uint8_t*)msg_mag, strlen(msg_mag), 0xFFFF);
			if (breath_warn) HAL_UART_Transmit(&huart1, (uint8_t*)msg_breath, strlen(msg_breath), 0xFFFF);

			if (icu_mode) {
				char msg1[64], msg2[64], msg3[64];

				sprintf(msg1, "%03i TEMP: %.2fC     | ACC: %.2fg %.2fg %.2fg\r\n", icu_timer, temp_data, accel_data[0], accel_data[1], accel_data[2]);
				sprintf(msg2, "%03i GYRO: %.2f      | MAG: %.2fmG %.2fmG %.2fmG\r\n", icu_timer, gyro_twitch, new_mag[0], new_mag[1], new_mag[2]);
				sprintf(msg3, "%03i HUMIDITY: %.2f%% | BARO: %.2fhPa\r\n\n", icu_timer, humidity_data, pressure_data);

				HAL_UART_Transmit(&huart1, (uint8_t*)msg1, strlen(msg1), 0xFFFF);
				HAL_UART_Transmit(&huart1, (uint8_t*)msg2, strlen(msg2), 0xFFFF);
				HAL_UART_Transmit(&huart1, (uint8_t*)msg3, strlen(msg3), 0xFFFF);

				icu_timer++;
			}

			counter = 0;
		}

		HAL_Delay(250);	// 250ms loops
		counter++;

	}
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if (GPIO_Pin == GPIO_PIN_13) {
		if (icu_mode == 0) { // healthy mode
			if (btn_count == 0) {
				tickstart = HAL_GetTick();
				btn_count = 1;
			} else {
				if ((HAL_GetTick() - tickstart) < btn_wait) { // double press = trigger icu mode
					HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET); // force led to be off
					temp_warn = 0, accel_warn = 0, gyro_warn = 0, mag_warn = 0, breath_warn = 0; // clear all warnings
					icu_mode = 1; // only way to trigger icu mode
				}
				btn_count = 0;
			}
		} else { // icu mode
			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET); // force led to be off
			temp_warn = 0, accel_warn = 0, gyro_warn = 0, mag_warn = 0, breath_warn = 0; // clear all warnings
			icu_mode = 0; // return to healthy mode
		}
	}

	if (GPIO_Pin == GPIO_PIN_11) {
		HAL_UART_Transmit(&huart1, (uint8_t*)msg_accel, strlen(msg_accel), 0xFFFF);
		accel_warn = 1;
	}
}

static void LSM6DSL_Interrupt_Init(void) {
	GPIO_InitTypeDef gpio_init_structure;

	/* Enable the GPIO D's clock */
	__HAL_RCC_GPIOD_CLK_ENABLE();

    /* Configure LSM6DSL pin as input with External interrupt */
    gpio_init_structure.Pin = LSM6DSL_INT1_EXTI11_Pin;
    gpio_init_structure.Pull = GPIO_PULLUP;
    gpio_init_structure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

    gpio_init_structure.Mode = GPIO_MODE_IT_RISING;

    HAL_GPIO_Init(LSM6DSL_INT1_EXTI11_GPIO_Port, &gpio_init_structure);

    /* Enable and set LSM6DSL EXTI Interrupt to 0x01 sub priority */
    HAL_NVIC_SetPriority((IRQn_Type)(LSM6DSL_INT1_EXTI11_EXTI_IRQn), 0x0F, 0x01);
    HAL_NVIC_EnableIRQ((IRQn_Type)(LSM6DSL_INT1_EXTI11_EXTI_IRQn));
}

static void LSM6DSL_Init(void) {
	SENSOR_IO_Init(); // initialize sensor read/write operations
	SENSOR_IO_Write(LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW, LSM6DSL_ACC_GYRO_CTRL1_XL, 0x60); // turn on accelerometer
	SENSOR_IO_Write(LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW, LSM6DSL_ACC_GYRO_TAP_CFG1, 0x80); // enable basic interrupts
	SENSOR_IO_Write(LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW, LSM6DSL_ACC_GYRO_FREE_FALL, 0xff); // set free fall threshold and duration to be highest setting at 500mg
	SENSOR_IO_Write(LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW, LSM6DSL_ACC_GYRO_MD1_CFG, 0x10); // enable routing of free fall event detection
};

static void UART1_Init(void)
{
	/* Pin configuration for UART. BSP_COM_Init() can do this automatically */
	__HAL_RCC_GPIOB_CLK_ENABLE();
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
	GPIO_InitStruct.Pin = GPIO_PIN_7|GPIO_PIN_6;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/* Configuring UART1 */
	huart1.Instance = USART1;
	huart1.Init.BaudRate = 115200;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;
	huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart1) != HAL_OK)
	{
		while(1);
	}
}
