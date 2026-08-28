#include "modbus.h"
#include "iap_trigger.h"

char buffer[50];
MODBUS modbus;//

char buffer1[50];
char buffer2[50];
char buffer3[50];

//128
u16 Reg[128];
	/*
	03
	Reg[0]	温度	0.1°C
	Reg[1]	湿度	0-100.0%
	Reg[2]	
	Reg[3]
	Reg[4]
	Reg[5]
	Reg[6]
	Reg[7]
	
	06只写
	reg[17]	写入0x1234进入升级模式
	
	03/06可读可写
	Reg[20]	温度校准值 -999-999
	Reg[21]	湿度校准值 -999-999
	Reg[22]	
	
	Reg[23]	地址		0-247
	Reg[24]	波特率		0=9600bps,1=19200bps,2=115200bps
	*/

static void Modbus_Send_Exception(u8 func, u8 code)
{
	u16 crc;
	u8 i, j;

	i = 0;
	modbus.sendbuf[i++] = modbus.myadd;
	modbus.sendbuf[i++] = func | 0x80;
	modbus.sendbuf[i++] = code;
	crc = Modbus_CRC16(modbus.sendbuf, i);
	modbus.sendbuf[i++] = crc / 256;
	modbus.sendbuf[i++] = crc % 256;

	RS485_TX_ENABLE;
	for (j = 0; j < i; j++)
	{
		Modbus_Send_Byte(modbus.sendbuf[j]);
	}
	RS485_RX_ENABLE;
}

void Update1()
{
	uint8_t slave_addr;
	uint32_t baudrate;
	int16_t temp_offset, humi_offset;
	if (Flash_Storage_Load_Config(&slave_addr, &baudrate, &temp_offset,
								  &humi_offset) == 0) {
		Reg[20] = temp_offset;  // 瀵勫瓨鍣?锛氭俯搴︽牎鍑嗗€?(鍗曚綅0.1掳C)
		Reg[21] = (uint16_t)humi_offset;
		Reg[22] = 0;
	} else {
		Reg[20] = 0;            // 榛樿娓╁害鏍″噯鍊间负0
		Reg[21] = 0;
		Reg[22] = 0;
	}
}

//娓╁害鏇存柊
void Update2()
{
	uint16_t temp1, temp2;
	// 鐩存帴鏇存柊瀵勫瓨鍣紝绉婚櫎杩囦簬涓ユ牸鐨勬鏌?
	temp1 = (u16)(SHT85.temp*10);    // 温度 x10
	Reg[0] = temp1 + Reg[20];
	
	temp2 = (u16)(SHT85.humi*10);    // 湿度 x10
	Reg[1] = temp2 + Reg[21];
	
	Reg[2] = 0;

	// 添加原始ADC值用于调试
	Reg[5]=SHT85.temp_raw;           // 温度原始ADC值
	Reg[6]=SHT85.humi_raw;           // 湿度原始ADC值
	Reg[7]=0;                        // 保留
	Reg[8]=0;                        // 保留

	Reg[23] = modbus.myadd;
	
	// 波特率(0=9600bps, 1=19200bps,2=115200)
	if (modbus.bound == 9600) {
		Reg[24] = 0;
	} else if (modbus.bound == 19200) {
		Reg[24] = 1;
	} else if (modbus.bound == 115200) {
		Reg[24] = 2;
	}else {
		Reg[24] = 0; // 默认9600
	}
}

void Modbus_Init()
{
	uint8_t slave_addr;
	uint32_t baudrate;
	int16_t temp_offset, humi_offset;
	
	uint32_t Bound;//娉㈢壒鐜?
	uint32_t actual_baudrate;//娉㈢壒鐜?
	uint8_t addr;//浠庢満鍦板潃
	
  if (Flash_Storage_Load_Config(&slave_addr, &baudrate, &temp_offset,
                                &humi_offset) == 0) {
    modbus.myadd = slave_addr;
    modbus.myadd_cached = slave_addr;
    modbus.bound = baudrate;
    Reg[20] = (uint16_t)temp_offset;
    Reg[21] = (uint16_t)humi_offset;
    Reg[22] = 0;
    // 鏍￠獙浣嶅拰娓╁害鏍″噯鍊间俊鎭瓨鍌ㄥ湪瀵勫瓨鍣ㄤ腑锛屽湪Modbus_Event涓鐞?
  } else {
    modbus.myadd = 0x01;
    modbus.myadd_cached = 0x01;
    modbus.bound = 9600;
    Reg[20] = 0;
    Reg[21] = 0;
    Reg[22] = 0;
	}
	//重新初始化串口，并打印Modbus配置信息
	Bound = (modbus.bound == 19200) ? 1 :
	        (modbus.bound == 115200) ? 2 : 0;
	if(Bound == 0){
		actual_baudrate = 9600;
	}
	else if(Bound == 1){
		actual_baudrate = 19200;
	}
	else if(Bound == 2){
		actual_baudrate = 115200;
	}
	else{
		actual_baudrate = 9600;		// 鏃犳晥鍊硷紝浣跨敤榛樿9600
	}
	modbus.bound = actual_baudrate;
	Reg[24] = Bound;
	
	addr = modbus.myadd;
	modbus.myadd_cached = addr;
	Reg[23] = addr;
	snprintf(buffer1, sizeof(buffer1), "Modbus slave address: %d\r\n", addr);
	Usart2_SendString(buffer1);
	delay_ms(500);
	snprintf(buffer2, sizeof(buffer2), "Baud index: %d (0=9600, 1=19200, 2=115200)\r\n", Bound);
	Usart2_SendString(buffer2);
	delay_ms(500);
	
	Modbus_uart2_init(actual_baudrate);//鏇存柊娉㈢壒鐜?
	delay_ms(100);

	modbus.timrun = 0;    // modbus瀹氭椂杩愯鏍囧織
}


// Modbus 3鍙峰姛鑳界爜鍑芥暟
void Modbus_Func3()
{
	u16 Regadd,Reglen,crc;
	u8 i,j;	
//	u16 response_size;
	
	Update1();
	Update2();

	//寰楀埌瑕佽鍙栧瘎瀛樺櫒鐨勯鍦板潃
	Regadd = modbus.rcbuf[2]*256+modbus.rcbuf[3];//璇诲彇鐨勯鍦板潃
	//寰楀埌瑕佽鍙栧瘎瀛樺櫒鐨勬暟鎹暱搴?
	Reglen = modbus.rcbuf[4]*256+modbus.rcbuf[5];//璇诲彇鐨勫瘎瀛樺櫒鏁伴噺

	if (Reglen == 0 || Regadd >= 128 || Reglen > (128 - Regadd)){
		Modbus_Send_Exception(0x03, 0x02);
		return;
	}
	if ((Reglen * 2 + 5) > sizeof(modbus.sendbuf)){
		Modbus_Send_Exception(0x03, 0x03);
		return;
	}
	
	//鍙戦€佸搷搴旀暟鎹寘
	i = 0;
	modbus.sendbuf[i++] = modbus.myadd;      //ID鍙凤細鍙戦€佹湰鏈鸿澶囧湴鍧€
	modbus.sendbuf[i++] = 0x03;              //鍙戦€佸姛鑳界爜
	modbus.sendbuf[i++] = ((Reglen*2)%256);   //杩斿洖瀛楄妭鏁?
	for(j=0;j<Reglen;j++)                    //杩斿洖鏁版嵁
	{

	  modbus.sendbuf[i++] = Reg[Regadd+j]/256;//楂樹綅鏁版嵁
		modbus.sendbuf[i++] = Reg[Regadd+j]%256;//浣庝綅鏁版嵁
	}
	crc = Modbus_CRC16(modbus.sendbuf,i);    //璁＄畻瑕佽繑鍥炴暟鎹殑CRC
	modbus.sendbuf[i++] = crc/256;//鏍￠獙浣嶉珮浣?
	modbus.sendbuf[i++] = crc%256;//鏍￠獙浣嶄綆浣?

	RS485_TX_ENABLE;//杩欐槸寮€鍚?85鍙戦€?
	
	for(j=0;j<i;j++)//鍙戦€佹暟鎹?
	{
	  Modbus_Send_Byte(modbus.sendbuf[j]);	
	}
	RS485_RX_ENABLE;//杩欓噷鏄叧闂?85鍙戦€?
}

// Modbus 6鍙峰姛鑳界爜鍑芥暟   淇敼娉㈢壒鐜?
// Modbus 涓绘満鍐欏叆瀵勫瓨鍣ㄥ€?
void Modbus_Func6()  
{
	u16 Regadd;//鍦板潃16浣?
	uint16_t val;//鍊?
	u16 i,crc,j;
	i=0;
	
	Regadd=modbus.rcbuf[2]*256+modbus.rcbuf[3];  //寰楀埌瑕佷慨鏀圭殑鍦板潃 
	val=modbus.rcbuf[4]*256+modbus.rcbuf[5];     //淇敼鍚庣殑鍊?瑕佸啓鍏ョ殑鏁版嵁)
	
	// 鏁扮粍瓒婄晫淇濇姢锛氬彧鍐欏叆鏈夋晥鑼冨洿鐨勫瘎瀛樺櫒
	if(Regadd < 128)
	{
		Reg[Regadd]=val;  //淇敼鏈澶囩浉搴旂殑瀵勫瓨鍣?
	}

	modbus.sendbuf[i++]=modbus.myadd;//鏈澶囧湴鍧€
	modbus.sendbuf[i++]=0x06;        //鍔熻兘鐮?
	modbus.sendbuf[i++]=Regadd/256;//鍐欏叆鐨勫湴鍧€
	modbus.sendbuf[i++]=Regadd%256;
	modbus.sendbuf[i++]=val/256;//鍐欏叆鐨勬暟鍊?
	modbus.sendbuf[i++]=val%256;
	crc=Modbus_CRC16(modbus.sendbuf,i);//鑾峰彇crc鏍￠獙浣?
	modbus.sendbuf[i++]=crc/256;  //crc鏍￠獙浣嶅姞鍏ュ寘涓?
	modbus.sendbuf[i++]=crc%256;
	//鏁版嵁鍙戦€佸寘鎵撳寘瀹屾瘯
	RS485_TX_ENABLE;//浣胯兘485鎺у埗绔?寮€鍚彂閫?  
	for(j=0;j<i;j++)
	{
	 Modbus_Send_Byte(modbus.sendbuf[j]);
	}
	delay_ms(100);
	RS485_RX_ENABLE;

	if (Regadd == 20) {
		int16_t signed_val = (int16_t)val;
		// 娓╁害鏍″噯鍊艰寖鍥存鏌?(0 ~ +999, 鍗曚綅0.1掳C)
		if (signed_val >= -999 && signed_val <= 999) {
			// 淇濆瓨鍒癋lash锛堜娇鐢ㄥ綋鍓嶄覆鍙ｈ缃級
			Reg[20] = (uint16_t)signed_val;
			Modbus_Save_Config(modbus.myadd, modbus.bound, signed_val,
			                   (int16_t)Reg[21]);
		}
	}
	else if (Regadd == 21) {
		int16_t signed_val = (int16_t)val;
		if (signed_val >= -999 && signed_val <= 999) {
			Reg[21] = (uint16_t)signed_val;
			Modbus_Save_Config(modbus.myadd, modbus.bound,
			                   (int16_t)Reg[20], (int16_t)Reg[21]);
		}
	}
	else if (Regadd == 22) {
		int16_t signed_val = (int16_t)val;
		if (signed_val >= -999 && signed_val <= 999) {

		}
	}
	// 鐗瑰埆澶勭悊锛氬鏋滃啓鍏ュ瘎瀛樺櫒64锛堣澶囧湴鍧€锛夛紝瑙﹀彂搴旂敤骞朵繚瀛?
	else if (Regadd == 23 && val >= 1 && val <= 247) {
		// 鏇存柊鏂扮殑浠庢満鍦板潃
		modbus.myadd = (uint8_t)val;
		modbus.myadd_cached = (uint8_t)val;
		// 淇濆瓨鍒癋lash锛堜娇鐢ㄥ綋鍓嶆尝鐗圭巼銆佹牎楠屼綅鍜屾俯搴︽牎鍑嗗€硷級
		Modbus_Save_Config((uint8_t)val, modbus.bound,
		                   (int16_t)Reg[20], (int16_t)Reg[21]);
		delay_ms(100);
		//NVIC_SystemReset();
	}	

	else if(Regadd==24)// 波特率索引：0=9600, 1=19200, 2=115200
	{
		u32 actual_baudrate = 9600; // 榛樿娉㈢壒鐜?
		
		if(val == 0){
			actual_baudrate = 9600;
		}
		else if(val == 1){
			actual_baudrate = 19200;
		}
		else if(val == 2){
			actual_baudrate = 115200;
		}
		else{
			actual_baudrate = 9600;
		}
		modbus.bound = actual_baudrate;
	
		Modbus_Save_Config(modbus.myadd, actual_baudrate,
		                   (int16_t)Reg[20], (int16_t)Reg[21]);
		// 閲嶆柊閰嶇疆涓插彛娉㈢壒鐜?
		Modbus_uart2_reconfig(actual_baudrate);
	
		// 娉㈢壒鐜囦慨鏀瑰悗閲嶅惎鐢熸晥
		delay_ms(100);
		//NVIC_SystemReset();
	}
	else if(Regadd==0x11)//IAP鍗囩骇瑙﹀彂瀵勫瓨鍣ㄥ湴鍧€涓?7
		{
		 if(val==0x1234)  // 鐗规畩鍊?x1234瑙﹀彂鍗囩骇
		 {
			 
			 IWDG_ReloadCounter(); // 鍠傜嫍锛岄槻姝㈠浣嶅墠瓒呮椂
			 trigger_iap_update(); 
		 }

	}
}

/*
// 骞挎挱妯″紡涓嬬殑鍗曞瘎瀛樺櫒鍐欏叆锛氭墽琛屽啓鍏ュ拰鐩稿叧鍔ㄤ綔锛屼絾涓嶅洖鍖?
void Modbus_Func6_Broadcast(void)
{
	u16 Regadd;
	u32 val;
	USART_InitTypeDef USART_InitStructure;

	Regadd = modbus.rcbuf[2] * 256 + modbus.rcbuf[3];
	val = modbus.rcbuf[4] * 256 + modbus.rcbuf[5];

	if (Regadd < 128)
	{
		Reg[Regadd] = val;
	}

	if (Regadd == 0x08)
	{
		u32 actual_baudrate = 9600;
		if (val == 0)
		{
			actual_baudrate = 9600;
		}
		else if (val == 1)
		{
			actual_baudrate = 115200;
		}
		else
		{
			actual_baudrate = 9600;
		}

		bound_add_write(val);
		delay_ms(100);

		USART_Cmd(USART2, DISABLE);
		USART_InitStructure.USART_BaudRate = actual_baudrate;
		USART_InitStructure.USART_WordLength = USART_WordLength_8b;
		USART_InitStructure.USART_StopBits = USART_StopBits_1;
		USART_InitStructure.USART_Parity = USART_Parity_No;
		USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
		USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
		USART_Init(USART2, &USART_InitStructure);
		USART_Cmd(USART2, ENABLE);
		RS485_RX_ENABLE;
		delay_ms(100);
		NVIC_SystemReset();
	}
	else if (Regadd == 0x10)
	{
		if (val != 0 && val < 100)
		{
			delay_ms(100);
			delay_ms(100);
			NVIC_SystemReset();
		}
	}
	else if (Regadd == 0x07)
	{
		if (val == 1)
		{
			delay_ms(500);
			delay_ms(500);
			NVIC_SystemReset();
		}
	}
	else if (Regadd == 0x09)
	{
		if (val == 1 || val == 0)
		{
			delay_ms(100);
			NVIC_SystemReset();
		}
	}
	else if (Regadd == 0x0A)
	{
		delay_ms(100);
		NVIC_SystemReset();
	}
	else if (Regadd == 0x11)
	{
		if (val == 0x1234)
		{
			IWDG_ReloadCounter();
			trigger_iap_update();
		}
	}
}
*/


// Modbus浜嬩欢澶勭悊鍑芥暟
void Modbus_Event()
{
	u16 crc,rccrc;//crc鍜屾帴鏀跺埌鐨刢rc		 
	//娌℃湁鎺ユ敹鍒版暟鎹寘
  if(modbus.reflag == 0)  //濡傛灉鎺ユ敹鏈畬鎴愬垯杩斿洖绌?
	{ 
	   return;
	}
	
	// 銆愪慨澶嶃€戞渶灏忓抚闀垮害妫€鏌ワ細Modbus RTU鏈€灏忓抚闀垮害涓?瀛楄妭锛堝湴鍧€+鍔熻兘鐮?鏁版嵁+CRC锛?
	if(modbus.recount < 8)
	{
		// printf("甯ц繃鐭紝闀垮害=%d锛屼涪寮僜r\n", modbus.recount);
		modbus.reflag = 0;
		modbus.recount = 0;
		return;
	}
	
	//鎺ユ敹鍒版暟鎹寘(鎺ユ敹瀹屾垚)
	//閫氳繃璇诲埌鐨勬暟鎹抚璁＄畻CRC
	//鍙傛暟1鏄暟缁勯鍦板潃锛屽弬鏁?鏄璁＄畻鐨勯暱搴?闄や簡CRC鏍￠獙浣嶅叾浣欏叏绠?
	crc = Modbus_CRC16(&modbus.rcbuf[0],modbus.recount-2); //鑾峰彇CRC鏍￠獙浣?
	// 璇诲彇鏁版嵁甯х殑CRC
	rccrc = modbus.rcbuf[modbus.recount-2]*256+modbus.rcbuf[modbus.recount-1];//璁＄畻璇诲彇鐨凜RC鏍￠獙浣?
	//绛変环浜庝笅闈㈣繖鏉¤鍙?
	//rccrc=modbus.rcbuf[modbus.recount-1]|(((u16)modbus.rcbuf[modbus.recount-2])<<8);//鑾峰彇鎺ユ敹鍒扮殑CRC
	 
	// 娉ㄦ剰锛氬湴鍧€銆佹尝鐗圭巼銆丳ID鍙傛暟绛夊凡鍦╩ain()涓鍙栧苟缂撳瓨
	// 淇敼鍙傛暟鏃朵細鑷姩鏇存柊缂撳瓨锛屼笉闇€瑕佹瘡娆￠兘璇籈EPROM
	
	if(crc == rccrc) //CRC鏍￠獙鎴愬姛 寮€濮嬪垎鏋愬寘
	{	 
	   // 銆愯皟璇曘€戞墦鍗版帴鏀跺埌鐨勫抚锛堝彲閫夛紝璋冭瘯鏃跺惎鐢級
	   // printf("RX[%d]: %02X %02X %02X...\r\n", modbus.recount, modbus.rcbuf[0], modbus.rcbuf[1], modbus.rcbuf[2]);

	   if(modbus.rcbuf[0] == modbus.myadd)  // 妫€鏌ュ湴鍧€鏄惁鏄嚜宸辩殑鍦板潃
		 { 
		   // 銆愪慨澶嶃€戜弗鏍奸獙璇佸姛鑳界爜锛屽彧鎺ュ彈鏀寔鐨勫姛鑳界爜
		   u8 func_code = modbus.rcbuf[1];
		   if(func_code != 3 && func_code != 6 && func_code != 16)
		   {
			   // printf("涓嶆敮鎸佺殑鍔熻兘鐮? %d锛屼涪寮僜r\n", func_code);
			   modbus.reflag = 0;
			   modbus.recount = 0;
			   return;
		   }
		   
		   switch(func_code)   //鍒嗘瀽modbus鍔熻兘鐮?
			 {
				 case 3:      Modbus_Func3();      break;//杩欐槸璇诲彇瀵勫瓨鍣ㄧ殑鏁版嵁
				 case 6:      Modbus_Func6();      break;//杩欐槸鍐欏叆鍗曚釜瀵勫瓨鍣ㄦ暟鎹?
				 case 16:     Modbus_Func16(); 			break;//鍐欏叆澶氫釜瀵勫瓨鍣ㄦ暟鎹?
				 default:     break; // 涓嶅簲璇ュ埌杈捐繖閲?
			 }
		 }
		 else if(modbus.rcbuf[0] == 0) //骞挎挱鍦板潃锛氭墽琛屽懡浠や絾涓嶅洖搴?
		 {
		    // 銆愪慨澶嶃€戝箍鎾懡浠や篃瑕侀獙璇佸姛鑳界爜
		    u8 func_code = modbus.rcbuf[1];
		    if(func_code != 6 && func_code != 16)
		    {
			    // printf("骞挎挱鍦板潃涓嶆敮鎸佸姛鑳界爜%d锛屼涪寮僜r\n", func_code);
			    modbus.reflag = 0;
			    modbus.recount = 0;
			    return;
		    }
		    
		    printf("Modbus broadcast received, function=%d\r\n", func_code);
		    
		    // 骞挎挱鍦板潃鍙敮鎸佸啓鎿嶄綔锛堝姛鑳界爜06鍜?6锛?
		    switch(func_code)
		    {
		      case 6:  // 鍐欏崟涓瘎瀛樺櫒
		        //Modbus_Func6_Broadcast();
		        break;
		      case 16: // 鍐欏涓瘎瀛樺櫒
		        //Modbus_Func16_Broadcast();
		        break;
		      default:
		        printf("骞挎挱涓嶆敮鎸佸姛鑳界爜 %d\r\n", modbus.rcbuf[1]);
		        break;
		    }
		 }	 
	}	
	 modbus.recount = 0;//鎺ユ敹璁℃暟娓呴浂
	modbus.reflag = 0; //鎺ユ敹鏍囧織娓呴浂
}


//杩欐槸寰€澶氫釜瀵勫瓨鍣ㄥ櫒涓啓鍏ユ暟鎹?
//鍔熻兘鐮?x10鎸囦护鍗冲崄杩涘埗16
void Modbus_Func16()
{
		u16 Regadd;//鍦板潃16浣?
		u16 Reglen;
		u16 frame_len;
		u16 i,crc,j;
		
		Regadd=modbus.rcbuf[2]*256+modbus.rcbuf[3];  //瑕佷慨鏀瑰唴瀹圭殑璧峰鍦板潃
		Reglen = modbus.rcbuf[4]*256+modbus.rcbuf[5];//璇诲彇鐨勫瘎瀛樺櫒鏁伴噺

		Reglen = modbus.rcbuf[4]*256+modbus.rcbuf[5];
		if (Reglen == 0 || Regadd >= 128 || Reglen > (128 - Regadd))
		{
			Modbus_Send_Exception(0x10, 0x02);
			return;
		}

		if (modbus.rcbuf[6] != (u8)(Reglen * 2))
		{
			Modbus_Send_Exception(0x10, 0x03);
			return;
		}

		frame_len = 9 + Reglen * 2;
		if (frame_len > modbus.recount || frame_len > sizeof(modbus.rcbuf))
		{
			Modbus_Send_Exception(0x10, 0x03);
			return;
		}
		
		// 鏁扮粍瓒婄晫淇濇姢锛氶槻姝㈠啓鍏ヨ秴鍑鸿寖鍥?
		for(i=0;i<Reglen;i++)//寰€瀵勫瓨鍣ㄤ腑鍐欏叆鏁版嵁
		{
			if(Regadd+i < 128)  // 妫€鏌ユ瘡涓瘎瀛樺櫒鍦板潃
			{
				//鎺ユ敹鏁扮粍鐨勭涓冧綅寮€濮嬫槸鏁版嵁
				Reg[Regadd+i]=modbus.rcbuf[7+i*2]*256+modbus.rcbuf[8+i*2];//瀵瑰瘎瀛樺櫒涓€娆″啓鍏ユ暟鎹?
			}
		}
		//鍐欏叆鏁版嵁瀹屾瘯锛屾帴涓嬫潵闇€瑕佽繘琛屾墦鍖呭洖澶嶆暟鎹寘
		
		//鍐呭=鎺ユ敹鏁扮粍鐨勫墠6浣?涓や綅鐨勬牎楠屼綅
		modbus.sendbuf[0]=modbus.rcbuf[0];//鏈澶囧湴鍧€
		modbus.sendbuf[1]=modbus.rcbuf[1];  //鍔熻兘鐮?
		modbus.sendbuf[2]=modbus.rcbuf[2];//鍐欏叆鐨勫湴鍧€
		modbus.sendbuf[3]=modbus.rcbuf[3];
		modbus.sendbuf[4]=modbus.rcbuf[4];
		modbus.sendbuf[5]=modbus.rcbuf[5];
		crc=Modbus_CRC16(modbus.sendbuf,6);//鑾峰彇crc鏍￠獙浣?
		modbus.sendbuf[6]=crc/256;  //crc鏍￠獙浣嶅姞鍏ュ寘涓?
		modbus.sendbuf[7]=crc%256;
		//鏁版嵁鍙戦€佸寘鎵撳寘瀹屾瘯
		
		RS485_TX_ENABLE;;//浣胯兘485鎺у埗绔?寮€鍚彂閫?  
		for(j=0;j<8;j++)
		{
			Modbus_Send_Byte(modbus.sendbuf[j]);
		}
		RS485_RX_ENABLE;//澶辫兘485鎺у埗绔?鏀逛负鎺ユ敹)
}
/*
// 骞挎挱妯″紡涓嬬殑澶氬瘎瀛樺櫒鍐欏叆锛氭墽琛屽啓鍏ワ紝浣嗕笉鍥炲寘
void Modbus_Func16_Broadcast(void)
{
	u16 Regadd;
	u16 Reglen;
	u16 i;

	Regadd = modbus.rcbuf[2] * 256 + modbus.rcbuf[3];
	Reglen = modbus.rcbuf[4] * 256 + modbus.rcbuf[5];

	for (i = 0; i < Reglen; i++)
	{
		if (Regadd + i < 128)
		{
			Reg[Regadd + i] = modbus.rcbuf[7 + i * 2] * 256 + modbus.rcbuf[8 + i * 2];
		}
	}
}
*/
//浣滀负浠庢満閮ㄥ垎鍐呭缁撴瀯

void Modbus_Save_Config(uint8_t slave_addr, uint32_t baudrate,
                        int16_t temp_offset, int16_t humi_offset)
{
    if (Flash_Storage_Save_Config(slave_addr, baudrate, temp_offset,
                                  humi_offset) == 0) {

        modbus.myadd = slave_addr;
        modbus.myadd_cached = slave_addr;
        modbus.bound = baudrate;
        Reg[20] = (uint16_t)temp_offset;
        Reg[21] = (uint16_t)humi_offset;
        printf("Config saved: Slave=0x%02X, Baud=%d, Offset=%d/%d\r\n",
               slave_addr, baudrate, temp_offset, humi_offset);
    } else {
        printf("Failed to save config\r\n");
    }
}

/**
 * @brief 
 */
void Modbus_Load_Config(void)
{
    uint8_t slave_addr;
    uint32_t baudrate;
    int16_t temp_offset, humi_offset;

    if (Flash_Storage_Load_Config(&slave_addr, &baudrate, &temp_offset,
                                  &humi_offset) == 0) {
        modbus.myadd = slave_addr;
        modbus.myadd_cached = slave_addr;
        modbus.bound = baudrate;
        Reg[20] = (uint16_t)temp_offset;
        Reg[21] = (uint16_t)humi_offset;
        printf("Config reloaded: Slave=0x%02X, Baud=%d, Offset=%d/%d\r\n",
               slave_addr, baudrate, temp_offset, humi_offset);
    } else {
        printf("Failed to load config\r\n");
    }
}










