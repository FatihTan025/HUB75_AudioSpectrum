/**
  ******************************************************************************
  * @file    Protocol.h
  * @brief   Haberlesme protokolu -- UART1 uzerinden (ESP32 WiFi/BLE kopru)
  ******************************************************************************
  * Cerceve:  A5 | LEN(2) | SEQ | CMD | PAYLOAD(LEN) | CRC16(2)
  *   Sayilar little-endian. CRC16-CCITT (0x1021, baslangic 0xFFFF),
  *   LEN'den PAYLOAD sonuna kadar. LEN en fazla 256.
  *   Yanit: CMD | 0x80, ayni SEQ, ilk bayt durum.
  *   Karttan kendiliginden: SEQ = 0 (0x60 olay, 0x70 durum akisi).
  * Ayrintili alan duzeni Protocol.c basinda.
  ******************************************************************************
  */
#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <stdint.h>

#define PROTO_VERSION			1U
#define PROTO_FW_VERSION		0x0100U		/* 1.0							*/
#define PROTO_MAX_PAYLOAD		256U

/* Durum kodlari */
#define PROTO_OK				0U
#define PROTO_ERR_CMD			1U
#define PROTO_ERR_PARAM			2U
#define PROTO_ERR_RANGE			3U
#define PROTO_ERR_CRC			4U
#define PROTO_ERR_BUSY			5U
#define PROTO_ERR_UNSUPPORTED	6U

/* Komutlar */
#define CMD_INFO				0x01U
#define CMD_SET_NAME			0x02U
#define CMD_PING				0x03U
#define CMD_MODE_LIST			0x10U
#define CMD_MODE				0x11U
#define CMD_GET_PARAM			0x20U
#define CMD_SET_PARAM			0x21U
#define CMD_GET_ALL				0x22U
#define CMD_PARAM_DESCRIBE		0x23U
#define CMD_SAVE				0x30U
#define CMD_LOAD				0x31U
#define CMD_FACTORY_RESET		0x32U
#define CMD_TIME				0x40U
#define CMD_SENSOR				0x41U
#define CMD_FILE_LIST			0x48U
#define CMD_FILE_WRITE			0x49U
#define CMD_FILE_DELETE			0x4AU
#define CMD_FILE_PLAY			0x4BU
#define CMD_AUTO				0x50U
#define CMD_POWER				0x51U
#define CMD_EVENT				0x60U
#define CMD_STATUS				0x70U
#define CMD_BRIDGE				0x7FU

/* Olay turleri (CMD_EVENT ilk bayti) */
#define EVT_BOOT				0x01U		/* Fw(2)						*/
#define EVT_MODE				0x02U		/* Mod(1)						*/
#define EVT_PARAM				0x03U		/* Mod(1) Id(2) Deger(4)		*/
#define EVT_POWER				0x04U		/* Acik(1)						*/

void PROTO_Init(void);						/* UART1_Init'ten sonra			*/
void PROTO_Poll(uint32_t Now);				/* Ana donguden, sik sik		*/
void PROTO_OnFrame(void);					/* Her SPEC_Process'ten sonra	*/

void PROTO_SendEvent(uint8_t Type, const uint8_t *Data, uint8_t Len);
void PROTO_NotifyMode(uint8_t Mode);		/* Dugme ile mod degisti		*/
void PROTO_NotifyParam(uint8_t Mode, uint16_t Id, int32_t Value);	/* Cihazda ayar degisti	*/
void PROTO_NotifyPower(uint8_t On);

#endif /* __PROTOCOL_H__ */
