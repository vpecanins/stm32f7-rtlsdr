/**
  ******************************************************************************
  * @file    Main.c
  * @author  Victor Pecanins
  * @version 
  * @date    
  * @brief   USB host RTLSDR demo main file
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
USBH_HandleTypeDef hUSBHost;

uint8_t currentScreen=0;

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void Error_Handler(void);
static void CPU_CACHE_Enable(void);
static void USBH_UserProcess(USBH_HandleTypeDef *phost, uint8_t id);
static void RTLSDR_InitApplication(void);

/* Private functions ---------------------------------------------------------*/


/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void)
{
  /* Enable the CPU Cache */
  CPU_CACHE_Enable();
  
  /* STM32F7xx HAL library initialization:
       - Configure the Flash ART accelerator on ITCM interface
       - Configure the Systick to generate an interrupt each 1 msec
       - Set NVIC Group Priority to 4
       - Low Level Initialization
     */
  HAL_Init();
  
  /* Configure the System clock to have a frequency of 200 MHz */
  SystemClock_Config();
  
  /* Init RTLSDR Application */
  RTLSDR_InitApplication();
  
  /* Init Host Library */
  USBH_Init(&hUSBHost, USBH_UserProcess, 0);
  
  /* Add Supported Class */
  USBH_RegisterClass(&hUSBHost, USBH_RTLSDR_CLASS);
  
  /* Start Host Process */
  USBH_Start(&hUSBHost);
  
  while (1)
  {
    /* USB Host Background task */
    USBH_Process(&hUSBHost); 
    
    
  }
}

/**
  * @brief  RTLSDR application Init.
  * @param  None
  * @retval None
  */
static void RTLSDR_InitApplication(void)
{
  /* Configure Key Button */
  BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_GPIO);
      
  /* Configure LED1 */
  BSP_LED_Init(LED1);
  
  /* Initialize the LCD */
  BSP_LCD_Init();
  
  /* LCD Layer Initialization */
  BSP_LCD_LayerDefaultInit(0, LCD_FB_START_ADDRESS); 
  
  /* Select the LCD Layer */
  BSP_LCD_SelectLayer(0);
  BSP_LCD_SetTransparency(0, 0xFF);
  
  /* Other layer*/
  BSP_LCD_LayerDefaultInit(1, ((uint32_t)(LCD_FB_START_ADDRESS + (RK043FN48H_WIDTH * RK043FN48H_HEIGHT * 4)))); 
  BSP_LCD_SelectLayer(1);
  BSP_LCD_SetTransparency(1, 0x00);
  
  BSP_LCD_Clear(LCD_COLOR_TRANSPARENT);
  BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
  BSP_LCD_DrawCircle(BSP_LCD_GetXSize()/2, BSP_LCD_GetYSize()/2, 10);
  
  BSP_LCD_SelectLayer(0);
  
  /* Enable the display */
  BSP_LCD_DisplayOn();
  
  /* Initialize the LCD Log module */
  LCD_LOG_Init();
  
  /* Configure Button pin as input with External interrupt */
  BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_EXTI);
  
#ifdef USE_USB_HS 
  //LCD_LOG_SetHeader((uint8_t *)" USB OTG HS RTLSDR Host");
  LCD_LOG_SetHeader((uint8_t *)" STM32F7 USB HS RTL-SDR Host");
#else
  LCD_LOG_SetHeader((uint8_t *)" STM32F7 USB FS RTL-SDR Host");
#endif
  
  LCD_UsrLog("USB Host library started.\n"); 
  
  /* Start RTLSDR Interface */
  USBH_UsrLog("Starting RTLSDR Demo");
  
}

/**
  * @brief EXTI line detection callbacks
  * @param GPIO_Pin: Specifies the pins connected EXTI line
  * @retval None
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if(GPIO_Pin==WAKEUP_BUTTON_PIN)
  {
    if (currentScreen==0) {
		BSP_LCD_SetTransparency(0, 0xFF);
		BSP_LCD_SetTransparency(1, 0x00);
		currentScreen=1;
	} else {
		BSP_LCD_SetTransparency(1, 0xFF);
		BSP_LCD_SetTransparency(0, 0x00);
		currentScreen=0;
	}
  }
}

/**
  * @brief  User Process
  * @param  phost: Host Handle
  * @param  id: Host Library user message ID
  * @retval None
  */
  uint8_t id_prev = HOST_USER_SELECT_CONFIGURATION;

static void USBH_UserProcess(USBH_HandleTypeDef *phost, uint8_t id)
{
  if (id != id_prev) {
    switch(id) { 
    case HOST_USER_SELECT_CONFIGURATION:
      USBH_UsrLog("Select config");
      break;
    
    case HOST_USER_CLASS_ACTIVE:
      USBH_UsrLog("Class active");
      
      break;

    case HOST_USER_CLASS_SELECTED:
      USBH_UsrLog("Class selected");
      break;
      
    case HOST_USER_CONNECTION:
      USBH_UsrLog("Connection");
      break;
    
    case HOST_USER_DISCONNECTION:
      USBH_UsrLog("Disconnection");
      break;
    
    case HOST_USER_UNRECOVERED_ERROR:
      USBH_UsrLog("Unrecovered Error");
      break;

    default:
    USBH_UsrLog("Unknown USBH User State: %d", id);
      break; 
    }
  }
  id_prev = id;
}

/**
  * @brief  Toggles LEDs to show user input state.
  * @param  None
  * @retval None
  */
void Toggle_Leds(void)
{
  static uint32_t ticks;

  if(ticks++ == 1000)
  {
    BSP_LED_Toggle(LED1);
    ticks = 0;
  }
}

/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow :
  *            System Clock source            = PLL (HSE)
  *            SYSCLK(Hz)                     = 200000000
  *            HCLK(Hz)                       = 200000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 4
  *            APB2 Prescaler                 = 2
  *            HSE Frequency(Hz)              = 25000000
  *            PLL_M                          = 25
  *            PLL_N                          = 400
  *            PLL_P                          = 2
  *            PLLSAI_N                       = 384
  *            PLLSAI_P                       = 8
  *            VDD(V)                         = 3.3
  *            Main regulator output voltage  = Scale1 mode
  *            Flash Latency(WS)              = 7
  * @param  None
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct;
  
  /* Enable HSE Oscillator and activate PLL with HSE as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_OFF;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 400;  
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 8;
  if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  
  /* Activate the OverDrive to reach the 200 Mhz Frequency */
  if(HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }
  
  /* Select PLLSAI output as USB clock source */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_CLK48;
  PeriphClkInitStruct.Clk48ClockSelection = RCC_CLK48SOURCE_PLLSAIP;
  PeriphClkInitStruct.PLLSAI.PLLSAIN = 192;
  PeriphClkInitStruct.PLLSAI.PLLSAIQ = 4; 
  PeriphClkInitStruct.PLLSAI.PLLSAIP = RCC_PLLSAIP_DIV4;
  if(HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct)  != HAL_OK)
  {
    Error_Handler();
  }
  
  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  if(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_6) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
static void Error_Handler(void)
{
  /* User may add here some code to deal with this error */
  while(1)
  {
  }
}

/**
  * @brief  CPU L1-Cache enable.
  * @param  None
  * @retval None
  */
static void CPU_CACHE_Enable(void)
{
  /* Enable I-Cache */
  SCB_EnableICache();

  /* Enable D-Cache */
  SCB_EnableDCache();
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
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


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
