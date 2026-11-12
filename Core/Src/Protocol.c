/**
  ******************************************************************************
  * @file    Protocol.c
  * @brief   Haberlesme protokolu -- bkz. Protocol.h
  ******************************************************************************
  * Yanitlarin ilk bayti her zaman durum (st). Alan duzenleri:
  *
  *   01 INFO        ->  st Proto(1) Fw(2) W(2) H(2) Bitplane(1) ModSayisi(1)
  *                      ParamSayisi(2) AktifMod(1) Guc(1) Ad(...)
  *   02 SET_NAME    Ad(...)                  -> st
  *   03 PING                                 -> st
  *   10 MODE_LIST   [Bas(1)]                 -> st Toplam Bas Adet
  *                      { No Tur Aile AdUz Ad } * Adet   (sigmayan: Bas ile devam)
  *   11 MODE        [Mod(1)]                 -> st Mod   (bos = oku)
  *   20 GET_PARAM   Mod(1) Id(2)             -> st Mod Id Deger(4)
  *   21 SET_PARAM   Mod(1) Id(2) Deger(4)    -> st Mod Id Uygulanan(4)
  *                      st = 3: aralik disi, sinirlanmis deger uygulandi
  *   22 GET_ALL     Mod(1)                   -> st Mod Adet { Id(2) Deger(4) } * Adet
  *                      Mod = FF: genel ayarlar, 0..: o modun kendi ayarlari
  *   23 PARAM_DESCRIBE Sira(2)               -> st Sira(2) Toplam(2) Id(2) Tur Bayrak
  *                      ModMaske(4) Min(4) Max(4) Adim(4) Varsayilan(4)
  *                      GosterId(2) GosterAlt(2) GosterUst(2)  (Id 0 = hep gorunur)
  *                      Ad\0 Grup\0 Secenek\0
  *   30 SAVE / 31 LOAD / 32 FACTORY_RESET    -> st
  *   40 TIME        [Sa Dk Sn [Gun Ay Yil-2000]] -> st Sa Dk Sn Gun Ay Yil-2000  (bos = oku)
  *   51 POWER       [Acik(1)]                -> st Acik  (bos = oku)
  *   70 STATUS      Aralik(2) ms, 0 = dur    -> st
  *        akis (SEQ 0, CMD 70): Mod Tasma(4) IslemUs(2) CizimUs(2) AktarimUs(2)
  *                      Kick Vurus Beat (son rapordan beri adet)
  *                      Ses Bas Tiz BagilSeviye (0..255)
  *   60 EVENT (SEQ 0): Tur Veri...  (bkz. EVT_*)
  *   Diger bilinen komutlar simdilik st = 6 (desteklenmiyor) doner.
  ******************************************************************************
  */
#pragma GCC optimize("O2")

#include <string.h>
#include "main.h"
#include "Protocol.h"
#include "Uart1.h"
#include "Settings.h"
#include "SpecDisplay.h"
#include "Spectrum.h"
#include "Audio_ADC.h"
#include "HUB75_Config.h"
#include "Saat.h"

#define SOF					0xA5U
#define RX_TIMEOUT_MS		100U
#define STATUS_MIN_MS		50U

typedef enum { ST_SOF = 0, ST_LEN0, ST_LEN1, ST_BODY, ST_CRC0, ST_CRC1 } RxState;

static RxState		State;
static uint16_t		RxLen, RxPos, RxCrc;
static uint8_t		RxBody[2U + PROTO_MAX_PAYLOAD];	/* SEQ CMD PAYLOAD	*/
static uint32_t		RxTick;

static uint8_t		Out[PROTO_MAX_PAYLOAD];
static uint16_t		StatusMs;
static uint32_t		StatusTick;
static uint8_t		CntKick, CntHit, CntBeat;

/* ==========================================================================
   Yardimcilar
   ========================================================================== */
static uint16_t Crc16(uint16_t Crc, const uint8_t *p, uint16_t n)
{
	while(n--)
	{
		Crc ^= (uint16_t)(*p++) << 8;
		for(uint8_t b = 0; b < 8U; b++)
			Crc = (Crc & 0x8000U) ? (uint16_t)((Crc << 1) ^ 0x1021U) : (uint16_t)(Crc << 1);
	}
	return Crc;
}

static uint8_t *Put16(uint8_t *p, uint16_t v)	{ p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); return p + 2; }
static uint8_t *Put32(uint8_t *p, uint32_t v)	{ p = Put16(p, (uint16_t)v); return Put16(p, (uint16_t)(v >> 16)); }
static uint16_t Get16(const uint8_t *p)			{ return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t Get32(const uint8_t *p)			{ return (uint32_t)Get16(p) | ((uint32_t)Get16(p + 2) << 16); }

static uint8_t *PutStr(uint8_t *p, const uint8_t *End, const char *s)
{
	while(*s && p < End - 1)
		*p++ = (uint8_t)*s++;
	*p++ = 0;
	return p;
}

static uint8_t Scale255(float v)
{
	if(v <= 0.0f) return 0;
	if(v >= 1.0f) return 255;
	return (uint8_t)(v * 255.0f);
}

static void Send(uint8_t Seq, uint8_t Cmd, const uint8_t *Data, uint16_t Len)
{
	uint8_t Hdr[5], Tail[2];
	uint16_t Crc;

	Hdr[0] = SOF;
	Hdr[1] = (uint8_t)Len;
	Hdr[2] = (uint8_t)(Len >> 8);
	Hdr[3] = Seq;
	Hdr[4] = Cmd;
	Crc = Crc16(0xFFFFU, &Hdr[1], 4U);
	Crc = Crc16(Crc, Data, Len);
	Tail[0] = (uint8_t)Crc;
	Tail[1] = (uint8_t)(Crc >> 8);

	/* Uc parca tek seferde sigmazsa hic gonderme (yarim cerceve olmasin) */
	{
		static uint8_t Frame[5U + PROTO_MAX_PAYLOAD + 2U];
		memcpy(Frame, Hdr, 5U);
		memcpy(&Frame[5], Data, Len);
		memcpy(&Frame[5U + Len], Tail, 2U);
		(void)UART1_Write(Frame, (uint16_t)(Len + 7U));
	}
}

static void Reply(uint8_t Seq, uint8_t Cmd, uint8_t St)
{
	Out[0] = St;
	Send(Seq, (uint8_t)(Cmd | 0x80U), Out, 1U);
}

/* ==========================================================================
   Komutlar
   ========================================================================== */
static void Handle(uint8_t Seq, uint8_t Cmd, const uint8_t *In, uint16_t Len)
{
	uint8_t *p = Out + 1;
	const uint8_t *End = Out + sizeof(Out);
	uint8_t St = PROTO_OK;

	switch(Cmd)
	{
	case CMD_INFO:
		*p++ = PROTO_VERSION;
		p = Put16(p, PROTO_FW_VERSION);
		p = Put16(p, SPEC_WIDTH);
		p = Put16(p, SPEC_HEIGHT);
		*p++ = HUB75_BITPLANES;
		*p++ = SET_LookCount();
		p = Put16(p, SET_Count());
		*p++ = SET_GetCurMode();
		*p++ = SET_GetPower();
		p = PutStr(p, End, SET_GetName()) - 1;		/* Sondaki 0 gonderilmez	*/
		break;

	case CMD_SET_NAME:
		SET_SetName((const char *)In, (uint8_t)((Len > SET_NAME_LEN) ? SET_NAME_LEN : Len));
		break;

	case CMD_PING:
		break;

	case CMD_MODE_LIST:
	{
		uint8_t Start = (Len >= 1U) ? In[0] : 0U, Count = 0;
		uint8_t *Hdr = p;

		p += 3;
		for(uint8_t m = Start; m < SET_LookCount(); m++)
		{
			const SET_ModeInfo *mi = SET_Mode(m);
			uint8_t n = (uint8_t)strlen(mi->Name);
			if(p + 4 + n > End)
				break;
			*p++ = m;
			*p++ = mi->Kind;
			*p++ = mi->Family;
			*p++ = n;
			memcpy(p, mi->Name, n);
			p += n;
			Count++;
		}
		Hdr[0] = SET_LookCount();
		Hdr[1] = Start;
		Hdr[2] = Count;
		break;
	}

	case CMD_MODE:
		if(Len >= 1U)
		{
			if(In[0] < SET_LookCount())
				SET_SetCurMode(In[0]);
			else
				St = PROTO_ERR_RANGE;
		}
		*p++ = SET_GetCurMode();
		break;

	case CMD_GET_PARAM:
	case CMD_SET_PARAM:
	{
		int32_t v = 0;
		uint16_t Id;

		if(Len < ((Cmd == CMD_SET_PARAM) ? 7U : 3U))
		{
			St = PROTO_ERR_PARAM;
			break;
		}
		Id = Get16(&In[1]);
		if(Cmd == CMD_SET_PARAM)
			St = SET_Set(In[0], Id, (int32_t)Get32(&In[3]), &v);
		else
			St = SET_Get(In[0], Id, &v);
		*p++ = In[0];
		p = Put16(p, Id);
		p = Put32(p, (uint32_t)v);
		break;
	}

	case CMD_GET_ALL:
	{
		uint8_t Mode = (Len >= 1U) ? In[0] : SET_MODE_GLOBAL, *Cnt;

		if(Mode != SET_MODE_GLOBAL && Mode >= SET_LookCount())
		{
			St = PROTO_ERR_RANGE;
			break;
		}
		*p++ = Mode;
		Cnt  = p++;
		*Cnt = 0;
		for(uint16_t i = 0; i < SET_Count() && p + 6 <= End; i++)
		{
			const SET_Param *d = SET_Desc(i);
			uint8_t PerMode = (d->Flags & SET_FLAG_PER_MODE) ? 1U : 0U;
			int32_t v;

			if(PerMode != (Mode != SET_MODE_GLOBAL))
				continue;
			if(SET_Get(Mode, d->Id, &v) != SET_OK)
				continue;			/* Bu modda yok */
			p = Put16(p, d->Id);
			p = Put32(p, (uint32_t)v);
			(*Cnt)++;
		}
		break;
	}

	case CMD_PARAM_DESCRIBE:
	{
		uint16_t Idx = (Len >= 2U) ? Get16(In) : 0xFFFFU;
		const SET_Param *d = SET_Desc(Idx);

		if(!d)
		{
			St = PROTO_ERR_RANGE;
			break;
		}
		p = Put16(p, Idx);
		p = Put16(p, SET_Count());
		p = Put16(p, d->Id);
		*p++ = d->Type;
		*p++ = d->Flags;
		p = Put32(p, d->ModeMask);
		p = Put32(p, (uint32_t)d->Min);
		p = Put32(p, (uint32_t)d->Max);
		p = Put32(p, (uint32_t)d->Step);
		p = Put32(p, (uint32_t)d->Def);
		p = Put16(p, d->ShowId);
		p = Put16(p, (uint16_t)d->ShowLo);
		p = Put16(p, (uint16_t)d->ShowHi);
		p = PutStr(p, End, d->Name);
		p = PutStr(p, End, d->Group);
		p = PutStr(p, End, d->Options);
		break;
	}

	case CMD_SAVE:			SET_Save();		break;
	case CMD_LOAD:			SET_Load();		break;
	case CMD_FACTORY_RESET:	SET_Factory();	break;

	case CMD_POWER:
		if(Len >= 1U)
			SET_SetPower(In[0]);
		*p++ = SET_GetPower();
		break;

	case CMD_STATUS:
		StatusMs = (Len >= 2U) ? Get16(In) : 0U;
		if(StatusMs && StatusMs < STATUS_MIN_MS)
			StatusMs = STATUS_MIN_MS;
		StatusTick = HAL_GetTick();
		CntKick = CntHit = CntBeat = 0;
		break;

	case CMD_TIME:											/* Sa Dk Sn [Gun Ay Yil-2000]	*/
	{
		uint8_t Sa, Dk, Sn, Gun, Ay;
		uint16_t Yil;

		if(Len >= 3U && !SAAT_Set(In[0], In[1], In[2]))
			St = PROTO_ERR_RANGE;
		else if(Len >= 6U && !SAAT_TarihSet(In[3], In[4], (uint16_t)(2000U + In[5])))
			St = PROTO_ERR_RANGE;
		SAAT_Get(&Sa, &Dk, &Sn);
		SAAT_TarihGet(&Gun, &Ay, &Yil);
		*p++ = Sa;
		*p++ = Dk;
		*p++ = Sn;
		*p++ = Gun;
		*p++ = Ay;
		*p++ = (uint8_t)(Yil - 2000U);
		break;
	}

	case CMD_SENSOR:
	case CMD_FILE_LIST:
	case CMD_FILE_WRITE:
	case CMD_FILE_DELETE:
	case CMD_FILE_PLAY:
	case CMD_AUTO:
	case CMD_BRIDGE:
		St = PROTO_ERR_UNSUPPORTED;
		break;

	default:
		St = PROTO_ERR_CMD;
		break;
	}

	if(St != PROTO_OK && St != PROTO_ERR_RANGE && p == Out + 1)
	{
		Reply(Seq, Cmd, St);
		return;
	}
	Out[0] = St;
	Send(Seq, (uint8_t)(Cmd | 0x80U), Out, (uint16_t)(p - Out));
}

static void SendStatus(void)
{
	uint8_t *p = Out;

	*p++ = SET_GetCurMode();
	p = Put32(p, AUDIO_OverrunCount);
	p = Put16(p, (uint16_t)SPEC_ProcessUs);
	p = Put16(p, (uint16_t)SPEC_DrawUs);
	p = Put16(p, (uint16_t)SPEC_UpdateUs);
	*p++ = CntKick;
	*p++ = CntHit;
	*p++ = CntBeat;
	*p++ = Scale255(SPEC_Volume);
	*p++ = Scale255(SPEC_Bass);
	*p++ = Scale255(SPEC_Treble);
	*p++ = Scale255(SPEC_LoudRel);
	CntKick = CntHit = CntBeat = 0;
	Send(0, CMD_STATUS, Out, (uint16_t)(p - Out));
}

/* ==========================================================================
   Disari acik fonksiyonlar
   ========================================================================== */
void PROTO_Init(void)
{
	uint8_t d[2];

	State    = ST_SOF;
	StatusMs = 0;
	Put16(d, PROTO_FW_VERSION);
	PROTO_SendEvent(EVT_BOOT, d, 2U);
}

void PROTO_Poll(uint32_t Now)
{
	uint8_t Buf[64];
	uint16_t n;

	UART1_Poll();

	if(State != ST_SOF && (Now - RxTick) > RX_TIMEOUT_MS)
		State = ST_SOF;							/* Yarim kalan cerceve		*/

	while((n = UART1_Read(Buf, sizeof(Buf))) != 0U)
	{
		RxTick = Now;
		for(uint16_t i = 0; i < n; i++)
		{
			uint8_t b = Buf[i];

			switch(State)
			{
			case ST_SOF:
				if(b == SOF)
					State = ST_LEN0;
				break;
			case ST_LEN0:
				RxLen = b;
				State = ST_LEN1;
				break;
			case ST_LEN1:
				RxLen |= (uint16_t)b << 8;
				if(RxLen > PROTO_MAX_PAYLOAD)
				{
					State = ST_SOF;
					break;
				}
				{
					uint8_t L[2] = { (uint8_t)RxLen, (uint8_t)(RxLen >> 8) };
					RxCrc = Crc16(0xFFFFU, L, 2U);
				}
				RxPos = 0;
				State = ST_BODY;
				break;
			case ST_BODY:
				RxBody[RxPos++] = b;
				if(RxPos >= RxLen + 2U)
				{
					RxCrc = Crc16(RxCrc, RxBody, RxPos);
					State = ST_CRC0;
				}
				break;
			case ST_CRC0:
				if(b != (uint8_t)RxCrc)
				{
					State = ST_SOF;			/* Bozuk: sessizce at		*/
					break;
				}
				State = ST_CRC1;
				break;
			case ST_CRC1:
				State = ST_SOF;
				if(b == (uint8_t)(RxCrc >> 8))
					Handle(RxBody[0], RxBody[1], &RxBody[2], RxLen);
				break;
			}
		}
	}

	if(StatusMs && (Now - StatusTick) >= StatusMs)
	{
		StatusTick += StatusMs;
		if((Now - StatusTick) >= StatusMs)
			StatusTick = Now;				/* Cok geride kaldiysa yakala	*/
		SendStatus();
	}

	UART1_Poll();
}

void PROTO_OnFrame(void)
{
	if(!StatusMs)
		return;
	if(SPEC_Kick && CntKick < 255U)	CntKick++;
	if(SPEC_Hit  && CntHit  < 255U)	CntHit++;
	if(SPEC_Beat && CntBeat < 255U)	CntBeat++;
}

void PROTO_SendEvent(uint8_t Type, const uint8_t *Data, uint8_t Len)
{
	uint8_t Buf[1U + 32U];

	if(Len > 32U)
		Len = 32U;
	Buf[0] = Type;
	memcpy(&Buf[1], Data, Len);
	Send(0, CMD_EVENT, Buf, (uint16_t)(Len + 1U));
}

void PROTO_NotifyMode(uint8_t Mode)
{
	PROTO_SendEvent(EVT_MODE, &Mode, 1U);
}

void PROTO_NotifyParam(uint8_t Mode, uint16_t Id, int32_t Value)
{
	uint8_t d[7];
	d[0] = Mode;
	d[1] = (uint8_t)Id;            d[2] = (uint8_t)(Id >> 8);
	d[3] = (uint8_t)Value;         d[4] = (uint8_t)((uint32_t)Value >> 8);
	d[5] = (uint8_t)((uint32_t)Value >> 16); d[6] = (uint8_t)((uint32_t)Value >> 24);
	PROTO_SendEvent(EVT_PARAM, d, 7U);
}

void PROTO_NotifyPower(uint8_t On)
{
	PROTO_SendEvent(EVT_POWER, &On, 1U);
}
