#ifndef I2C_H
#define I2C_H

#define  Addr_EEP_Adj_Radi          0x0000
#define  Addr_EEP_Radi_Accum        Addr_EEP_Adj_Radi + 2
//-----------------------------------------------------------------------------//
#define  Addr_EEP_Saved_NO          Addr_EEP_Radi_Accum + 80  //是否已初始化地址,清除数据就是清这个
#define  Addr_EEP_Saved_Time        Addr_EEP_Saved_NO + 2
#define  Addr_EEP_Adj    				Addr_EEP_Saved_Time+2
#define   AT24C_VALUE        2//256//1024
#if AT24C_VALUE ==1
#define PAGE_SIZE 8
#define SIZE 0x007f
#elif AT24C_VALUE==2
#define PAGE_SIZE 8
#define SIZE 0x00ff
#elif AT24C_VALUE==4
#define PAGE_SIZE 16
#define SIZE 0x01ff
#elif AT24C_VALUE==8
#define PAGE_SIZE 16
#define SIZE 0x03ff
#elif AT24C_VALUE==16
#define PAGE_SIZE 16
#define SIZE 0x07ff
#elif AT24C_VALUE==32
#define PAGE_SIZE 32
#define SIZE 0x0fff
#elif AT24C_VALUE==64
#define PAGE_SIZE 32
#define SIZE 0x1fff
#elif AT24C_VALUE==128
#define PAGE_SIZE 64
#define SIZE 0x3fff
#elif AT24C_VALUE==256
#define PAGE_SIZE 64
#define SIZE 0x7fff
#elif AT24C_VALUE==512
#define PAGE_SIZE 128
#define SIZE 0xffff
#elif AT24C_VALUE==1024
#define PAGE_SIZE 256
#define SIZE 0xffff
#endif
extern unsigned int Param_Radi[2];      //
extern unsigned int Param_Radi_ALL[2];
extern unsigned int Saved_Time;              //存储数据时间间隔
extern unsigned int Saved_NO;                //已存储数据大小,清除数据就是清这个
extern unsigned int Param_Adj[2];
extern void Read_EEPROM(void);//读出设定参数
extern void Init_EEPROM(void); //初始化数据
extern void I2C_Init(void);
//I2C写Char数据
extern void I2C_Write_Byte(unsigned char *byte,unsigned int address,unsigned int count);
//I2C读Char数据
extern void I2C_Read_Byte(unsigned char *byte,unsigned int address,unsigned int count);
//I2C写Int数据
extern void I2C_Write_Int(unsigned int *byte,unsigned int address,unsigned int count);
//I2C读Int数据
extern void I2C_Read_Int(unsigned int*byte,unsigned int address,unsigned int count);
extern void Clear_Accum_Data(void);
#endif

