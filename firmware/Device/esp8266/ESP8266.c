#include "ESP8266.h"
/**
  * @brief  ESP8266初始化 
            PA2--TX
            PA3--RX
            PA5--ESP8266使能引脚
  */
WIFI wifi = {0};
void ESP8266_Init(void)
{
  //使能时钟
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);//使能时钟A
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2,ENABLE);//使能时钟USART2
  //为初始化函数做准备
  GPIO_InitTypeDef GPIO_InitStructure;//定义结构体
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;//设置PA2引脚
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP ;//设置输出模式为复用推挽输出
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz ;//设置输出速度为50MHZ
  //初始化函数PIN2
  GPIO_Init(GPIOA,&GPIO_InitStructure);//初始化
  
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;//设置PA3引脚
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU ;//设置输出模式为上拉输入
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz ;//设置输出速度为50MHZ
  //初始化函数PIN3↓
  GPIO_Init(GPIOA,&GPIO_InitStructure);//初始化
  
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;//设置PA5引脚 -- //ESP8266使能引脚
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP ;//设置输出模式为推挽输出
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz ;//设置输出速度为50MHZ
  //初始化函数PIN5↓
  GPIO_Init(GPIOA,&GPIO_InitStructure);//初始化
  
  USART_InitTypeDef USART_InitStructure; //定义串口结构体
  USART_InitStructure.USART_BaudRate = 115200; //波特率
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;//数字帧长度
  USART_InitStructure.USART_StopBits = USART_StopBits_1; //停止位
  USART_InitStructure.USART_Parity = USART_Parity_No; //不使用校验位 
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//不使用硬件流控制
  USART_InitStructure.USART_Mode = USART_Mode_Tx|USART_Mode_Rx; //模式为发送+接收
  //初始化串口2
  USART_Init(USART2, &USART_InitStructure); 
  
  USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);//使能接收中断
  USART_ITConfig(USART2, USART_IT_IDLE, ENABLE);//使能空闲中断
   
  NVIC_InitTypeDef NVIC_InitStructure; //定义结构体
  NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn; //根据上面的我们所选取的USART2
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;//这里选择的是抢占 1
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1; //这里选择的是响应2
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; //使能指定的中断通道
  //初始化函数↓
  NVIC_Init(&NVIC_InitStructure);
  //使能串口2
  USART_Cmd(USART2, ENABLE);

  GPIO_SetBits(GPIOA,GPIO_Pin_5);//GPIOA 高电平有效 -- 使能ESP8266
  Delay_ms(2000);//延时  等待WiFi模块稳定
}

/**
  * @brief  串口2发送字节 -- 发送的最基本的函数 -->其它发送函数都是基于它
  * @param  
  * @retval 
  */
void Usart2_SendByte(uint8_t Byte)
{
  USART_SendData(USART2,Byte);
  while(USART_GetFlagStatus(USART2,USART_FLAG_TXE) == RESET );
}
/**
  * @brief  串口中断函数
            中断函数里面可以放你想要实现的功能函数
  * @param  
  * @retval 
  */
void USART2_IRQHandler(void)
{
  uint8_t data = 0;
  if (USART_GetITStatus(USART2, USART_IT_RXNE) == SET)
  {
    USART_ClearITPendingBit(USART2, USART_IT_RXNE);
    
    data = USART_ReceiveData(USART2);//ESP8266 发送给 STM32的数据
    
    wifi.rxbuff[wifi.rxcount++] = data;//将 ESP8266 发送给单片机的数据 转存到rxbuff里面

    /* ★★★ 已删除：USART_SendData(USART1,data);
     * 原因：这一行会抢占 LD6002 用 USART1 输出的调试文本，
     * 导致串口助手看到的内容是 ESP8266 的 AT 回显和 LD6002 文本
     * 混在一起的乱码。如果以后想重新打开 ESP8266 调试，
     * 推荐用专门的串口或者打印开关。
     */
  }
  if(USART_GetITStatus(USART2, USART_IT_IDLE) == 1) //串口空闲中断
  {
      data = USART2->SR;
    
      data = USART2->DR;
    
      wifi.rxover = 1;
  }
}
/**
  * @brief  发送字符串
  * @param  string 字符串
  * @retval 
  */
void Usart2_SendString(uint8_t *string)
{
  for(uint8_t i = 0;string[i] != '\0';i++)
  {
    Usart2_SendByte(string[i]);
  }
}
/**
  * @brief  发送字符串 + 带长度
  * @param  
  * @retval 
  */
void Esp8266_SendString(uint8_t *string,uint16_t len)//串口3 发送字符串
{
  for(uint16_t i=0;i<len;i++)
  Usart2_SendByte(string[i]);
}

/**
  * @brief  发送AT指令，检查AT指令的返回，判断AT指令是否发送成功
  * @param  cmd:发送的AT指令
  * @param  Rcmd:AT指令对应的返回值
  * @param  outtime:ESP8266超时回复判断
  * @retval 
  */
uint8_t EspSendCmdAndCheckRecvData(uint8_t *cmd,uint8_t *Rcmd,uint16_t outtime)
{
  uint8_t data = 0;
  
  //发送AT指令之前要清空
  
  memset(wifi.rxbuff,0,1024);
  
  wifi.rxover = 0;
  
  wifi.rxcount = 0;
  
  Usart2_SendString(cmd);//发送AT指令 -- > 才会进入串口2中断服务
  
  while(outtime) 
  {
    if(wifi.rxover == 1)
    {
      if(strstr((char *)wifi.rxbuff,(char *)Rcmd) != NULL)
      {
        data = 1;
        break;
      }
    }
    outtime--;
    Delay_ms(1);
  }
  return data;
}
