/**
 * @file    main.c
 * @brief   APP 主程序
 * @details 实现 GD32F130F8T6 单串口（PA9=TX / PA10=RX，RS485）辐射监测从机：
 *          1. 系统初始化（时钟由 startup 阶段调用的 SystemInit 完成，48MHz HXTAL PLL）
 *          2. 使用 HX711 采集辐射值，换算为整型与浮点型
 *          3. 通过 Modbus RTU 从机协议对外输出（整型 + 浮点）
 *          4. 根据启动标志选择启动模式（跳转由 Bootloader 完成）
 *          5. 支持 Ymodem 在线升级（写启动标志后复位进入 Bootloader）
 *
 * @note    Bootloader 地址分配（详见 bootloader/Hardware/bootloader.h）：
 *          BOOT    0x08000000 ~ 0x08003FFF (16KB)
 *          META A  0x08004000 ~ 0x080043FF (1KB)
 *          META B  0x08004400 ~ 0x080047FF (1KB)
 *          APP     0x08005000 ~ 0x0800A3FF (21KB)  <- 本程序链接基址 0x08005000
 *          BACKUP  0x0800A400 ~ 0x0800F7FF (21KB)
 *          CONFIG  0x0800F800 ~ 0x0800FBFF (1KB)
 *          RESERVE 0x0800FC00 ~ 0x0800FFFF (1KB)
 */
#include "main.h"
#include "timer.h"
#include "modbus.h"
#include "eeprom.h"
#include "wdg.h"
#include "Hx711.h"
#include "iap_trigger.h"

int main(void)
{
    uint32_t bound;
    uint8_t  addr;

    /* 1. 中断向量重定位到 APP 起点 0x08005000。
          必须与 Bootloader 约定的 APP_SECTOR_ADDR 一致，否则中断不响应。 */
    SCB->VTOR = FLASH_BASE | 0x5000;

    /* 2. SysTick 延时初始化。SystemInit（startup 阶段调用）已把主频配为 48MHz，
          delay_init() 根据 SystemCoreClock 计算 us/ms 延时系数，务必在延时函数前调用。 */
    delay_init();

    /* 2.1 NVIC 优先级分组（2 位抢占 / 2 位子优先级）。
          与 Bootloader 保持一致，使 USART0/TIMER2 的中断优先级确定，
          不依赖复位默认值或 Bootloader 残留状态。 */
    nvic_priority_group_set(NVIC_PRIGROUP_PRE2_SUB2);

    __enable_irq();

    /* 3. 独立看门狗：预分频 /256，重载 2344 -> 约 15s 超时。
          后续每轮主循环都要喂狗，业务代码中任何 >15s 的阻塞都会触发复位。 */
    IWDG_Init(6, 2344);
    delay_ms(500);

    /* 4. 串口（RS485，PA9/PA10）+ 1ms 帧间超时定时器 + Modbus 协议栈。
          先用 9600 波特率启动，读完 EEPROM 后按保存值重配。 */
    Modbus_uart2_init(9600);
    TIM3_Int_Init(1000 - 1, 48 - 1);   /* 函数名历史遗留，实际配置的是 TIMER2，1ms 中断 */

    Modbus_Init();
    AT24CXX_Init();

    Usart2_SendString("radiation Modbus Slave V6.0\r\n");
    delay_ms(100);

    /* 5. 检测 EEPROM，缺料时提示并喂狗（不会进入业务，也不会触发看门狗复位） */
    while (AT24CXX_Check()) {
        Usart2_SendString("EEPROM not found\r\n");
        delay_ms(1000);
        IWDG_Feed();
    }

    /* 6. 从 EEPROM 读取波特率与从机地址（并同步到 Modbus 寄存器） */
    bound = bound_add_read();
    if (bound != 0 && bound != 1) {
        bound = 0;                       /* 非法值回落为 0（9600） */
    }
    modbus.bound = (bound == 1) ? 115200 : 9600;
    Reg[8] = (uint16_t)bound;            /* 波特率寄存器：0=9600，1=115200 */

    addr = slave_add_read();
    modbus.myadd = addr;
    modbus.myadd_cached = addr;
    Reg[16] = addr;                      /* 从机地址寄存器：1~99 */

    /* 7. 装载 HX711 标定参数并初始化（标定参数存于 EEPROM，无效时用默认值） */
    Hx711_Load_Calibration();
    Hx711_init();

    /* 8. 用最终波特率重配串口 */
    Modbus_uart2_init(modbus.bound);
    delay_ms(100);

    /* 试运行 APP 仅在完成 EEPROM、参数、HX711 和通信初始化后确认。
       若新版程序在这些关键步骤前反复复位，Bootloader 会保留 Backup 并回滚。 */
    iap_confirm_app_boot();

    /* 9. 主循环：喂狗 -> 采样辐射 -> 更新寄存器 -> 处理 Modbus */
    for (;;) {
        IWDG_Feed();

        Read_Hx711();             /* 非阻塞采样（HX711 就绪才读，其余轮次直接返回） */
        Update_Radiation_Regs();  /* 整型 + 浮点辐射值写入 Modbus 寄存器 */

        Modbus_Event();           /* 处理一帧完整的 Modbus 请求 */
    }
}
