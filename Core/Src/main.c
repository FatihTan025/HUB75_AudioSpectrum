/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : HUB75 Audio Spectrum Analyser - STM32F446
  ******************************************************************************
  *   PA5 (ADC1_IN5) -> FFT -> log bantlar + ritim -> 9 gorsel mod
  *   AYARLAR: SpecConfig.h      MOD: PB3 (+) / PB4 (-), aktif LOW -> 128x32 panel
  *   HUB75 surucusu HUB75_32F446RC ile AYNI (dosyalar degistirilmedi).
  *   Uygulama kodu USER CODE bloklarinin icinde.
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
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "dma.h"
#include "tim.h"
#include "HUB75.h"
#include "Audio_ADC.h"
#include "Spectrum.h"
#include "SpecDisplay.h"
#include "SpecConfig.h"
#include "Buttons.h"
#include "Encoder.h"
#include "Settings.h"
#include "Uart1.h"
#include "Protocol.h"
#include "Saat.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Tum ayarlar SpecConfig.h icinde */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static uint32_t		LastModeChange	= 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
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
  /* USER CODE BEGIN 2 */

	/* --- HUB75: TIM1 + DMA2_Stream5, OE = TIM3_CH2 (PC7) --------------- */
	MX_DMA_Init();
	MX_TIM1_Init();
	MX_TIM3_Init();				/* HUB75_DMA_Init'ten ONCE				*/

	HUB75_Clear();
	HUB75_UpdateDisplay();
	HUB75_DMA_Init();

	BTN_Init();					/* PB3 = +, PB4 = -  (aktif LOW)		*/
	ENC_Init();					/* PB6/PB7 TIM4 encoder, PB5 buton		*/

	/* --- Ses zinciri ------------------------------------------------- */
	SPEC_Init();
	SPEC_DisplayInit();
	SET_Init();					/* Ayarlar yedek SRAM'den: mod, parlaklik */
	SAAT_Init();				/* RTC (LSE): beklemeden baslar		*/
	/* --- Haberlesme: USART1 PA9/PA10 --------------------------------- */
	UART1_Init(UART1_BAUD);
	PROTO_Init();
	AUDIO_Init();				/* En son: ornekleme burada baslar		*/

	LastModeChange = HAL_GetTick();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	const uint16_t *Block = AUDIO_GetReadyBlock();
	int Step;

	if(Block)
	{
		/* Islemci yetismeyip kare atlanirsa tempo izci zamani kaybetmesin */
		static uint32_t LastFrameCount = 0;
		uint32_t Fc = AUDIO_FrameCount, Fs = Fc - LastFrameCount;
		LastFrameCount  = Fc;
		SPEC_FrameStep  = (uint8_t)((Fs < 1U) ? 1U : (Fs > 8U) ? 8U : Fs);

		SPEC_Process(Block);	/* Ornekler burada okunur				*/
		PROTO_OnFrame();
		AUDIO_ReleaseBlock();

		{
			/* Sure olcumu (test): panelin sag ustunde gosterilir */
			uint32_t t0 = DWT->CYCCNT, t1;
			SPEC_Draw();
			t1 = DWT->CYCCNT;
			HUB75_UpdateDisplay();
			SPEC_DrawUs   = (t1 - t0) / (SystemCoreClock / 1000000U);
			SPEC_UpdateUs = (DWT->CYCCNT - t1) / (SystemCoreClock / 1000000U);
		}
	}

	/* Mod butonlari */
	Step = BTN_Poll();
	if(Step != 0)
	{
		SET_SetCurMode((uint8_t)((SET_GetCurMode() + SET_LookCount() + Step) % SET_LookCount()));
		SET_MarkLocalChange();
		PROTO_NotifyMode(SET_GetCurMode());
		LastModeChange = HAL_GetTick();
	}

	/* Encoder: cevir = mod, basili cevir = parlaklik, uzun bas = ekran ac/kapa */
	{
		ENC_Event e;
		ENC_Poll(HAL_GetTick(), &e);
		if(e.Steps && e.Held)
		{
			int32_t v = SET_Global(SET_ID_BRIGHTNESS) + e.Steps * CFG_ENC_BRIGHT_STEP, Uyg = v;
			if(SET_Set(SET_MODE_GLOBAL, SET_ID_BRIGHTNESS, v, &Uyg) == SET_OK || Uyg != v)
			{
				SET_MarkLocalChange();
				PROTO_NotifyParam(SET_MODE_GLOBAL, SET_ID_BRIGHTNESS, Uyg);
				SPEC_ShowLevel((uint8_t)Uyg);
			}
		}
		else if(e.Steps)
		{
			int32_t m = ((int32_t)SET_GetCurMode() + e.Steps) % (int32_t)SET_LookCount();
			if(m < 0) m += (int32_t)SET_LookCount();
			SET_SetCurMode((uint8_t)m);
			SET_MarkLocalChange();
			PROTO_NotifyMode(SET_GetCurMode());
		}
		if(e.Long)
		{
			SET_SetPower(!SET_GetPower());
			SET_MarkLocalChange();
			PROTO_NotifyPower(SET_GetPower());
		}
		/* e.Click: menu (sonraki adim) */
	}

	/* Otomatik mod gecisi (ayar: saniye, 0 = kapali). Mod nereden degisirse
	   degissin (dugme, uygulama) sayac bastan baslar. */
	{
		static uint8_t PrevMode = 0xFFU;
		if(SET_GetCurMode() != PrevMode)
		{
			PrevMode       = SET_GetCurMode();
			LastModeChange = HAL_GetTick();
		}
	}
	{
		uint32_t AutoSec = (uint32_t)SET_Global(SET_ID_AUTO_SEC);
		if(AutoSec && (HAL_GetTick() - LastModeChange) >= (AutoSec * 1000U))
		{
			LastModeChange = HAL_GetTick();
			SET_SetCurMode((uint8_t)((SET_GetCurMode() + 1U) % SET_LookCount()));
			PROTO_NotifyMode(SET_GetCurMode());
		}
	}

	/* Haberlesme + otomatik kayit */
	PROTO_Poll(HAL_GetTick());
	SET_Poll(HAL_GetTick());
	SAAT_Poll();
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
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
