#ifndef  __ESP8266_H__//如果没有定义了则参加以下编译
#define  __ESP8266_H__//一旦定义就有了定义 所以 其目的就是防止模块重复编译

#include "stm32f10x.h"
#include "delay.h"
#include "stdlib.h"
#include "string.h"

void ESP8266_Init(void);
void Usart2_SendByte(uint8_t Byte);
void Usart2_SendString(uint8_t *string);
void Esp8266_SendString(uint8_t *string,uint16_t len);//串口2 发送字符串
uint8_t EspSendCmdAndCheckRecvData(uint8_t *cmd,uint8_t *Rcmd,uint16_t outtime);
void WIFI_ConnectTaoBao(void);

typedef  struct{
    uint8_t rxbuff[1024];
    uint16_t rxcount;
    uint8_t rxover;
    
    uint8_t txbuff[1024];
    uint16_t txcount;
}WIFI;

extern WIFI wifi;

#endif  //结束编译
