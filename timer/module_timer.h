/*
定时器模块
可分为一般直接执行任务定时器，和发送消息式的，两种定时器
若有疑问，可联系邮箱：xs595@qq.com
*/

#ifndef TIMER_BFDX_201503021339
#define TIMER_BFDX_201503021339

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include<semaphore.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "clist.h"

#if 1
// #ifndef TIMER_LOG_LEVEL
// #define TIMER_LOG_LEVEL LOG_NOTICE
// #endif

#define printf_debug printf
#define printf_debug2 //printf
#define printf_debug3 //printf
#endif

#define TIMER_EXIST -2
#define TIMER_ERROR -1
#define TIMER_OK 1
#define MAX_TIMER_NUMBER 0XFFF
//最小等待粒度（微秒），主要是防止当前定时器头的最小等待时间过长，而突然加入新的定时器时间比较小时，还再继续傻等中，不能及时处理新的定时器
#define MIN_DELAY_TIME 500000

//定时器消息类型
#define MSG_TYPE_FOR_TIMER 888

typedef unsigned long ULONG;
typedef unsigned short USHORT;
typedef unsigned char UCHAR;


typedef void* (*pFuncTimer)(void* p);

typedef struct tag_S_TIMER_MSG_BUF
{
	long type; /* 消息类型，必须 > 0 , MSG_TYPE_FOR_TIMER */
	USHORT id; /* 消息文本, 被触发的定时器ID */
}S_TIMER_MSG_BUF;

typedef struct tag_Timer
{
	clist list;					//链表结构体
	char state;					//ID被使用的状态
	int times;					//定时器执行次数，目前只分一次性和无线性定时器
	int msgid;					//接收定时器触发的消息的消息队列ID
	USHORT id;					//定时器id
	struct timeval tv;			//下次触发时间
	struct timeval interval;	//间隔时间触发
	pFuncTimer func;			//函数指针
	void* parameter;			//函数参数
}st_timer;




#ifdef __cplusplus
}
#endif


#endif



