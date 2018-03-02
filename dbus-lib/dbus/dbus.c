#include "dbus.h"
#include "string.h"
#include "stdarg.h"
#include "stdio.h"
#include "stdlib.h"

//������ַ
u16 LocalAddress;
//�Ĵ���
u16 Register[DBUS_REGISTER_LENGTH];
//���ݽ��ջ����
char DBUS_RECIVE_BUF[DBUS_MAX_RECIVE_BUF];
//��Ӧ��Ϣ���л���س���
char DBUS_RESPONSE_BUF[DBUS_MAX_RESPONSE_BUF][DBUS_MAX_LENGTH];

//����˫����
char DBUS_RECIVE_DOUBLE_BUF[DBUS_MAX_RECIVE_BUF];
//����˫˫���峤��
u16 DBUS_RECIVE_DOUBLE_LEN;


//���ջ���������ݳ��ȣ���ǰ���ȣ�
u16 DBUS_RECIVE_LEN;

//����ص�����ָ��
typedef void (*callback_fun_type)(char*,u16);
callback_fun_type SEND_CALLBACK;
typedef void (*callback_void_type)(void);
callback_void_type DELAY_CALLBACK;

//֡ͷ
char DBUS_HEAD = '$'; 
//֡β
char DBUS_END  = '!';
//�������ʽ
char* DBUS_REGEX = "$%[^!]";
//֡ID
u16 FrameID=0;


//��֡��ʱ����
char OpenBox_temp[DBUS_MAX_LENGTH];
//��֡ת���������
char OpenBox_buf[DBUS_MAX_LENGTH];
//����ǰ����������
char TX_BUF[DBUS_MAX_LENGTH];
//��������������
char Send_TX_BUF[DBUS_MAX_LENGTH];

u16 dataBuf[DBUS_REGISTER_LENGTH];
struct _Dbus Dbus = 
{
    //����
    Register,   
    0,
    0,
    dataBuf,
    //����
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
    //ʵʱ֡
    Post_Register,
    Post_Multiple_Registers
};


/**********�����ṹ��***********************************************/

/*********�ڲ����ܺ���********************************************************************/
			
/*********CRC16У��*******************************************************************/
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

/*********���ͺ���******************************************************************/
void Send(char* buf,u16 len)
{
	u8 hight=0;
	u8 low=0;
    u16 i=0;
	//��շ��ͻ�������
    memset(Send_TX_BUF,0,DBUS_MAX_LENGTH);
	//��Ϣͷ 
	Send_TX_BUF[0] = DBUS_HEAD;

	//��Ч���ݲ���ת��16����ASCII��
	for(i=0;i<len;i++)
	{
		hight = buf[i]/0x10;//ȡ��λ
		low   = buf[i]%0x10;//ȡ��λ
		
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
	
	//��Ϣβ
	Send_TX_BUF[2*len+1]=DBUS_END;
	//���÷��ͻص�����
	SEND_CALLBACK(Send_TX_BUF,2*len+2);
}

/*********��16�����ַ���ת��ֵ******************************************************************/
void HexStrToDec(char* str,char* dec)
{
  u16 l;
  u16 hight=0;
  u16 low=0  ;
  u16 i=0;
  l=strlen(str);
    
	for(i=0;i<l ;i++)
	{   
        hight = str[i+0];//ȡ��λ
	    low   = str[i+1];//ȡ��λ
		
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


/**********��������*******************************************************************/
void Init(u16 Address) 
{
	//������ַ
	LocalAddress = Address; 

}


void InPut(char c) 
{
	//���յ�������׷�ӵ����ջ���
    if(DBUS_RECIVE_LEN<DBUS_MAX_RECIVE_BUF)
        DBUS_RECIVE_BUF[DBUS_RECIVE_LEN++] = c;
    else
    {
        //������󳤶�����,�ڽ�β���ӽ�����־���������
        DBUS_RECIVE_BUF[DBUS_RECIVE_LEN-1] = DBUS_END;
    }
}


/*********�������******************************************************************/
void OpenBox()
{
    //��ʼ��־
    u16 Start = 0; 
    //������־
    u16 Stop = 0;
    //�������
    u8 reault = 0;
    u16 i = 0;
    
    if (DBUS_RECIVE_BUF[DBUS_RECIVE_LEN - 1] == DBUS_END)
    {
        //���ƻ��������ݵ�˫����
        memcpy(DBUS_RECIVE_DOUBLE_BUF,DBUS_RECIVE_BUF,DBUS_RECIVE_LEN);
        //����˫���峤��
        DBUS_RECIVE_DOUBLE_LEN = DBUS_RECIVE_LEN;

        //��λ���ջ�����
        memset(DBUS_RECIVE_BUF,0,DBUS_MAX_RECIVE_BUF);
        //��ս��ջ��峤��
        DBUS_RECIVE_LEN = 0;
        
        //ѭ��������������Ч����
        while (DBUS_RECIVE_DOUBLE_LEN>(Stop+1))
        {
            reault = 0;
            for(i = Stop;i<DBUS_RECIVE_DOUBLE_LEN;i++)
            {
                //���ҿ�ʼ��
                if(DBUS_RECIVE_DOUBLE_BUF[i] == DBUS_HEAD)
                {
                    Start = i;
                }
                //���ҽ�����
                if((Start>=Stop)&&DBUS_RECIVE_DOUBLE_BUF[i] == DBUS_END)
                {
                    Stop = i;
                    reault = 1;
                    break;
                }
            }
            if(reault == 1)//�ҵ�����Ч������
            {
                //���ͷβ�м�û�����ݣ�������
                if((Stop-Start)==1)
                {
                    continue;
                }
                else
                {
                    //����ȡ�������ݿ�������ʱ����
                    memcpy(OpenBox_temp,&DBUS_RECIVE_DOUBLE_BUF[Start+1],Stop-Start+1-2);
                    //���ְ���ʮ�������ַ���tempת��Ϊ��ֵ����buf
                    HexStrToDec(OpenBox_temp,OpenBox_buf);
                    //ִ�н�������
                    Analyze(OpenBox_buf,(Stop-Start+1-2)/2);//ָ��֮�����ַ������ȣ�Ӧ�ó�2
                }
                
            }
            else//û���ҵ���Ч����
            {
                //�˳�ѭ��
                break;
            }
            //��ʱ10ms����ֹʵʱϵͳ����ʱ����
            DELAY_CALLBACK();
        }
        //��λ���ջ�����
       memset(DBUS_RECIVE_DOUBLE_BUF,0,DBUS_MAX_RECIVE_BUF);
    }

}




/*У�鲢��������֡
 *@Return          NONE 
*/
void Analyze(char *buf ,u16 len)
{
	u16 CRC;
	u8 C1,C2;
	u16 i=0,j=0,k=0;
	//�ж�Ŀ���ַ�Ƿ�Ϊ����
	if(((buf[5]<<8)|buf[6]) == LocalAddress)
	{
		//CRCУ��
		CRC=CRC_CALC(buf,(len-2));  
		C1=CRC>>8; //CRC���ֽ�
		C2=CRC;    //CRC���ֽ�
		if(C1==buf[len-2]&&C2==buf[len-1])//У����ȷ
		{		
			//����
			if(buf[4]==1)//����֡
			{
				switch(buf[7])//������
				{
					case 0x01:Response_Read_Register(buf);break; //�������Ĵ���
					case 0x02:Response_Write_Register(buf);break; //д�����Ĵ���				      
					case 0x03:Response_Read_Multiple_Registers(buf);break; //������Ĵ���
					case 0x04:Response_Write_Multiple_Registers(buf);break; //д����Ĵ���
					default:break;  	
				}	
			} 
            else
            if(buf[4]==0x10)//ʵʱ֡
            {
                switch(buf[7])//������
				{
					case 0x02:Response_Post_Register(buf);break; //д�����Ĵ���				      
					case 0x04:Response_Post_Multiple_Registers(buf);break; //д����Ĵ���
					default:break;  	
				}	
            }
			else 
			if(buf[4]==2)//��Ӧ֡
			{
				//���ӵ���Ӧ֡�����
				for(i=0;i<DBUS_MAX_RESPONSE_BUF;i++)
				{
					//����Ӧ֡������е���Ӧ֡�����
					if(DBUS_RESPONSE_BUF[i][0]==0)
					{
						//����ص�1�ֽ�Ϊ֡����
						DBUS_RESPONSE_BUF[i][0] = len;
						//������Ӧ֡���뻺���
						for(j=0;j<len;j++)
							DBUS_RESPONSE_BUF[i][j+1] = buf[j];
					}
                    //��Ӧ�ر�ռ��,��ջ���أ�����������Ӧ�����һ��������
                    else if(i==DBUS_MAX_RESPONSE_BUF-1)
                    {
                        //��ջ����
                        for(k=0;k<DBUS_MAX_RESPONSE_BUF;k++)
                            DBUS_RESPONSE_BUF[k][0] = 0;
                        
                        //����ص�1�ֽ�Ϊ֡����
						DBUS_RESPONSE_BUF[0][0] = len;
						//������Ӧ֡���뻺���
						for(j=0;j<len;j++)
							DBUS_RESPONSE_BUF[i][j+1] = buf[j];
                    }
				}
			}
		} 
		
	}
}



/*�����жϺ�������
 *@Function        ��ʼ�������жϺ���
 *@callback_fun    Ŀ�꺯��
 *@Return          NONE 
*/
void OutPut_interrupt(void (*callback_fun)(char*,u16))
{
	SEND_CALLBACK = callback_fun;
}

/*��ʱ�жϺ�������
 *@callback_fun    Ŀ�꺯��
 *@Return          NONE 
*/
void Delay_interrupt(void (*callback_delay)(void))
{
	DELAY_CALLBACK = callback_delay;
}
/*��������
 *@Function        ��ʱ�㱨�豸����
 *@TargetAdress    Ŀ���豸��     
 *@Return          1:�㱨�ɹ���0:�㱨ʧ��  
*/
void Heart(u16 TargetAddress)//��������
{
	//�洢CRC��������ʱ����
	u16 CRC;
	u16 frameid = FrameID;
	FrameID++;

	//��շ��ͻ�����
    memset(TX_BUF,0,DBUS_MAX_LENGTH);
    
	TX_BUF[0] = frameid>>8;//֡ID��
	TX_BUF[1] = frameid;//֡ID��		
	TX_BUF[2] = LocalAddress >>8;//������ַ��
	TX_BUF[3] = LocalAddress;//������ַ��
	TX_BUF[4] = 0;//֡����
	TX_BUF[5] = TargetAddress>>8;//Ŀ���ַ��
	TX_BUF[6] = TargetAddress;//Ŀ���ַ��
	
	CRC=CRC_CALC(TX_BUF,7);
	
	TX_BUF[7] = CRC>>8;//CRC��
	TX_BUF[8] = CRC;   //CRC��

	Send(TX_BUF,9);
}

//�������Ĵ���
struct _ReturnMsg Read_Register(u16 TargetAddress,u16 RegisterAddress)
{
	u16 CRC=0;	
	struct _ReturnMsg msg;
	u16 frameid = FrameID;
    u16 i=0,j=0,k=0;
	FrameID++;

	//��շ��ͻ�����
    memset(TX_BUF,0,DBUS_MAX_LENGTH);
    
	TX_BUF[0] = frameid>>8;//֡ID��
	TX_BUF[1] = frameid;//֡ID��	
	TX_BUF[2] = LocalAddress>>8;//������ַ��
	TX_BUF[3] = LocalAddress;//������ַ��
	TX_BUF[4] = 1;//֡����
	TX_BUF[5] = TargetAddress>>8;//Ŀ���ַ��
	TX_BUF[6] = TargetAddress;//Ŀ���ַ��
	TX_BUF[7] = 1;//������
	TX_BUF[8] = RegisterAddress>>8;//�Ĵ�����ַ��
	TX_BUF[9] = RegisterAddress;//�Ĵ�����ַ��

	CRC=CRC_CALC(TX_BUF,10);
	
	TX_BUF[10] = CRC>>8;//CRC��
	TX_BUF[11] = CRC;//CRC��	
		
	for(j=0;j<DBUS_MAX_REPEAT_NUM; j++)
	{
		//��������
		Send(TX_BUF,12);
		//�ȴ���Ӧ
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
			//��ʱ10ms����ֹʵʱϵͳ����ʱ����
			DELAY_CALLBACK();
		}
	}
	msg.resault = 0;
	return msg;
}
//������Ĵ���
struct _ReturnMsg Read_Multiple_Registers(u16 TargetAddress,u16 RegisterAddress,u16 Num)
{
	u16 CRC=0;	
	struct _ReturnMsg msg;
	u16 frameid = FrameID;
    u16 i=0,j=0,k=0,t;
	FrameID++;

	//��շ��ͻ�����
    memset(TX_BUF,0,DBUS_MAX_LENGTH);
    
	TX_BUF[0] = frameid>>8;//֡ID��
	TX_BUF[1] = frameid;//֡ID��	
	TX_BUF[2] = LocalAddress>>8;//������ַ��
	TX_BUF[3] = LocalAddress;//������ַ��
	TX_BUF[4] = 1;//֡����
	TX_BUF[5] = TargetAddress>>8;//Ŀ���ַ��
	TX_BUF[6] = TargetAddress;//Ŀ���ַ��
	TX_BUF[7] = 3;//������
	TX_BUF[8] = RegisterAddress>>8;//�Ĵ�����ַ��
	TX_BUF[9] = RegisterAddress;//�Ĵ�����ַ��
	TX_BUF[10] = Num;//����ȡ�Ĵ�������

	CRC=CRC_CALC(TX_BUF,11);
	
	TX_BUF[11] = CRC>>8;//CRC��
	TX_BUF[12] = CRC;//CRC��	
		
	for(j=0;j<DBUS_MAX_REPEAT_NUM; j++)
	{
		//��������
		Send(TX_BUF,13);
		//�ȴ���Ӧ
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
			//��ʱ1ms����ֹʵʱϵͳ����ʱ����
			DELAY_CALLBACK();
		}
	}
	msg.resault = 0;
	return msg;
}

/*д�����Ĵ���
 *@Function        �������Ĵ���дֵ
 *@TargetAdress    Ŀ���豸��ַ
 *@RegisterAddress Ŀ��Ĵ�����ַ
 *@Data            ��д����ֵ
 *@Return          1:д��ɹ���0:д��ʧ��  
*/
u8 Write_Register(u16 TargetAddress,u16 RegisterAddress,u16 Data)//д�����Ĵ���
{
	u16 CRC=0;		
	u16 frameid = FrameID;
    u16 i=0,j=0,k=0;
	FrameID++;

	//��շ��ͻ�����
    memset(TX_BUF,0,DBUS_MAX_LENGTH);
    
	TX_BUF[0] = frameid>>8;//֡ID��
	TX_BUF[1] = frameid;//֡ID��	
	TX_BUF[2] = LocalAddress>>8;//������ַ��
	TX_BUF[3] = LocalAddress;//������ַ��
	TX_BUF[4] = 1;//֡����
	TX_BUF[5] = TargetAddress>>8;//Ŀ���ַ��
	TX_BUF[6] = TargetAddress;//Ŀ���ַ��
	TX_BUF[7] = 2;//������
	TX_BUF[8] = RegisterAddress>>8;//�Ĵ�����ַ��
	TX_BUF[9] = RegisterAddress;//�Ĵ�����ַ��
	TX_BUF[10] = Data>>8;//���ݸ�
	TX_BUF[11] = Data;//���ݵ�
 
	CRC=CRC_CALC(TX_BUF,12);
	
	TX_BUF[12] = CRC>>8;//CRC��
	TX_BUF[13] = CRC;//CRC��	
		
	for(j=0;j<DBUS_MAX_REPEAT_NUM; j++)
	{
		//��������
		Send(TX_BUF,14);
		//�ȴ���Ӧ
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
			//��ʱ10ms����ֹʵʱϵͳ����ʱ����
			DELAY_CALLBACK();
		}
	}
	return 0;
}





/*д����Ĵ���
 *@TargetAdress    Ŀ���豸��ַ
 *@RegisterAddress Ŀ��Ĵ�����ַ
 *@Num             ��д��Ĵ�������
 *@Data            ��д����ֵָ��
 *@Return          1:д��ɹ���0:д��ʧ��  
*/
u8 Write_Multiple_Registers(u16 TargetAdress,u16 RegisterAddress,u16 Num,u16* Data)//д�����Ĵ���
{
	u16 CRC=0;
	u16 frameid = FrameID;
    u16 i=0,j=0,k=0;
	FrameID++;

	//��շ��ͻ�����
    memset(TX_BUF,0,DBUS_MAX_LENGTH);
    
	TX_BUF[0] = frameid>>8;//֡ID��
	TX_BUF[1] = frameid;//֡ID��		
	TX_BUF[2] = LocalAddress>>8;//������ַ��
	TX_BUF[3] = LocalAddress;//������ַ��
	TX_BUF[4] = 1;//֡����
	TX_BUF[5] = TargetAdress>>8;//Ŀ���ַ��
	TX_BUF[6] = TargetAdress;//Ŀ���ַ��
	TX_BUF[7] = 4;//������
	TX_BUF[8] = RegisterAddress>>8;//�Ĵ�����ַ��
	TX_BUF[9] = RegisterAddress;//�Ĵ�����ַ��
	TX_BUF[10] = Num;//�Ĵ�������

	//ѭ��д�����ݵ����ͻ�����
	for(i=0;i<Num;i++)
	{
		TX_BUF[11+2*i] = Data[i]>>8;//���ݸ�
		TX_BUF[12+2*i] = Data[i];//���ݵ�
	}

	CRC=CRC_CALC(TX_BUF,11+2*Num);

	TX_BUF[11+2*Num] = CRC>>8;//CRC��
	TX_BUF[11+2*Num+1] = CRC;//CRC��
	
	for(j=0;j<DBUS_MAX_REPEAT_NUM; j++)
	{
		//��������
		Send(TX_BUF,11+2*Num+2);
		//�ȴ���Ӧ
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
			//��ʱ1ms����ֹʵʱϵͳ����ʱ����
			DELAY_CALLBACK();
		}
	}
	return 0;
}




/*****************ʵʱ֡����������Ӧ��*****************************/

/*���͵����Ĵ���
 *@Function        �������Ĵ���дֵ
 *@TargetAdress    Ŀ���豸��ַ
 *@RegisterAddress Ŀ��Ĵ�����ַ
 *@Data            ��д����ֵ
 *@Return          1:д��ɹ���0:д��ʧ��  
*/
void Post_Register(u16 TargetAddress,u16 RegisterAddress,u16 Data)//д�����Ĵ���
{
	u16 CRC=0;		
	u16 frameid = FrameID;
	FrameID++;

	//��շ��ͻ�����
    memset(TX_BUF,0,DBUS_MAX_LENGTH);
    
	TX_BUF[0] = frameid>>8;//֡ID��
	TX_BUF[1] = frameid;//֡ID��	
	TX_BUF[2] = LocalAddress>>8;//������ַ��
	TX_BUF[3] = LocalAddress;//������ַ��
	TX_BUF[4] = 0x10;//֡����
	TX_BUF[5] = TargetAddress>>8;//Ŀ���ַ��
	TX_BUF[6] = TargetAddress;//Ŀ���ַ��
	TX_BUF[7] = 2;//������
	TX_BUF[8] = RegisterAddress>>8;//�Ĵ�����ַ��
	TX_BUF[9] = RegisterAddress;//�Ĵ�����ַ��
	TX_BUF[10] = Data>>8;//���ݸ�
	TX_BUF[11] = Data;//���ݵ�
 
	CRC=CRC_CALC(TX_BUF,12);
	
	TX_BUF[12] = CRC>>8;//CRC��
	TX_BUF[13] = CRC;//CRC��	
	
    //��������
	Send(TX_BUF,14);	
}

/*���͸��Ĵ���
 *@TargetAdress    Ŀ���豸��ַ
 *@RegisterAddress Ŀ��Ĵ�����ַ
 *@Num             ��д��Ĵ�������
 *@Data            ��д����ֵָ��
 *@Return          1:д��ɹ���0:д��ʧ��  
*/
void Post_Multiple_Registers(u16 TargetAdress,u16 RegisterAddress,u16 Num,u16* Data)//д�����Ĵ���
{
	u16 CRC=0;
	u16 frameid = FrameID;
    u16 i=0;
	FrameID++;

	//��շ��ͻ�����
    memset(TX_BUF,0,DBUS_MAX_LENGTH);
    
	TX_BUF[0] = frameid>>8;//֡ID��
	TX_BUF[1] = frameid;//֡ID��		
	TX_BUF[2] = LocalAddress>>8;//������ַ��
	TX_BUF[3] = LocalAddress;//������ַ��
	TX_BUF[4] = 0x10;//֡����
	TX_BUF[5] = TargetAdress>>8;//Ŀ���ַ��
	TX_BUF[6] = TargetAdress;//Ŀ���ַ��
	TX_BUF[7] = 4;//������
	TX_BUF[8] = RegisterAddress>>8;//�Ĵ�����ַ��
	TX_BUF[9] = RegisterAddress;//�Ĵ�����ַ��
	TX_BUF[10] = Num;//�Ĵ�������

	//ѭ��д�����ݵ����ͻ�����
	for(i=0;i<Num;i++)
	{
		TX_BUF[11+2*i] = Data[i]>>8;//���ݸ�
		TX_BUF[12+2*i] = Data[i];//���ݵ�
	}

	CRC=CRC_CALC(TX_BUF,11+2*Num);

	TX_BUF[11+2*Num] = CRC>>8;//CRC��
	TX_BUF[11+2*Num+1] = CRC;//CRC��
	
    //��������
    Send(TX_BUF,11+2*Num+2);
}



/*****************��Ӧ֡����*****************************/

/*��Ӧ�������Ĵ���*/ 
void Response_Read_Register(char *buf)
{
	u16 CRC=0;	
	//����ȡ�Ĵ�����ַ
	u16 regAdd = buf[8]<<8|buf[9];

	//��շ��ͻ�����
    memset(TX_BUF,0,DBUS_MAX_LENGTH);
    
	TX_BUF[0] = buf[0];//֡ID��
	TX_BUF[1] = buf[1];//֡ID��	
	TX_BUF[2] = LocalAddress>>8;//������ַ��
	TX_BUF[3] = LocalAddress;//������ַ��
	TX_BUF[4] = 2;//֡����
	TX_BUF[5] = buf[5];//Ŀ���ַ��
	TX_BUF[6] = buf[6];//Ŀ���ַ��
	TX_BUF[7] = 1;//������
	TX_BUF[8] = buf[8];//�Ĵ�����ַ��
	TX_BUF[9] = buf[9];//�Ĵ�����ַ��
	
	//��������ַ�������ƣ���������Ϊ0xFFFF
	if(regAdd>DBUS_REGISTER_LENGTH)
	{
		TX_BUF[10] = 0xFF;//���ݸ�
		TX_BUF[11] = 0xFF;//���ݵ�
	}
	else
	{
		TX_BUF[10] = Register[regAdd]>>8;//���ݸ�
		TX_BUF[11] = Register[regAdd];//���ݵ�
	}
	CRC=CRC_CALC(TX_BUF,12);
	TX_BUF[12] = CRC>>8;//CRC��
	TX_BUF[13] = CRC;//CRC��	
	
	//��������
	Send(TX_BUF,14);		
}   

/*��Ӧд�����Ĵ���*/ 
void Response_Write_Register(char *buf)
 {
	u16 CRC=0;	
	//��д��Ĵ�����ַ
	u16 regAdd = buf[8]<<8|buf[9];
	//��д������
	u16 data = (buf[10]<<8)|buf[11];

	//��շ��ͻ�����
    memset(TX_BUF,0,DBUS_MAX_LENGTH);
    
	//�ظ���Ӧ֡
	TX_BUF[0] = buf[0];//֡ID��
	TX_BUF[1] = buf[1];//֡ID��	
	TX_BUF[2] = LocalAddress>>8;//������ַ��
	TX_BUF[3] = LocalAddress;//������ַ��
	TX_BUF[4] = 2;//֡����
	TX_BUF[5] = buf[5];//Ŀ���ַ��
	TX_BUF[6] = buf[6];//Ŀ���ַ��
	TX_BUF[7] = 2;//������
	
	if(regAdd>DBUS_REGISTER_LENGTH)
	{
		TX_BUF[8] = 0;//���	
	}
	else
	{
		TX_BUF[8] = 1;//���	
		//��������
		Register[regAdd] = data;
	}
		
	CRC=CRC_CALC(TX_BUF,9);
	
	TX_BUF[9] = CRC>>8;//CRC��
	TX_BUF[10] = CRC;//CRC��	
	
	//��������
	Send(TX_BUF,11);	
} 
 
/*��Ӧ������Ĵ���*/ 
void Response_Read_Multiple_Registers(char *buf)
{
	u16 CRC=0;	
	u16 Num = buf[10];
	//����ȡ�Ĵ�����ʼ��ַ
	u16 regStartAdd = buf[8]<<8|buf[9];
    u16 i=0;
	//��շ��ͻ�����
    memset(TX_BUF,0,DBUS_MAX_LENGTH);
    
	TX_BUF[0] = buf[0];//֡ID��
	TX_BUF[1] = buf[1];//֡ID��	
	TX_BUF[2] = LocalAddress>>8;//������ַ��
	TX_BUF[3] = LocalAddress;//������ַ��
	TX_BUF[4] = 2;//֡����
	TX_BUF[5] = buf[5];//Ŀ���ַ��
	TX_BUF[6] = buf[6];//Ŀ���ַ��
	TX_BUF[7] = 3;//������
	TX_BUF[8] = buf[8];//�Ĵ�����ʼ��ַ��
	TX_BUF[9] = buf[9];//�Ĵ�����ʼ��ַ��
	TX_BUF[10] = buf[10];//����
	
	for(i=0;i<Num;i++)
	{
		//��������ַ�������ƣ���������Ϊ0xFFFF
		if((regStartAdd+i)>DBUS_REGISTER_LENGTH)
		{
			TX_BUF[11+i*2] = 0xFF;//���ݸ�
			TX_BUF[12+i*2] = 0xFF;//���ݵ�
		}
		else
		{
			TX_BUF[11+i*2] = Register[regStartAdd+i]>>8;//���ݸ�
			TX_BUF[12+i*2] = Register[regStartAdd+i];//���ݵ�
		}
	}

	CRC=CRC_CALC(TX_BUF,11+Num*2);
	
	TX_BUF[11+Num*2] = CRC>>8;//CRC��
	TX_BUF[11+Num*2+1] = CRC;//CRC��	
	
	//��������
	Send(TX_BUF,11+Num*2+2);
}
 
/*��Ӧд����Ĵ���*/ 
void Response_Write_Multiple_Registers(char *buf)
 {
	u16 CRC=0;	
	//��д��Ĵ�����ʼ��ַ
	u16 regStartAdd = buf[8]<<8|buf[9];
	u16 Num = buf[10];
    u16 i=0;
     
	//��շ��ͻ�����
    memset(TX_BUF,0,DBUS_MAX_LENGTH);
    
	//�ظ���Ӧ֡
	TX_BUF[0] = buf[0];//֡ID��
	TX_BUF[1] = buf[1];//֡ID��	
	TX_BUF[2] = LocalAddress>>8;//������ַ��
	TX_BUF[3] = LocalAddress;//������ַ��
	TX_BUF[4] = 2;//֡����
	TX_BUF[5] = buf[5];//Ŀ���ַ��
	TX_BUF[6] = buf[6];//Ŀ���ַ��
	TX_BUF[7] = 4;//������
	TX_BUF[8] = 1;//���
	for(i=0;i<Num;i++) 
	{
		if((regStartAdd+i)>DBUS_REGISTER_LENGTH)
		{
			TX_BUF[8] = 0;//���	
		}
		else
		{
			TX_BUF[8] = 1;//���	
			//��������
			Register[regStartAdd+i] = buf[11+i*2]<<8|buf[12+i*2];
		}
	}

	CRC=CRC_CALC(TX_BUF,9);
	
	TX_BUF[9] = CRC>>8;//CRC��
	TX_BUF[10] = CRC;//CRC��	
	
	//��������
	Send(TX_BUF,11);		 
 }  
 


/*****************ʵʱ֡��������*****************************/
 
/*��Ӧ���͵����Ĵ���*/ 
void Response_Post_Register(char *buf)
 {
	//��д��Ĵ�����ַ
	u16 regAdd = buf[8]<<8|buf[9];
	//��д������
	u16 data = (buf[10]<<8)|buf[11];
	
	if(regAdd<DBUS_REGISTER_LENGTH)
	{
        //��������
		Register[regAdd] = data;
	}
} 
 
/*��Ӧ���Ͷ���Ĵ���*/ 
void Response_Post_Multiple_Registers(char *buf)
 {
	//��д��Ĵ�����ʼ��ַ
	u16 regStartAdd = buf[8]<<8|buf[9];
	u16 Num = buf[10];
    u16 i=0;
     
	for(i=0;i<Num;i++) 
	{
		if((regStartAdd+i)<=DBUS_REGISTER_LENGTH)
		{
			//��������
			Register[regStartAdd+i] = buf[11+i*2]<<8|buf[12+i*2];	
		}
	}		 
 } 
 
