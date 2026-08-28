#ifndef _modbus_h_
#define _modbus_h_
#include "sys.h"
#include "modbus_crc.h"
#include "timer.h"
#include "rs485.h"
#include "usart.h"
#include "delay.h"
#include "eeprom.h"
#include "sht85.h"

//
extern int16_t temperature;

typedef struct 
{
	//
	u8  myadd;        //
	u8  myadd_cached; //
	u8  rcbuf[100];   //
	u8  timout;       //
	u8  recount;      //
	u8  timrun;       //
	u8  reflag;       //
	u8  sendbuf[100]; //
	u32  bound;
	
	
	//
	u8 Host_Txbuf[8];	//modbus
	u8 slave_add;		//
	u8 Host_send_flag;//
	int Host_Sendtime;//
	u8 Host_time_flag;//
	u8 Host_End;//
}MODBUS;

// 寄存器联合体，支持有符号和无符号访问
typedef union {
    u16 u_val;      // 无符号值（用于Modbus传输）
    int16_t s_val;  // 有符号值（用于温度等带负数的数据）
} reg_union_t;

extern MODBUS modbus;
extern u16 Reg[];
void Modbus_Init(void);
void Modbus_Func3(void);
void Modbus_Func6(void);
//void Modbus_Func6_Broadcast(void);
void Modbus_Func16(void);
//void Modbus_Func16_Broadcast(void);
void Modbus_Event(void);

// 配置保存加载函数
void Modbus_Save_Config(uint8_t slave_addr, uint32_t baudrate,
                        int16_t temp_offset, int16_t humi_offset);
void Modbus_Load_Config(void);

//void Host_send03(void);
void Host_Read03_slave(uint8_t slave,uint16_t StartAddr,uint16_t num);
void Host_RX(void);
//涓绘満鎺ユ敹浠庢満鏁版嵁淇℃伅鍚庤繘琛屽?勭悊
void HOST_ModbusRX(void);
void RS485_Usart_SendArray(USART_TypeDef* pUSARTx,uint8_t *array,uint8_t num);

#endif

