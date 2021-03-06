#include "dbus.h"
#include "string.h"
#include "stdarg.h"
#include "stdio.h"
#include "stdlib.h"

//本机地址
u16 LocalAddress;
//寄存器
u16 Register[DBUS_REGISTER_LENGTH];
//数据接收缓冲池
char DBUS_RECIVE_BUF[DBUS_MAX_RECIVE_BUF];
//响应消息队列缓冲池长度
char DBUS_RESPONSE_BUF[DBUS_MAX_RESPONSE_BUF][DBUS_MAX_LENGTH];

//接收双缓冲
char DBUS_RECIVE_DOUBLE_BUF[DBUS_MAX_RECIVE_BUF];
//接收双双缓冲长度
u16 DBUS_RECIVE_DOUBLE_LEN;


//接收缓冲池中数据长度（当前长度）
u16 DBUS_RECIVE_LEN;

//定义回调函数指针
typedef void (*callback_fun_type)(char*,u16);
callback_fun_type SEND_CALLBACK;
typedef void (*callback_void_type)(void);
callback_void_type DELAY_CALLBACK;

//帧头
char DBUS_HEAD = '$'; 
//帧尾
char DBUS_END  = '!';
//正则表达式
char* DBUS_REGEX = "$%[^!]";
//帧ID
u16 FrameID=0;


//单帧临时数组
char OpenBox_temp[DBUS_MAX_LENGTH];
//单帧转换后的数组
char OpenBox_buf[DBUS_MAX_LENGTH];
//编码前待发送数组
char TX_BUF[DBUS_MAX_LENGTH];
//编码后待发送数组
char Send_TX_BUF[DBUS_MAX_LENGTH];

u16 dataBuf[DBUS_REGISTER_LENGTH];
struct _Dbus Dbus = 
{
    //变量
    Register,   
    0,
    0,
    dataBuf,
    //函数
    Init,
    InPut,
    OpenBox,
    OutPut_interrupt,
    Delay_interrupt,
    Heart,
    Write_Register,
    Write_Multiple_Registers,
    Read_Register,
    Read_Multiple_Registers,
    //实时帧
    Post_Register,
    Post_Multiple_Registers
};


/**********函数结构体***********************************************/

/*********内部功能函数********************************************************************/
			
/*********CRC16校验*******************************************************************/
u16 CRC_CALC(char *chData,unsigned short uNo)
{
	u16 crc=0xffff;
	u16 i,j;
	for(i=0;i<uNo;i++)
	{
	  crc^=chData[i];
	  for(j=0;j<8;j++)
	  {
		if(crc&1)
		{
		 crc>>=1;
		 crc^=0xA001;
		}
		else
		 crc>>=1;
	  }
	}
	return (crc);
}

/*********发送函数******************************************************************/
void Send(char* buf,u16 len)
{
	u8 hight=0;
	u8 low=0;
    u16 i=0;
	//清空发送缓冲数组
    memset(Send_TX_BUF,0,DBUS_MAX_LENGTH);
	//消息头 
	Send_TX_BUF[0] = DBUS_HEAD;

	//有效数据部分转成16进制ASCII码
	for(i=0;i<len;i++)
	{
		hight = buf[i]/0x10;//取高位
		low   = buf[i]%0x10;//取低位
		
		if(hight<=9)  
		{  
			hight += 0x30;  
		}  
		else if((hight>=10)&&(hight<=15))//Capital  
		{  
			hight += 0x37;  
		}  
		
		if(low<=9)  
		{  
			low += 0x30;  
		}  
		else if((low>=10)&&(low<=15))//Capital  
		{  
			low += 0x37;  
		} 
		
		Send_TX_BUF[2*i+1] = hight;
		Send_TX_BUF[2*i+2] = low;
	}
	
	//消息尾
	Send_TX_BUF[2*len+1]=DBUS_END;
	//调用发送回调函数
	SEND_CALLBACK(Send_TX_BUF,2*len+2);
}

/*********将16进制字符串转数值******************************************************************/
void HexStrToDec(char* str,char* dec)
{
  u16 l;
  u16 hight=0;
  u16 low=0  ;
  u16 i=0;
  l=strlen(str);
    
	for(i=0;i<l ;i++)
	{   
        hight = str[i+0];//取高位
	    low   = str[i+1];//取低位
		
		if((hight>=0x30)&&(hight<=0x39))  
		{  
			hight -= 0x30;  
		} 
        else
        if((hight>=0x41)&&(hight<=0x46))//Capital  
		{  
			hight -= 0x37;  
		}  
        
        if((low>=0x30)&&(low<=0x39))  
		{  
			low -= 0x30;  
		} 
        else
        if((low>=0x41)&&(low<=0x46))//Capital  
		{  
			low -= 0x37;  
		}  
        
		dec[i/2] = hight*0x10+low;
		i++;
	}
}


/**********公共函数*******************************************************************/
void Init(u16 Address) 
{
	//本机地址
	LocalAddress = Address; 

}


void InPut(char c) 
{
	//将收到的数据追加到接收缓存
    if(DBUS_RECIVE_LEN<DBUS_MAX_RECIVE_BUF)
        DBUS_RECIVE_BUF[DBUS_RECIVE_LEN++] = c;
    else
    {
        //超过最大长度限制,在结尾增加结束标志，触发解包
        DBUS_RECIVE_BUF[DBUS_RECIVE_LEN-1] = DBUS_END;
    }
}


/*********解包函数******************************************************************/
void OpenBox()
{
    //开始标志
    u16 Start = 0; 
    //结束标志
    u16 Stop = 0;
    //搜索结果
    u8 reault = 0;
    u16 i = 0;
    
    if (DBUS_RECIVE_BUF[DBUS_RECIVE_LEN - 1] == DBUS_END)
    {
        //复制缓冲区数据到双缓冲
        memcpy(DBUS_RECIVE_DOUBLE_BUF,DBUS_RECIVE_BUF,DBUS_RECIVE_LEN);
        //更新双缓冲长度
        DBUS_RECIVE_DOUBLE_LEN = DBUS_RECIVE_LEN;

        //复位接收缓冲区
        memset(DBUS_RECIVE_BUF,0,DBUS_MAX_RECIVE_BUF);
        //清空接收缓冲长度
        DBUS_RECIVE_LEN = 0;
        
        //循环搜索并解析有效数据
        while (DBUS_RECIVE_DOUBLE_LEN>(Stop+1))
        {
            reault = 0;
            for(i = Stop;i<DBUS_RECIVE_DOUBLE_LEN;i++)
            {
                //查找开始符
                if(DBUS_RECIVE_DOUBLE_BUF[i] == DBUS_HEAD)
                {
                    Start = i;
                }
                //查找结束符
                if((Start>=Stop)&&DBUS_RECIVE_DOUBLE_BUF[i] == DBUS_END)
                {
                    Stop = i;
                    reault = 1;
                    break;
                }
            }
            if(reault == 1)//找到了有效的数据
            {
                //如果头尾中间没有数据，不处理
                if((Stop-Start)==1)
                {
                    continue;
                }
                else
                {
                    //将获取到的数据拷贝到临时数组
                    memcpy(OpenBox_temp,&DBUS_RECIVE_DOUBLE_BUF[Start+1],Stop-Start+1-2);
                    //将分包的十六进制字符串temp转换为数值数组buf
                    HexStrToDec(OpenBox_temp,OpenBox_buf);
                    //执行解析函数
                    Analyze(OpenBox_buf,(Stop-Start+1-2)/2);//指针之差是字符串长度，应该除2
                }
                
            }
            else//没有找到有效数据
            {
                //退出循环
                break;
            }
            //延时10ms，防止实时系统调用时卡死
            DELAY_CALLBACK();
        }
        //复位接收缓冲区
       memset(DBUS_RECIVE_DOUBLE_BUF,0,DBUS_MAX_RECIVE_BUF);
    }

}




/*校验并解析数据帧
 *@Return          NONE 
*/
void Analyze(char *buf ,u16 len)
{
	u16 CRC;
	u8 C1,C2;
	u16 i=0,j=0,k=0;
	//判断目标地址是否为本机
	if(((buf[5]<<8)|buf[6]) == LocalAddress)
	{
		//CRC校验
		CRC=CRC_CALC(buf,(len-2));  
		C1=CRC>>8; //CRC高字节
		C2=CRC;    //CRC低字节
		if(C1==buf[len-2]&&C2==buf[len-1])//校验正确
		{		
			//解析
			if(buf[4]==1)//操作帧
			{
				switch(buf[7])//功能码
				{
					case 0x01:Response_Read_Register(buf);break; //读单个寄存器
					case 0x02:Response_Write_Register(buf);break; //写单个寄存器				      
					case 0x03:Response_Read_Multiple_Registers(buf);break; //读多个寄存器
					case 0x04:Response_Write_Multiple_Registers(buf);break; //写多个寄存器
					default:break;  	
				}	
			} 
            else
            if(buf[4]==0x10)//实时帧
            {
                switch(buf[7])//功能码
				{
					case 0x02:Response_Post_Register(buf);break; //写单个寄存器				      
					case 0x04:Response_Post_Multiple_Registers(buf);break; //写多个寄存器
					default:break;  	
				}	
            }
			else 
			if(buf[4]==2)//响应帧
			{
				//添加到响应帧缓冲池
				for(i=0;i<DBUS_MAX_RESPONSE_BUF;i++)
				{
					//将响应帧编入空闲的响应帧缓冲池
					if(DBUS_RESPONSE_BUF[i][0]==0)
					{
						//缓冲池第1字节为帧长度
						DBUS_RESPONSE_BUF[i][0] = len;
						//将该响应帧加入缓冲池
						for(j=0;j<len;j++)
							DBUS_RESPONSE_BUF[i][j+1] = buf[j];
					}
                    //响应池被占满,清空缓冲池，并将本次响应加入第一个缓冲区
                    else if(i==DBUS_MAX_RESPONSE_BUF-1)
                    {
                        //清空缓冲池
                        for(k=0;k<DBUS_MAX_RESPONSE_BUF;k++)
                            DBUS_RESPONSE_BUF[k][0] = 0;
                        
                        //缓冲池第1字节为帧长度
						DBUS_RESPONSE_BUF[0][0] = len;
						//将该响应帧加入缓冲池
						for(j=0;j<len;j++)
							DBUS_RESPONSE_BUF[i][j+1] = buf[j];
                    }
				}
			}
		} 
		
	}
}



/*发送中断函数定义
 *@Function        初始化发送中断函数
 *@callback_fun    目标函数
 *@Return          NONE 
*/
void OutPut_interrupt(void (*callback_fun)(char*,u16))
{
	SEND_CALLBACK = callback_fun;
}

/*延时中断函数定义
 *@callback_fun    目标函数
 *@Return          NONE 
*/
void Delay_interrupt(void (*callback_delay)(void))
{
	DELAY_CALLBACK = callback_delay;
}
/*心跳函数
 *@Function        定时汇报设备在线
 *@TargetAdress    目标设备号     
 *@Return          1:汇报成功，0:汇报失败  
*/
void Heart(u16 TargetAddress)//心跳函数
{
	//存储CRC计算结果临时变量
	u16 CRC;
	u16 frameid = FrameID;
	FrameID++;

	//清空发送缓冲区
    memset(TX_BUF,0,DBUS_MAX_LENGTH);
    
	TX_BUF[0] = frameid>>8;//帧ID高
	TX_BUF[1] = frameid;//帧ID低		
	TX_BUF[2] = LocalAddress >>8;//本机地址高
	TX_BUF[3] = LocalAddress;//本机地址低
	TX_BUF[4] = 0;//帧类型
	TX_BUF[5] = TargetAddress>>8;//目标地址高
	TX_BUF[6] = TargetAddress;//目标地址低
	
	CRC=CRC_CALC(TX_BUF,7);
	
	TX_BUF[7] = CRC>>8;//CRC高
	TX_BUF[8] = CRC;   //CRC低

	Send(TX_BUF,9);
}

//读单个寄存器
struct _ReturnMsg Read_Register(u16 TargetAddress,u16 RegisterAddress)
{
	u16 CRC=0;	
	struct _ReturnMsg msg;
	u16 frameid = FrameID;
    u16 i=0,j=0,k=0;
	FrameID++;

	//清空发送缓冲区
    memset(TX_BUF,0,DBUS_MAX_LENGTH);
    
	TX_BUF[0] = frameid>>8;//帧ID高
	TX_BUF[1] = frameid;//帧ID低	
	TX_BUF[2] = LocalAddress>>8;//本机地址高
	TX_BUF[3] = LocalAddress;//本机地址低
	TX_BUF[4] = 1;//帧类型
	TX_BUF[5] = TargetAddress>>8;//目标地址高
	TX_BUF[6] = TargetAddress;//目标地址低
	TX_BUF[7] = 1;//功能码
	TX_BUF[8] = RegisterAddress>>8;//寄存器地址高
	TX_BUF[9] = RegisterAddress;//寄存器地址低

	CRC=CRC_CALC(TX_BUF,10);
	
	TX_BUF[10] = CRC>>8;//CRC高
	TX_BUF[11] = CRC;//CRC低	
		
	for(j=0;j<DBUS_MAX_REPEAT_NUM; j++)
	{
		//发送数据
		Send(TX_BUF,12);
		//等待响应
		for(k=0;k<DBUS_TIMEOUT;k++)
		{
			for(i=0;i<DBUS_MAX_RESPONSE_BUF ;i++)
			{
				if(DBUS_RESPONSE_BUF[i][0]!=0)
				{
					if(((DBUS_RESPONSE_BUF[i][1]<<8)|DBUS_RESPONSE_BUF[i][2]) == frameid)
					{
						msg.resault = 1;
						msg.Data = DBUS_RESPONSE_BUF[i][11]<<8|DBUS_RESPONSE_BUF[i][12];
						DBUS_RESPONSE_BUF[i][0]=0;
						return msg;
					}
				}
			}
			//延时10ms，防止实时系统调用时卡死
			DELAY_CALLBACK();
		}
	}
	msg.resault = 0;
	return msg;
}
//读多个寄存器
struct _ReturnMsg Read_Multiple_Registers(u16 TargetAddress,u16 RegisterAddress,u16 Num)
{
	u16 CRC=0;	
	struct _ReturnMsg msg;
	u16 frameid = FrameID;
    u16 i=0,j=0,k=0,t;
	FrameID++;

	//清空发送缓冲区
    memset(TX_BUF,0,DBUS_MAX_LENGTH);
    
	TX_BUF[0] = frameid>>8;//帧ID高
	TX_BUF[1] = frameid;//帧ID低	
	TX_BUF[2] = LocalAddress>>8;//本机地址高
	TX_BUF[3] = LocalAddress;//本机地址低
	TX_BUF[4] = 1;//帧类型
	TX_BUF[5] = TargetAddress>>8;//目标地址高
	TX_BUF[6] = TargetAddress;//目标地址低
	TX_BUF[7] = 3;//功能码
	TX_BUF[8] = RegisterAddress>>8;//寄存器地址高
	TX_BUF[9] = RegisterAddress;//寄存器地址低
	TX_BUF[10] = Num;//待读取寄存器数量

	CRC=CRC_CALC(TX_BUF,11);
	
	TX_BUF[11] = CRC>>8;//CRC高
	TX_BUF[12] = CRC;//CRC低	
		
	for(j=0;j<DBUS_MAX_REPEAT_NUM; j++)
	{
		//发送数据
		Send(TX_BUF,13);
		//等待响应
		for(k=0;k<DBUS_TIMEOUT;k++)
		{
			for(i=0;i<DBUS_MAX_RESPONSE_BUF ;i++)
			{
				if(DBUS_RESPONSE_BUF[i][0]!=0)
				{
					if(((DBUS_RESPONSE_BUF[i][1]<<8)|DBUS_RESPONSE_BUF[i][2]) == frameid)
					{
						msg.resault = 1;
						for(t=0;t<Num;t++)
						{
							msg.DataBuf[t] = DBUS_RESPONSE_BUF[i][12+2*t]<<8|DBUS_RESPONSE_BUF[i][13+2*t];
						}
						DBUS_RESPONSE_BUF[i][0]=0;
						return msg;
					}
				}
			}
			//延时1ms，防止实时系统调用时卡死
			DELAY_CALLBACK();
		}
	}
	msg.resault = 0;
	return msg;
}

/*写单个寄存器
 *@Function        给单个寄存器写值
 *@TargetAdress    目标设备地址
 *@RegisterAddress 目标寄存器地址
 *@Data            待写入数值
 *@Return          1:写入成功，0:写入失败  
*/
u8 Write_Register(u16 TargetAddress,u16 RegisterAddress,u16 Data)//写单个寄存器
{
	u16 CRC=0;		
	u16 frameid = FrameID;
    u16 i=0,j=0,k=0;
	FrameID++;

	//清空发送缓冲区
    memset(TX_BUF,0,DBUS_MAX_LENGTH);
    
	TX_BUF[0] = frameid>>8;//帧ID高
	TX_BUF[1] = frameid;//帧ID低	
	TX_BUF[2] = LocalAddress>>8;//本机地址高
	TX_BUF[3] = LocalAddress;//本机地址低
	TX_BUF[4] = 1;//帧类型
	TX_BUF[5] = TargetAddress>>8;//目标地址高
	TX_BUF[6] = TargetAddress;//目标地址低
	TX_BUF[7] = 2;//功能码
	TX_BUF[8] = RegisterAddress>>8;//寄存器地址高
	TX_BUF[9] = RegisterAddress;//寄存器地址低
	TX_BUF[10] = Data>>8;//数据高
	TX_BUF[11] = Data;//数据低
 
	CRC=CRC_CALC(TX_BUF,12);
	
	TX_BUF[12] = CRC>>8;//CRC高
	TX_BUF[13] = CRC;//CRC低	
		
	for(j=0;j<DBUS_MAX_REPEAT_NUM; j++)
	{
		//发送数据
		Send(TX_BUF,14);
		//等待响应
		for(k=0;k<DBUS_TIMEOUT;k++)
		{
			for(i=0;i<DBUS_MAX_RESPONSE_BUF ;i++)
			{
				if(DBUS_RESPONSE_BUF[i][0]!=0)
				{
					if(((DBUS_RESPONSE_BUF[i][1]<<8)|DBUS_RESPONSE_BUF[i][2]) == frameid)
					{
						DBUS_RESPONSE_BUF[i][0]=0;
						return 1;
					}
				}
			}
			//延时10ms，防止实时系统调用时卡死
			DELAY_CALLBACK();
		}
	}
	return 0;
}





/*写多个寄存器
 *@TargetAdress    目标设备地址
 *@RegisterAddress 目标寄存器地址
 *@Num             待写入寄存器个数
 *@Data            待写入数值指针
 *@Return          1:写入成功，0:写入失败  
*/
u8 Write_Multiple_Registers(u16 TargetAdress,u16 RegisterAddress,u16 Num,u16* Data)//写单个寄存器
{
	u16 CRC=0;
	u16 frameid = FrameID;
    u16 i=0,j=0,k=0;
	FrameID++;

	//清空发送缓冲区
    memset(TX_BUF,0,DBUS_MAX_LENGTH);
    
	TX_BUF[0] = frameid>>8;//帧ID高
	TX_BUF[1] = frameid;//帧ID低		
	TX_BUF[2] = LocalAddress>>8;//本机地址高
	TX_BUF[3] = LocalAddress;//本机地址低
	TX_BUF[4] = 1;//帧类型
	TX_BUF[5] = TargetAdress>>8;//目标地址高
	TX_BUF[6] = TargetAdress;//目标地址低
	TX_BUF[7] = 4;//功能码
	TX_BUF[8] = RegisterAddress>>8;//寄存器地址高
	TX_BUF[9] = RegisterAddress;//寄存器地址低
	TX_BUF[10] = Num;//寄存器数量

	//循环写入数据到发送缓冲区
	for(i=0;i<Num;i++)
	{
		TX_BUF[11+2*i] = Data[i]>>8;//数据高
		TX_BUF[12+2*i] = Data[i];//数据低
	}

	CRC=CRC_CALC(TX_BUF,11+2*Num);

	TX_BUF[11+2*Num] = CRC>>8;//CRC高
	TX_BUF[11+2*Num+1] = CRC;//CRC低
	
	for(j=0;j<DBUS_MAX_REPEAT_NUM; j++)
	{
		//发送数据
		Send(TX_BUF,11+2*Num+2);
		//等待响应
		for(k=0;k<DBUS_TIMEOUT;k++)
		{
			for(i=0;i<DBUS_MAX_RESPONSE_BUF ;i++)
			{
				if(DBUS_RESPONSE_BUF[i][0]!=0)
				{
					if(((DBUS_RESPONSE_BUF[i][1]<<8)|DBUS_RESPONSE_BUF[i][2]) == frameid)
					{
						DBUS_RESPONSE_BUF[i][0]=0;
						return 1;
					}
				}
			}
			//延时1ms，防止实时系统调用时卡死
			DELAY_CALLBACK();
		}
	}
	return 0;
}




/*****************实时帧函数（无响应）*****************************/

/*发送单个寄存器
 *@Function        给单个寄存器写值
 *@TargetAdress    目标设备地址
 *@RegisterAddress 目标寄存器地址
 *@Data            待写入数值
 *@Return          1:写入成功，0:写入失败  
*/
void Post_Register(u16 TargetAddress,u16 RegisterAddress,u16 Data)//写单个寄存器
{
	u16 CRC=0;		
	u16 frameid = FrameID;
	FrameID++;

	//清空发送缓冲区
    memset(TX_BUF,0,DBUS_MAX_LENGTH);
    
	TX_BUF[0] = frameid>>8;//帧ID高
	TX_BUF[1] = frameid;//帧ID低	
	TX_BUF[2] = LocalAddress>>8;//本机地址高
	TX_BUF[3] = LocalAddress;//本机地址低
	TX_BUF[4] = 0x10;//帧类型
	TX_BUF[5] = TargetAddress>>8;//目标地址高
	TX_BUF[6] = TargetAddress;//目标地址低
	TX_BUF[7] = 2;//功能码
	TX_BUF[8] = RegisterAddress>>8;//寄存器地址高
	TX_BUF[9] = RegisterAddress;//寄存器地址低
	TX_BUF[10] = Data>>8;//数据高
	TX_BUF[11] = Data;//数据低
 
	CRC=CRC_CALC(TX_BUF,12);
	
	TX_BUF[12] = CRC>>8;//CRC高
	TX_BUF[13] = CRC;//CRC低	
	
    //发送数据
	Send(TX_BUF,14);	
}

/*发送个寄存器
 *@TargetAdress    目标设备地址
 *@RegisterAddress 目标寄存器地址
 *@Num             待写入寄存器个数
 *@Data            待写入数值指针
 *@Return          1:写入成功，0:写入失败  
*/
void Post_Multiple_Registers(u16 TargetAdress,u16 RegisterAddress,u16 Num,u16* Data)//写单个寄存器
{
	u16 CRC=0;
	u16 frameid = FrameID;
    u16 i=0;
	FrameID++;

	//清空发送缓冲区
    memset(TX_BUF,0,DBUS_MAX_LENGTH);
    
	TX_BUF[0] = frameid>>8;//帧ID高
	TX_BUF[1] = frameid;//帧ID低		
	TX_BUF[2] = LocalAddress>>8;//本机地址高
	TX_BUF[3] = LocalAddress;//本机地址低
	TX_BUF[4] = 0x10;//帧类型
	TX_BUF[5] = TargetAdress>>8;//目标地址高
	TX_BUF[6] = TargetAdress;//目标地址低
	TX_BUF[7] = 4;//功能码
	TX_BUF[8] = RegisterAddress>>8;//寄存器地址高
	TX_BUF[9] = RegisterAddress;//寄存器地址低
	TX_BUF[10] = Num;//寄存器数量

	//循环写入数据到发送缓冲区
	for(i=0;i<Num;i++)
	{
		TX_BUF[11+2*i] = Data[i]>>8;//数据高
		TX_BUF[12+2*i] = Data[i];//数据低
	}

	CRC=CRC_CALC(TX_BUF,11+2*Num);

	TX_BUF[11+2*Num] = CRC>>8;//CRC高
	TX_BUF[11+2*Num+1] = CRC;//CRC低
	
    //发送数据
    Send(TX_BUF,11+2*Num+2);
}



/*****************响应帧函数*****************************/

/*响应读单个寄存器*/ 
void Response_Read_Register(char *buf)
{
	u16 CRC=0;	
	//待读取寄存器地址
	u16 regAdd = buf[8]<<8|buf[9];

	//清空发送缓冲区
    memset(TX_BUF,0,DBUS_MAX_LENGTH);
    
	TX_BUF[0] = buf[0];//帧ID高
	TX_BUF[1] = buf[1];//帧ID低	
	TX_BUF[2] = LocalAddress>>8;//本机地址高
	TX_BUF[3] = LocalAddress;//本机地址低
	TX_BUF[4] = 2;//帧类型
	TX_BUF[5] = buf[2];//目标地址高
	TX_BUF[6] = buf[3];//目标地址低
	TX_BUF[7] = 1;//功能码
	TX_BUF[8] = buf[8];//寄存器地址高
	TX_BUF[9] = buf[9];//寄存器地址低
	
	//如果请求地址超出限制，返回数据为0xFFFF
	if(regAdd>DBUS_REGISTER_LENGTH)
	{
		TX_BUF[10] = 0xFF;//数据高
		TX_BUF[11] = 0xFF;//数据低
	}
	else
	{
		TX_BUF[10] = Register[regAdd]>>8;//数据高
		TX_BUF[11] = Register[regAdd];//数据低
	}
	CRC=CRC_CALC(TX_BUF,12);
	TX_BUF[12] = CRC>>8;//CRC高
	TX_BUF[13] = CRC;//CRC低	
	
	//发送数据
	Send(TX_BUF,14);		
}   

/*响应写单个寄存器*/ 
void Response_Write_Register(char *buf)
 {
	u16 CRC=0;	
	//待写入寄存器地址
	u16 regAdd = buf[8]<<8|buf[9];
	//待写入数据
	u16 data = (buf[10]<<8)|buf[11];

	//清空发送缓冲区
    memset(TX_BUF,0,DBUS_MAX_LENGTH);
    
	//回复响应帧
	TX_BUF[0] = buf[0];//帧ID高
	TX_BUF[1] = buf[1];//帧ID低	
	TX_BUF[2] = LocalAddress>>8;//本机地址高
	TX_BUF[3] = LocalAddress;//本机地址低
	TX_BUF[4] = 2;//帧类型
	TX_BUF[5] = buf[2];//目标地址高
	TX_BUF[6] = buf[3];//目标地址低
	TX_BUF[7] = 2;//功能码
	
	if(regAdd>DBUS_REGISTER_LENGTH)
	{
		TX_BUF[8] = 0;//结果	
	}
	else
	{
		TX_BUF[8] = 1;//结果	
		//更新数据
		Register[regAdd] = data;
	}
		
	CRC=CRC_CALC(TX_BUF,9);
	
	TX_BUF[9] = CRC>>8;//CRC高
	TX_BUF[10] = CRC;//CRC低	
	
	//发送数据
	Send(TX_BUF,11);	
} 
 
/*响应读多个寄存器*/ 
void Response_Read_Multiple_Registers(char *buf)
{
	u16 CRC=0;	
	u16 Num = buf[10];
	//待读取寄存器起始地址
	u16 regStartAdd = buf[8]<<8|buf[9];
    u16 i=0;
	//清空发送缓冲区
    memset(TX_BUF,0,DBUS_MAX_LENGTH);
    
	TX_BUF[0] = buf[0];//帧ID高
	TX_BUF[1] = buf[1];//帧ID低	
	TX_BUF[2] = LocalAddress>>8;//本机地址高
	TX_BUF[3] = LocalAddress;//本机地址低
	TX_BUF[4] = 2;//帧类型
	TX_BUF[5] = buf[2];//目标地址高
	TX_BUF[6] = buf[3];//目标地址低
	TX_BUF[7] = 3;//功能码
	TX_BUF[8] = buf[8];//寄存器起始地址高
	TX_BUF[9] = buf[9];//寄存器起始地址低
	TX_BUF[10] = buf[10];//数量
	
	for(i=0;i<Num;i++)
	{
		//如果请求地址超出限制，返回数据为0xFFFF
		if((regStartAdd+i)>DBUS_REGISTER_LENGTH)
		{
			TX_BUF[11+i*2] = 0xFF;//数据高
			TX_BUF[12+i*2] = 0xFF;//数据低
		}
		else
		{
			TX_BUF[11+i*2] = Register[regStartAdd+i]>>8;//数据高
			TX_BUF[12+i*2] = Register[regStartAdd+i];//数据低
		}
	}

	CRC=CRC_CALC(TX_BUF,11+Num*2);
	
	TX_BUF[11+Num*2] = CRC>>8;//CRC高
	TX_BUF[11+Num*2+1] = CRC;//CRC低	
	
	//发送数据
	Send(TX_BUF,11+Num*2+2);
}
 
/*响应写多个寄存器*/ 
void Response_Write_Multiple_Registers(char *buf)
 {
	u16 CRC=0;	
	//待写入寄存器起始地址
	u16 regStartAdd = buf[8]<<8|buf[9];
	u16 Num = buf[10];
    u16 i=0;
     
	//清空发送缓冲区
    memset(TX_BUF,0,DBUS_MAX_LENGTH);
    
	//回复响应帧
	TX_BUF[0] = buf[0];//帧ID高
	TX_BUF[1] = buf[1];//帧ID低	
	TX_BUF[2] = LocalAddress>>8;//本机地址高
	TX_BUF[3] = LocalAddress;//本机地址低
	TX_BUF[4] = 2;//帧类型
	TX_BUF[5] = buf[2];//目标地址高
	TX_BUF[6] = buf[3];//目标地址低
	TX_BUF[7] = 4;//功能码
	TX_BUF[8] = 1;//结果
	for(i=0;i<Num;i++) 
	{
		if((regStartAdd+i)>DBUS_REGISTER_LENGTH)
		{
			TX_BUF[8] = 0;//结果	
		}
		else
		{
			TX_BUF[8] = 1;//结果	
			//更新数据
			Register[regStartAdd+i] = buf[11+i*2]<<8|buf[12+i*2];
		}
	}

	CRC=CRC_CALC(TX_BUF,9);
	
	TX_BUF[9] = CRC>>8;//CRC高
	TX_BUF[10] = CRC;//CRC低	
	
	//发送数据
	Send(TX_BUF,11);		 
 }  
 


/*****************实时帧函数解析*****************************/
 
/*响应发送单个寄存器*/ 
void Response_Post_Register(char *buf)
 {
	//待写入寄存器地址
	u16 regAdd = buf[8]<<8|buf[9];
	//待写入数据
	u16 data = (buf[10]<<8)|buf[11];
	
	if(regAdd<DBUS_REGISTER_LENGTH)
	{
        //更新数据
		Register[regAdd] = data;
	}
} 
 
/*响应发送多个寄存器*/ 
void Response_Post_Multiple_Registers(char *buf)
 {
	//待写入寄存器起始地址
	u16 regStartAdd = buf[8]<<8|buf[9];
	u16 Num = buf[10];
    u16 i=0;
     
	for(i=0;i<Num;i++) 
	{
		if((regStartAdd+i)<=DBUS_REGISTER_LENGTH)
		{
			//更新数据
			Register[regStartAdd+i] = buf[11+i*2]<<8|buf[12+i*2];	
		}
	}		 
 } 
 

