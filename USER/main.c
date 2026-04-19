#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "timer.h"
#include "lcd.h"
#include "key.h"
#include "beep.h"
#include "exti.h"
#include "malloc.h"
#include "string.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "myiic.h"
#include "24cxx.h"
#include "adc.h"
/************************************************
 ALIENTEK 精英STM32F103开发板 FreeRTOS实验15-1
 FreeRTOS软件定时器实验-库函数版本
 技术支持：www.openedv.com
 淘宝店铺：http://eboard.taobao.com 
 关注微信公众平台微信号："正点原子"，免费获取STM32资料。
 广州市星翼电子科技有限公司  
 作者：正点原子 @ALIENTEK
  修改时间20260419
************************************************/
volatile unsigned long long FreeRTOSRunTimeTicks;

//任务优先级
#define START_TASK_PRIO			1
//任务堆栈大小	
#define START_STK_SIZE 			256  
//任务句柄
TaskHandle_t StartTask_Handler;
//任务函数
void start_task(void *pvParameters);

//任务优先级
#define TIMERCONTROL_TASK_PRIO	3
//任务堆栈大小	
#define TIMERCONTROL_STK_SIZE 	256  
//任务句柄
TaskHandle_t TimerControlTask_Handler;
//任务函数
void timercontrol_task(void *pvParameters);

////////////////////////////////////////////////////////
TimerHandle_t 	AutoReloadTimer_Handle;			//周期定时器句柄
TimerHandle_t	OneShotTimer_Handle;			//单次定时器句柄
TimerHandle_t AutotestTimer_Handle;       //周期定时器句柄

void AutoReloadCallback(TimerHandle_t xTimer); 	//周期定时器回调函数
void OneShotCallback(TimerHandle_t xTimer);		//单次定时器回调函数
void AutotestCallback(TimerHandle_t xTimer);

//LCD刷屏时使用的颜色
int lcd_discolor[14]={	WHITE, BLACK, BLUE,  BRED,      
						GRED,  GBLUE, RED,   MAGENTA,       	 
						GREEN, CYAN,  YELLOW,BROWN, 			
						BRRED, GRAY };

						//要写入到24c02的字符串数组
const u8 TEXT_Buffer[]={"Elite STM32 IIC TEST"};
#define SIZE sizeof(TEXT_Buffer)	

int main(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);//设置系统中断优先级分组4	 
	delay_init();	    				//延时函数初始化	 
	uart_init(115200);					//初始化串口
	LED_Init();		  					//初始化LED
	KEY_Init();							//初始化按键
	BEEP_Init();						//初始化蜂鸣器
	LCD_Init();							//初始化LCD
	EXTIX_Init();              //外部中段初始化
	AT24CXX_Init();                 //IIC初始化
	my_mem_init(SRAMIN);            	//初始化内部内存池
    AT24CXX_Write(0,(u8*)TEXT_Buffer,SIZE); //存入数据
	Adc_Init();   //AD采样初始化
    POINT_COLOR = RED;
	
	LCD_ShowString(30,10,200,16,16,"ATK STM32F103/407");	
	LCD_ShowString(30,30,200,16,16,"FreeRTOS Examp 15-1");
	LCD_ShowString(30,50,200,16,16,"KEY_UP:Start Tmr1");
	LCD_ShowString(30,70,200,16,16,"KEY0:Start Tmr2");
	LCD_ShowString(30,90,200,16,16,"KEY1:Stop Tmr1 and Tmr2");

	LCD_DrawLine(0,108,239,108);		//画线
	LCD_DrawLine(119,108,119,319);		//画线
	
	POINT_COLOR = BLACK;
	LCD_DrawRectangle(5,110,115,314); 	//画一个矩形	
	LCD_DrawLine(5,130,115,130);		//画线
	
	LCD_DrawRectangle(125,110,234,314); //画一个矩形	
	LCD_DrawLine(125,130,234,130);		//画线
	POINT_COLOR = BLUE;
	LCD_ShowString(6,111,110,16,16,	 "AutoTim:000");
	LCD_ShowString(126,111,110,16,16,"OneTim: 000");
	
	//创建开始任务
    xTaskCreate((TaskFunction_t )start_task,            //任务函数
                (const char*    )"start_task",          //任务名称
                (uint16_t       )START_STK_SIZE,        //任务堆栈大小
                (void*          )NULL,                  //传递给任务函数的参数
                (UBaseType_t    )START_TASK_PRIO,       //任务优先级
                (TaskHandle_t*  )&StartTask_Handler);   //任务句柄              
    vTaskStartScheduler();          //开启任务调度
}

//开始任务任务函数
void start_task(void *pvParameters)
{
    taskENTER_CRITICAL();           //进入临界区
    //创建软件周期定时器
    AutoReloadTimer_Handle=xTimerCreate((const char*		)"AutoReloadTimer",
									    (TickType_t			)1000,
							            (UBaseType_t		)pdTRUE,
							            (void*				)1,
							            (TimerCallbackFunction_t)AutoReloadCallback); //周期定时器，周期1s(1000个时钟节拍)，周期模式
    //创建单次定时器
	  OneShotTimer_Handle=xTimerCreate((const char*			)"OneShotTimer",
							         (TickType_t			)2000,
							         (UBaseType_t			)pdTRUE,
							         (void*					)2,
							         (TimerCallbackFunction_t)OneShotCallback); //单次定时器，周期2s(2000个时钟节拍)，单次模式					  
    
											//创建定时器控制任务
		AutotestTimer_Handle=xTimerCreate((const char*		)"AutoTestTimer",  //500ms周期定时
									    (TickType_t			)500,
							            (UBaseType_t		)pdTRUE,
							            (void*				)1,
							            (TimerCallbackFunction_t)AutotestCallback);
											 
    xTaskCreate((TaskFunction_t )timercontrol_task,             
                (const char*    )"timercontrol_task",           
                (uint16_t       )TIMERCONTROL_STK_SIZE,        
                (void*          )NULL,                  
                (UBaseType_t    )TIMERCONTROL_TASK_PRIO,        
                (TaskHandle_t*  )&TimerControlTask_Handler);    
    vTaskDelete(StartTask_Handler); //删除开始任务
    taskEXIT_CRITICAL();            //退出临界区
}

//TimerControl的任务函数
void timercontrol_task(void *pvParameters)
{
	u8 key;
	//u16 count=0;
	//u8 key_last=0;  //模拟key操作
	
	while(1)
	{
		  /*key_last=key;		
			key=count/30;		
			if(key>=4) 
				{key=0;
				count=0;
		     }*/
		//只有两个定时器都创建成功了才能对其进行操作
		if((AutoReloadTimer_Handle!=NULL)&&(OneShotTimer_Handle!=NULL))
		{

			key = KEY_Scan(0);
			switch(key)
			{
				case 1:     //当key_up按下的话打开周期定时器
					xTimerStart(AutoReloadTimer_Handle,0);	//开启周期定时器
                    xTimerStart(AutotestTimer_Handle,0);      
					printf("开启定时器1和定时3\r\n");
					break;
				case 2:		//当key0按下的话打开单次定时器
					xTimerStart(OneShotTimer_Handle,0);		//开启单次定时器
					printf("开启定时器2\r\n");
					break;
				case 3:		//当key1按下话就关闭定时器
					xTimerStop(AutoReloadTimer_Handle,0); 	//关闭周期定时器
					xTimerStop(OneShotTimer_Handle,0); 		//关闭单次定时器
                    xTimerStop(AutotestTimer_Handle,0); 
					printf("关闭定时器1和2和3\r\n");
					break;	
			}
		}
		//num++;
		//if(num==50) 	//每500msLED0闪烁一次
		/*{
			num=0;
			LED0=!LED0;	
			//count=count+2;
			//printf("key=%d,count=%d,key_last=%d\n",key,count,key_last);
		}*/
        vTaskDelay(10); //延时10ms，也就是10个时钟节拍
	}
}

//周期定时器的回调函数
void AutoReloadCallback(TimerHandle_t xTimer)
{ 
	u8 datatemp[SIZE];
	//char *p=(char*)malloc(sizeof(char)*20);
	//char* p;
	//char str[20]="Hello World!";	
	static u8 tmr1_num=0;
	tmr1_num++;									//周期定时器执行次数加1
	//p=mymalloc(sizeof(char),20);
	
	
	//p=&str[0];
  //printf("输出的字符串为:%s\n",p);	
	
	LCD_ShowxNum(70,111,tmr1_num,3,16,0x80); 	//显示周期定时器的执行次数
	LCD_Fill(6,131,114,313,lcd_discolor[tmr1_num%14]);//填充区域
	
	LED1=!LED1;
	//printf("AutoReload_Task num is %d,systemtime=%lld\n",tmr1_num,FreeRTOSRunTimeTicks);
	AT24CXX_Read(0,datatemp,SIZE);
	printf("AT24C02 Store data is:%s\r\n",datatemp);
	//free(p);
}

//单次定时器的回调函数
void OneShotCallback(TimerHandle_t xTimer)
{ 
	u16 adcx;
	float temp;
	static u8 tmr2_num = 0;
	tmr2_num++;		//周期定时器执行次数加1
	LCD_ShowxNum(190,111,tmr2_num,3,16,0x80);  //显示单次定时器执行次数
	LCD_Fill(126,131,233,313,lcd_discolor[tmr2_num%14]); //填充区域
	LED0=!LED0;
	adcx=Get_Adc_Average(ADC_Channel_1,10);
	LCD_ShowxNum(156,320,adcx,4,16,0);//显示ADC的值
    temp=(float)adcx*(3.3/4096);
    printf("采集的电压为:%7.3f\r\n",temp);
	adcx=temp;
	LCD_ShowxNum(156,350,adcx,1,16,0);//显示电压值
	temp-=adcx;
	temp*=1000;
	LCD_ShowxNum(172,350,temp,3,16,0X80);
	
	
	
  //printf("定时器2运行结束\r\n");
}

//500ms定时器的回调函数
void AutotestCallback(TimerHandle_t xTimer)
{
	static u8 tmr3_num = 0;
	tmr3_num++;		//周期定时器执行次数加1
	//LCD_ShowxNum(190,111,tmr2_num,3,16,0x80);  //显示单次定时器执行次数
	//LCD_Fill(126,131,233,313,lcd_discolor[tmr2_num%14]); //填充区域
	LED0=!LED0;
	printf("AutotestTimer_Task num is %d,systemtime=%lld\r\n",tmr3_num,FreeRTOSRunTimeTicks);
}




