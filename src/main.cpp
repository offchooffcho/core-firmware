/**
 ******************************************************************************
 * @file    main.c
 * @author  Spark Application Team
 * @version V1.0.0
 * @date    13-March-2013
 * @brief   Main program body.
 ******************************************************************************
 */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "spark_utilities.h"
extern "C" {
#include "usb_conf.h"
#include "usb_lib.h"
#include "usb_desc.h"
#include "usb_pwr.h"
#include "usb_prop.h"
#include "sst25vf_spi.h"
}

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
/* Priorities at which the tasks are created. */
#define mainQUEUE_RECEIVE_TASK_PRIORITY		( tskIDLE_PRIORITY + 2 )
#define	mainQUEUE_SEND_TASK_PRIORITY		( tskIDLE_PRIORITY + 1 )

/* The rate at which data is sent to the queue, specified in milliseconds, and
converted to ticks using the portTICK_RATE_MS constant. */
#define mainQUEUE_SEND_FREQUENCY_MS			( 200 / portTICK_RATE_MS )

/* The number of items the queue can hold.  This is 1 as the receive task
will remove items as they are added, meaning the send task should always find
the queue empty. */
#define mainQUEUE_LENGTH					( 1 )

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
__IO uint32_t TimingMillis;

uint8_t  USART_Rx_Buffer[USART_RX_DATA_SIZE];
uint32_t USART_Rx_ptr_in = 0;
uint32_t USART_Rx_ptr_out = 0;
uint32_t USART_Rx_length  = 0;

uint8_t USB_Rx_Buffer[VIRTUAL_COM_PORT_DATA_SIZE];
uint16_t USB_Rx_length = 0;
uint16_t USB_Rx_ptr = 0;

uint8_t  USB_Tx_State = 0;
uint8_t  USB_Rx_State = 0;

uint32_t USB_USART_BaudRate = 9600;

/* The queue used by both tasks. */
xQueueHandle xQueue = NULL;

/* The LED software timer.  This uses vLEDTimerCallback() as its callback
 * function.
 */
xTimerHandle xLEDTimer = NULL;

/*
 * Setup the NVIC, LED outputs, and button inputs.
 */
static void prvSetupHardware( void );

/*
 * The tasks as described in the comments at the top of this file.
 */
static void prvQueueReceiveTask( void *pvParameters );
static void prvQueueSendTask( void *pvParameters );

/*
 * The LED timer callback function.  This does nothing but switch the red LED
 * off.
 */
static void vLEDTimerCallback( xTimerHandle xTimer );

static void IntToUnicode (uint32_t value , uint8_t *pbuf , uint8_t len);

/* Extern variables ----------------------------------------------------------*/
extern LINE_CODING linecoding;

/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

/*******************************************************************************
 * Function Name  : main.
 * Description    : main routine.
 * Input          : None.
 * Output         : None.
 * Return         : None.
 *******************************************************************************/
int main(void)
{
#ifdef DFU_BUILD_ENABLE
	/* Set the Vector Table(VT) base location at 0x5000 */
	NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x5000);

	USE_SYSTEM_FLAGS = 1;
#endif

#ifdef SWD_JTAG_DISABLE
    /* Disable the Serial Wire JTAG Debug Port SWJ-DP */
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);
#endif

	/* Configure the NVIC, LED outputs and button inputs. */
	prvSetupHardware();

	/* Create the queue. */
	xQueue = xQueueCreate( mainQUEUE_LENGTH, sizeof( unsigned long ) );

	if( xQueue != NULL )
	{
		/* Start the two tasks as described in the comments at the top of this
		file. */
		xTaskCreate( prvQueueReceiveTask, ( signed char * ) "Rx", configMINIMAL_STACK_SIZE, NULL, mainQUEUE_RECEIVE_TASK_PRIORITY, NULL );
		xTaskCreate( prvQueueSendTask, ( signed char * ) "TX", configMINIMAL_STACK_SIZE, NULL, mainQUEUE_SEND_TASK_PRIORITY, NULL );

		/* Create the software timer that is responsible for turning off the LED
		if the button is not pushed within 5000ms, as described at the top of
		this file. */
		xLEDTimer = xTimerCreate( 	( const signed char * ) "LEDTimer", /* A text name, purely to help debugging. */
									( 5000 / portTICK_RATE_MS ),		/* The timer period, in this case 5000ms (5s). */
									pdFALSE,							/* This is a one shot timer, so xAutoReload is set to pdFALSE. */
									( void * ) 0,						/* The ID is not used, so can be set to anything. */
									vLEDTimerCallback					/* The callback function that switches the LED off. */
								);

		/* Start the tasks and timer running. */
		vTaskStartScheduler();
	}

	/* If all is well, the scheduler will now be running, and the following line
	will never be reached.  If the following line does execute, then there was
	insufficient FreeRTOS heap memory available for the idle and/or timer tasks
	to be created.  See the memory management section on the FreeRTOS web site
	for more details. */
	for( ;; );

	/* Following lines barred from execution to test FreeRTOS */

	Set_System();

	/* Enable CRC clock */
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC, ENABLE);

#if defined (USE_SPARK_CORE_V02)
    LED_SetRGBColor(RGB_COLOR_WHITE);
    LED_On(LED_RGB);
    SPARK_LED_FADE = 1;

#if defined (SPARK_RTC_ENABLE)
    RTC_Configuration();
#endif
#endif

#ifdef IWDG_RESET_ENABLE
	/* Check if the system has resumed from IWDG reset */
	if (RCC_GetFlagStatus(RCC_FLAG_IWDGRST) != RESET)
	{
		/* IWDGRST flag set */
		IWDG_SYSTEM_RESET = 1;

		/* Clear reset flags */
		RCC_ClearFlag();
	}

	/* Set IWDG Timeout to 3 secs */
	IWDG_Reset_Enable(3 * TIMING_IWDG_RELOAD);
#endif

#ifdef DFU_BUILD_ENABLE
	Load_SystemFlags();
#endif

#ifdef SPARK_SFLASH_ENABLE
	sFLASH_Init();
#endif

#ifdef SPARK_WIRING_ENABLE
	if((IWDG_SYSTEM_RESET != 1) && (NULL != setup))
	{
		setup();
	}
#endif

#ifdef SPARK_WLAN_ENABLE
	SPARK_WLAN_Setup();
#endif

	/* Main loop */
	while (1)
	{
#ifdef SPARK_WLAN_ENABLE
		SPARK_WLAN_Loop();
#endif

#ifdef SPARK_WIRING_ENABLE
#ifdef SPARK_WLAN_ENABLE
		if(SPARK_DEVICE_ACKED && !SPARK_FLASH_UPDATE && !IWDG_SYSTEM_RESET)
		{
#endif

			if(NULL != loop)
			{
				//Execute user application loop
				loop();
			}

			if(NULL != pHandleMessage)
			{
				pHandleMessage();
			}

			userFuncExecute();

			userVarReturn();

			userEventSend();

#ifdef SPARK_WLAN_ENABLE
		}
#endif
#endif
	}
}

static void vLEDTimerCallback( xTimerHandle xTimer )
{
	/* The timer has expired - so no button pushes have occurred
	in the last five seconds - change the RGB color back to CYAN.
	NOTE - accessing the LED port should use a critical section because
	it is accessed from multiple tasks, and the button interrupt - in this
	trivial case, for simplicity, the critical section is omitted. */
	LED_SetRGBColor(RGB_COLOR_RED);
	LED_On(LED_RGB);
}

static void prvQueueSendTask( void *pvParameters )
{
	portTickType xNextWakeTime;
	const unsigned long ulValueToSend = 100UL;

	/* Initialise xNextWakeTime - this only needs to be done once. */
	xNextWakeTime = xTaskGetTickCount();

	for( ;; )
	{
		/* Place this task in the blocked state until it is time to run again.
		The block time is specified in ticks, the constant used converts ticks
		to ms.  While in the Blocked state this task will not consume any CPU
		time. */
		vTaskDelayUntil( &xNextWakeTime, mainQUEUE_SEND_FREQUENCY_MS );

		/* Send to the queue - causing the queue receive task to unblock and
		toggle an LED.  0 is used as the block time so the sending operation
		will not block - it shouldn't need to block as the queue should always
		be empty at this point in the code. */
		xQueueSend( xQueue, &ulValueToSend, 0 );
	}
}

static void prvQueueReceiveTask( void *pvParameters )
{
	unsigned long ulReceivedValue;

	for( ;; )
	{
		/* Wait until something arrives in the queue - this task will block
		indefinitely provided INCLUDE_vTaskSuspend is set to 1 in
		FreeRTOSConfig.h. */
		xQueueReceive( xQueue, &ulReceivedValue, portMAX_DELAY );

		/*  To get here something must have been received from the queue, but
		is it the expected value?  If it is, toggle the green LED. */
		if( ulReceivedValue == 100UL )
		{
			/* NOTE - accessing the LED port should use a critical section
			because it is accessed from multiple tasks, and the button interrupt
			- in this trivial case, for simplicity, the critical section is
			omitted. */
			LED_Toggle( LED_RGB );
		}
	}
}

static void prvSetupHardware( void )
{
	/* Enable PWR and BKP clock */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);

	/* Enable write access to Backup domain */
	PWR_BackupAccessCmd(ENABLE);

	/* NVIC configuration */
	NVIC_Configuration();

	/* Enable AFIO clock */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

	/* Configure TIM1 for LED-PWM and BUTTON-DEBOUNCE usage */
	UI_Timer_Configure();

	/* Configure the LEDs and set the default states */
	int LEDx;
	for(LEDx = 0; LEDx < LEDn; ++LEDx)
	{
		LED_Init((Led_TypeDef)LEDx);
	}

	LED_SetRGBColor(RGB_COLOR_RED);
	LED_On(LED_RGB);

	/* Configure the Button */
	BUTTON_Init(BUTTON1, BUTTON_MODE_EXTI);
}

extern "C" {

void vApplicationMallocFailedHook( void )
{
	/* Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
	for( ;; );
}

void vApplicationStackOverflowHook( xTaskHandle pxTask, signed char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configconfigCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected. */
	for( ;; );
}

void vApplicationIdleHook( void )
{
	volatile size_t xFreeStackSpace;

	/* This function is called on each cycle of the idle task.  In this case it
	does nothing useful, other than report the amout of FreeRTOS heap that
	remains unallocated. */
	xFreeStackSpace = xPortGetFreeHeapSize();

	if( xFreeStackSpace > 100 )
	{
		/* By now, the kernel has allocated everything it is going to, so
		if there is a lot of heap remaining unallocated then
		the value of configTOTAL_HEAP_SIZE in FreeRTOSConfig.h can be
		reduced accordingly. */
	}
}

}

/*******************************************************************************
* Function Name  : Timing_Decrement
* Description    : Decrements the various Timing variables related to SysTick.
* Input          : None
* Output         : Timing
* Return         : None
*******************************************************************************/
void Timing_Decrement(void)
{
	TimingMillis++;

    if (TimingDelay != 0x00)
    {
        TimingDelay--;
    }

    if (LED_RGB_OVERRIDE != 0)
    {
    	if (NULL != LED_Signaling_Override)
    	{
    		LED_Signaling_Override();
    	}
    }
    else if (TimingLED != 0x00)
    {
    	TimingLED--;
    }
    else if(WLAN_SMART_CONFIG_START || SPARK_FLASH_UPDATE || Spark_Error_Count)
    {
    	//Do nothing
    }
    else if(SPARK_LED_FADE)
    {
#if defined (USE_SPARK_CORE_V02)
    	LED_Fade(LED_RGB);
    	if(SPARK_DEVICE_ACKED)
    		TimingLED = 20;
    	else
    		TimingLED = 1;
#endif
    }
    else if(SPARK_DEVICE_ACKED)
    {
#if defined (USE_SPARK_CORE_V01)
    	LED_On(LED1);
#elif defined (USE_SPARK_CORE_V02)
    	LED_SetRGBColor(RGB_COLOR_CYAN);
    	LED_On(LED_RGB);
#endif
    	SPARK_LED_FADE = 1;
    }
    else
    {
#if defined (USE_SPARK_CORE_V01)
    	LED_Toggle(LED1);
#elif defined (USE_SPARK_CORE_V02)
    	LED_Toggle(LED_RGB);
#endif
    	if(SPARK_SOCKET_CONNECTED)
    		TimingLED = 50;		//50ms
    	else
    		TimingLED = 100;	//100ms
    }

	if(BUTTON_GetDebouncedTime(BUTTON1) >= 3000)
	{
		BUTTON_ResetDebouncedState(BUTTON1);

		WLAN_SMART_CONFIG_START = 1;
	}

#ifdef IWDG_RESET_ENABLE
	if (TimingIWDGReload >= TIMING_IWDG_RELOAD)
	{
		TimingIWDGReload = 0;

	    /* Reload IWDG counter */
	    IWDG_ReloadCounter();
	}
	else
	{
		TimingIWDGReload++;
	}
#endif

#ifdef SPARK_WLAN_ENABLE
	SPARK_WLAN_Timing();
#endif
}

/*******************************************************************************
* Function Name  : USB_USART_Init
* Description    : Start USB-USART protocol.
* Input          : baudRate.
* Return         : None.
*******************************************************************************/
void USB_USART_Init(uint32_t baudRate)
{
	linecoding.bitrate = baudRate;
	USB_Disconnect_Config();
	Set_USBClock();
	USB_Interrupts_Config();
	USB_Init();
}

/*******************************************************************************
* Function Name  : USB_USART_Available_Data.
* Description    : Return the length of available data received from USB.
* Input          : None.
* Return         : Length.
*******************************************************************************/
uint8_t USB_USART_Available_Data(void)
{
	if(bDeviceState == CONFIGURED)
	{
		if(USB_Rx_State == 1)
		{
			return (USB_Rx_length - USB_Rx_ptr);
		}
	}

	return 0;
}

/*******************************************************************************
* Function Name  : USB_USART_Receive_Data.
* Description    : Return data sent by USB Host.
* Input          : None
* Return         : Data.
*******************************************************************************/
int32_t USB_USART_Receive_Data(void)
{
	if(bDeviceState == CONFIGURED)
	{
		if(USB_Rx_State == 1)
		{
			if((USB_Rx_length - USB_Rx_ptr) == 1)
			{
				USB_Rx_State = 0;

				/* Enable the receive of data on EP3 */
				SetEPRxValid(ENDP3);
			}

			return USB_Rx_Buffer[USB_Rx_ptr++];
		}
	}

	return -1;
}

/*******************************************************************************
* Function Name  : USB_USART_Send_Data.
* Description    : Send Data from USB_USART to USB Host.
* Input          : Data.
* Return         : None.
*******************************************************************************/
void USB_USART_Send_Data(uint8_t Data)
{
	if(bDeviceState == CONFIGURED)
	{
		USART_Rx_Buffer[USART_Rx_ptr_in] = Data;

		USART_Rx_ptr_in++;

		/* To avoid buffer overflow */
		if(USART_Rx_ptr_in == USART_RX_DATA_SIZE)
		{
			USART_Rx_ptr_in = 0;
		}
	}
}

/*******************************************************************************
* Function Name  : Handle_USBAsynchXfer.
* Description    : send data to USB.
* Input          : None.
* Return         : None.
*******************************************************************************/
void Handle_USBAsynchXfer (void)
{

  uint16_t USB_Tx_ptr;
  uint16_t USB_Tx_length;

  if(USB_Tx_State != 1)
  {
    if (USART_Rx_ptr_out == USART_RX_DATA_SIZE)
    {
      USART_Rx_ptr_out = 0;
    }

    if(USART_Rx_ptr_out == USART_Rx_ptr_in)
    {
      USB_Tx_State = 0;
      return;
    }

    if(USART_Rx_ptr_out > USART_Rx_ptr_in) /* rollback */
    {
      USART_Rx_length = USART_RX_DATA_SIZE - USART_Rx_ptr_out;
    }
    else
    {
      USART_Rx_length = USART_Rx_ptr_in - USART_Rx_ptr_out;
    }

    if (USART_Rx_length > VIRTUAL_COM_PORT_DATA_SIZE)
    {
      USB_Tx_ptr = USART_Rx_ptr_out;
      USB_Tx_length = VIRTUAL_COM_PORT_DATA_SIZE;

      USART_Rx_ptr_out += VIRTUAL_COM_PORT_DATA_SIZE;
      USART_Rx_length -= VIRTUAL_COM_PORT_DATA_SIZE;
    }
    else
    {
      USB_Tx_ptr = USART_Rx_ptr_out;
      USB_Tx_length = USART_Rx_length;

      USART_Rx_ptr_out += USART_Rx_length;
      USART_Rx_length = 0;
    }
    USB_Tx_State = 1;
    UserToPMABufferCopy(&USART_Rx_Buffer[USB_Tx_ptr], ENDP1_TXADDR, USB_Tx_length);
    SetEPTxCount(ENDP1, USB_Tx_length);
    SetEPTxValid(ENDP1);
  }

}

/*******************************************************************************
* Function Name  : Get_SerialNum.
* Description    : Create the serial number string descriptor.
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void Get_SerialNum(void)
{
  uint32_t Device_Serial0, Device_Serial1, Device_Serial2;

  Device_Serial0 = *(uint32_t*)ID1;
  Device_Serial1 = *(uint32_t*)ID2;
  Device_Serial2 = *(uint32_t*)ID3;

  Device_Serial0 += Device_Serial2;

  if (Device_Serial0 != 0)
  {
    IntToUnicode (Device_Serial0, &Virtual_Com_Port_StringSerial[2] , 8);
    IntToUnicode (Device_Serial1, &Virtual_Com_Port_StringSerial[18], 4);
  }
}

/*******************************************************************************
* Function Name  : HexToChar.
* Description    : Convert Hex 32Bits value into char.
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
static void IntToUnicode (uint32_t value , uint8_t *pbuf , uint8_t len)
{
  uint8_t idx = 0;

  for( idx = 0 ; idx < len ; idx ++)
  {
    if( ((value >> 28)) < 0xA )
    {
      pbuf[ 2* idx] = (value >> 28) + '0';
    }
    else
    {
      pbuf[2* idx] = (value >> 28) + 'A' - 10;
    }

    value = value << 4;

    pbuf[ 2* idx + 1] = 0;
  }
}

#ifdef USE_FULL_ASSERT
/*******************************************************************************
 * Function Name  : assert_failed
 * Description    : Reports the name of the source file and the source line number
 *                  where the assert_param error has occurred.
 * Input          : - file: pointer to the source file name
 *                  - line: assert_param error line source number
 * Output         : None
 * Return         : None
 *******************************************************************************/
void assert_failed(uint8_t* file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	while (1)
	{
	}
}
#endif