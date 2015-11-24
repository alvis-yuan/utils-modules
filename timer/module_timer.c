/*
定时器模块
可分为一般直接执行任务定时器，和发送消息式的，两种定时器
若有疑问，可联系邮箱：xs595@qq.com
*/



#ifdef __cplusplus
extern "C" {
#endif


#include "bfdx_timer.h"
#include "log.h"

//S_LOG g_logTimer;
static st_timer g_timerArray[MAX_TIMER_NUMBER] = {0};
static clist g_listTimerWork = {&g_listTimerWork, &g_listTimerWork};//在执行的队列
static clist g_listTimerFree = {&g_listTimerFree, &g_listTimerFree};//闲置的队列

//修改变量时互斥用
static sem_t g_sem;
static sem_t g_sem_new;

int g_num = 0;


//对比时间, srctv >= dsttv 时，返回1
static int cmptime(struct timeval* srctv, struct timeval* dsttv)
{
	if(srctv->tv_sec > dsttv->tv_sec
		|| (srctv->tv_sec == dsttv->tv_sec && srctv->tv_usec >= dsttv->tv_usec))
	{
		return 1;
	}else
	{
		return 0;
	}
}

static int test(int id)
{
	clist* list_t = list_begin(&g_listTimerWork);
	int i = 0;
	while(list_end(&g_listTimerWork) != list_t)
	{
		st_timer* timer_t = (st_timer*)list_t;
		LOG_WRITE_POS(LOG_ERR, "%d  ", timer_t->id);
		if (i++ > 20)
		{
			LOG_WRITE_POS(LOG_ERR, "insert id = %d, link_number = %d\r\n", id, i);
			exit(1);
		}
		list_t = list_t->next;
	}
	LOG_WRITE_POS(LOG_ERR, "id=%d,   link_number = %d, %ld,  %ld\r\n", id, i, g_timerArray[id].tv.tv_sec, g_timerArray[id].tv.tv_usec);
	return 1;
}

//将定时器按时间插入对应位置
static int insertTimer(int id)
{
	clist* list_t = list_begin(&g_listTimerWork);
	st_timer* timer_t = (st_timer*)list_t;
	st_timer* TimerVal = &g_timerArray[id];
	
	//插入到头部
	if (list_empty(&g_listTimerWork))
	{
		list_push_front(&g_listTimerWork, &TimerVal->list);
		return 1;
	}

	//插入到中间
	while(list_end(&g_listTimerWork) != list_t/* && !list_empty(list_t)*/)
	{
		timer_t = (st_timer*)list_t;
		if(cmptime(&timer_t->tv, &TimerVal->tv))
		{
			list_insert(list_t, &TimerVal->list);
			return 2;
		}
		list_t = list_t->next;
	}
	//插入到尾部
	list_push_back(&g_listTimerWork, &TimerVal->list);
	return 3;
}

//定时器执行任务的循环线程
static void* thread_timer(void* p)
{
	struct timespec tn;
	S_TIMER_MSG_BUF buf;
	const UCHAR sizeSend = sizeof(buf.id);
	buf.type = MSG_TYPE_FOR_TIMER;
	
	while (1)
	{
		sem_wait(&g_sem);
		if (list_end(&g_listTimerWork) == list_begin(&g_listTimerWork))
		{
			sem_post(&g_sem);
			sem_wait(&g_sem_new);
			continue;
		}
		
		st_timer* timer_t = (st_timer*)list_begin(&g_listTimerWork);
		struct timeval tv={0, 0};
		gettimeofday (&tv , NULL);
		
		if (timer_t->state == 0)
		{
			list_pop_front(&g_listTimerWork);
			list_push_back(&g_listTimerFree, &timer_t->list);
			printf_debug3("KillTimer  id=%d\n", timer_t->id);
			sem_post(&g_sem);
			continue;
		}

		if(cmptime(&tv, &timer_t->tv))
		{
			timer_t->tv.tv_usec = (timer_t->tv.tv_usec + timer_t->interval.tv_usec);
			timer_t->tv.tv_sec = (timer_t->tv.tv_sec + timer_t->interval.tv_sec) + timer_t->tv.tv_usec / 1000000;
			timer_t->tv.tv_usec %= 1000000;
			list_pop_front(&g_listTimerWork);
			if (timer_t->func != NULL)
			{
				pFuncTimer func = (pFuncTimer)timer_t->func;
				insertTimer(timer_t->id);
				sem_post(&g_sem);
				func(timer_t->parameter);
			}else if (timer_t->msgid >= 0)
			{
				buf.id = timer_t->id;
				msgsnd(timer_t->msgid, &buf, sizeSend, 0);
				if (timer_t->times > 0)
				{
					timer_t->times--;
					if (timer_t->times <= 0)
					{
						timer_t->state = 0;
						list_push_back(&g_listTimerFree, &timer_t->list);
						printf_debug3("KillTimer  id=%d\n", timer_t->id);
						sem_post(&g_sem);
						continue;
					}
				}
				insertTimer(timer_t->id);
				sem_post(&g_sem);
			}else
			{
				timer_t->state = 0;
				list_push_back(&g_listTimerFree, &timer_t->list);
				sem_post(&g_sem);
				LOG_WRITE_POS(LOG_ERR, "id=%d, msgid=%d, func=%x\n", timer_t->id, timer_t->msgid, timer_t->func);
			}
		}else
		{
			printf_debug3("Head time: %ld, %ld\r\n", timer_t->tv.tv_sec, timer_t->tv.tv_usec);
			printf_debug3("Cur  time: %ld, %ld\r\n", tv.tv_sec, tv.tv_usec);
			sem_post(&g_sem);
			tn.tv_sec = timer_t->tv.tv_sec;//timer_t->tv.tv_sec - tv.tv_sec;
			tn.tv_nsec = timer_t->tv.tv_usec * 1000;//(timer_t->tv.tv_usec - tv.tv_usec) * 1000;
			sem_timedwait(&g_sem_new, &tn);
		}
	}

	return NULL;
};

void timerInit()
{
	static int b = -1;
	if (b == -1)
	{
		b = 0;
// 		memset(&g_logTimer, 0, sizeof(S_LOG));
// 		log_new(&g_logTimer, TIMER_LOG_LEVEL, " TIMER "); // 创建日志模块变量。
		log_write(LOG_NOTICE, "VER_TIME=%s %s\n", __DATE__, __TIME__);

		memset(&g_timerArray, 0, sizeof(g_timerArray));
		list_init(&g_listTimerWork);
		list_init(&g_listTimerFree);
		while (b < MAX_TIMER_NUMBER)
		{
			g_timerArray[b].id = b;
			list_push_back(&g_listTimerFree, &g_timerArray[b++].list);
		}
		b = 0;
		int err = sem_init(&g_sem, 0, 1);
		if(err != 0)  
		{
			LOG_WRITE_POS(LOG_ERR, "sem_init failed\n");  
			exit(EXIT_FAILURE);  
		}
		err = sem_init(&g_sem_new, 0, 0);
		if(err != 0)  
		{
			LOG_WRITE_POS(LOG_ERR, "sem_init failed\n");  
			exit(EXIT_FAILURE);  
		} 

		pthread_t ntid;

		err = pthread_create(&ntid, NULL, thread_timer, NULL);
		if (err != 0)
		{
			LOG_WRITE_POS(LOG_ERR, "Can't create thread: %s\n", strerror(err));
			exit(EXIT_FAILURE); 
		}
	}
}

static int GetTimerID()
{
	st_timer *pTimer = (st_timer*)list_begin(&g_listTimerFree);
	if (list_empty(&g_listTimerFree))
	{
		LOG_WRITE_POS(LOG_ERR, "ID using transfinite.\n");
		return TIMER_ERROR;
	}
	list_pop_front(&g_listTimerFree);
	return pTimer->id;
}

//功  能: 设置新定时器执行操作
//描  叙: 设置新定时器，按时间间隔执行操作
//参  数: 
//		  timeInterval: 间隔时间，以毫秒为单位
//		  pFunc:执行动作的函数指针
//		  parameter: 函数参数的指针
//返回值: 错误则小于0， 正确则返回id: 定时器唯一标识号，KillTimer函数的参数
int SetTimer(long timeInterval, pFuncTimer pFunc, void* parameter)
{
	if (0 >= timeInterval || NULL == pFunc)
	{
		LOG_WRITE_POS(LOG_ERR, "Parameter error .  timeInterval=%ld, pFunc=%x\n", timeInterval, pFunc);
		return TIMER_ERROR;
	}

	timerInit();

	sem_wait(&g_sem);
	
	int id = GetTimerID();
	if (id == TIMER_ERROR)
	{
		sem_post(&g_sem);
		return TIMER_ERROR;
	}
	struct timeval tv={0, 0};
	gettimeofday (&tv , NULL);
	g_timerArray[id].state = 1;
	g_timerArray[id].id = id;
	g_timerArray[id].interval.tv_sec = timeInterval / 1000;
	g_timerArray[id].interval.tv_usec = timeInterval % 1000 * 1000;
	g_timerArray[id].tv.tv_sec = (tv.tv_sec + g_timerArray[id].interval.tv_sec)
		+ (tv.tv_usec + g_timerArray[id].interval.tv_usec) / 1000000;
	g_timerArray[id].tv.tv_usec = (tv.tv_usec + g_timerArray[id].interval.tv_usec) % 1000000;
	g_timerArray[id].func = pFunc;
	g_timerArray[id].parameter = parameter;
	g_timerArray[id].msgid = -1;

	//bool b = list_empty(&g_listTimerWork);
	insertTimer(id);
	//if (b)
	{
		sem_post(&g_sem_new);
	}
	printf_debug3("SetTimer  id=%d\n", id);
	sem_post(&g_sem);

	return id;
}

//功  能: 关闭对应id的定时器
//描  叙: 关闭对应id的定时器任务
//参  数: id: 设置新定时器时返回的id值
//返回值: 无
void KillTimer(USHORT id)
{
	if (id >= MAX_TIMER_NUMBER)
	{
		LOG_WRITE_POS(LOG_NOTICE, "Parameter error, id=%d, max id=%d\n", id, MAX_TIMER_NUMBER-1);
		return;
	}
	sem_wait(&g_sem);
	if (g_timerArray[id].state == 0)
	{
		LOG_WRITE_POS(LOG_NOTICE, "Parameter error, id=%d, state=%d\n", id, g_timerArray[id].state);
		sem_post(&g_sem);
		return;
	}
	list_erase(&g_timerArray[id].list);
	g_timerArray[id].state = 0;
	list_push_back(&g_listTimerFree, &g_timerArray[id].list);
	printf_debug3("KillTimer  id=%d\n", id) ;
	sem_post(&g_sem);
	return;
}


//功  能: 设置新定时器执行操作
//描  叙: 设置新定时器，按时间间隔发送消息到消息队列
//参  数: 
//		  timeInterval: 间隔时间，以毫秒为单位
//		  msgid:消息队列ID
//		  times: 定时器执行次数，<=0为无限次, >0为执行times次数后自动删除定时器
//返回值: 错误则小于0， 正确则返回id: 定时器唯一标识号，KillTimer函数的参数
int SetMsgTimer(long timeInterval, int msgid, int times)
{
	if (0 >= timeInterval || 0 > msgid)
	{
		LOG_WRITE_POS(LOG_ERR, "Parameter error. timeInterval=%ld, msgid=%d\n", timeInterval, msgid);
		return TIMER_ERROR;
	}

	timerInit();

	sem_wait(&g_sem);

	int id = GetTimerID();
	if (id == TIMER_ERROR)
	{
		sem_post(&g_sem);
		return TIMER_ERROR;
	}
	struct timeval tv={0, 0};
	gettimeofday (&tv , NULL);
	g_timerArray[id].state = 1;
	g_timerArray[id].id = id;
	g_timerArray[id].interval.tv_sec = timeInterval / 1000;
	g_timerArray[id].interval.tv_usec = timeInterval % 1000 * 1000;
	g_timerArray[id].tv.tv_sec = (tv.tv_sec + g_timerArray[id].interval.tv_sec)
		+ (tv.tv_usec + g_timerArray[id].interval.tv_usec) / 1000000;
	g_timerArray[id].tv.tv_usec = (tv.tv_usec + g_timerArray[id].interval.tv_usec) % 1000000;
	g_timerArray[id].msgid = msgid;
	g_timerArray[id].times = times;// > 0 ? 1 : 0;

	//bool b = list_empty(&g_listTimerWork);
	insertTimer(id);
	//if (b)
	{
		sem_post(&g_sem_new);
	}
	sem_post(&g_sem);

	return id;
}


//功  能: 更改定时器间隔时间并重新开始计时
//描  叙: 更改定时器间隔时间并重新开始计时
//参  数: id: 定时器ID
//		  timeInterval: 间隔时间，以毫秒为单位
//返回值: 错误返回TIMER_ERROR，正确返回TIMER_OK
int ChangeTimerTime(USHORT id, long timeInterval)
{
	if (id >= MAX_TIMER_NUMBER || timeInterval <= 0)
	{
		LOG_WRITE_POS(LOG_WARNING, "Parameter error, id=%d, max id=%d, timeInterval=%ld\n", id, MAX_TIMER_NUMBER-1, timeInterval);
		return TIMER_ERROR;
	}
	sem_wait(&g_sem);
	if (g_timerArray[id].state == 0)
	{
		LOG_WRITE_POS(LOG_WARNING, "Parameter error, id=%d, state=%d\n", id, g_timerArray[id].state);
		sem_post(&g_sem);
		return TIMER_ERROR;
	}
	
	struct timeval tv={0, 0};
	gettimeofday (&tv , NULL);
	g_timerArray[id].interval.tv_sec = timeInterval / 1000;
	g_timerArray[id].interval.tv_usec = timeInterval % 1000 * 1000;
	g_timerArray[id].tv.tv_sec = (tv.tv_sec + g_timerArray[id].interval.tv_sec)
		+ (tv.tv_usec + g_timerArray[id].interval.tv_usec) / 1000000;
	g_timerArray[id].tv.tv_usec = (tv.tv_usec + g_timerArray[id].interval.tv_usec) % 1000000;
	list_erase(&g_timerArray[id].list);
	insertTimer(id);

	sem_post(&g_sem);
	return TIMER_OK;
}

//功  能: 获取定时器间隔时间
//描  叙: 获取定时器间隔时间
//参  数: id: 定时器ID
//返回值: 间隔时间，以毫秒为单位
long GetTimerTime(USHORT id)
{
	if (id >= MAX_TIMER_NUMBER)
	{
		LOG_WRITE_POS(LOG_WARNING, "Parameter error, id=%d, max id=%d\n", id, MAX_TIMER_NUMBER-1);
		return 0;
	}
	if (g_timerArray[id].state == 0)
	{
		LOG_WRITE_POS(LOG_WARNING, "Parameter error, id=%d, state=%d\n", id, g_timerArray[id].state);
		return 0;
	}
	return g_timerArray[id].interval.tv_sec * 1000 + g_timerArray[id].interval.tv_usec / 1000;
}


#ifdef __cplusplus
}
#endif
