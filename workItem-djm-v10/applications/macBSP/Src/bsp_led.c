/*
 * bsp_led.c
 *
 *  Created on: May 16, 2024
 *      Author: zphu
 */
#include "bsp_led.h"




/*---------------------------------------------------------------------------------------------------------------*/
/* 以下是LED驱动函数（可移植通用）                                                                                                                                                          */
/*---------------------------------------------------------------------------------------------------------------*/
#define 	LED_MAX				(30)				// LED's Number
#define 	LED_Name_First		(1)					// bsp_led.h中的首个LED值应与此值一致，且其他LED值须在此基础上连续递增(枚举类型)
#define		OUT_CYCLE			(100)				// LED输出周期
#define		GRAD_DEC			(5)					// LED渐灭速度（占空比递减的间隔时间毫秒数）
#define		GRAD_INC			(5)					// LED渐亮速度（占空比递增的间隔时间毫秒数）
#define		GRAD_OFF_TIME		(50)				// LED完全熄灭后的停留时间（单位：毫秒，精度为GRAD_DEC的整数倍）
#define		GRAD_ON_TIME		(5)					// LED完全点亮后的停留时间（单位：毫秒，精度为GRAD_INC的整数倍）


static	int8_t	_ledOutStt[LED_MAX]={0};			// LED输出状态
static	int8_t	_ledOutClk[LED_MAX]={0};			// LED输出时钟
static	int8_t	_ledOutDty[LED_MAX]={0};			// LED输出占空（0~OUT_CYCLE，决定LED的亮度）
//----------------------------------------------------------------------------
static	int8_t	_ledGrad[LED_MAX]={0};				// LED渐变开关
static	int8_t	_ledGradDir[LED_MAX]={0};			// LED渐变方向
static	int8_t	_ledGradCnt[LED_MAX]={0};			// LED全亮/全灭后的停留计数
//----------------------------------------------------------------------------
static	int8_t	_ledBlnkClk[LED_MAX]={0};			// LED闪烁时钟（单位：0.1s）
static	int8_t	_ledBlnkCyc[LED_MAX]={0};			// LED闪烁周期（一次完整亮+灭的总时间，单位：0.1s）
static	int8_t	_ledBlnkDty[LED_MAX]={0};			// LED闪烁占空（一次完整亮的时间，单位：0.1s）
static	int8_t	_ledBlnkCnt[LED_MAX]={0};			// LED闪烁计数
static	int8_t	_ledBlnkCry[LED_MAX]={0};			// LED闪烁次数（0表示不闪烁）
static	int8_t	_ledBlnkMut[LED_MAX]={0};			// LED停顿周期数（0表示不停顿）
static	int8_t	_ledBlnkCct[LED_MAX]={0};			// LED重复计数
static	int8_t	_ledBlnkRep[LED_MAX]={0};			// LED重复闪烁次数（0表示不重复，100以上表示无限重复）
//----------------------------------------------------------------------------
static	int16_t	msCnt=0;							// 毫秒计数器
//----------------------------------------------------------------------------



/*****************************************************************************
* 功能:		LED常灭
*****************************************************************************/
static void _Off(int8_t ledName)
{
	_ledOutDty[ledName - LED_Name_First] = 0;			// 完全熄灭
}




/*****************************************************************************
* 功能:		LED常亮
*****************************************************************************/
static void _On(int8_t ledName)
{
	_ledOutDty[ledName - LED_Name_First] = OUT_CYCLE;	// 完全点亮
}



/*****************************************************************************
* 功能:		LED常灭
*****************************************************************************/
void LED_Off(int8_t ledName)
{
	int8_t	i = ledName - LED_Name_First;
	_ledGrad[i] = 0;									// 禁止呼吸
	_ledBlnkCry[i] = 0;									// 禁止闪烁
	_Off(ledName);
	LED_Out(ledName, 0);								// 立即熄灭
}




/*****************************************************************************
* 功能:		LED常亮
*****************************************************************************/
void LED_On(int8_t ledName)
{
	int8_t	i = ledName - LED_Name_First;
	_ledGrad[i] = 0;									// 禁止呼吸
	_ledBlnkCry[i] = 0;									// 禁止闪烁
	_On(ledName);
	LED_Out(ledName, 1);								// 立即点亮
}



/*****************************************************************************
* 功能:		LED翻转
*****************************************************************************/
void LED_Toggle(int8_t ledName)
{
	int8_t	i = ledName - LED_Name_First;
	if (_ledOutStt[i])	LED_Off(ledName);
	else				LED_On(ledName);
}



/*****************************************************************************
* 功能:		LED渐变（呼吸）
*****************************************************************************/
void LED_Grad(int8_t ledName)
{
	int8_t	i = ledName - LED_Name_First;
	_ledOutDty[i] = 0;
	_ledGradDir[i] = 0;
	_ledGradCnt[i] = 0;
	_ledGrad[i] = 1;
}


/*****************************************************************************
* 功能:		LED设置闪烁周期及占空比
* 参数:		Cycle	周期（一次完整亮+灭的总时间，单位：0.1s）
* 			Duty	占空比（周期中亮所占的时间，单位：0.1s）
*****************************************************************************/
void LED_BlinkSetCycleDuty(int8_t ledName, int8_t Cycle, int8_t Duty)
{
	int8_t	i = ledName-LED_Name_First;
	_ledBlnkCyc[i]=Cycle;
	_ledBlnkDty[i]=Duty;
	_ledBlnkClk[i]=0;
	msCnt=0;
}


/*****************************************************************************
* 功能:		LED闪烁（指定次数）
* 参数:		cry		闪烁次数（0表示不闪烁）
* 			mute	停顿周期数（0表示不停顿）
* 			repeat	重复闪烁次数（0表示不重复，100以上表示无限重复）
*****************************************************************************/
void LED_Blink(int8_t ledName, int8_t cry, int8_t mute, int8_t repeat)
{
	int8_t	i = ledName-LED_Name_First;
	_ledBlnkCry[i]=cry;
	_ledBlnkMut[i]=mute;
	_ledBlnkRep[i]=repeat;
	_ledBlnkCnt[i]=0;
	_ledBlnkCct[i]=0;
}


/*****************************************************************************
* 功能:		LED花式显示
*****************************************************************************/
void LED_Fancy(int8_t mode)
{
	switch(mode){
	case 1:
		break;
	case 2:
		break;
	}
}


/*****************************************************************************
* 功能:		LED扫描
* 说明:		扫描周期：1ms。
*****************************************************************************/
void LED_DrvScan(void)
{
	int8_t	i,num;

	if(++msCnt>=60000)	msCnt=0;

	num=LED_GetNumber();											// 获取LED个数
	if(num>LED_MAX)	num=LED_MAX;									// LED数量不能超范围
	for(i=0;i<num;i++){												// 逐个LED扫描
		if(_ledBlnkCry[i]){											// 需要闪烁
			if((msCnt%OUT_CYCLE)==0){								// 闪烁精度0.1s
				if(++_ledBlnkClk[i] >= _ledBlnkCyc[i]){ 			// 闪烁小周期结束
					_ledBlnkClk[i]=0;
					if(++_ledBlnkCnt[i] >= (_ledBlnkCry[i]+_ledBlnkMut[i])){	// 闪烁中周期结束
						_ledBlnkCnt[i]=0;
						if(++_ledBlnkCct[i] >= _ledBlnkRep[i]){		// 闪烁大周期结束
							_ledBlnkCct[i]=0;
							if(_ledBlnkRep[i] < 100)	_ledBlnkCry[i]=0;	// 重复次数到达，结束闪烁
						}
					}
				}else if(_ledBlnkClk[i] >= _ledBlnkDty[i]){			// 后半小周 不亮
					_Off(LED_Name_First+i);
				}else if(_ledBlnkCnt[i] < _ledBlnkCry[i]){			// （仅非停顿期间的）前半小周 亮
					_On(LED_Name_First+i);
				}
			}
		}

		if(_ledGrad[i]){											// 呼吸渐变控制
			if(_ledGradDir[i]){										// 渐灭
				if((msCnt%GRAD_DEC)==0){							// 渐灭速度控制
					if(_ledOutDty[i]>0){
						_ledOutDty[i]-=1;
						_ledGradCnt[i]=0;
					}
					else {// 完全熄灭后，停留时间控制
						_ledGradCnt[i]+=1;
						if(_ledGradCnt[i]>=(GRAD_OFF_TIME/GRAD_DEC)) _ledGradDir[i]=0;
					}
				}
			}else{													// 渐亮
				if((msCnt%GRAD_INC)==0){							// 渐亮速度控制
					if(_ledOutDty[i]<OUT_CYCLE){
						_ledOutDty[i]+=1;
						_ledGradCnt[i]=0;
					}
					else {// 完全点亮后，停留时间控制
						_ledGradCnt[i]+=1;
						if(_ledGradCnt[i]>=(GRAD_ON_TIME/GRAD_INC))	_ledGradDir[i]=1;
					}
				}
			}
		}

		_ledOutClk[i]+=1;
		if(_ledOutClk[i]>=OUT_CYCLE)	_ledOutClk[i]=0;			// 输出计时
		if(_ledOutClk[i]>=_ledOutDty[i]){							// 输出控制（占空比决定亮度）
			_ledOutStt[i]=0;										// LED熄灭
		}else{
			_ledOutStt[i]=1;										// LED点亮
		}
		LED_Out(LED_Name_First+i,_ledOutStt[i]);					// 输出
	}
}



/***************************************
 * @brief  LED 初始化函数
 * @param  无
 * @retval 无
 ***************************************/
void LED_Init(void)
{
	int8_t	i,num;

	num=LED_GetNumber();											// 获取LED个数
	if(num>LED_MAX)	num=LED_MAX;									// LED数量不能超范围
	for(i=0;i<num;i++){												// 逐个LED扫描
		LED_BlinkSetCycleDuty(LED_Name_First+i,3, 2);
	}
}



/*****************************************************************************
* 功能:		LED输出
*****************************************************************************/
void LED_Out(int8_t ledName, int8_t ledState)
{
	#if 1
	switch(ledName)
	{
		case LED_Name_Green:
			if(ledState)	macLED_GREEN_ON();
			else			macLED_GREEN_OFF();
			break;
	}
	#endif
}



/*****************************************************************************
* 功能:		获取LED数量
*****************************************************************************/
int8_t LED_GetNumber(void)
{
	return LED_NUM;
}




/*---------------------------------------------------------------------------------------------------------------*/
/* 以下是按键扫描线程的创建以及回调函数                                                                          */
/*---------------------------------------------------------------------------------------------------------------*/
/**
  * @brief  This thread entry is used for key scan
  * @retval void
  */
void LED_Thread_entry(void* parameter)
{
    LED_Init();
    for(;;)
    {
        LED_DrvScan();
        rt_thread_mdelay(10);
    }
}



/**
  * @brief  This is a Initialization for matrix key
  * @retval int
  */
rt_thread_t LED_Task_Handle = RT_NULL;
int LED_Thread_Init(void)
{
    LED_Task_Handle = rt_thread_create("LED_Thread_entry", LED_Thread_entry, RT_NULL, 4096, 9, 20);
    /* 检查是否创建成功,成功就启动线程 */
    if(LED_Task_Handle != RT_NULL)
    {
        rt_kprintf("PRINTF:%d. LED_Thread_entry is Succeed!! \r\n",Record.kprintf_cnt++);
        rt_thread_startup(LED_Task_Handle);
    }
    else {
        rt_kprintf("PRINTF:%d. LED_Thread_entry is Failed \r\n",Record.kprintf_cnt++);
    }

    return RT_EOK;
}
INIT_APP_EXPORT(LED_Thread_Init);

