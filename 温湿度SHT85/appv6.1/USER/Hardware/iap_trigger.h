/**
 * @file    iap_trigger.h
 * @brief   IAP升级触发功能头文件
 */

#ifndef __IAP_TRIGGER_H
#define __IAP_TRIGGER_H

#include "stm32f10x.h"

/**
 * @brief  触发进入Bootloader升级模式
 * @retval 不会返回（系统复位）
 * @note   调用此函数后会写入升级标志并立即复位进入Bootloader
 *         如果写入失败则返回正常运行，不会复位
 */
void trigger_iap_update(void);

#endif /* __IAP_TRIGGER_H */
