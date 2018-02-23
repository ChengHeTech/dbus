#ifndef __USART2_H
#define __USART2_H	 
#include "sys.h"  

#define USART2_MAX_RECV_LEN		400					//�����ջ����ֽ���
#define USART2_MAX_SEND_LEN		400					//����ͻ����ֽ���
#define USART2_RX_EN 			1					//0,������;1,����.
extern u8  comm_END;
extern u8  Reciver_bit;//����λ��һ
extern u8 TxCounter2;
extern u8 RxCounter2;

extern u8  USART2_RX_BUF[USART2_MAX_RECV_LEN]; 		//���ջ���,���USART2_MAX_RECV_LEN�ֽ�
extern u8  USART2_TX_BUF[USART2_MAX_SEND_LEN]; 		//���ͻ���,���USART2_MAX_SEND_LEN�ֽ�
extern u16 USART2_RX_STA;   						//��������״̬

void usart2_init(u32 bound);				//����2��ʼ�� 
void TIM3_Int_Init(u16 arr,u16 psc);
void u2_printf(char* fmt, ...);
void USART_OUT(USART_TypeDef* USARTx, char *Data,uint8_t len);
#endif












