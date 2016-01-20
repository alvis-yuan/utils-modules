/*
基于UDP协议开发的稳定通信模块，保证不丢包不乱序，保证报文正确
若有疑问，可联系邮箱：xs595@qq.com
*/

#ifndef EX_DUP_X_201501161046
#define EX_DUP_X_201501161046

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/clist.h"

#define printf_SendMsg 1
#define printf_RecvMsg 1


#define EX_UDP_VER 18

// #ifndef EX_UDP_LOG_LEVEL
// #define EX_UDP_LOG_LEVEL LOG_NOTICE
// #endif

//#define printf_debug printf
#define printf_debug2 //printf
#define printf_debug6 //if (pslink->SeqMaxSend == MAX_PACKET_NUMBER-1 && pslink->SeqCheckSend == 0)printf
#define printf_debug7 //printf

	typedef unsigned long ULONG;
	typedef unsigned int UINT;
	typedef unsigned short USHORT;
	typedef unsigned char UCHAR;


//最大连接数
#define MAX_LINK_COUNT 0xFFF
//同进程内最大socket数
#define MAX_SOCKET_NUM 0x1FFF
//最大报文数组个数
#define MAX_PACKET_NUMBER 0x1FF
//最大报文序号(必须是MAX_PACKET_NUMBER的整倍数)
static const USHORT MAX_PACKET_SEQ  =  (MAX_PACKET_NUMBER << 2);
//包头长度+buf数据长度, 不加len的size, 1472+8(UDP头长度)+20(IP头长度) = 1500(mtu值)
#define MAX_PACKET_SIZE        1472//512//1024
//包头长度
#define LEN_PACKET_HEAD        8    //sizeof(s_packet_head)    //8
//最大数据报文长度,1464+8(ex_udp头长度)+8(UDP头长度)+20(IP头长度) = 1500(mtu值)
#define MAX_PACKET_DATA_SIZE  1464 //(MAX_PACKET_SIZE-LEN_PACKET_HEAD);//504//1464//1016    //

//监听线程的循环触发时间间隔(微秒),也是检查发送队列丢包情况的最小间隔时间
#define LISTEN_INTERVAL 10000
//初始化检查间隔值(微秒)
#define CHECK_INTERVAL_INIT 20000
//一小时间隔时间(毫秒)
#define HOUR_TIME 3600000

//心跳检查次数
long LISTEN_LINK_TIMES = 3;

//心跳检查时间间隔(毫秒)
long LISTEN_LINK_INTERVAL = 3000;

//连接目地端时的尝试次数
#define MAX_LINK_TIMES LISTEN_LINK_TIMES

//连接目地端时每次尝试的间隔时间(毫秒)
#define MAX_LINK_INTERVAL LISTEN_LINK_INTERVAL

//释放内存等待间隔时间(毫秒)
#define FREE_MEM_INTERVAL (5000)



#define EX_UDP_OK              (1)
#define EX_UDP_ERROR              (-1)
#define SOCKET_ERROR              (-1)
#define ID_ERROR              (-1)
#ifndef INVALID_SOCKET
#define INVALID_SOCKET            (~0)
#endif

/* message type */
enum {
    MSG_EX_UDP_PACKET = 1 << 8,
    MSGTYPE_UNCONNECTED = 1 << 9,
};


#define CHECKSNUMMAT_ENABLE 0

//消息报文数据
typedef struct tag_msg_packet
{
	long type;//消息类型
	int id;//连接的id
	int len;//包数据长度
	char buf[MAX_PACKET_DATA_SIZE];//包数据内容
}st_msg_packet;


#ifdef WIN32
#include <windows.h>
#include <process.h>
#include <Mmsystem.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment( lib,"winmm.lib" )

#define MAX_MSG_QUEUE 0x1FFF

#ifndef IPC_NOWAIT
#define IPC_NOWAIT	04000
#endif

#define sem_t HANDLE
#define pthread_t HANDLE

static int sem_init (sem_t *psemaphore, int pshared, unsigned int initCount)
{
	*psemaphore = CreateSemaphore(NULL, initCount, 1, NULL);
	if(*psemaphore == 0)
		return EX_UDP_ERROR;
	else 
		return 0;
}

#define sem_wait(semaphore) WaitForSingleObject(*semaphore, INFINITE)
#define sem_post(semaphore) ReleaseSemaphore(*semaphore, 1, NULL)
#define sem_destroy(semaphore) CloseHandle(*semaphore)
#define usleep(time) Sleep(time/1000)


typedef struct
{
	sem_t semRead;
	sem_t semWrite;
	sem_t semReadOnly;
	sem_t semWriteOnly;
	st_msg_packet msgQueue[MAX_MSG_QUEUE];
	int read;
	int write;
}stMsgQueue;


// #define msgrcv(a,b,c,d,e) \
// 	(g_msgQueue.write != g_msgQueue.read) ?\
// 	(*b = g_msgQueue.msgQueue[g_msgQueue.read], g_msgQueue.read = (g_msgQueue.read+1) % 0xFFF, sem_post(&g_msgQueue.semWrite),1) :\
// 	sem_wait(&g_msgQueue.semRead)\
// 
// #define msgsnd(a,b,c,d) \
// 	((g_msgQueue->write+1)%0xFFF != g_msgQueue->read) ? \
// 	(g_msgQueue->msgQueue[g_msgQueue->write] = *b, g_msgQueue->write = (g_msgQueue->write+1)%0xFFF,sem_post(&g_msgQueue->semRead)) :\
// 	sem_wait(&g_msgQueue->semWrit

#define pthread_create(pntid, NULL, thread_func, sock) \
	(\
	*pntid = CreateThread(NULL, 0, thread_func, sock, 0, NULL),\
	CloseHandle(*pntid),\
	(*pntid == 0) ? -1 : 0\
	);


#define SEM_TIMED_WAIT(sem, time, ret)\
{\
	ret = WaitForSingleObject(sem, time);\
	if(ret == WAIT_TIMEOUT || ret == WAIT_FAILED)ret = -1;\
}

#define gettimeofday(ptv, NULL)\
	(*ptv).tv_usec = timeGetTime();\
	(*ptv).tv_sec = (*ptv).tv_usec / 1000;\
	(*ptv).tv_usec %= 1000;\
	(*ptv).tv_usec *= 1000;


#else
#ifndef linux
#define linux
#endif
#endif

#ifdef linux
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>
#include <sys/ipc.h>   
#include <sys/msg.h> 

//printf_debug("1 tn.tv_sec=%ld, tn.tv_nsec=%ld, timeVal=%ld\r\n", tn.tv_sec, tn.tv_nsec, timeVal);\

#define  SEM_TIMED_WAIT(sem, timeVal, ret)\
{\
	struct timespec tn;\
	gettimeofday((struct timeval*)&tn, NULL);\
	printf_debug2("1 tn.tv_sec=%ld, tn.tv_nsec=%ld, timeVal=%ld\r\n", tn.tv_sec, tn.tv_nsec, timeVal);\
	tn.tv_nsec = tn.tv_nsec * 1000 + timeVal % 1000 * 1000000;\
	tn.tv_sec += timeVal / 1000 + tn.tv_nsec / 1000000000;\
	tn.tv_nsec %= 1000000000;\
	printf_debug2("2 tn.tv_sec=%ld, tn.tv_nsec=%ld, timeVal=%ld\r\n", tn.tv_sec, tn.tv_nsec, timeVal);\
	ret = sem_timedwait(&(sem), &tn);\
}

#endif

#define GET_LINK_FOR_ID \
	s_link* pslink = NULL;\
	int sock = id >> 16 & MAX_SOCKET_NUM;\
	st_sock* sts = g_pSock[sock];\
	if (EX_UDP_ERROR == id)\
{\
	LOG_WRITE_POS(LOG_ERR, "Parameter error ! sock id=%d\r\n", id);\
	return EX_UDP_ERROR;\
}\
	if (sts == NULL || sts->linkState != 1)\
{\
	LOG_WRITE_POS(LOG_ERR, "Parameter error ! sock=%d, id=%d\r\n", sock, id & MAX_LINK_COUNT);\
	return EX_UDP_ERROR;\
}\
	id = id & MAX_LINK_COUNT;\
	pslink = sts->pLinkIDArray[id & MAX_LINK_COUNT];\
	if(NULL == pslink)\
{\
	LOG_WRITE_POS(LOG_ERR, "Parameter error ! id=%d\r\n", id);\
	return EX_UDP_ERROR;\
}

#define ASSERT_SOCK if (sock >= MAX_SOCKET_NUM || sock < 0 || g_pSock[sock] == NULL || g_pSock[sock]->linkState != 1)\
	{\
		LOG_WRITE_POS(LOG_DEBUG, "ERROR: This socket is closed ! sock=%d\r\n", sock);\
		return EX_UDP_ERROR;\
	}\

#define ASSERT_ID \
{\
	if(NULL == sts->pLinkIDArray[id & MAX_LINK_COUNT])\
{\
	return EX_UDP_ERROR;\
}\
}

#define ASSERT_ID_SEND \
{\
	if(NULL == sts->pLinkIDArray[id & MAX_LINK_COUNT])\
{\
	sem_post(&pslink->semSendListMutex);\
	return EX_UDP_ERROR;\
}\
}

#define ASSERT_ID_RECV \
{\
	if(NULL == sts->pLinkIDArray[id & MAX_LINK_COUNT])\
{\
	sem_post(&pslink->semRecvListMutex);\
	return EX_UDP_ERROR;\
}\
}

#define CREATE_LINK \
{\
if (NULL == pslink)\
{\
	LOG_WRITE_POS(LOG_ERR, "ERROR: malloc failed -> s_link !\n");\
	sem_post(&sts->semLink);\
	return NULL;\
}\
memset(pslink, 0, LEN_S_LINK);\
pslink->ip = ip;\
pslink->port = port;\
pslink->socket = sts->socket;\
if (EX_UDP_ERROR == add_list(sts, pslink))\
{\
	sem_post(&sts->semLink);\
	return NULL;\
}\
	sem_post(&sts->semLink);\
}

#define ASSERT_CREATE_NEW_LINK\
	if (type != LINK_SYN && type != LINK_SYN_ACK)\
				{\
				return NULL;\
				}\
				sem_wait(&sts->semLink);\
if (sts->linkState != 1)\
{\
	sem_post(&sts->semLink);\
	return 0;\
}

//判断序号是否正确,判断head是否在tail和tail+MAX_PACKET_NUMBER之间
#define	CMP_SEQ(head, tail)\
	((head > tail && head < tail + MAX_PACKET_NUMBER)\
	|| (head < tail && head + MAX_PACKET_SEQ - MAX_PACKET_NUMBER < tail))

//判断序号排列是否正确，判断mid是否在head和tail之间，head可以等于mid
#define	CMP_SEQ_ORDER(head, mid, tail)\
	((head <= mid && mid < tail && head+MAX_PACKET_NUMBER > tail)\
	|| (head <= mid && head > tail && head > tail+MAX_PACKET_SEQ-MAX_PACKET_NUMBER)\
	|| (head > tail && mid < tail && head > tail+MAX_PACKET_SEQ-MAX_PACKET_NUMBER))

//获取毫秒差值
#define GET_MS_DIFF(tv1, tv2) (((((tv1).tv_sec - (tv2).tv_sec) & 0x1FFFFF) * 1000) + ((tv1).tv_usec - (tv2).tv_usec) / 1000)

//消息类型
typedef enum tag_packet_type
{
	DEFAULT_NULL = 0,
	//连接请求
	LINK_SYN = 1, 
	//连接请求加回复
	LINK_SYN_ACK,
	//连接成功
	LINK_ACK, 
	//正常报文
	USER_DATA, 
	//发送本地发送队列的最大序号到对端
	SEQ_CHECK,
	//报告序号
	SEQ_REPORT,
	//缺包需要重传
	SEQ_LACK,
	//断开信号
	LINK_FIN,
}e_packet_type;

const char* PACKET_TYPE[] = {
	"DEFAULT_NULL",
	//连接请求
	"LINK_SYN", 
	//连接请求加回复
	"LINK_SYN_ACK",
	//连接成功
	"LINK_ACK", 
	//正常报文
	"USER_DATA", 
	//发送本地发送队列的最大序号到对端
	"SEQ_CHECK",
	//报告序号
	"SEQ_REPORT",
	//缺包需要重传
	"SEQ_LACK",
	//断开信号
	"LINK_FIN",
};

//#pragma pack()

typedef struct tag_link s_link;

//报文头结构
typedef struct tag_packet_head
{
	//报文类型
	UCHAR type;
	//连接状态
	UCHAR state;
	//报文序号
	USHORT seq;
	//检查序号
	USHORT seqCheck;
	//效验和，数据报文需要,先将效验和清0再把所有报文内容 (包括本结构体) 按字节异或得出
	USHORT checksum;
}s_packet_head;

//报文完整数据
typedef struct tag_packet_all
{
	s_packet_head head;
	char buf[MAX_PACKET_DATA_SIZE];
	//head加buf有效数据长度
	USHORT len;
}s_packet_all;

//一个连接的数据
struct tag_link
{
	clist list;//链表指针
	//本连接的数据
	char linkState;//状态为1,只能接收序号数据包,状态为2才能收发
	struct timeval timeoutFree;//释放内存的超时时间
	int id;
	int socket;
	USHORT port;
	ULONG ip;
	//char LackReportState:4;
	//char linkStateOther;
	//char SendTimeout;

	//int idNextMsgQueueRead;
	char PrioritySendLack;//优先发送缺包
	char accept;//本队列是否被用户请求走了
	UCHAR timeout_times;	//超时次数
	struct timeval timeout;//检查连接超时的时间
	struct timeval timeoutSend;//执行检查后保存的时间值,如果下次检查超时时间不够,不做检查
	struct timeval timeoutCheckLack;//检查缺包情况的间隔时间
	struct timeval timeoutReportLack;//报告缺包情况的时间
	long tsWaitTime;//以毫秒为单位，设置堵塞和异步模式，以及超时用
	//USHORT RecvCount;//读取到数据报文计数
	//USHORT RecvCountLast;//读取到数据报文计数的前一次listen值

	//信号量
	sem_t semRecvList;
	sem_t semSendList;
	sem_t semRecvListMutex;
	sem_t semSendListMutex;

	//ULONG RecvRepeatCount;//接收报文重复计数
	USHORT SeqRead;//用户调用接收接口读取的当前序号
	USHORT SeqReadMax;//最大的应该读到的序号，也就是本地保存的对端的最大发送序号值
	//USHORT SeqReadLast;//序号报告时做对比,判断是否需要再次报告
	USHORT SeqMaxSend;//当前发送的最大序号
	//USHORT SeqMaxSendReturn;//当前发送的检查的返回的最大序号报告
	USHORT SeqCheckSend;//客户端已经检查过,收到哪的正确报文序号
	//USHORT SeqMaxSendLast;//最后发送检查时的最大序号
	//USHORT SeqCheckSendLast;//最后发送检查时,客户端已经检查过,收到哪的正确报文序号
	//UCHAR SeqCheckTimes;  //相同序号的检查次数
	//接收队列,循环数组
	s_packet_all paArrayRecv[MAX_PACKET_NUMBER];
	//发送队列,循环数组
	s_packet_all paArraySend[MAX_PACKET_NUMBER];

	//同端口不同IP的下个节点
	s_link* same_port_next;
};


//#pragma pack()

typedef struct  
{
	clist list;
	int id;
}st_idList;


//每个sock对应的数据
typedef struct
{
	clist list;//链表指针，本sock下的所有link
	char linkState;
	char threadState;//线程数状态
	//struct timeval timeoutFree;//释放内存的超时时间
	clist listIDMsgReadHead;//链表指针，本sock下有数据读的link的ID队列头，链表成员是stIDListMsgRead
	clist listIDAllocationHead;//本socket下可分配的ID队列，链表成员是stIDListAllocation
	sem_t semAccept;//请求连接用
	sem_t semListen;//listen线程等待通知用
	sem_t semRecvToMsg;//读取本sock的连接id中有可读的报文到消息队列中
	sem_t semListRead;//增加删除互斥用
	sem_t semLink;//对连接的添加删除获取互斥用//增加删除互斥用
	//控制空闲时间隔等待时间
	struct timeval time_send;
	struct timeval time_listen;
	int socket;//socket值
	int msgid;
	long tsWaitTime;//本sock的堵塞模式、异步、超时模式，以及堵塞超时时间
	bool connectPriority;
	clist listFreeHead;
	s_link* nextListenLink;
	//接收时方便分辨是哪个端口和IP发来的包,可以迅速将报文放入相应的队列中去
	s_link* pLinkPortArray[0xFFFF];
	//保存每一个独立的连接对应的指针,方便用ID来访问,效率高还可以判断指针是否为空,为了安全
	s_link* pLinkIDArray[MAX_LINK_COUNT+1];
	//判断本ID是否有数据要写入消息队列
	st_idList stIDListMsgRead[MAX_LINK_COUNT+1];
	//可分配的ID链表
	st_idList stIDListAllocation[MAX_LINK_COUNT+1];
}st_sock;


#ifdef __cplusplus
}
#endif


#endif

