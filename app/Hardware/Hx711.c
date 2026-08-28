/**
 * @file    Hx711.c
 * @brief   HX711 24 位 ADC 驱动实现
 * @details 通过 GPIO 位控时序读取 HX711，采用通道 A、128 倍增益配置（27 个时钟）。
 *          读取结果先做零点补偿（减 0x2000，对应差分零点），再做量程标定与限幅。
 *          标定参数可从 EEPROM 装载，无效时用默认值兜底，避免除零。
 */
#include "Hx711.h"
#include "eeprom.h"

/* 标定参数在 EEPROM 中的存储地址（与 slave_add=0x10、bound=0x20 错开） */
#define EEPROM_ADDR_PARAM_RADI   0x0030   /* Param_Adj_Radi[0]，2 字节 */
#define EEPROM_ADDR_PARAM_DIR    0x0040   /* Param_Adj_Dir[0]，2 字节 */

/* 标定参数默认值：EEPROM 未写入/无效时使用，保证“1:1 直通”且避免除零。
 * 实际标定请写入 EEPROM（或用自定义 Modbus 功能码在线更新）。
 * 说明：默认值让 Radi_Back=(Tmp-0x2000)、Param_Radi_1=Radi_Back，即原始读数直通。 */
#define DEFAULT_PARAM_RADI        10000   /* Param_Adj_Radi[0] 默认 10000（直通） */
#define DEFAULT_PARAM_DIR         1000    /* Param_Adj_Dir[0] 默认 1000（直通） */

/* ---- 全局变量定义 ---- */
unsigned char HX711_Stable, HX711_UnStable;   /* 稳定/不稳定连续计数 */
unsigned char HX711_Count;                    /* 采样节拍计数 */
unsigned int  Radi_Back[2];                   /* 原始定标后的整型测量值 */
unsigned int  Param_Adj_Radi[2];              /* 量程标定系数 */
unsigned int  Param_Adj_Dir[5];               /* 方向/校准系数 */
unsigned int  Param_Radi_1[2];                /* 最终整型测量值 */
unsigned int  Param_Radi_ALL_1[2];            /* （保留） */
unsigned int  Radi_ALL[2];                    /* （保留） */

/**
 * @brief  读取一次 HX711 24 位转换结果
 * @retval 24 位数据（已做补码换算：原始差分值 + 2^23，零点对应 0x800000）
 * @note   每读一次需要 27 个 SCK 时钟：前 24 个读数据，第 25~27 个选择
 *          通道 A / 128 倍增益，供下一次转换使用。
 */
unsigned int Hx711_Data(void)
{
    unsigned long Data = 0x0000;
    unsigned char i;

    /* 24 个时钟：DOUT 高位先出 */
    for (i = 0; i < 24; i++) {
        gpio_bit_set(Hx711_Port, Hx711_SCK);
        Delay_NOP(10);
        gpio_bit_reset(Hx711_Port, Hx711_SCK);
        Delay_NOP(10);
        Data = Data << 1;
        if (gpio_input_bit_get(Hx711_Port, Hx711_DOUT) == 1) {
            Data++;
        }
    }

    /* 再补 3 个时钟，完成通道/增益选择时序（共 27 个时钟） */
    gpio_bit_set(Hx711_Port, Hx711_SCK);
    Delay_NOP(10);
    gpio_bit_reset(Hx711_Port, Hx711_SCK);
    Delay_NOP(10);
    gpio_bit_set(Hx711_Port, Hx711_SCK);
    Delay_NOP(10);
    gpio_bit_reset(Hx711_Port, Hx711_SCK);
    Delay_NOP(10);
    gpio_bit_set(Hx711_Port, Hx711_SCK);
    Delay_NOP(10);
    gpio_bit_reset(Hx711_Port, Hx711_SCK);
    Delay_NOP(10);

    /* 差分零点补偿：原始值异或 0x800000，即 (原始差分值 + 2^23) */
    Data = Data ^ 0x800000;
    return Data;
}

/**
 * @brief  初始化 HX711 相关 GPIO
 * @note   上电后丢弃前两次转换结果，让 HX711 内部模拟通道稳定。
 */
void Hx711_init(void)
{
    rcu_periph_clock_enable(Hx711_RCU);

    /* SCK 推挽输出，初始低电平 */
    gpio_mode_set(Hx711_Port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, Hx711_SCK);
    gpio_output_options_set(Hx711_Port, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, Hx711_SCK);
    gpio_bit_reset(Hx711_Port, Hx711_SCK);

    /* DOUT 上拉输入（HX711 数据就绪引脚，低电平表示有数据） */
    gpio_mode_set(Hx711_Port, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, Hx711_DOUT);
    gpio_bit_set(Hx711_Port, Hx711_DOUT);

    /* 上电稳定：丢弃前两次转换结果 */
    delay_1ms(10);
    Hx711_Data();
    delay_1ms(10);
    Hx711_Data();
}

/**
 * @brief  从 EEPROM 装载标定参数，无效值时使用默认值兜底
 * @note   EEPROM 未初始化读出的 0x00/0xFFFF 视为无效，回落默认值，
 *          保证后续计算不会除零。
 */
void Hx711_Load_Calibration(void)
{
    uint32_t v;

    /* 方向/校准系数 */
    v = AT24CXX_ReadLenByte(EEPROM_ADDR_PARAM_DIR, 2);
    if (v == 0 || v == 0xFFFF) {
        Param_Adj_Dir[0] = DEFAULT_PARAM_DIR;
    } else {
        Param_Adj_Dir[0] = (unsigned int)v;
    }

    /* 量程标定系数 */
    v = AT24CXX_ReadLenByte(EEPROM_ADDR_PARAM_RADI, 2);
    if (v == 0 || v == 0xFFFF) {
        Param_Adj_Radi[0] = DEFAULT_PARAM_RADI;
    } else {
        Param_Adj_Radi[0] = (unsigned int)v;
    }

    /* 兜底：任何情况下都不允许除零 */
    if (Param_Adj_Dir[0] == 0) {
        Param_Adj_Dir[0] = DEFAULT_PARAM_DIR;
    }
    if (Param_Adj_Radi[0] == 0) {
        Param_Adj_Radi[0] = DEFAULT_PARAM_RADI;
    }
}

/**
 * @brief  将标定参数写回 EEPROM
 * @note   Modbus 写灵敏度/修正系数时调用，掉电不丢失。
 */
void Hx711_Save_Calibration(void)
{
    AT24CXX_WriteLenByte(EEPROM_ADDR_PARAM_RADI, Param_Adj_Radi[0], 2);
    AT24CXX_WriteLenByte(EEPROM_ADDR_PARAM_DIR, Param_Adj_Dir[0], 2);
}

/**
 * @brief  读取并处理一次 HX711 测量（非阻塞）
 * @note   DOUT 为高（数据未就绪）时直接返回；就绪后做零点判断、稳定滤波、
 *          量程标定与限幅。每 4 个采样节拍重置一次稳定性统计。
 */
void Read_Hx711(void)
{
    int32_t Tmp;

    /* 数据未就绪直接返回，不阻塞主循环 */
    if (gpio_input_bit_get(Hx711_Port, Hx711_DOUT) != 0) {
        return;
    }

    if (HX711_Count != 0) {
        /* 取高 14 位（24 位右移 10 位），0x2000 对应差分零点 */
        Tmp = (int32_t)(Hx711_Data() >> 10);

        if (Tmp >= 0x2000) {
            /* 高于零点阈值：信号有效，做稳定计数 */
            HX711_UnStable = 0x00;
            if (++HX711_Stable > 1) {
                /* 连续稳定：零点补偿 + 方向/量程标定 */
                Tmp = (Tmp - 0x2000) * 1000 / (int32_t)Param_Adj_Dir[0];
                Radi_Back[0] = (unsigned int)Tmp;
            } else {
                /* 首拍：沿用上一次的值，避免突变 */
                Tmp = (int32_t)Radi_Back[0];
            }
        } else {
            /* 低于零点阈值：判为不稳定/无信号 */
            Tmp = 0;
            if (++HX711_UnStable > 1) {
                Tmp = 0;
                Radi_Back[0] = 0;
            } else {
                Tmp = (int32_t)Radi_Back[0];
            }
        }

        /* 量程标定并限幅 */
        Param_Radi_1[0] = (unsigned int)(Tmp * 10000 / (int32_t)Param_Adj_Radi[0]);
        if (Param_Radi_1[0] > 1368) {     /* 原逻辑：阈值 1368，限幅到 1268(0x04F4) */
            Param_Radi_1[0] = 1268;
        }

        /* 每 4 个采样节拍重置一次稳定性统计 */
        if (++HX711_Count > 3) {
            HX711_Count = 0;
            HX711_Stable = 0;
            HX711_UnStable = 0;
        }
    } else {
        /* 第一拍：仅推进状态机，丢弃本次读数 */
        ++HX711_Count;
        Hx711_Data();
    }
}
