#include "stm32f10x.h"
#include "OLED.h"
#include "LED.h"
#include "Key.h"
#include "LD2410B.h"
#include "Serial.h"
#include "string.h"
#include "Device.h"
#include "DHT11.h"
//#include "USART1.h"
//#include "ESP8266.h"

// 页面定义
typedef enum {
    PAGE_MAIN = 0,     // 主页
    PAGE_SETTINGS = 1 , // 设置页
		PAGE_PARA =2      //温湿度页
} Page_t;

// 设置项定义 
typedef enum {
    ITEM_1 = 0,
    ITEM_2 = 1,
		ITEM_3 = 1
} SettingItem_t;

// 设备控制开关
uint8_t ledControlEnabled = 1;    // 照明控制默认开启
uint8_t fanControlEnabled = 1;    // 风扇控制默认开启

// 保存实际设备状态
uint8_t ledStatus = 0;
uint8_t fanStatus = 0;

// 全局变量
Page_t currentPage = PAGE_MAIN;      // 当前页面
SettingItem_t cursorPos = ITEM_1;    // 设置页光标位置
DHT11_Data_TypeDef DHT11_Data;

// 函数声明
void ShowMainPage(void);
void ShowSettingsPage(void);
void ShowSensorPage(void);
void ProcessKey(uint8_t keyNum);

int main(void)
{
    OLED_Init();
    Key_Init();
    LD2410B_Init();
    LED_Init();
    Serial_Init();
    Device_Init();
		DHT11_GPIO_Config();
//		ESP8266_Init();
//		WIFI_ConnectTaoBao();
//		NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	
    while (1)
    {
        uint8_t keyNum = Key_GetNum();
        ProcessKey(keyNum);
        
        // 显示当前页面
        if (currentPage == PAGE_MAIN) {
            ShowMainPage();
        } else if(currentPage == PAGE_SETTINGS) {
            ShowSettingsPage();
        }else{
							ShowSensorPage();
				}
        
        OLED_Update();
				
				
    }
		
		
}

// 显示主页面
void ShowMainPage(void)
{
    OLED_Clear();
    OLED_ShowChinese(15, 0, "人体存在");
    OLED_ShowChinese(15, 22, "感应：");
    OLED_ShowChinese(15, 42, "状态：");

    if(people_Get()==0)
    {
        OLED_ShowChinese(60, 22, "有人");
        
        // 根据控制开关决定是否开启设备 
        if (ledControlEnabled && !ledStatus) {
            LED_ON();
            ledStatus = 1;
        }
        if (fanControlEnabled && !fanStatus) {
            FEN_ON();
            fanStatus = 1;
        }
    }
    else
    {
        OLED_ShowChinese(60, 22, "无人");
        
        // 无人时关闭设备 
        if (ledStatus) {
            LED_OFF();
            ledStatus = 0;
        }
        if (fanStatus) {
            FEN_OFF();
            fanStatus = 0;
        }
    }
    
    if (Serial_GetRxFlag() == 1)   //串口数据接收 
    {
        switch(Serial_RxPacket[7]) {//判断对应数据位
            case 0x00:
                OLED_ShowChinese(60, 42, "无人");
                LED1_ON();
                LED2_OFF();
                LED3_OFF();
                break;
            case 0x02:
                OLED_ShowChinese(60, 42, "静止");
                LED2_ON();
                LED1_OFF();
                LED3_OFF();
                break;
            case 0x01:  
            case 0x03:
                OLED_ShowChinese(60, 42, "运动");
                LED3_ON();
                LED1_OFF();
                LED2_OFF();
                break;
        }
        Serial_RxFlag = 0;
    }
}

// 显示设置页面 
void ShowSettingsPage(void)
{
    OLED_Clear();
    OLED_ShowChinese(30, 0, "设备设置");
    
    // 显示设置项和光标 
    for (int i = 0; i < 2; i++) {
        if (cursorPos == i) {
            OLED_ShowString(0, 22 + i*22, "->", OLED_8X16);
        }
        
        switch (i) {
            case ITEM_1:
                OLED_ShowChinese(25, 22, "照明：");
                OLED_ShowChinese(70, 22, ledControlEnabled ? "开启" : "关闭");
                break;
            case ITEM_2:
                OLED_ShowChinese(25, 42, "风扇：");
                OLED_ShowChinese(70, 42, fanControlEnabled ? "开启" : "关闭");
                break;

						
        }
    }
}

//显示温湿度
void ShowSensorPage(void)
{
			OLED_Clear();
			OLED_ShowChinese(30, 0, "环境参数");
			OLED_ShowString(10, 24, "temp:",OLED_8X16);
			OLED_ShowString(10, 48, "humi:",OLED_8X16);
    
	
	if(Read_DHT11(&DHT11_Data) == SUCCESS)
		{
			OLED_ShowNum(55, 48, DHT11_Data.humi_int, 2,OLED_8X16);
			OLED_ShowString(75, 48, ".",OLED_6X8);
			OLED_ShowNum(85, 48, DHT11_Data.humi_deci, 2,OLED_8X16);
			
			OLED_ShowNum(55, 24, DHT11_Data.temp_int, 2,OLED_8X16);
			OLED_ShowString(75, 24, ".",OLED_6X8);
			OLED_ShowNum(85, 24, DHT11_Data.temp_deci, 1,OLED_8X16);
			
		}


}


// 按键处理 
void ProcessKey(uint8_t keyNum)
{
    static uint8_t lastKey = 0;
    
    
    if (keyNum == lastKey) return;
    lastKey = keyNum;
    
    switch (currentPage) {
        case PAGE_MAIN:
            if (keyNum == 1) {  // 按键1：切换到设置页
                currentPage = PAGE_SETTINGS;
                cursorPos = ITEM_1;  // 重置光标到第一个设置项
            }
            break;
            
        case PAGE_SETTINGS:
            if (keyNum == 1) {  // 按键1：切换参数页
                currentPage = PAGE_PARA;
            } else if (keyNum == 2) {  // 按键2：光标下移 (修正：模2而不是模3)
                cursorPos = (SettingItem_t)((cursorPos + 1) % 2);
            } else if (keyNum == 3) {  // 按键3：开启当前选中的设备控制
                switch (cursorPos) {
                    case ITEM_1:
                        ledControlEnabled = 1;
                        // 如果有人存在，立即开启照明
                        if (people_Get() == 0) {
                            LED_ON();
                            ledStatus = 1;
                        }
                        break;
                    case ITEM_2:
                        fanControlEnabled = 1;
                        // 如果有人存在，立即开启风扇
                        if (people_Get() == 0) {
                            FEN_ON();
                            fanStatus = 1;
                        }
                        break;
                    default:
                        break;
                }
            } else if (keyNum == 4) {  // 按键4：关闭当前选中的设备控制
                switch (cursorPos) {
                    case ITEM_1:
                        ledControlEnabled = 0;
                        // 立即关闭照明
                        LED_OFF();
                        ledStatus = 0;
                        break;
                    case ITEM_2:
                        fanControlEnabled = 0;
                        // 立即关闭风扇
                        FEN_OFF();
                        fanStatus = 0;
                        break;
                    default:
                        break;
                }
            }
            break;
						
			case PAGE_PARA:
					if (keyNum == 1) {  // 按键1：切换到主页
                currentPage = PAGE_MAIN;
                cursorPos = ITEM_1;  
            }
            break;
    }
}
