/**
  ******************************************************************************
  * File Name          : main.c
  * Description        : Main program body
  ******************************************************************************
  *
  * Copyright (c) 2017 STMicroelectronics International N.V. 
  * All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without 
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice, 
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other 
  *    contributors to this software may be used to endorse or promote products 
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this 
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under 
  *    this license is void and will automatically terminate your rights under 
  *    this license. 
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" 
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT 
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT 
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f1xx_hal.h"
#include "usb_device.h"

/* USER CODE BEGIN Includes */
#include <stdlib.h>
#include <string.h> //for memcpy and memcmp

#include "..\\..\\Common\\led.h"
#include "..\\..\\Common\\SensorFusion.h"
#include "..\\..\\Common\\gy8x.h"
#include "..\\..\\Common\\nrf24l01.h"
#include "..\\..\\Common\\usb.h"
#include "usbd_custom_hid_if.h"
#include "..\\..\\Common\\eeprom_flash.h"

/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

I2C_HandleTypeDef hi2c2;

SPI_HandleTypeDef hspi2;

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
enum RawModes
{
	RawMode_Off = 0,
	RawMode_Raw = 1,
	RawMode_Filtered = 2,        
	RawMode_Compensated = 3,
	RawMode_DRVectors = 4
};

struct CommChannel
{	
	uint8_t remainingTransmits;
	uint32_t lastReceive;
	uint32_t nextTransmit;
	uint16_t lastInSequence;
	uint16_t lastOutSequence;
	USBPacket lastCommand;	
};

struct CommChannel Channels[MAX_SOURCE] = {0};

int8_t ctlSource = HMD_SOURCE;
SensorData tSensorData;
int32_t blinkDelay = 1000;
USBPacket USBDataPacket = {0};
extern uint8_t USB_RX_Buffer[CUSTOM_HID_EPIN_SIZE];
USBPacket *pUSBCommandPacket = (USBPacket *) USB_RX_Buffer;
//volatile uint8_t rfReady = 1;
uint8_t rfStatus = 0;
bool sensorStatus = false;

#if CUSTOM_HID_EPOUT_SIZE != 32
#error HID out packet size not 32
#end
#elif CUSTOM_HID_EPIN_SIZE != 32
#error HID packet in size not 32
#end
#elif USB_CUSTOM_HID_DESC_SIZ != 34
#error HID packet size not 34
#end
#endif

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void Error_Handler(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_SPI2_Init(void);
static void MX_I2C2_Init(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/
RF_InitTypeDef RF_InitStruct = {0};
unsigned char TrackedDevice_NRF24L01_Init (void)
{	
	RF_InitStruct.RF_Power_State=RF_Power_On;
	RF_InitStruct.RF_Config=RF_Config_IRQ_RX_Off|RF_Config_IRQ_TX_Off|RF_Confing_IRQ_Max_Rt_Off;
	RF_InitStruct.RF_CRC_Mode=RF_CRC8_On;
	RF_InitStruct.RF_Mode=RF_Mode_RX;
	RF_InitStruct.RF_Pipe_Auto_Ack=0;	
	RF_InitStruct.RF_Enable_Pipe=1;	
	RF_InitStruct.RF_Setup=RF_Setup_5_Byte_Adress;
	RF_InitStruct.RF_TX_Power=RF_TX_Power_High;
	RF_InitStruct.RF_Data_Rate=RF_Data_Rate_2Mbs;
	RF_InitStruct.RF_Channel=250;
	RF_InitStruct.RF_TX_Adress[0]='B';
	RF_InitStruct.RF_TX_Adress[1]='A';
	RF_InitStruct.RF_TX_Adress[2]='S';
	RF_InitStruct.RF_TX_Adress[3]='E';
	RF_InitStruct.RF_TX_Adress[4]='1';
	RF_InitStruct.RF_RX_Adress_Pipe0[0]='C';
	RF_InitStruct.RF_RX_Adress_Pipe0[1]='T';
	RF_InitStruct.RF_RX_Adress_Pipe0[2]='R';
	RF_InitStruct.RF_RX_Adress_Pipe0[3]='L';
	RF_InitStruct.RF_RX_Adress_Pipe0[4]='0';
	RF_InitStruct.RF_Payload_Size_Pipe0=32;
	RF_InitStruct.RF_Auto_Retransmit_Count=0;
	RF_InitStruct.RF_Auto_Retransmit_Delay=0;
	return RF_Init(&hspi2, &RF_InitStruct);
}	
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

uint64_t HAL_GetMicros()
{
	uint64_t ticks = HAL_GetTick();
	uint64_t systick = (uint64_t)SysTick->VAL;
	return (ticks * 1000ull) + 1000ull - (systick / 72ul);
}

void HAL_MicroDelay(uint64_t delay)
{
	uint64_t tickstart = 0;	
	tickstart = HAL_GetMicros();
	while((HAL_GetMicros() - tickstart) < delay)
	{
	}
}

inline uint8_t sgn(float n)
{
	return (n<0?-1:1);
}

volatile uint32_t LastDigitalChange = 0;
uint16_t DigitalValues = 0;
uint16_t DigitalCache = 0;

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	switch(GPIO_Pin)
	{
		case IR_SENS0_Pin:
			break;
		case IR_SENS1_Pin:
			break;
		case IR_SENS2_Pin:
			break;
		case IR_SENS3_Pin:
			break;
		case IR_SENS4_Pin:
			break;
		case IR_SENS5_Pin:
			break;
		case CTL_BTN0_Pin:
		case CTL_BTN1_Pin:
		case CTL_BTN2_Pin:
		case CTL_BTN3_Pin:
		case CTL_BTN4_Pin:
		case CTL_BTN5_Pin:
		case CTL_BTN6_Pin:
		case CTL_BTN7_Pin:
			LastDigitalChange = HAL_GetTick();
			break;
	}
}

uint32_t LastADCChange = 0;	
uint32_t ADC_Values[4] = {0};
float ADC_Value[2] = {0};
uint32_t ADC_Cache[2] = {0};

//void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
//{		
//	if (hadc->DMA_Handle != hadc1.DMA_Handle) return;
//	ADC_Value[0] = (uint32_t)((ADC_Values[0] + ADC_Values[2]) / 2);
//	ADC_Value[1] = (uint32_t)((ADC_Values[1] + ADC_Values[3]) / 2);
//}

uint64_t lastRotationUpdate = 0; 
uint64_t now = 0;     
uint32_t ticks = 0;
uint32_t nextSend = 0; 

uint32_t sndCounter = 0;
uint32_t rcvCounter = 0;

CalibrationCacheData calibrationCache = {0};
bool isCalibrated = false;
bool isPositionReady = false;

void CalibrateSensors(void)
{
	for(int i=0; i<3; i++)
	{
		if (calibrationCache.Accel.Sensor == SENSOR_ACCEL)
		{
			tSensorData.Accel.Offset[i] = (float)(calibrationCache.Accel.RawMax[i] + calibrationCache.Accel.RawMin[i]) / 2.0f;			
			tSensorData.Accel.PosScale[i] = calibrationCache.Accel.RawMax[i] == 0? 1.0f : (1.0f / aRes) / (float)(calibrationCache.Accel.RawMax[i] - tSensorData.Accel.Offset[i]);
			tSensorData.Accel.NegScale[i] = calibrationCache.Accel.RawMin[i] == 0? 1.0f : (1.0f / aRes) / (float)(calibrationCache.Accel.RawMin[i] - tSensorData.Accel.Offset[i]);
		}
		if (calibrationCache.Gyro.Sensor == SENSOR_GYRO)
		{
			tSensorData.Gyro.Offset[i] = (float)(calibrationCache.Gyro.RawMax[i]);
			tSensorData.Gyro.PosScale[i] = 1.0f;
			tSensorData.Gyro.NegScale[i] = 1.0f;
		}
		if (calibrationCache.Mag.Sensor == SENSOR_MAG)
		{
			tSensorData.Mag.Offset[i] = (float)(calibrationCache.Mag.RawMax[i] + calibrationCache.Mag.RawMin[i]) / 2.0f;			
			tSensorData.Mag.PosScale[i] = calibrationCache.Mag.RawMax[i] == 0? 1.0f : (1.0f / mRes) / (float)(calibrationCache.Mag.RawMax[i] - tSensorData.Mag.Offset[i]);
			tSensorData.Mag.NegScale[i] = calibrationCache.Mag.RawMin[i] == 0? 1.0f : (1.0f / mRes) / (float)(calibrationCache.Mag.RawMin[i] - tSensorData.Mag.Offset[i]);
		}
	}		
	isCalibrated = true;	
}

/* USER CODE END 0 */

int main(void)
{

  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* MCU Configuration----------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_SPI2_Init();
  MX_I2C2_Init();
  MX_USB_DEVICE_Init();

  /* USER CODE BEGIN 2 */
  
	LedOff();	 


	//load eeprom calibration data	
	char *eepromBytes = (char *)&calibrationCache;	
	int addr = 0;
	for (int i=0; i<sizeof(calibrationCache); i+=sizeof(uint32_t))
	{
		*((uint32_t *)(&eepromBytes[i])) = readEEPROMWord(addr);
		addr += sizeof(uint32_t);
	}
	
	
	if (calibrationCache.Magic == VALID_EEPROM_MAGIC)	
		CalibrateSensors();		
	
//	ctlSource = HMD_SOURCE;
//	ctlSource += (GPIO_PIN_SET == HAL_GPIO_ReadPin(CTL_MODE1_GPIO_Port, CTL_MODE1_Pin)?1:0); 
//	ctlSource += (GPIO_PIN_SET == HAL_GPIO_ReadPin(CTL_MODE2_GPIO_Port, CTL_MODE2_Pin)?2:0); 	
	srand(ctlSource);


	if (ctlSource == RIGHTCTL_SOURCE || ctlSource == LEFTCTL_SOURCE)
	{
		HAL_GPIO_WritePin(CTL_VIBRATE0_GPIO_Port, CTL_VIBRATE0_Pin, GPIO_PIN_SET); 
		BlinkRease(30);
		HAL_GPIO_WritePin(CTL_VIBRATE0_GPIO_Port, CTL_VIBRATE0_Pin, GPIO_PIN_RESET);
		HAL_Delay(250);
	}

	
//	GPIO_InitTypeDef GPIO_InitStruct;
//	GPIO_InitStruct.Pin = I2C_SCL_Pin;
//	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
//	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
//	HAL_GPIO_Init(I2C_SCL_GPIO_Port, &GPIO_InitStruct);
//	HAL_Delay(10);

//	for (int x=0; x<9;x++)
//	{
//		HAL_GPIO_WritePin(I2C_SCL_GPIO_Port, I2C_SCL_Pin, GPIO_PIN_RESET);
//		HAL_Delay(10);
//		HAL_GPIO_WritePin(I2C_SCL_GPIO_Port, I2C_SCL_Pin, GPIO_PIN_SET);
//		HAL_Delay(10);
//	}
//	
//	HAL_GPIO_DeInit(I2C_SCL_GPIO_Port, I2C_SCL_Pin);
//	HAL_Delay(10);
//	

	rfStatus = TrackedDevice_NRF24L01_Init();	
	if (rfStatus == 0x00 || rfStatus == 0xff)
		Error_Handler();
	LedOff();	
	

	sensorStatus = initSensors();		
	if (!sensorStatus)
		Error_Handler();
	LedOff();		
	
	uint32_t buttonRefreshTimeout = 1000;
	
	if (ctlSource == RIGHTCTL_SOURCE || ctlSource == LEFTCTL_SOURCE)
	{
		buttonRefreshTimeout = 250;
		HAL_GPIO_WritePin(CTL_VIBRATE0_GPIO_Port, CTL_VIBRATE0_Pin, GPIO_PIN_SET); 
		BlinkRease(20);
		HAL_GPIO_WritePin(CTL_VIBRATE0_GPIO_Port, CTL_VIBRATE0_Pin, GPIO_PIN_RESET);
		HAL_Delay(100);
	}
	
	//bool sensorWarmup = false;
	int extraDelay = ctlSource>HMD_SOURCE? 10:15;	
	bool hasRotationData = false;
	bool hasPositionData = false;
	uint8_t buttonRetransmit = 0;
	//bool hasTriggerData = false;

	tSensorData.Setup(aRes, gRes, mRes);
	
	CSensorFusion orientFuse(0.2f);
	CSensorFusion gravFuse(1.0f);
	
	//do some warmup
	for (int i=0; i<100; i++)
	{
		readAccelData(tSensorData.Accel.Raw);  // Read the x/y/z adc values		
		tSensorData.Accel.ProcessNew();
		HAL_Delay(1);
		readGyroData(tSensorData.Gyro.Raw); 
		tSensorData.Gyro.ProcessNew();
		HAL_Delay(1);
		readMagData(tSensorData.Mag.Raw);
		tSensorData.Mag.ProcessNew();
		HAL_Delay(10);
	}
	
//	float s = 0.9f;
//	tSensorData.HighPassVelocity[0].Setup(Highpass, 0.5, 200, s);
//	tSensorData.HighPassVelocity[1].Setup(Highpass, 0.5, 200, s);
//	tSensorData.HighPassVelocity[2].Setup(Highpass, 0.5, 200, s);	
//	tSensorData.HighPassPosition[0].Setup(Highpass, 0.5, 200, s);
//	tSensorData.HighPassPosition[1].Setup(Highpass, 0.5, 200, s);
//	tSensorData.HighPassPosition[2].Setup(Highpass, 0.5, 200, s);

	//if (ctlSource == RIGHTCTL_SOURCE || ctlSource == LEFTCTL_SOURCE)
	//{
		//start background adc conversion
		if( HAL_ADC_Start(&hadc1) != HAL_OK)  
			Error_Handler();
		if (HAL_ADC_Start_DMA(&hadc1, (uint32_t*)ADC_Values, 4) != HAL_OK)  
			Error_Handler();
		HAL_Delay(1000);
		
		//calculate analog averages
		//for (int c=0; c<100; c++)
		//{
		//	HAL_Delay(1);
			//for (int i=0; i<2; i++)
			//	ADC_Averages[i] = (ADC_Averages[i] * 0.9f) + ((float)ADC_Values[i] * 0.1f);
		//}
		//for (int i=0; i<2; i++)
		//	ADC_Offsets[i] = ADC_Averages[i];		
	//}
	
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	
	
	RawModes feedRawMode = RawMode_Off;
	uint8_t readyToSend = 0;
	uint8_t readyToReceive = 0;
	
	Quaternion orientQuat;
	Quaternion gravQuat;
	Quaternion positionQuat;
	//Quaternion gravQuat(0, 0, 0, 1);
	//RF_ReceiveMode(&hspi2, NULL); //default to receive mode			
	uint32_t lastCommandSequence = 0;
	uint32_t vibrationStopTime0 = 0;	
	uint32_t vibrationStopTime1 = 0;	
	uint64_t lastButtonRefresh = 0;		
	//uint64_t lastPositionIntegrateTime = 0;
	//uint64_t lastPositionSend = 0;
	DigitalValues = DigitalCache = 0;
	
	//float AccelVector[3] = {0};
	float GravityVector[3] = {0};
	float CompAccelVector[3] = {0};
	float VelocityVector[3] = {0};
	//float ComplimentaryAccelVector[3] = {0};
	//float ComplimentaryVelocityVector[3] = {0};
	USBPositionData position = {0};
	
	//uint8_t sendTurn = 0;
	
//	float sensorReadings = 0;
//	float AccelReadings[3] = {0};
//	float GyroReadings[3] = {0};
//	float MagReadings[3] = {0};
	
	KalmanSingle AccelFilter[3] = {
		KalmanSingle(0.125,32,1,0),
		KalmanSingle(0.125,32,1,0),
		KalmanSingle(0.125,32,1,0)
	};
	
	uint8_t SensorCalibRequest = 0xFF;
	uint32_t eepromSaveTime = 0;
	
	while (true)
	{	
		Blink(blinkDelay);
		blinkDelay = 1000;
		
		now = HAL_GetMicros();
		ticks = now / 1000;
		
		if (eepromSaveTime && eepromSaveTime >= ticks)
		{
			CalibrateSensors();
			calibrationCache.Magic = VALID_EEPROM_MAGIC;
			enableEEPROMWriting();
			char *eepromData = (char *)&calibrationCache;	
			int addr = 0;
			for (int i=0; i<sizeof(calibrationCache); i+=sizeof(uint32_t))
			{
				writeEEPROMWord(addr, *((uint32_t *)(&eepromData[i])));
				addr += sizeof(uint32_t);
			}
			disableEEPROMWriting();
			eepromSaveTime = 0;
		}
		
		//if ((ctlSource == RIGHTCTL_SOURCE || ctlSource == LEFTCTL_SOURCE))
		//{
			if (ticks - LastADCChange >= 50) //20 analog values per second
			{				 
				uint8_t changed = 0;
				LastADCChange = ticks;
				ADC_Value[0] = (((float)(ADC_Values[0] + ADC_Values[2]) / 2.0f) * 0.5f) + (ADC_Value[0] * 0.5f);
				ADC_Value[1] = (((float)(ADC_Values[1] + ADC_Values[3]) / 2.0f) * 0.5f) + (ADC_Value[1] * 0.5f);
				
				uint32_t value0 = ((uint32_t)(ADC_Value[0] / 40.96f));
				uint32_t value1 = ((uint32_t)(ADC_Value[1] / 40.96f));
				
				if (ADC_Cache[0] != value0) { changed = 1; ADC_Cache[0] = value0;}				
				if (ADC_Cache[1] != value1) { changed = 1; ADC_Cache[1] = value1;}
				if (changed)
				{
					//if any analog value has changes
					buttonRetransmit = 5;					
					lastButtonRefresh = ticks;
				}
			}
			
			if (LastDigitalChange && (ticks - LastDigitalChange >= 25)) //using sw debounce 
			{			
				LastDigitalChange = 0;
				DigitalValues = 0;
				if (HAL_GPIO_ReadPin(CTL_BTN0_GPIO_Port, CTL_BTN0_Pin) == GPIO_PIN_SET) DigitalValues |= BUTTON_0;
				if (HAL_GPIO_ReadPin(CTL_BTN1_GPIO_Port, CTL_BTN1_Pin) == GPIO_PIN_SET) DigitalValues |= BUTTON_1;
				if (HAL_GPIO_ReadPin(CTL_BTN2_GPIO_Port, CTL_BTN2_Pin) == GPIO_PIN_SET) DigitalValues |= BUTTON_2;
				if (HAL_GPIO_ReadPin(CTL_BTN3_GPIO_Port, CTL_BTN3_Pin) == GPIO_PIN_SET) DigitalValues |= BUTTON_3;				
				if (HAL_GPIO_ReadPin(CTL_BTN4_GPIO_Port, CTL_BTN4_Pin) == GPIO_PIN_SET) DigitalValues |= BUTTON_4;
				if (HAL_GPIO_ReadPin(CTL_BTN5_GPIO_Port, CTL_BTN5_Pin) == GPIO_PIN_SET) DigitalValues |= BUTTON_5;
				if (HAL_GPIO_ReadPin(CTL_BTN6_GPIO_Port, CTL_BTN6_Pin) == GPIO_PIN_SET) DigitalValues |= BUTTON_6;
				if (HAL_GPIO_ReadPin(CTL_BTN7_GPIO_Port, CTL_BTN7_Pin) == GPIO_PIN_SET) DigitalValues |= BUTTON_7;
				
				if (DigitalCache != DigitalValues)
				{
					//if any digital value has changes
					DigitalCache = DigitalValues;
					//hasTriggerData = true;
					buttonRetransmit = 5;
					lastButtonRefresh = ticks;
				}
			}
			
			if (ticks - lastButtonRefresh >= buttonRefreshTimeout)
			{
				//update button status every 250ms.
				buttonRetransmit++;
				lastButtonRefresh = ticks;
			}
		//}
		
		if (vibrationStopTime0 && (ticks > vibrationStopTime0))
		{
			HAL_GPIO_WritePin(CTL_VIBRATE0_GPIO_Port, CTL_VIBRATE0_Pin, GPIO_PIN_RESET);
			vibrationStopTime0 = 0;
		}
		
		if (vibrationStopTime1 && (ticks > vibrationStopTime1))
		{
			HAL_GPIO_WritePin(CTL_VIBRATE1_GPIO_Port, CTL_VIBRATE1_Pin, GPIO_PIN_RESET);
			vibrationStopTime1 = 0;
		}

		float elapsedTime = (now - lastRotationUpdate) / 1000000.0f; // set integration time by time elapsed since last filter update				
		
		if (elapsedTime >= 0.005f) //update at 200hz
		{	
			hasRotationData |= readAccelData(tSensorData.Accel.Raw);  // Read the x/y/z adc values			
			hasRotationData |= readGyroData(tSensorData.Gyro.Raw); 
			hasRotationData |= readMagData(tSensorData.Mag.Raw);
					
			
			if (hasRotationData) // recalc position if still has position data to be sent
			{
				tSensorData.Accel.ProcessNew();
				tSensorData.Gyro.ProcessNew();
				tSensorData.Mag.ProcessNew();
				
				tSensorData.TimeElapsed = elapsedTime;
				lastRotationUpdate = now;	
				orientQuat = orientFuse.FuseOrient(&tSensorData);						

				//gravQuat = orientQuat;
//				gravQuat.w = 0; gravQuat.x = 0; gravQuat.y = 0; gravQuat.z = 0;
//				gravQuat = (orientQuat * gravQuat) * orientQuat.conjugate();

				if (isPositionReady && (hasPositionData || (isCalibrated && (ticks >= 10000))))
				{
					//float elapsedPositionTime = (float)(now - lastPositionIntegrateTime) / 1000000.0f;
					//if (elapsedPositionTime >= 0.001f) // integrate every 2 ms
					{
						GravityVector[0] = 2.0f * (orientQuat.x * orientQuat.z - orientQuat.w * orientQuat.y);
						GravityVector[1] = 2.0f * (orientQuat.w * orientQuat.x + orientQuat.y * orientQuat.z);
						GravityVector[2] = orientQuat.w * orientQuat.w - orientQuat.x * orientQuat.x - orientQuat.y * orientQuat.y + orientQuat.z * orientQuat.z;
						
						for (int i=0; i<3; i++)
						{
							float prevAccel = CompAccelVector[i];
							float prevVelocity = VelocityVector[i];																									
							float newAccel = tSensorData.Accel.Converted[i] - GravityVector[i];
							float newVelocity = ((prevAccel + newAccel) / 2.0f) * 9.81f * elapsedTime;		
							float prevPosition = position.Position[i];
							
							//if velocity changes direction hold for a moment
//							if (sgn(prevVelocity) != sgn(newVelocity))
//							{
//								newVelocity = prevVelocity = 0;
//								//newAccel = 0;
//							}
							
							
							position.Position[i] = prevPosition + ((prevVelocity + newVelocity) / 2.0f) * elapsedTime * 10.0f;											
							CompAccelVector[i] = newAccel;
							VelocityVector[i] = prevVelocity + newVelocity;							
						}	
						
						//if (now - lastPositionSend >= 20000) //50 ms
						{
							positionQuat.w = 0;
							positionQuat.x = position.Position[0];
							positionQuat.y = position.Position[1];
							positionQuat.z = position.Position[2];
							
							positionQuat = (orientQuat * positionQuat) * orientQuat.conjugate();
							
							hasPositionData = 1;							
							//lastPositionSend = now;	
						}
						
						//lastPositionIntegrateTime = now;
					}
				}
				//else
				//	lastPositionIntegrateTime = now;
			}
		}

		rfStatus = RF_Status(&hspi2);
		readyToReceive = (rfStatus & RF_RX_DR_IRQ_CLEAR) == RF_RX_DR_IRQ_CLEAR ? 1 : 0;

		if (readyToReceive)
		{
			rfStatus = RF_FifoStatus(&hspi2);
			readyToReceive = (rfStatus & RF_RX_FIFO_EMPTY_Bit) == 0x00 ? 1: 0;
			while (readyToReceive)
			{			
				rcvCounter++;
				//receive command
				//has fifo, receive
				readyToReceive = RF_ReceivePayload(&hspi2, (uint8_t*)pUSBCommandPacket, sizeof(USBPacket)) == 0 ? 1: 0; //if not empty 
				
				if (CheckPacketCrc(pUSBCommandPacket) && ((pUSBCommandPacket->Header.Type & 0x0F)) == ctlSource)
				{
					if (lastCommandSequence != pUSBCommandPacket->Header.Sequence) //discard duplicates
					{
						lastCommandSequence = pUSBCommandPacket->Header.Sequence; 
						switch (pUSBCommandPacket->Command.Command)
						{
							case CMD_RAW_DATA:								
								feedRawMode = (RawModes) pUSBCommandPacket->Command.Data.Raw.State;
								break;
							case CMD_VIBRATE:
								if (pUSBCommandPacket->Command.Data.Vibration.Axis == 0)
								{
									vibrationStopTime0 = ticks + pUSBCommandPacket->Command.Data.Vibration.Duration;
									HAL_GPIO_WritePin(CTL_VIBRATE0_GPIO_Port, CTL_VIBRATE0_Pin, GPIO_PIN_SET);							
								}
								else if (pUSBCommandPacket->Command.Data.Vibration.Axis == 1)
								{
									vibrationStopTime1 = ticks + pUSBCommandPacket->Command.Data.Vibration.Duration;
									HAL_GPIO_WritePin(CTL_VIBRATE1_GPIO_Port, CTL_VIBRATE1_Pin, GPIO_PIN_SET);							
								}
								break;
							case CMD_CALIBRATE:
								//manually set sensor offsets
								int sensorId = pUSBCommandPacket->Command.Data.Calibration.Sensor;									
								if (pUSBCommandPacket->Command.Data.Calibration.Command == CALIB_GET)							
								{
									SensorCalibRequest = sensorId;
								}
								else
								{
									for (int i=0; i<3; i++)
									{
										position.Position[i] = 0;
										VelocityVector[i] = 0;
										//AccelVector[i] = 0;
										CompAccelVector[i] = 0;
									}
									if (sensorId == SENSOR_NONE)
										break;
									else if (sensorId == SENSOR_ACCEL)
										calibrationCache.Accel = pUSBCommandPacket->Command.Data.Calibration;
									else if (sensorId == SENSOR_GYRO)
										calibrationCache.Gyro = pUSBCommandPacket->Command.Data.Calibration;
									else if (sensorId == SENSOR_MAG)
										calibrationCache.Mag = pUSBCommandPacket->Command.Data.Calibration;
									eepromSaveTime = ticks + 1000; //save after 1 second
									//lastPositionIntegrateTime = 
									//lastPositionSend = now;
								}
								break;
						}
					}
					pUSBCommandPacket->Header.Type = 0xff;				
				}									
			}
		}		
		
		if (((SensorCalibRequest != 0xFF) || hasRotationData || feedRawMode || hasPositionData || buttonRetransmit) && ticks >= nextSend)
		{
			rfStatus = RF_FifoStatus(&hspi2); readyToSend = (rfStatus & RF_TX_FIFO_FULL_Bit) == 0x00 ? 1 : 0;			
		}
		
		if (readyToSend)
		{
			RF_TransmitMode(&hspi2, RF_InitStruct.RF_TX_Adress); //switch to tx mode
			//HAL_MicroDelay(10);			
			blinkDelay = 100;

			sndCounter++;
			
			//RF_DisableTransmit(&hspi2);
			
			//rfStatus = RF_Status(&hspi2); readyToSend = (rfStatus & RF_TX_STATUS_FULL_Bit) == 0x00 ? 1 : 0;	
			//sendTurn = (sendTurn + 1) % 4;
			
			//4 types of data				
			//uint8_t currTurn = sendTurn;
			
			//if (currTurn == 0)
			{
				if (SensorCalibRequest != 0xFF)
				{
					USBDataPacket.Header.Type = ctlSource | COMMAND_DATA;
					USBDataPacket.Header.Sequence++;				
					USBDataPacket.Command.Command = CMD_CALIBRATE;
					if (SensorCalibRequest == SENSOR_ACCEL)
						USBDataPacket.Command.Data.Calibration = calibrationCache.Accel;
					else if (SensorCalibRequest == SENSOR_GYRO)
						USBDataPacket.Command.Data.Calibration = calibrationCache.Gyro;
					else if (SensorCalibRequest == SENSOR_MAG)
						USBDataPacket.Command.Data.Calibration = calibrationCache.Mag;
					USBDataPacket.Command.Data.Calibration.Command = CALIB_GET;
					USBDataPacket.Command.Data.Calibration.Sensor = SensorCalibRequest;
					SensorCalibRequest = 0xFF;
					SetPacketCrc(&USBDataPacket);				
					RF_SendPayload(&hspi2, (uint8_t*)&USBDataPacket, sizeof(USBPacket));
				}
				
				if (feedRawMode)
				{	
					//sendTurn = currTurn;
					//send raw data
					USBDataPacket.Header.Type = ctlSource | COMMAND_DATA;
					USBDataPacket.Header.Sequence++;				
					USBDataPacket.Command.Command = CMD_RAW_DATA;
					USBDataPacket.Command.Data.Raw.State = feedRawMode;
					for (int idx=0; idx<3; idx++)
					{
						switch(feedRawMode)
						{
							case RawMode_Off:
								break;
							case RawMode_Raw:
							{
								//raw sensor values
								USBDataPacket.Command.Data.Raw.Accel[idx] = tSensorData.Accel.Raw[idx];
								USBDataPacket.Command.Data.Raw.Gyro[idx] = tSensorData.Gyro.Raw[idx];
								USBDataPacket.Command.Data.Raw.Mag[idx] = tSensorData.Mag.Raw[idx];
								break;
							}
							case RawMode_Filtered:
							{
								//filtered sensor data
								USBDataPacket.Command.Data.Raw.Accel[idx] = tSensorData.Accel.Filtered[idx];
								USBDataPacket.Command.Data.Raw.Gyro[idx] = tSensorData.Gyro.Filtered[idx];
								USBDataPacket.Command.Data.Raw.Mag[idx] = tSensorData.Mag.Filtered[idx];
								break;
							}
							case RawMode_Compensated:
							{
								//compensated sensor data
								USBDataPacket.Command.Data.Raw.Accel[idx] = tSensorData.Accel.Compensated[idx];
								USBDataPacket.Command.Data.Raw.Gyro[idx] = tSensorData.Gyro.Compensated[idx];
								USBDataPacket.Command.Data.Raw.Mag[idx] = tSensorData.Mag.Compensated[idx];
								break;
							}
							case RawMode_DRVectors:
							{
								//deadreckoning vectors
								USBDataPacket.Command.Data.Raw.Accel[idx] = (int16_t)(GravityVector[idx] / aRes);
								USBDataPacket.Command.Data.Raw.Gyro[idx] = (int16_t)(CompAccelVector[idx] / aRes); //in m/s2
								USBDataPacket.Command.Data.Raw.Mag[idx] = VelocityVector[idx] * 1000; //in mm/s
								break;
							}
						}
					}
					SetPacketCrc(&USBDataPacket);				
					RF_SendPayload(&hspi2, (uint8_t*)&USBDataPacket, sizeof(USBPacket));
					//rfStatus = RF_FifoStatus(&hspi2); readyToSend = (rfStatus & RF_TX_FIFO_FULL_Bit) == 0x00 ? 1 : 0;
				}
				//else 
				//	currTurn++;
			}
			//if (currTurn == 1)
			//{	
				if (hasPositionData)
				{					
					//sendTurn = currTurn;
					//send position
					hasPositionData = 0;				
					USBDataPacket.Header.Type = ctlSource | POSITION_DATA;
					USBDataPacket.Header.Sequence++;
					
					//remap
					USBDataPacket.Position.Position[0] = positionQuat.x; //position.Position[0];
					USBDataPacket.Position.Position[1] = positionQuat.z; //position.Position[2];
					USBDataPacket.Position.Position[2] = -positionQuat.y; //-position.Position[1];
					SetPacketCrc(&USBDataPacket);				
					RF_SendPayload(&hspi2, (uint8_t*)&USBDataPacket, sizeof(USBPacket));
					//rfStatus = RF_FifoStatus(&hspi2); readyToSend = (rfStatus & RF_TX_FIFO_FULL_Bit) == 0x00 ? 1 : 0;					
				}
				//else 
				//	currTurn++;
			//} 
			//if (currTurn == 2)
			//{				
				if (hasRotationData)
				{			
					//sendTurn = currTurn;					
					//send rot data
					hasRotationData = 0;				
					USBDataPacket.Header.Type = ctlSource | ROTATION_DATA;
					USBDataPacket.Header.Sequence++;
					//remap				
					USBDataPacket.Rotation.w = orientQuat.w;
					USBDataPacket.Rotation.x = orientQuat.x;
					USBDataPacket.Rotation.y = orientQuat.z;
					USBDataPacket.Rotation.z = -orientQuat.y;					
					SetPacketCrc(&USBDataPacket);				
					RF_SendPayload(&hspi2, (uint8_t*)&USBDataPacket, sizeof(USBPacket));
					//rfStatus = RF_FifoStatus(&hspi2); readyToSend = (rfStatus & RF_TX_FIFO_FULL_Bit) == 0x00 ? 1 : 0;	
				}			
				//else
				//	currTurn++;
			//} 
			//if (currTurn == 3)
			//{
				if (buttonRetransmit)
				{			
					//sendTurn = currTurn;					
					//send trigger data				
					buttonRetransmit--;				
					USBDataPacket.Header.Type = ctlSource | TRIGGER_DATA;
					USBDataPacket.Header.Sequence++;				
					USBDataPacket.Trigger.Analog[0].x = ((float)ADC_Cache[0]) / 100.0f; //trigger
					USBDataPacket.Trigger.Analog[0].y = ((float)ADC_Cache[1]) / 100.0f; //IPD
					USBDataPacket.Trigger.Analog[1].x = 0; //reserved
					USBDataPacket.Trigger.Analog[1].y = 0; //reserved
					
					USBDataPacket.Trigger.Digital = DigitalCache;
					SetPacketCrc(&USBDataPacket);				
					RF_SendPayload(&hspi2, (uint8_t*)&USBDataPacket, sizeof(USBPacket));				
					rfStatus = RF_FifoStatus(&hspi2); readyToSend = (rfStatus & RF_TX_FIFO_FULL_Bit) == 0x00 ? 1 : 0;									
				}
				//currTurn++;
			//}			
			
			//RF_EnableTransmit(&hspi2);
			//if (!(hasRotationData | hasPositionData | buttonRetransmit | feedRawData))
			do { rfStatus = RF_FifoStatus(&hspi2); } while ((rfStatus & RF_TX_FIFO_EMPTY_Bit) != RF_TX_FIFO_EMPTY_Bit); //wait for send complete			
			
			if (!(feedRawMode || hasPositionData || hasRotationData || buttonRetransmit))
				nextSend = ticks + (extraDelay + (rand() % 10));
			readyToSend = 0;

			HAL_MicroDelay(10);
			RF_ReceiveMode(&hspi2, RF_InitStruct.RF_RX_Adress_Pipe0); //back to rx mode
			//
			//continue;
		}
	}
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */
  /* USER CODE END 3 */

}

/** System Clock Configuration
*/
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInit;

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC|RCC_PERIPHCLK_USB;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure the Systick interrupt time 
    */
  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

    /**Configure the Systick 
    */
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/* ADC1 init function */
static void MX_ADC1_Init(void)
{

  ADC_ChannelConfTypeDef sConfig;

    /**Common config 
    */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T1_CC1;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 2;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure Regular Channel 
    */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure Regular Channel 
    */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

}

/* I2C2 init function */
static void MX_I2C2_Init(void)
{

  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 400000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

}

/* SPI2 init function */
static void MX_SPI2_Init(void)
{

  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }

}

/** 
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void) 
{
  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

}

/** Configure pins as 
        * Analog 
        * Input 
        * Output
        * EVENT_OUT
        * EXTI
        * Free pins are configured automatically as Analog (this feature is enabled through 
        * the Code Generation settings)
*/
static void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, SPI_RF_NSS_Pin|SPI_RF_CE_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, CTL_VIBRATE1_Pin|CTL_VIBRATE0_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : IR_SENS4_Pin IR_SENS5_Pin */
  GPIO_InitStruct.Pin = IR_SENS4_Pin|IR_SENS5_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA2 PA14 PA15 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : CTL_BTN0_Pin CTL_BTN1_Pin CTL_BTN2_Pin CTL_BTN3_Pin 
                           CTL_BTN4_Pin */
  GPIO_InitStruct.Pin = CTL_BTN0_Pin|CTL_BTN1_Pin|CTL_BTN2_Pin|CTL_BTN3_Pin 
                          |CTL_BTN4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : CTL_BTN5_Pin CTL_BTN6_Pin CTL_BTN7_Pin */
  GPIO_InitStruct.Pin = CTL_BTN5_Pin|CTL_BTN6_Pin|CTL_BTN7_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : IR_SYNC_Pin */
  GPIO_InitStruct.Pin = IR_SYNC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(IR_SYNC_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : IR_SENS0_Pin IR_SENS1_Pin IR_SENS2_Pin IR_SENS3_Pin */
  GPIO_InitStruct.Pin = IR_SENS0_Pin|IR_SENS1_Pin|IR_SENS2_Pin|IR_SENS3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB3 PB4 PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : SPI_RF_NSS_Pin SPI_RF_CE_Pin */
  GPIO_InitStruct.Pin = SPI_RF_NSS_Pin|SPI_RF_CE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : CTL_VIBRATE1_Pin CTL_VIBRATE0_Pin */
  GPIO_InitStruct.Pin = CTL_VIBRATE1_Pin|CTL_VIBRATE0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  HAL_NVIC_SetPriority(EXTI2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);

  HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler */
  /* User can add his own implementation to report the HAL error return state */
	//while(1)
	//{
		BlinkRease(80, false);
		//BlinkRease(80, true);
	//}
  LedOff();
//  __HAL_RCC_SPI1_FORCE_RESET();
//  __HAL_RCC_SPI1_CLK_ENABLE();
//  __HAL_RCC_I2C2_FORCE_RESET();
//  __HAL_RCC_I2C2_CLK_ENABLE();
  NVIC_SystemReset();
  /* USER CODE END Error_Handler */ 
}

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
	ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
  * @}
  */ 

/**
  * @}
*/ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
