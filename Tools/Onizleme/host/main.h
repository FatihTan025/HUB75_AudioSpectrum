/* PC (host) derlemesi icin sahte main.h -- firmware'in cizim kodunu PC'de calistirmak icin */
#ifndef __MAIN_H
#define __MAIN_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>

uint32_t HAL_GetTick(void);
extern uint32_t SystemCoreClock;

typedef struct { volatile uint32_t APB1ENR, AHB1ENR; } RCC_TypeDef;
typedef struct { volatile uint32_t CR, CSR; } PWR_TypeDef;
extern RCC_TypeDef HostRcc;
extern PWR_TypeDef HostPwr;
extern uint8_t     HostBkpSram[4096];
#define RCC  (&HostRcc)
#define PWR  (&HostPwr)
#define RCC_APB1ENR_PWREN      (1U << 28)
#define RCC_AHB1ENR_BKPSRAMEN  (1U << 18)
#define PWR_CR_DBP             (1U << 8)
#define PWR_CSR_BRE            (1U << 9)
#define PWR_CSR_BRR            (1U << 3)
#define BKPSRAM_BASE           ((uintptr_t)HostBkpSram)
#endif
