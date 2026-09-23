#include "main.h"
#include "Hx711.h"
#include "modbus.h"
#include "iap_trigger.h"
#include "wdg.h"
#include "stdio.h"

#define VERSION  60062

MODBUS modbus;
uint16_t Reg[128];
char buffer1[50];
char buffer2[50];
char buffer3[50];

void Modbus_Init(void)
{
    modbus.myadd        = 0x01;
    modbus.myadd_cached = 0x01;
    modbus.bound        = 9600;
    modbus.timrun       = 0;
    modbus.Host_Sendtime  = 0;
    modbus.Host_time_flag = 0;
}

/**
 * @brief  浮点数按 IEEE754 大端序拆成两个 16 位寄存器（高字在前，Modbus 惯例）
 * @param  f   待转换的浮点数
 * @param  hi  输出：高 16 位
 * @param  lo  输出：低 16 位
 */
static void float_to_regs(float f, uint16_t *hi, uint16_t *lo)
{
    union {
        float    f;
        uint32_t u;
    } conv;

    conv.f = f;
    *hi = (uint16_t)(conv.u >> 16);        /* 高 16 位 */
    *lo = (uint16_t)(conv.u & 0xFFFFU);    /* 低 16 位 */
}
/**
 * @brief  更新 Modbus 保持寄存器中的辐射测量值（整型 + 浮点型）
 * @note   每轮主循环调用。Reg[0~3] 只在主循环写、在 Modbus_Func3 中读，
 *          二者同线程执行，不存在中断竞争。
 */
void Update_Modbus_Regs(void)
{
		uint32_t bound;
    uint8_t  addr;
	
		Reg[15] = (uint16_t)VERSION;
		snprintf(buffer1, sizeof(buffer1), "app: %d\r\n", (uint16_t)VERSION);
		Usart2_SendString(buffer1);
		delay_ms(500);
  /* 5. 检测 EEPROM，缺料时提示并喂狗（不会进入业务，也不会触发看门狗复位） */
    while (AT24CXX_Check()) {
        Usart2_SendString("EEPROM not found\r\n");
        delay_ms(1000);
        IWDG_Feed();
    }

    /* 6. 从 EEPROM 读取波特率与从机地址（并同步到 Modbus 寄存器） */
    bound = bound_add_read();
    if (bound > 2U) {
        bound = 0;                       /* 非法值回落为 0（9600） */
    }
    modbus.bound = (bound == 1U) ? 19200U : ((bound == 2U) ? 115200U : 9600U);
    Reg[24] = (uint16_t)bound;            /* 波特率寄存器：0=9600，1=19200,2=115200 */

    addr = slave_add_read();
    modbus.myadd = addr;
    modbus.myadd_cached = addr;
    Reg[23] = addr;                      /* 从机地址寄存器：1~99 */

		snprintf(buffer2, sizeof(buffer2), "Modbus slave address: %d\r\n", modbus.myadd);
		Usart2_SendString(buffer2);
		delay_ms(500);
		
		snprintf(buffer3, sizeof(buffer3),
						 "Baud index: %d (0=9600, 1=19200, 2=115200)\r\n", bound);
		Usart2_SendString(buffer3);
		delay_ms(500);
}

/**
 * @brief  更新 Modbus 保持寄存器中的辐射测量值（整型 + 浮点型）
 * @note   每轮主循环调用。Reg[0~3] 只在主循环写、在 Modbus_Func3 中读，
 *          二者同线程执行，不存在中断竞争。
 */
void Update_Radiation_Regs(void)
{
    uint16_t hi, lo;
    float    rad_float;

    /* 整型辐射值（HX711 驱动内已做量程标定并限幅 0~1268） */
    Reg[0] = (uint16_t)(Param_Radi_1[0] + 0.5f);//+0.5f四舍五入

    /* 原始定标值（低 16 位，供上位机调试/诊断用） */
    /* Raw pre-sensitivity F32, MSW first then LSW. */
    float_to_regs(Radi_Back[0], &hi, &lo);
    Reg[3] = hi;
    Reg[4] = lo;

    /* 浮点辐射值 = 整型值 / 换算系数，占 Reg[1](高) + Reg[2](低) 两个寄存器 */
    rad_float = Param_Radi_1[0];
    float_to_regs(rad_float, &hi, &lo);
    Reg[1] = hi;
    Reg[2] = lo;

    /* Mirror calibration params into Reg[] for Func3 read-back */
    Reg[REG_PARAM_RADI] = (uint16_t)Param_Adj_Radi[0];
    Reg[REG_PARAM_DIR]  = (uint16_t)Param_Adj_Dir[0];
}

/**
 * @brief  更新 Modbus 06功能码写入后的常规寄存器
 * @note   每次运行读取03后运行，在 Modbus_Func3 中读
 *          
 */
static void Update_Reg(void)
{
	
    Reg[23] = modbus.myadd;
    if (modbus.bound == 19200)
    {
        Reg[24] = 1;
    }
    else if (modbus.bound == 115200)
    {
        Reg[24] = 2;
    }
    else
    {
        Reg[24] = 0;
    }
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

    for (i = 0; i < 5; i++) {
        Modbus_Send_Byte(modbus.sendbuf[i]);
    }
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
		
		Update_Reg();

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

    for (j = 0; j < i; j++) {
        Modbus_Send_Byte(modbus.sendbuf[j]);
    }
}

#if 0 /* Broadcast-address support disabled. */
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
            baud = 19200;
        } else if (val == 2) {
            baud = 115200;
        }else {
            baud = 9600;
        }

        bound_add_write(val);
        Modbus_uart2_init(baud);
        delay_ms(100);
        NVIC_SystemReset();
    }
}

#endif

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

    for (j = 0; j < i; j++) {
        Modbus_Send_Byte(modbus.sendbuf[j]);
    }
    delay_ms(100);

    if (Regadd == 0x18) {
        uint32_t baud = 9600;
        if (val == 0) {
            baud = 9600;
        } else if (val == 1) {
            baud = 19200;
        } else if (val == 2) {
            baud = 115200;
        } else {
            baud = 9600;
        }
        bound_add_write(val);
        delay_ms(100);
        Modbus_uart2_init(baud);
        delay_ms(100);
        NVIC_SystemReset();
    } else if (Regadd == 0x17) {        /* 从机地址：1~99，写入EEPROM */
        if (val != 0 && val < 100) {
            delay_ms(100);
            slave_add_write((uint16_t)val);
            delay_ms(100);
						modbus.myadd = (uint8_t)val;
						modbus.myadd_cached = (uint8_t)val;
        }
    } else if (Regadd == REG_PARAM_RADI) {	//辐射灵敏度
        if (val >= RADIATION_SENSITIVITY_MIN && val <= RADIATION_SENSITIVITY_MAX) {
            Param_Adj_Radi[0] = (unsigned int)val;
            Hx711_Save_Calibration();
        }
    } else if (Regadd == REG_PARAM_DIR) {	//修正系数
        if (val >= RADIATION_CALIBRATION_MIN && val <= RADIATION_CALIBRATION_MAX) {
            Param_Adj_Dir[0] = (unsigned int)val;
            Hx711_Save_Calibration();
        }
    } else if (Regadd == 0x11) {    /* IAP 升级触发：写 0x1234 进入Bootloader */
        if (val == 0x1234) {
            IWDG_Feed();
            trigger_iap_update();
        }
    }
}
/*
void Modbus_Func16_Broadcast(void)
{
    uint16_t Regadd, Reglen, i;

    Regadd = modbus.rcbuf[2] * 256 + modbus.rcbuf[3];
    Reglen = modbus.rcbuf[4] * 256 + modbus.rcbuf[5];

    // 越界保护：Reglen 不得超过本帧实际携带的数据字数 (recount-10)/2 
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
}*/
/*
void Modbus_Func16(void)
{
    uint16_t Regadd, Reglen;
    uint16_t i, crc, j;

    Regadd = modbus.rcbuf[2] * 256 + modbus.rcbuf[3];
    Reglen = modbus.rcbuf[4] * 256 + modbus.rcbuf[5];

    // 越界保护：Reglen 不得超过本帧实际携带的数据字数 (recount-10)/2 
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

    if (Regadd <= REG_PARAM_RADI && REG_PARAM_RADI < Regadd + Reglen) {
        if (Reg[REG_PARAM_RADI] != 0 && Reg[REG_PARAM_RADI] != 0xFFFF) {
            Param_Adj_Radi[0] = Reg[REG_PARAM_RADI];
            Hx711_Save_Calibration();
        }
    }
    if (Regadd <= REG_PARAM_DIR && REG_PARAM_DIR < Regadd + Reglen) {
        if (Reg[REG_PARAM_DIR] != 0 && Reg[REG_PARAM_DIR] != 0xFFFF) {
            Param_Adj_Dir[0] = Reg[REG_PARAM_DIR];
            Hx711_Save_Calibration();
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

    for (j = 0; j < 8; j++) {
        Modbus_Send_Byte(modbus.sendbuf[j]);
    }
}*/

void Modbus_Event(void)
{
    uint16_t crc;
    uint16_t rccrc;
    uint8_t func;

    if (modbus.reflag == 0U) {
        return;
    }

    if (modbus.recount < 8U) {
        modbus.reflag = 0U;
        modbus.recount = 0U;
        return;
    }

    crc = Modbus_CRC16(modbus.rcbuf, modbus.recount - 2U);
    rccrc = (uint16_t)(modbus.rcbuf[modbus.recount - 2U] * 256U) +
            modbus.rcbuf[modbus.recount - 1U];

    /* Only unicast FC03 and FC06 are enabled. Broadcast address and FC16 are disabled. */
    if (crc == rccrc && modbus.rcbuf[0] == modbus.myadd) {
        func = modbus.rcbuf[1];
        if (func == 3U) {
            Modbus_Func3();
        } else if (func == 6U) {
            Modbus_Func6();
        }
    }

    modbus.recount = 0U;
    modbus.reflag = 0U;
}
