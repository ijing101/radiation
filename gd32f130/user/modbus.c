#include "gd32_common.h"
#include "modbus.h"
#include "iap_trigger.h"
#include "wdg.h"

MODBUS modbus;
uint16_t Reg[128];

void Modbus_Init(void)
{
    modbus.myadd        = 0x01;
    modbus.myadd_cached = 0x01;
    modbus.bound        = 9600;
    modbus.timrun       = 0;
    modbus.Host_Sendtime  = 0;
    modbus.Host_time_flag = 0;
}

static void Modbus_Exception(uint8_t func, uint8_t code)
{
    uint16_t crc, i = 0;

    /* 越界/非法请求返回标准 Modbus 异常帧：功能码|0x80 + 异常码 */
    modbus.sendbuf[i++] = modbus.myadd;
    modbus.sendbuf[i++] = func | 0x80;
    modbus.sendbuf[i++] = code;
    crc = Modbus_CRC16(modbus.sendbuf, i);
    modbus.sendbuf[i++] = (uint8_t)(crc / 256);
    modbus.sendbuf[i++] = (uint8_t)(crc % 256);

    RS485_TX_ENABLE;
    for (i = 0; i < 5; i++) {
        Modbus_Send_Byte(modbus.sendbuf[i]);
    }
    RS485_RX_ENABLE;
}

void Modbus_Func3(void)
{
    uint16_t Regadd, Reglen, crc;
    uint8_t  i, j;

    Regadd = modbus.rcbuf[2] * 256 + modbus.rcbuf[3];
    Reglen = modbus.rcbuf[4] * 256 + modbus.rcbuf[5];

    /* 越界保护：起始地址越界或寄存器数量为0，返回异常帧（否则会越界读/写） */
    if (Regadd >= 128U || Reglen == 0U) {
        Modbus_Exception(0x03, 0x02);
        return;
    }
    /* Reg 数组边界 */
    if (Reglen > (uint16_t)(128U - Regadd)) {
        Modbus_Exception(0x03, 0x02);
        return;
    }
    /* sendbuf[100] 容量：5 + 2*Reglen <= 100 -> Reglen <= 46 */
    if (Reglen > 46U) {
        Modbus_Exception(0x03, 0x02);
        return;
    }

    i = 0;
    modbus.sendbuf[i++] = modbus.myadd;
    modbus.sendbuf[i++] = 0x03;
    modbus.sendbuf[i++] = (uint8_t)((Reglen * 2) % 256);
    for (j = 0; j < Reglen; j++) {
        modbus.sendbuf[i++] = (uint8_t)(Reg[Regadd + j] / 256);
        modbus.sendbuf[i++] = (uint8_t)(Reg[Regadd + j] % 256);
    }
    crc = Modbus_CRC16(modbus.sendbuf, i);
    modbus.sendbuf[i++] = (uint8_t)(crc / 256);
    modbus.sendbuf[i++] = (uint8_t)(crc % 256);

    RS485_TX_ENABLE;
    for (j = 0; j < i; j++) {
        Modbus_Send_Byte(modbus.sendbuf[j]);
    }
    RS485_RX_ENABLE;
}

void Modbus_Func6_Broadcast(void)
{
    uint16_t Regadd;
    uint32_t val;

    Regadd = modbus.rcbuf[2] * 256 + modbus.rcbuf[3];
    val    = modbus.rcbuf[4] * 256 + modbus.rcbuf[5];

    if (Regadd < 128) {
        Reg[Regadd] = (uint16_t)val;
    }

    if (Regadd == 0x08) {
        uint32_t baud = 9600;
        if (val == 0) {
            baud = 9600;
        } else if (val == 1) {
            baud = 115200;
        } else {
            baud = 9600;
        }

        bound_add_write(val);
        Modbus_uart2_init(baud);
        delay_ms(100);
        nvic_system_reset();
    }
}

void Modbus_Func6(void)
{
    uint16_t Regadd;
    uint32_t val;
    uint16_t i, crc, j;

    i = 0;
    Regadd = modbus.rcbuf[2] * 256 + modbus.rcbuf[3];
    val    = modbus.rcbuf[4] * 256 + modbus.rcbuf[5];

    if (Regadd < 128) {
        Reg[Regadd] = (uint16_t)val;
    }

    modbus.sendbuf[i++] = modbus.myadd;
    modbus.sendbuf[i++] = 0x06;
    modbus.sendbuf[i++] = (uint8_t)(Regadd / 256);
    modbus.sendbuf[i++] = (uint8_t)(Regadd % 256);
    modbus.sendbuf[i++] = (uint8_t)(val / 256);
    modbus.sendbuf[i++] = (uint8_t)(val % 256);
    crc = Modbus_CRC16(modbus.sendbuf, i);
    modbus.sendbuf[i++] = (uint8_t)(crc / 256);
    modbus.sendbuf[i++] = (uint8_t)(crc % 256);

    RS485_TX_ENABLE;
    for (j = 0; j < i; j++) {
        Modbus_Send_Byte(modbus.sendbuf[j]);
    }
    delay_ms(100);

    if (Regadd == 0x08) {
        uint32_t baud = 9600;
        if (val == 0) {
            baud = 9600;
        } else if (val == 1) {
            baud = 115200;
        } else {
            baud = 9600;
        }
        bound_add_write(val);
        delay_ms(100);
        Modbus_uart2_init(baud);
        delay_ms(100);
        nvic_system_reset();
    } else if (Regadd == 0x10) {        /* 从机地址：1~99，写入EEPROM并复位 */
        if (val != 0 && val < 100) {
            delay_ms(100);
            slave_add_write((uint16_t)val);
            delay_ms(100);
            nvic_system_reset();
        }
    } else if (Regadd == 0x11) {    /* IAP 升级触发：写 0x1234 进入Bootloader */
        if (val == 0x1234) {
            IWDG_Feed();
            trigger_iap_update();
        }
    }
}

void Modbus_Func16_Broadcast(void)
{
    uint16_t Regadd, Reglen, i;

    Regadd = modbus.rcbuf[2] * 256 + modbus.rcbuf[3];
    Reglen = modbus.rcbuf[4] * 256 + modbus.rcbuf[5];

    /* 越界保护：Reglen 不得超过本帧实际携带的数据字数 (recount-10)/2 */
    if (modbus.recount < 10U) {
        Reglen = 0U;
    } else if (Reglen > (uint16_t)((modbus.recount - 10U) / 2U)) {
        Reglen = (uint16_t)((modbus.recount - 10U) / 2U);
    }

    for (i = 0; i < Reglen; i++) {
        if (Regadd + i < 128) {
            Reg[Regadd + i] = modbus.rcbuf[7 + i * 2] * 256 + modbus.rcbuf[8 + i * 2];
        }
    }
}

void Modbus_Func16(void)
{
    uint16_t Regadd, Reglen;
    uint16_t i, crc, j;

    Regadd = modbus.rcbuf[2] * 256 + modbus.rcbuf[3];
    Reglen = modbus.rcbuf[4] * 256 + modbus.rcbuf[5];

    /* 越界保护：Reglen 不得超过本帧实际携带的数据字数 (recount-10)/2 */
    if (modbus.recount < 10U) {
        Reglen = 0U;
    } else if (Reglen > (uint16_t)((modbus.recount - 10U) / 2U)) {
        Reglen = (uint16_t)((modbus.recount - 10U) / 2U);
    }

    for (i = 0; i < Reglen; i++) {
        if (Regadd + i < 128) {
            Reg[Regadd + i] = modbus.rcbuf[7 + i * 2] * 256 + modbus.rcbuf[8 + i * 2];
        }
    }

    modbus.sendbuf[0] = modbus.rcbuf[0];
    modbus.sendbuf[1] = modbus.rcbuf[1];
    modbus.sendbuf[2] = modbus.rcbuf[2];
    modbus.sendbuf[3] = modbus.rcbuf[3];
    modbus.sendbuf[4] = modbus.rcbuf[4];
    modbus.sendbuf[5] = modbus.rcbuf[5];
    crc = Modbus_CRC16(modbus.sendbuf, 6);
    modbus.sendbuf[6] = (uint8_t)(crc / 256);
    modbus.sendbuf[7] = (uint8_t)(crc % 256);

    RS485_TX_ENABLE;
    for (j = 0; j < 8; j++) {
        Modbus_Send_Byte(modbus.sendbuf[j]);
    }
    RS485_RX_ENABLE;
}

void Modbus_Event(void)
{
    uint16_t crc, rccrc;

    if (modbus.reflag == 0) {
        return;
    }

    if (modbus.recount < 8) {
        modbus.reflag  = 0;
        modbus.recount = 0;
        return;
    }

    crc = Modbus_CRC16(modbus.rcbuf, modbus.recount - 2);
    rccrc = modbus.rcbuf[modbus.recount - 2] * 256 + modbus.rcbuf[modbus.recount - 1];

    if (crc == rccrc) {
        if (modbus.rcbuf[0] == modbus.myadd) {
            uint8_t func = modbus.rcbuf[1];
            if (func != 3 && func != 6 && func != 16) {
                modbus.reflag  = 0;
                modbus.recount = 0;
                return;
            }
            switch (func) {
                case 3:  Modbus_Func3();  break;
                case 6:  Modbus_Func6();  break;
                case 16: Modbus_Func16(); break;
                default: break;
            }
        } else if (modbus.rcbuf[0] == 0) {
            uint8_t func = modbus.rcbuf[1];
            if (func != 6 && func != 16) {
                modbus.reflag  = 0;
                modbus.recount = 0;
                return;
            }
            switch (func) {
                case 6:  Modbus_Func6_Broadcast();  break;
                case 16: Modbus_Func16_Broadcast(); break;
                default: break;
            }
        }
    }

    modbus.recount = 0;
    modbus.reflag  = 0;
}