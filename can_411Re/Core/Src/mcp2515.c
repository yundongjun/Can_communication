/*
 * mcp2515.c
 *
 *  Created on: Sep 24, 2025
 *      Author: STC
 */

#include "mcp2515.h"

static inline void CS_LOW(mcp2515_t *d){ HAL_GPIO_WritePin(d->cs_port, d->cs_pin, GPIO_PIN_RESET); }
static inline void CS_HIGH(mcp2515_t *d){ HAL_GPIO_WritePin(d->cs_port, d->cs_pin, GPIO_PIN_SET); }

void MCP2515_Begin(mcp2515_t *dev, SPI_HandleTypeDef *hspi, GPIO_TypeDef *CS_Port, uint16_t CS_Pin){
    dev->hspi = hspi;
    dev->cs_port = CS_Port;
    dev->cs_pin = CS_Pin;
    CS_HIGH(dev);
}

static void spi_tx(mcp2515_t *d, const uint8_t *buf, size_t len){
    HAL_SPI_Transmit(d->hspi, (uint8_t*)buf, len, 10);
}
static void spi_rx(mcp2515_t *d, uint8_t *buf, size_t len){
    HAL_SPI_Receive(d->hspi, buf, len, 10);
}
static void spi_txrx(mcp2515_t *d, const uint8_t *tx, uint8_t *rx, size_t len){
    HAL_SPI_TransmitReceive(d->hspi, (uint8_t*)tx, rx, len, 10);
}

bool MCP2515_Reset(mcp2515_t *d){
    uint8_t cmd = MCP_RESET;
    CS_LOW(d); spi_tx(d, &cmd, 1); CS_HIGH(d);
    HAL_Delay(5);
    return true;
}

uint8_t MCP2515_Read(mcp2515_t *d, uint8_t addr){
    uint8_t tx[3] = {MCP_READ, addr, 0xFF};
    uint8_t rx[3] = {0};
    CS_LOW(d); spi_txrx(d, tx, rx, 3); CS_HIGH(d);
    return rx[2];
}

void MCP2515_Write(mcp2515_t *d, uint8_t addr, uint8_t val){
    uint8_t tx[3] = {MCP_WRITE, addr, val};
    CS_LOW(d); spi_tx(d, tx, 3); CS_HIGH(d);
}

void MCP2515_BitModify(mcp2515_t *d, uint8_t addr, uint8_t mask, uint8_t data){
    uint8_t tx[4] = {MCP_BITMOD, addr, mask, data};
    CS_LOW(d); spi_tx(d, tx, 4); CS_HIGH(d);
}

/* 500kbps @ 16MHz (일반적으로 많이 쓰는 설정)
   - SJW=1, BRP=1?  (아래 값은 검증된 관용값 셋: CNF1=0x00, CNF2=0x90, CNF3=0x02)
   - 샘플: 1, PropSeg=2Tq, PhaseSeg1=3Tq, PhaseSeg2=3Tq 근처
   모듈이 8MHz라면 이 값으로는 동작하지 않습니다. 8MHz용 별도 셋 필요.
*/
bool MCP2515_SetConfig_500k_16MHz(mcp2515_t *d){
    // Configuration mode
    MCP2515_BitModify(d, CANCTRL, MODE_MASK, MODE_CONF);
    HAL_Delay(1);
    // CNF 설정
    MCP2515_Write(d, CNF1, 0x00);
    MCP2515_Write(d, CNF2, 0x90); // BTLMODE=1 | PHSEG1=3Tq | PRSEG=1Tq
    MCP2515_Write(d, CNF3, 0x02); // PHSEG2=3Tq
    // RX buffer: 필터 무시(모두 수신)
    MCP2515_SetRXAcceptAll(d);
    // 인터럽트: RX0IE | RX1IE
    MCP2515_Write(d, CANINTE, 0x03);
    return true;
}

bool MCP2515_SetRXAcceptAll(mcp2515_t *d){
    // RXB0CTRL/RXB1CTRL: BUKT=1, RXM=11 (모두 허용)
    MCP2515_Write(d, RXB0CTRL, 0x64); // BUKT=1, RXM1:0=11
    MCP2515_Write(d, RXB1CTRL, 0x60); // RXM1:0=11
    return true;
}

bool MCP2515_SetNormalMode(mcp2515_t *d){
    MCP2515_BitModify(d, CANCTRL, MODE_MASK, MODE_NORM);
    HAL_Delay(1);
    // 확인 (선택)
    uint8_t stat = MCP2515_Read(d, CANSTAT);
    return ((stat & MODE_MASK) == MODE_NORM);
}

static void write_id_std(mcp2515_t *d, uint16_t sid, uint8_t addr_sidh){
    uint8_t sidh = (sid >> 3) & 0xFF;
    uint8_t sidl = (sid & 0x07) << 5; // EID off
    MCP2515_Write(d, addr_sidh, sidh);
    MCP2515_Write(d, addr_sidh+1, sidl);
    // clear EID regs (std frame)
    MCP2515_Write(d, addr_sidh+2, 0x00);
    MCP2515_Write(d, addr_sidh+3, 0x00);
}

bool MCP2515_SendTXB0(mcp2515_t *d, const can_frame_t *fr){
    // ID
    write_id_std(d, (uint16_t)fr->id, TXB0SIDH);
    // DLC
    uint8_t dlc = (fr->dlc > 8) ? 8 : fr->dlc;
    MCP2515_Write(d, TXB0DLC, dlc & 0x0F);
    // Data
    for (uint8_t i=0;i<dlc;i++){
        MCP2515_Write(d, TXB0D0 + i, fr->data[i]);
    }
    // RTS
    uint8_t cmd = MCP_RTS | 0x01; // TXB0
    CS_LOW(d); spi_tx(d, &cmd, 1); CS_HIGH(d);
    return true;
}

bool MCP2515_ReadRXB(mcp2515_t *d, can_frame_t *out){
    // RX Status
    uint8_t cmd = MCP_RX_STATUS;
    uint8_t rx[2] = {0};
    CS_LOW(d); spi_tx(d, &cmd, 1); spi_rx(d, rx, 1); CS_HIGH(d);
    uint8_t stat = rx[0];
    uint8_t which = 0xFF;
    if (stat & 0x40) which = 0; // RXB0
    else if (stat & 0x80) which = 1; // RXB1
    else return false;

    uint8_t base = (which==0)? RXB0SIDH : RXB1SIDH;

    uint8_t sidh = MCP2515_Read(d, base+0);
    uint8_t sidl = MCP2515_Read(d, base+1);
    uint16_t sid = ( (sidh<<3) | (sidl>>5) ) & 0x7FF;
    out->id = sid;

    uint8_t dlc = MCP2515_Read(d, base+4) & 0x0F;
    out->dlc = dlc;

    for(uint8_t i=0;i<dlc;i++){
        out->data[i] = MCP2515_Read(d, (uint8_t)(base+5+i));
    }

    // 인터럽트 플래그는 자동 클리어되거나 CANINTF에서 클리어 (여기서는 생략)

    return true;
}

