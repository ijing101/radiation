#ifndef _MODBUS_CRC_H
#define _MODBUS_CRC_H

#include "gd32_common.h"

uint16_t Modbus_CRC16(uint8_t *puchMsg, uint16_t usDataLen);

#endif /* _MODBUS_CRC_H */