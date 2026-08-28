#include "gd32_common.h"
#include "wdg.h"

static uint16_t wdg_prescaler_sel(uint8_t prer)
{
    switch (prer & 0x07U) {
        case 0:  return FWDGT_PSC_DIV4;
        case 1:  return FWDGT_PSC_DIV8;
        case 2:  return FWDGT_PSC_DIV16;
        case 3:  return FWDGT_PSC_DIV32;
        case 4:  return FWDGT_PSC_DIV64;
        case 5:  return FWDGT_PSC_DIV128;
        default: return FWDGT_PSC_DIV256;
    }
}

void IWDG_Init(uint8_t prer, uint16_t rlr)
{
    /* FWDGT：LSI≈40kHz，超时 = 分频(prescaler) × rlr / 40ms。
       main 用 IWDG_Init(6,2344)：分频/256、rlr=2344 -> 约15s。
       注意 rlr 为 12 位计数器，最大 0xFFF(4095)。 */
    fwdgt_write_enable();
    fwdgt_config(wdg_prescaler_sel(prer), rlr);
    fwdgt_enable();
    fwdgt_counter_reload();
}

void IWDG_Feed(void)
{
    fwdgt_counter_reload();
}