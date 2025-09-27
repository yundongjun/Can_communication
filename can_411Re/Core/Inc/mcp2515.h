/*
 * mcp2515.h
 *
 *  Created on: Sep 24, 2025
 *      Author: STC
 */

#ifndef INC_MCP2515_H_
#define INC_MCP2515_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* MCP2515 SPI 명령 */
#define MCP_RESET       0xC0
#define MCP_READ        0x03
#define MCP_READ_RX     0x90  // + (0..3)
#define MCP_WRITE       0x02
#define MCP_LOAD_TX     0x40  // + (0..2)*2
#define MCP_RTS         0x80  // | (1<<txbufn)
#define MCP_READ_STATUS 0xA0
#define MCP_RX_STATUS   0xB0
#define MCP_BITMOD      0x05

/* 레지스터 */
#define CANCTRL   0x0F
#define CANSTAT   0x0E
#define CNF3      0x28
#define CNF2      0x29
#define CNF1      0x2A
#define CANINTE   0x2B
#define CANINTF   0x2C
#define EFLG      0x2D
#define RXB0CTRL  0x60
#define RXB1CTRL  0x70
#define TXB0CTRL  0x30
#define TXB0SIDH  0x31
#define TXB0SIDL  0x32
#define TXB0DLC   0x35
#define TXB0D0    0x36
#define RXB0SIDH  0x61
#define RXB0SIDL  0x62
#define RXB0DLC   0x65
#define RXB0D0    0x66
#define RXB1SIDH  0x71
#define RXB1SIDL  0x72
#define RXB1DLC   0x75
#define RXB1D0    0x76

/* CANCTRL 모드 */
#define MODE_MASK  0xE0
#define MODE_CONF  0x80
#define MODE_NORM  0x00

typedef struct {
    uint32_t id;     // 11-bit only in this minimal sample
    uint8_t  dlc;
    uint8_t  data[8];
} can_frame_t;

typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
} mcp2515_t;

void MCP2515_Begin(mcp2515_t *dev, SPI_HandleTypeDef *hspi, GPIO_TypeDef *CS_Port, uint16_t CS_Pin);
bool MCP2515_Reset(mcp2515_t *dev);
uint8_t MCP2515_Read(mcp2515_t *dev, uint8_t addr);
void MCP2515_Write(mcp2515_t *dev, uint8_t addr, uint8_t val);
void MCP2515_BitModify(mcp2515_t *dev, uint8_t addr, uint8_t mask, uint8_t data);
bool MCP2515_SetConfig_500k_16MHz(mcp2515_t *dev);
bool MCP2515_SetNormalMode(mcp2515_t *dev);
bool MCP2515_SetRXAcceptAll(mcp2515_t *dev);
bool MCP2515_SendTXB0(mcp2515_t *dev, const can_frame_t *fr);
bool MCP2515_ReadRXB(mcp2515_t *dev, can_frame_t *out); // checks RX status and pulls from RXB0/1


#endif /* INC_MCP2515_H_ */
