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
 ALIENTEK Elite STM32F103 Development Board FreeRTOS Example 15-1
 FreeRTOS Software Timer Example - External Function Version
 Technical Support: www.openedv.com
 Taobao Store: http://eboard.taobao.com
 Follow WeChat Official Account: "ALIENTEK" to get STM32 materials.
 Guangzhou Star Electronic Technology Co., Ltd.
 Author: ALIENTEK @ALIENTEK
************************************************/
volatile unsigned long long FreeRTOSRunTimeTicks;

//任务优先级
#define START_TASK_PRIO			1
//任务堆栈大小
#define START_STK_SIZE 			256
//任务句柄
TaskHandle_t StartTask_Handler;
//任务句柄
void start_task(void *pvParameters);

//任务优先级
#define TIMERCONTROL_TASK_PRIO	2
//任务堆栈大小
#define TIMERCONTROL_STK_SIZE 	256
//任务句柄
TaskHandle_t TimerControlTask_Handler;
//任务句柄
void timercontrol_task(void *pvParameters);

////////////////////////////////////////////////////////
TimerHandle_t 	AutoReloadTimer_Handle;			//周期性定时器句柄
TimerHandle_t	OneShotTimer_Handle;			//单次定时器句柄
TimerHandle_t AutotestTimer_Handle;       //周期性定时器句柄

void AutoReloadCallback(TimerHandle_t xTimer); 	//周期性定时器回调函数
void OneShotCallback(TimerHandle_t xTimer);		//单次定时器回调函数
void AutotestCallback(TimerHandle_t xTimer);
void DrawOlympicRings(u16 center_x, u16 center_y, u8 radius);

//LCD刷新时使用的颜色
int lcd_discolor[14]={	WHITE, BLACK, BLUE,  BRED,
						GRED,  GBLUE, RED,   MAGENTA,
						GREEN, CYAN,  YELLOW,BROWN,
						BRRED, GRAY };

						//要写入24c02的字符串数组
const u8 TEXT_Buffer[]={"Elite STM32 IIC TEST"};
#define SIZE sizeof(TEXT_Buffer)

int main(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);//设置系统中断优先级分组4
	delay_init();	    				//延时初始化
	uart_init(115200);					//串口初始化
	LED_Init();		  					//初始化LED
	KEY_Init();							//初始化按键
	BEEP_Init();						//初始化蜂鸣器
	LCD_Init();							//初始化LCD
	EXTIX_Init();              //外部中断初始化
	AT24CXX_Init();                 //IIC初始化
	my_mem_init(SRAMIN);            	//初始化内部内存
    AT24CXX_Write(0,(u8*)TEXT_Buffer,SIZE); //写入数据
	Adc_Init();   //ADC初始化
    POINT_COLOR = RED;

	LCD_ShowString(30,10,200,16,16,"ATK STM32F103/407");
	LCD_ShowString(30,30,200,16,16,"FreeRTOS Examp 15-1");
	LCD_ShowString(30,50,200,16,16,"KEY_UP:Start Tmr1");
	LCD_ShowString(30,70,200,16,16,"KEY0:Start Tmr2");
	LCD_ShowString(30,90,200,16,16,"KEY1:Stop Tmr1 and Tmr2");
	LCD_ShowString(30,110,200,16,16,"I use gemma4:e4b model");
	LCD_ShowString(30,130,200,16,16,"I am an AI RoBot");

	// 加粗文字(偏移1像素重绘模拟加粗效果)，放在五环下方
	POINT_COLOR = RED;
	LCD_ShowString(10,390,220,16,16,"Wish you good luck forever!");
	LCD_ShowString(11,390,220,16,16,"Wish you good luck forever!");

	LCD_DrawLine(0,168,239,168);		//画线
	LCD_DrawLine(119,168,119,319);		//画线

	POINT_COLOR = BLACK;
	LCD_DrawRectangle(5,170,115,314); 	//画一个矩形
	LCD_DrawLine(5,190,115,190);		//画线

	LCD_DrawRectangle(125,170,234,314); //画一个矩形
	LCD_DrawLine(125,190,234,190);		//画线
	POINT_COLOR = BLUE;
	LCD_ShowString(6,171,110,16,16,	 "AutoTim:000");
	LCD_ShowString(126,171,110,16,16,"OneTim: 000");

	//任务句柄初始化
    xTaskCreate((TaskFunction_t )start_task,            //任务句柄
                (const char*    )"start_task",          //任务句柄名称
                (uint16_t       )START_STK_SIZE,        //任务堆栈大小
                (void*          )NULL,                  //传递给任务函数的参数
                (UBaseType_t    )START_TASK_PRIO,       //任务优先级
                (TaskHandle_t*  )&StartTask_Handler);   //任务句柄
    vTaskStartScheduler();          //任务句柄启动
}

//串口初始化完成后
void start_task(void *pvParameters)
{
    taskENTER_CRITICAL();           //进入临界区
    //创建周期定时器
    AutoReloadTimer_Handle=xTimerCreate((const char*		)"AutoReloadTimer",
									    (TickType_t			)1000,
								            (UBaseType_t		)pdTRUE,
								            (void*				)1,
								            (TimerCallbackFunction_t)AutoReloadCallback); //周期性定时器，1s(1000个时钟节拍)，自动重载模式
    //创建单次定时器
		  OneShotTimer_Handle=xTimerCreate((const char*			)"OneShotTimer",
								         (TickType_t			)2000,
								         (UBaseType_t			)pdTRUE,
								         (void*					)2,
								         (TimerCallbackFunction_t)OneShotCallback); //单次定时器，2s(2000个时钟节拍)，自动重载模式
												//创建测试定时器
			AutotestTimer_Handle=xTimerCreate((const char*		)"AutoTestTimer",  //500ms周期定时器
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
    vTaskDelete(StartTask_Handler); //删除初始任务
    taskEXIT_CRITICAL();            //退出临界区
}

//TimerControl任务函数
void timercontrol_task(void *pvParameters)
{
	u8 key;
	//u16 count=0;
	//u8 key_last=0;  //模拟key变化

	while(1)
	{
		  /*key_last=key;
			key=count/30;
			if(key>=4)
				{key=0;
				count=0;
		     }*/
		//只有当两个定时器都创建成功才能对其中操作
		if((AutoReloadTimer_Handle!=NULL)&&(OneShotTimer_Handle!=NULL))
		{

			key = KEY_Scan(0);
			switch(key)
			{
				case 1:     //按下key_up则启动定时器
					xTimerStart(AutoReloadTimer_Handle,0);	//启动周期定时器
                    xTimerStart(AutotestTimer_Handle,0);
					printf("启动定时器1和定时3\r\n");
					break;
				case 2:		//按下key0则开启单次定时器
					xTimerStart(OneShotTimer_Handle,0);		//启动单次定时器
					printf("启动定时器2\r\n");
					break;
				case 3:		//按下key1则关闭定时器
					xTimerStop(AutoReloadTimer_Handle,0); 	//关闭周期定时器
					xTimerStop(OneShotTimer_Handle,0); 		//关闭单次定时器
                    xTimerStop(AutotestTimer_Handle,0);
					printf("关闭定时器1、2、3\r\n");
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
	tmr1_num++;									//周期定时器执行次数+1
	//p=mymalloc(sizeof(char),20);


	//p=&str[0];
  //printf("指针的值为:%s\n",p);

	LCD_ShowxNum(70,171,tmr1_num,3,16,0x80); 	//显示周期定时器执行次数
	LCD_Fill(7,172,114,313,lcd_discolor[tmr1_num%14]);//定时器1

	LED1=!LED1;
	//printf("AutoReload_Task num is %d,systemtime=%lld\n",tmr1_num,FreeRTOSRunTimeTicks);
	AT24CXX_Read(0,datatemp,SIZE);
	printf("AT24C02 Store data is:%s\r\n",datatemp);
	//free(p);
}

//画三角形函数
static void DrawTriangle(u16 x1, u16 y1, u16 x2, u16 y2, u16 x3, u16 y3)
{
    LCD_DrawLine(x1, y1, x2, y2);
    LCD_DrawLine(x2, y2, x3, y3);
    LCD_DrawLine(x3, y3, x1, y1);
}

//单次定时器的回调函数
void OneShotCallback(TimerHandle_t xTimer)
{
	u16 adcx;
	float temp;
	static u8 tmr2_num = 0;
	static u8 show_shape = 0; // 0:四边形, 1:三角形

	tmr2_num++;		//周期定时器执行次数+1
	LCD_ShowxNum(190,171,tmr2_num,3,16,0x80);  //显示单次定时器执行次数

	// 清除右侧显示区域(用白色填充)
	LCD_Fill(126,171,233,313,WHITE);
	// 重画右面板边框(被白色填充覆盖)
	POINT_COLOR = BLACK;
	LCD_DrawRectangle(125,170,234,314);
	LCD_DrawLine(125,190,234,190);
	POINT_COLOR = BLUE;

	// 根据show_shape显示不同的图形
	if(show_shape == 0)
	{
		// 显示四边形(矩形)
		LCD_DrawRectangle(136, 161, 223, 303);
	}
	else
	{
		// 显示三角形(等腰三角形)
		// 顶点坐标
		u16 top_x = 180;    // 中间
		u16 top_y = 161;
		// 底边两个顶点
		u16 left_x = 136;
		u16 left_y = 303;
		u16 right_x = 223;
		u16 right_y = 303;

		DrawTriangle(top_x, top_y, left_x, left_y, right_x, right_y);
	}

	// 切换图形标志
	show_shape = !show_shape;

	LED0=!LED0;
	adcx=Get_Adc_Average(ADC_Channel_1,10);
	LCD_ShowxNum(156,320,adcx,4,16,0);//显示ADC值
    temp=(float)adcx*(3.3/4096);
    printf("采集的电压为:%7.3f\r\n",temp);
	adcx=temp;
	LCD_ShowxNum(156,350,adcx,1,16,0);//显示电压值
	temp-=adcx;
	temp*=1000;
	LCD_ShowxNum(172,350,temp,3,16,0X80);



  //printf("定时器2运行中\r\n");
}

//500ms定时器的回调函数
void AutotestCallback(TimerHandle_t xTimer)
{
	static u8 tmr3_num = 0;
	tmr3_num++;		//周期定时器执行次数+1
	//LCD_ShowxNum(190,131,tmr2_num,3,16,0x80);  //显示单次定时器执行次数
	//LCD_Fill(126,171,233,313,lcd_discolor[tmr2_num%14]); //定时器1
	LED0=!LED0;
	printf("AutotestTimer_Task num is %d,systemtime=%lld\r\n",tmr3_num,FreeRTOSRunTimeTicks);
	// 闪烁奥运五环
	DrawOlympicRings(120, 360, 10);
}

// 画同心圆(用于加粗奥运五环)
static void DrawThickCircle(u16 x0, u16 y0, u8 radius, u16 color)
{
    u16 saved = POINT_COLOR;
    POINT_COLOR = color;
    LCD_Draw_Circle(x0, y0, radius - 1);
    LCD_Draw_Circle(x0, y0, radius);
    LCD_Draw_Circle(x0, y0, radius + 1);
    POINT_COLOR = saved;
}

// 擦除圆环(用白色同心圆覆盖)
static void EraseRing(u16 x0, u16 y0, u8 radius)
{
    DrawThickCircle(x0, y0, radius, WHITE);
}

// 画奥运五环 - 每次调用交替显示/擦除(闪烁效果)
void DrawOlympicRings(u16 center_x, u16 center_y, u8 radius)
{
    static u8 visible = 0;
    u16 ring_colors[5] = {BLUE, YELLOW, BLACK, GREEN, RED};
    u16 horizontal_spacing = radius * 2 + 4;
    u16 vertical_spacing = radius + 2;
    u16 ring_centers_x[5];
    u16 ring_centers_y[5];
    int i;

    ring_centers_x[0] = center_x - horizontal_spacing;
    ring_centers_y[0] = center_y - vertical_spacing;
    ring_centers_x[1] = center_x;
    ring_centers_y[1] = center_y - vertical_spacing;
    ring_centers_x[2] = center_x + horizontal_spacing;
    ring_centers_y[2] = center_y - vertical_spacing;
    ring_centers_x[3] = center_x - horizontal_spacing / 2;
    ring_centers_y[3] = center_y + vertical_spacing;
    ring_centers_x[4] = center_x + horizontal_spacing / 2;
    ring_centers_y[4] = center_y + vertical_spacing;

    if (visible)
    {
        // 绘制加粗五环
        for (i = 0; i < 5; i++)
            DrawThickCircle(ring_centers_x[i], ring_centers_y[i], radius, ring_colors[i]);
    }
    else
    {
        // 擦除五环(闪烁)
        for (i = 0; i < 5; i++)
            EraseRing(ring_centers_x[i], ring_centers_y[i], radius);
    }
    visible = !visible;
}
