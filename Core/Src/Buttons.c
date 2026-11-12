/**
  ******************************************************************************
  * @file    Buttons.c
  * @brief   Mod butonlari -- aktif LOW, dahili pull-up
  ******************************************************************************
  * Basma kenarinda bir kez tetikler. Sinyal CFG_BTN_DEBOUNCE_MS boyunca
  * sabit kalmadikca durum degismez.
  ******************************************************************************
  */
#include "Buttons.h"
#include "SpecConfig.h"

typedef struct
{
	uint8_t		Pin;
	uint8_t		Stable;			/* 1 = basili */
	uint8_t		Last;
	uint32_t	ChangeTick;
} Button;

static Button Btn[2] =
{
	{ CFG_BTN_NEXT_PIN, 0, 0, 0 },
	{ CFG_BTN_PREV_PIN, 0, 0, 0 }
};

void BTN_Init(void)
{
	GPIO_InitTypeDef g = {0};

	__HAL_RCC_GPIOB_CLK_ENABLE();

	g.Pin  = (1U << CFG_BTN_NEXT_PIN) | (1U << CFG_BTN_PREV_PIN);
	g.Mode = GPIO_MODE_INPUT;
	g.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(CFG_BTN_PORT, &g);
}

/* Basildiysa 1 doner (yalnizca basma aninda) */
static uint8_t Update(Button *b, uint32_t Now)
{
	uint8_t Raw = (CFG_BTN_PORT->IDR & (1U << b->Pin)) ? 0U : 1U;	/* aktif LOW */

	if(Raw != b->Last)
	{
		b->Last       = Raw;
		b->ChangeTick = Now;
	}
	else if(Raw != b->Stable && (Now - b->ChangeTick) >= CFG_BTN_DEBOUNCE_MS)
	{
		b->Stable = Raw;
		return Raw;
	}
	return 0;
}

int BTN_Poll(void)
{
	uint32_t Now = HAL_GetTick();

	if(Update(&Btn[0], Now)) return  1;
	if(Update(&Btn[1], Now)) return -1;
	return 0;
}
