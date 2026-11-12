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
	uint8_t		UzunOldu;		/* Bu basista uzun bildirildi: birakinca kisa sayilmaz */
	uint32_t	ChangeTick;
	uint32_t	BasTick;
} Button;

static Button Btn[2] =
{
	{ CFG_BTN_NEXT_PIN, 0, 0, 0, 0, 0 },
	{ CFG_BTN_PREV_PIN, 0, 0, 0, 0, 0 }
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

/* Donus: 1 kisa basma bitti (birakildi), 2 uzun basma (basiliyken, bir kez), 0 yok */
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
		if(Raw) { b->BasTick = Now; b->UzunOldu = 0; return 0; }
		return b->UzunOldu ? 0U : 1U;
	}
	if(b->Stable && !b->UzunOldu && (Now - b->BasTick) >= CFG_BTN_LONG_MS)
	{
		b->UzunOldu = 1;
		return 2;
	}
	return 0;
}

void BTN_Poll(BTN_Olay *o)
{
	const uint32_t Now = HAL_GetTick();
	const uint8_t  a = Update(&Btn[0], Now), b = Update(&Btn[1], Now);

	o->Kisa = (a == 1U) ? 1 : (b == 1U) ? -1 : 0;
	o->Uzun = (a == 2U) ? 1U : 0U;					/* PB4 uzun: bir sey yapmaz */
}
