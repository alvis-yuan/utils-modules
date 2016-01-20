
/*
基于UDP协议开发的稳定通信模块，保证不丢包不乱序，保证报文正确
若有疑问，可联系邮箱：xs595@qq.com
*/


#ifdef __cplusplus
extern "C" {
#endif

#include "ex_udp.h"
#include "../include/log.h"

#ifdef WIN32
	stMsgQueue g_msgQueue = {0};
#endif

//S_LOG g_logExudp = {0};

//以双端队列的形式保存所有连接的指针地址,方便thread_listen线程遍历执行定时事件
//static clist g_list_head = {&g_list_head, &g_list_head};
//接收时方便分辨是哪个端口和IP发来的包,可以迅速将报文放入相应的队列中去
//static s_link* g_pLinkPortArray[0xFFFF] = {0};
//保存每一个独立的连接对应的指针,方便用ID来访问,效率高还可以判断指针是否为空,为了安全
//static s_link* sts->pLinkIDArray[MAX_LINK_COUNT] = {0};
//判断本ID是否有数据要写入消息队列
//static st_idList sts->stIDListMsgRead[MAX_LINK_COUNT] = {0};
//对socket的添加删除互斥用
static sem_t g_semLink;

//常用长度值变量
static int g_lensockaddr = sizeof(struct sockaddr);
static const int LEN_PACKET = sizeof(s_packet_all);
static const ULONG MAX_CHECK_INTERVAL = CHECK_INTERVAL_INIT * LEN_PACKET_HEAD;
static const ULONG LEN_S_LINK = sizeof(s_link);
static const USHORT LEN_MSG_HEAD = sizeof(st_msg_packet)-MAX_PACKET_DATA_SIZE - sizeof(long);
//所有socket数据数组
static st_sock* g_pSock[MAX_SOCKET_NUM+1] = {0};
static const ULONG LEN_S_SOCK = sizeof(st_sock);

static long g_tsWaitTime = -1;//全局设置堵塞和异步模式，以及超时用，以毫秒为单位


#ifndef FUNC_START
#define FUNC_START

static int SeqLack(s_link* pslink);
static int SeqReport(s_link *pslink);

//控制listen线程等待时间
static void ctlIntervalTime(st_sock* sts)
{
	if (sts == NULL || sts->linkState != 1)
	{
		return;
	}
	if (GET_MS_DIFF(sts->time_listen, sts->time_send) >= LISTEN_LINK_INTERVAL)
	{
		sem_post(&sts->semListen);
		//gettimeofday(&sts->time_send, NULL);
	}
	//LOG_WRITE_POS(LOG_ERR, "sts->time_listen.tv_sec=%d - sts->time_send.tv_sec=%d\r\n", sts->time_listen.tv_sec, sts->time_send.tv_sec);
	gettimeofday(&sts->time_send, NULL);
}

//计算校验和
static USHORT checksummat(s_packet_all* const pa)
{
	USHORT *pchecksum = NULL;
	int i = 0;
	const int len = pa->len;
	USHORT checksum = 0, checksum_t = 0;

	if (NULL == pa || pa->len < LEN_PACKET_HEAD || pa->len > MAX_PACKET_SIZE)
	{
		LOG_WRITE_POS(LOG_ERR, "ERROR: checksummat error \n");
		return EX_UDP_ERROR;
	}

	checksum_t = pa->head.checksum;
	pa->head.checksum = 0;
	pchecksum = (USHORT*)pa;
	if (len & 1 && len != MAX_PACKET_SIZE)
	{
		pa->buf[len-LEN_PACKET_HEAD] = 0;
	}
	i = (len+1) % 2;
	while (i-- > 0)
	{
		checksum ^= *pchecksum++;
	}
	pa->head.checksum = checksum_t;
	return checksum;
}

int add_listRead(st_sock* sts, s_link *pslink)
{
	sem_wait(&sts->semListRead);
	if (list_empty(&sts->stIDListMsgRead[pslink->id].list))
	{
		list_push_back(&sts->listIDMsgReadHead, &sts->stIDListMsgRead[pslink->id].list);
		sem_post(&sts->semRecvToMsg);
	}
	sem_post(&sts->semListRead);
	return EX_UDP_OK;
}

int del_listRead(st_sock* sts, s_link *pslink)
{
	sem_wait(&sts->semListRead);
	if (!list_empty(&sts->stIDListMsgRead[pslink->id].list))
	{
		list_erase(&sts->stIDListMsgRead[pslink->id].list);
		//list_init(&sts->stIDListMsgRead[pslink->id].list);
	}
	sem_post(&sts->semListRead);
	return EX_UDP_OK;
}

static SendToMsgQueue(st_sock* sts, s_link* pslink)
{
	int msgid = 0;
	int count = 0;
	int id = pslink->id;
	
	msgid = sts->msgid;

	if (0 <= msgid)
	{
		int ret_value = 0;
		st_msg_packet msg;
		msg.type = MSG_EX_UDP_PACKET;
		msg.id = (pslink->id | pslink->socket << 16);
		while(pslink->paArrayRecv[pslink->SeqRead % MAX_PACKET_NUMBER].head.seq == pslink->SeqRead
			&& pslink->paArrayRecv[pslink->SeqRead % MAX_PACKET_NUMBER].len != 0)
		{
			msg.len = pslink->paArrayRecv[pslink->SeqRead % MAX_PACKET_NUMBER].len-LEN_PACKET_HEAD;
			if (msg.len > MAX_PACKET_DATA_SIZE)
			{
				LOG_WRITE_POS(LOG_ERR, "[SendToMsgQueue] ERROR: len=%d, SeqRead=%d\n", msg.len, pslink->SeqRead);
				return;
			}
			//LOG_WRITE_POS(LOG_ERR, "SendToMsgQueue: id=%d, sock=%d, len=%d, seq=%d\r\n", pslink->id, pslink->socket, pslink->paArrayRecv[pslink->SeqRead % MAX_PACKET_NUMBER].len, pslink->paArrayRecv[pslink->SeqRead % MAX_PACKET_NUMBER].head.seq);
			memcpy(msg.buf, pslink->paArrayRecv[pslink->SeqRead % MAX_PACKET_NUMBER].buf, msg.len);
			ret_value = msgsnd(msgid, &msg, LEN_MSG_HEAD + msg.len, 0);
			ASSERT_ID;
			if ( ret_value < 0 )
			{  
// 				if (EAGAIN == errno)
// 				{
// 					return;
// 				}
				printf_debug2("Write msg queue failed, errno=%d[%s]\n",errno,strerror(errno));
// 				ASSERT_ID;
// 				{
// 					int sock = pslink->socket;
// 					ASSERT_SOCK;
// 					g_pSock[pslink->socket]->msgid = -1;
// 				}
				break;
				//exit(-1);
			}
			count++;
			pslink->paArrayRecv[pslink->SeqRead % MAX_PACKET_NUMBER].len = 0;
			//pslink->paArrayRecv[pslink->SeqRead % MAX_PACKET_NUMBER].head.seq++;
			//最后读取序号+1
			pslink->SeqRead = (pslink->SeqRead+1) % MAX_PACKET_SEQ;
		}
		del_listRead(sts, pslink);
		//if (count)
		{
			SeqReport(pslink);
		}
	}
	return;
}

//发送报文
static int SendMsg(s_link* pslink, s_packet_all* pa, int len)
{
	int ret = 0;
	struct sockaddr_in dstAddr;

	dstAddr.sin_family=AF_INET;
	dstAddr.sin_addr.s_addr = pslink->ip;
	dstAddr.sin_port = htons(pslink->port);
	if (len >= LEN_PACKET_HEAD)
	{
		//pa->len = len;
		//pa->head.checksum = checksummat(pa);
		// 		static ULONG ul = 1;
		// 		if (ul++ % 10000 == 0)
		// 		{
		// 			LOG_WRITE_POS(LOG_ERR, "SendMsg: type=%d, len=%d, seq=%d\n", pa->head.type, pa->len, pa->head.seq);
		// 		}
		pa->head.state = pslink->linkState;
		pa->head.seqCheck = (pslink->SeqCheckSend + 1) % MAX_PACKET_SEQ;
		ret = sendto(pslink->socket, (const char *)pa, len, 0, (struct sockaddr*)&dstAddr, g_lensockaddr);

#if printf_SendMsg
		if (USER_DATA != pa->head.type)
		{
			log_write(LOG_DEBUG, "[SendMsg]: id=%d, type=%s, len=%d, seq=%d, SeqCheckSend=%d, SeqMaxSend=%d, Send=%s, port=%d, sock=%d\n",
				pslink->id, PACKET_TYPE[pa->head.type], pa->len, pa->head.seq, pslink->SeqCheckSend, pslink->SeqMaxSend, inet_ntoa(*(struct in_addr*)&pslink->ip), pslink->port, pslink->socket);
		}
#endif
	}else
	{
		LOG_WRITE_POS(LOG_ERR, "[SendMsg]: id=%d, type=%d, len=%d, seq=%d, SeqCheckSend=%d, SeqMaxSend=%d, Send=%s, port=%d\n", 
			pslink->id, pa->head.type, pa->len, pa->head.seq, pslink->SeqCheckSend, pslink->SeqMaxSend, inet_ntoa(*(struct in_addr*)&pslink->ip), pslink->port);
		exit(1);
	}

	return ret;
}

//删除一个连接目标
static int del_list(st_sock* sts, s_link* pslink)
{
	s_link* pLinkPort = NULL;
	s_packet_all pa;
	USHORT id = 0;
	int sock = sts->socket;
	memset(&pa, 0, sizeof(pa));
	id = pslink->id;
	//sem_wait(&sts->semLink);
	//清空ID数组的指针
	if(id > MAX_LINK_COUNT || NULL == sts->pLinkIDArray[id & MAX_LINK_COUNT])
	{
		//sem_post(&sts->semLink);
		return EX_UDP_ERROR;
	}
	pa.head.type = LINK_FIN;
	SendMsg(pslink, &pa, LEN_PACKET_HEAD);

	if (sts->msgid != -1)
	{
		del_listRead(sts, pslink);
	}
	
	sts->pLinkIDArray[id & MAX_LINK_COUNT] = NULL;

	sts->nextListenLink = (s_link*)pslink->list.next;
	//删除链表中的节点
	list_erase(&pslink->list);
	//pslink->accept = 0;
	//清空同端口数组队列中的同IP的指针
	pLinkPort = sts->pLinkPortArray[pslink->port];
	if (NULL == pLinkPort)
	{
		LOG_WRITE_POS(LOG_ERR, "No matching connection id. id=%d, ip=%s, port=%d, sock=%d\r\n", id, inet_ntoa(*(struct in_addr*)&pslink->ip), pslink->port, pslink->socket);
	}else if (pLinkPort->ip == pslink->ip && pLinkPort->socket == sock)
	{
		sts->pLinkPortArray[pslink->port] = pLinkPort->same_port_next;
	}else
	{
		while (pLinkPort->same_port_next != NULL)
		{
			if (pLinkPort->same_port_next->ip == pslink->ip && pLinkPort->same_port_next->socket == sock)
			{
				pLinkPort->same_port_next = pLinkPort->same_port_next->same_port_next;
				break;
			}
			pLinkPort = pLinkPort->same_port_next;
		}
	}

	//释放内存和数据
	sem_post(&pslink->semSendList);
	sem_post(&pslink->semRecvList);
	sem_post(&pslink->semSendListMutex);
	sem_post(&pslink->semRecvListMutex);
	sem_destroy(&pslink->semRecvList);
	sem_destroy(&pslink->semSendList);
	sem_destroy(&pslink->semRecvListMutex);
	sem_destroy(&pslink->semSendListMutex);
	
	//等待其它正在使用此数据地方退出
	//usleep(40000);//linux下不等待一会会段错误,信号量删除存在延迟
	
	gettimeofday(&pslink->timeoutFree, 0);
	pslink->linkState = -1;
	list_push_back(&sts->listFreeHead, (clist*)pslink);
	list_push_back(&sts->listIDAllocationHead, &sts->stIDListAllocation[id].list);
	LOG_WRITE_POS(LOG_NOTICE, "Del connection id. id=%d, ip=%s, port=%d, sock=%d\r\n", id, inet_ntoa(*(struct in_addr*)&pslink->ip), pslink->port, pslink->socket);
	sem_post(&sts->semListen);
	//sem_post(&sts->semLink);
	return EX_UDP_OK;
}

//添加一个新的连接目标
static int add_list(st_sock* sts, s_link* pslink)
{
	int res = EX_UDP_ERROR;
	int id = 0;
	st_idList* idList = NULL;

	if (list_empty(&sts->listIDAllocationHead))
	{
		LOG_WRITE_POS(LOG_ERR, "ERROR: More than the maximum number of links !\r\n");
		free(pslink);
		return EX_UDP_ERROR;
	}
	idList = (st_idList*)list_begin(&sts->listIDAllocationHead);
	id = idList->id;
	list_erase(&idList->list);

	//初始从第一个序号开始读和发送
	pslink->SeqRead = 1;
	pslink->SeqMaxSend = 1;
	pslink->SeqCheckSend = 0;
	pslink->timeoutCheckLack.tv_sec = 0;
	pslink->timeoutCheckLack.tv_usec = CHECK_INTERVAL_INIT;
	pslink->paArrayRecv[0].head.seq = 1;
	pslink->paArraySend[0].head.seq = 1;
	pslink->timeout_times = 1;
	pslink->timeout.tv_sec = 0;
	pslink->tsWaitTime = sts->tsWaitTime;
	gettimeofday(&pslink->timeoutSend, NULL);
	//pslink->timeoutReportLack = pslink->timeoutSend;
	pslink->linkState = 0;

	printf_debug2("&pslink=%x, pslink=%x, pslink->SeqRead=%d\n", &pslink, pslink, pslink->SeqRead);
	
	//初始化互斥量，使用默认的互斥量属性
	res = sem_init(&pslink->semRecvList, 0, 1);
	if(res != 0)  
	{
		LOG_WRITE_POS(LOG_ERR, "sem_init failed\n");
		exit(EXIT_FAILURE);  
	}

	res = sem_init(&pslink->semSendList, 0, 1);
	if(res != 0)  
	{  
		LOG_WRITE_POS(LOG_ERR, "sem_init failed\n");  
		exit(EXIT_FAILURE);  
	}

	res = sem_init(&pslink->semRecvListMutex, 0, 1);
	if(res != 0)  
	{
		LOG_WRITE_POS(LOG_ERR, "sem_init failed\n");
		exit(EXIT_FAILURE);  
	}

	res = sem_init(&pslink->semSendListMutex, 0, 1);
	if(res != 0)  
	{  
		LOG_WRITE_POS(LOG_ERR, "sem_init failed\n");  
		exit(EXIT_FAILURE);  
	}

	pslink->id = id;
	sts->pLinkIDArray[id] = pslink;
	list_push_back(&sts->list, &pslink->list);
	LOG_WRITE_POS(LOG_NOTICE, "Add connection id. id=%d, ip=%s, port=%d, sock=%d\r\n", pslink->id, inet_ntoa(*(struct in_addr*)&pslink->ip), pslink->port, pslink->socket);
	sem_post(&sts->semAccept);
	sem_post(&sts->semListen);
	printf_debug2("add_list id=%d\r\n", pslink->id);
	return EX_UDP_OK;
}

//功  能: 获取一个可连接目标
//参  数: sock是exSocket函数返回的socket id
//返回值: 连接目标的数据地址指针
static s_link* GetLinkAccept(st_sock* sts)
{
	s_link* pslink = NULL;

	sem_wait(&sts->semLink);
	pslink = (s_link*)list_begin(&sts->list);
	while ((s_link*)list_end(&sts->list) != pslink)
	{
		if (pslink->accept == 0)
		{
			pslink->accept = 1;
			pslink->tsWaitTime = sts->tsWaitTime;
			printf_debug2("pslink=%x, pslink->SeqRead=%d\n", pslink, pslink->SeqRead);
			sem_post(&sts->semLink);
			return pslink;
		}
		pslink = (s_link*)pslink->list.next;
	}
	sem_post(&sts->semLink);
	return NULL;
}

//功  能: 获取一个连接的指针，如果无此连接，就新建一个连接指针
//参  数: sock是exSocket函数返回的socket id
//        以sock、ip和port来区别一个连接
//返回值: 连接目标的数据地址指针
static s_link* get_link(st_sock* sts, ULONG ip, USHORT port, USHORT type)
{
	s_link* pslink = NULL;

	if (NULL == sts->pLinkPortArray[port])
	{
		ASSERT_CREATE_NEW_LINK;
		pslink = (s_link*)malloc(LEN_S_LINK);
		sts->pLinkPortArray[port] = pslink;
		CREATE_LINK;
	}else
	{
		pslink = sts->pLinkPortArray[port];
		while (pslink->ip != ip)
		{
			if (pslink->same_port_next == NULL)
			{
				ASSERT_CREATE_NEW_LINK;
				pslink = pslink->same_port_next = (s_link*)malloc(LEN_S_LINK);
				CREATE_LINK;
				break;
			}
			pslink = pslink->same_port_next;
		}
	}

// 	if (pslink->socket != sts->socket)
// 	{
// 		pslink = (s_link*)list_begin(&sts->list);
// 		while(pslink != (s_link*)list_end(&sts->list))
// 		{
// 			if (pslink->ip == ip && pslink->port == port)
// 			{
// 				break;
// 			}
// 			pslink = (s_link*)pslink->list.next;
// 		}
// 		if (pslink == (s_link*)list_end(&sts->list))
// 		{
// 			ASSERT_CREATE_NEW_LINK;
// 			pslink = (s_link*)malloc(LEN_S_LINK);
// 			CREATE_LINK;
// 			pslink->same_port_next = sts->pLinkPortArray[port]->same_port_next;
// 			sts->pLinkPortArray[port]->same_port_next = pslink;
// 		}
// 	}
	return pslink;
}


#endif

#ifndef MESSAGE_FUNC
#define MESSAGE_FUNC

//只做序号报告
static int SeqReport(s_link* pslink)
{
	if(pslink->linkState == 2)
	{
		s_packet_all pa;
		memset(&pa, 0, sizeof(pa));

		pa.head.type = SEQ_REPORT;
		pa.head.seq = pslink->SeqRead == 0 ? MAX_PACKET_SEQ -1 : pslink->SeqRead-1;
		pa.len = LEN_PACKET_HEAD;
		SendMsg(pslink, &pa, pa.len);

		SeqLack(pslink);

		printf_debug2("[SeqReport] pa.head.seq=%d  pslink->SeqRead=%d\n", pa.head.seq, pslink->SeqRead);
	}
	return EX_UDP_OK;
}

//收到序号报告的应答
static int SeqReportAck(s_link* pslink, s_packet_all* pa)
{
	if(pslink->linkState < 2)
	{
		return EX_UDP_ERROR;
	}
	if (CMP_SEQ_ORDER(pslink->SeqCheckSend, pa->head.seq, pslink->SeqMaxSend))
	{
		//printf_debug7("SeqCheckSend=%d, seq=%d, SeqMaxSend=%d\r\n", pslink->SeqCheckSend, pa->head.seq, pslink->SeqMaxSend);

		pslink->SeqCheckSend = pa->head.seq;
		sem_post(&pslink->semSendList);
		if (pa->len > LEN_PACKET_HEAD)//有检查报文发出去的时间信息
		{
			//动态获取-检查间隔时间
			long usec = 0;
			struct timeval *ptv = (struct timeval *)pa->buf;
			struct timeval timeCur={0, 0};
			gettimeofday (&timeCur , NULL);
			usec = (GET_MS_DIFF(timeCur, *ptv) & 0x1FFFFF) * 1000;

			//增乘减除，适应各种网络环境
			if (usec > pslink->timeoutCheckLack.tv_usec)
			{
				pslink->timeoutCheckLack.tv_usec <<= 1;
				pslink->timeoutCheckLack.tv_sec = 0;
				//LOG_WRITE_POS(LOG_ERR, "0  pslink->timeoutCheckLack.tv_usec=%-16ld, usec=%-16ld\r\n", pslink->timeoutCheckLack.tv_usec, usec);
			}else if ((usec << 1) < pslink->timeoutCheckLack.tv_usec)
			{
				pslink->timeoutCheckLack.tv_usec > LISTEN_INTERVAL ? pslink->timeoutCheckLack.tv_usec >>= 1 : LISTEN_INTERVAL;
				pslink->timeoutCheckLack.tv_sec = 0;
				//LOG_WRITE_POS(LOG_ERR, "1  pslink->timeoutCheckLack.tv_usec=%-16ld, usec=%-16ld\r\n", pslink->timeoutCheckLack.tv_usec, usec);
			}
		}
	}

	return EX_UDP_OK;
}


//报告缺包序号信息
static int SeqLack(s_link* pslink)
{
	USHORT SeqReadLast = pslink->SeqRead;
	struct timeval tvCur;
	gettimeofday(&tvCur, NULL);

	if (GET_MS_DIFF(tvCur, pslink->timeoutReportLack) < pslink->timeoutCheckLack.tv_usec / 1000)
	{
		return EX_UDP_ERROR;
	}
	if(CMP_SEQ(pslink->SeqReadMax, SeqReadLast))
	{//中间有包丢失,做缺包报告
		//if (pslink->SeqReadMax != SeqReadLast)
		{
			s_packet_all pa;
			USHORT* usp = NULL;
			const int size_us = sizeof(USHORT);
			int iLackCount = 0;
			memset(&pa, 0, sizeof(s_packet_all));
			pa.head.type = SEQ_LACK;
			usp = (USHORT*)pa.buf;

			pslink->timeoutReportLack = tvCur;
			//printf_debug2("[%s][%s][%d]pslink->SeqReadLast=%d, seq=%d\r\n",  __FILE__, __func__, __LINE__, pslink->SeqReadLast, seq);
			//iLackCount是缺包的总个数

			do{
				if (pslink->paArrayRecv[SeqReadLast % MAX_PACKET_NUMBER].head.seq != SeqReadLast
					|| pslink->paArrayRecv[SeqReadLast % MAX_PACKET_NUMBER].len == 0)
				{
					//LOG_WRITE_POS(LOG_ERR, "%d ", SeqReadLast);
					iLackCount++;
					*usp = SeqReadLast;
					usp++;
					if (iLackCount*size_us % MAX_PACKET_DATA_SIZE == 0)
					{
						pa.len = MAX_PACKET_SIZE;
						SendMsg(pslink, &pa, pa.len);
						//return EX_UDP_OK;
						usp = (USHORT*)pa.buf;
					}
				}
				SeqReadLast = (SeqReadLast + 1) % MAX_PACKET_SEQ;
			}while(SeqReadLast != pslink->SeqReadMax);

			iLackCount = iLackCount*size_us % MAX_PACKET_DATA_SIZE;
			if (iLackCount != 0)
			{
				pa.len = LEN_PACKET_HEAD+iLackCount;
				SendMsg(pslink,& pa, pa.len);
			}
			//LOG_WRITE_POS(LOG_ERR, "\r\n");
		}
	}

	return EX_UDP_OK;
}

//重传缺包
static int SeqLackAck(s_link* pslink, s_packet_all* pa)
{
	int len = 0;
	USHORT* usp = NULL;
	s_packet_all *pspa = NULL;
	USHORT SeqCheckSend_t = (pslink->SeqCheckSend+1)%MAX_PACKET_SEQ;

	if(pslink->linkState < 2)
	{
		return EX_UDP_ERROR;
	}

	len = (pa->len-LEN_PACKET_HEAD)/sizeof(USHORT);
	usp = (USHORT*)pa->buf;

	printf_debug7("[SeqLackAck] SeqCheckSend=%d, SeqMaxSend=%d, len=%d\r\n", pslink->SeqCheckSend, pslink->SeqMaxSend, len);

	pslink->PrioritySendLack = 1;
	while (len-- > 0)
	{
		if (MAX_PACKET_SEQ > *usp)
		{
			pspa = &pslink->paArraySend[*usp % MAX_PACKET_NUMBER];
			if(CMP_SEQ_ORDER(SeqCheckSend_t, *usp, pslink->SeqMaxSend))
			{
				//printf_debug6("INFO: *usp=%d\r\n", *usp);
				SendMsg(pslink, pspa, pspa->len);
			}
			else
			{
//				LOG_WRITE_POS(LOG_ERR, "INFO: len=%d, pslink->SeqCheckSend=%d, *usp=%d, pslink->SeqMaxSend=%d\r\n", len, pslink->SeqCheckSend, *usp, pslink->SeqMaxSend);
// 				exit(0);
				break;
			}
		}else
		{
			LOG_WRITE_POS(LOG_ERR, "ERROR: seq=%d > MAX_PACKET_SEQ\n", *usp);
			break;
		}
		usp++;
	}

	pslink->PrioritySendLack = 0;
	sem_post(&pslink->semSendList);
	return EX_UDP_OK;
}

//做序号检查
static int SeqCheck(st_sock* sts, s_link* pslink)
{
	if (pslink->linkState == 2 && GET_MS_DIFF(sts->time_listen, pslink->timeout) < LISTEN_LINK_INTERVAL)
	{
		if (pslink->SeqMaxSend != (pslink->SeqCheckSend+1) % MAX_PACKET_SEQ)//没有发包就不检查了
		{
			s_packet_all pa;
			struct timeval *pTimeCur = (struct timeval *)pa.buf;
			memset(&pa, 0, sizeof(pa));

			//printf_debug2("pslink->SeqMaxSend=%d, pslink->SeqCheckSend=%d\n", pslink->SeqMaxSend, pslink->SeqCheckSend);

			//检查间隔时间不到就返回不检查
			gettimeofday (pTimeCur , NULL);
			if (GET_MS_DIFF(*pTimeCur, pslink->timeoutSend) < pslink->timeoutCheckLack.tv_usec / 1000)
			{
				return EX_UDP_ERROR;
			}

			//printf_debug2("timeCur-timeoutSend=%ld\n", (timeCur.tv_sec - pslink->timeoutSend.tv_sec)*1000000+timeCur.tv_usec - pslink->timeoutSend.tv_usec);
			//需要做检查
			pslink->timeoutSend = *pTimeCur;
			//gettimeofday (&pslink->timeoutSend , NULL);

			pa.head.type = SEQ_CHECK;
			pa.head.seq = pslink->SeqMaxSend;
			pa.head.seqCheck = (pslink->SeqCheckSend + 1) % MAX_PACKET_SEQ;
			pa.len = LEN_PACKET_HEAD + sizeof(struct timeval);
			SendMsg(pslink, &pa, pa.len);
			ctlIntervalTime(sts);
		}
	}
	return EX_UDP_OK;
}

//接收到队列序号检查的消息时回复
static int SeqCheckAck(st_sock* sts, s_link* pslink, s_packet_all* pa)
{
	if(pslink->linkState < 2)
	{
		return EX_UDP_ERROR;
	}
	printf_debug2("INFO: seqCheck=%d, SeqRead=%d, SeqMaxSend=%d\r\n", pa->head.seqCheck, pslink->SeqRead, pa->head.seq);
	if (pslink->SeqRead == pa->head.seq || CMP_SEQ_ORDER(pa->head.seqCheck, pslink->SeqRead, pa->head.seq))
	{
		pslink->SeqReadMax = pa->head.seq;

		if (pa->head.seqCheck != pslink->SeqRead)
		{
			USHORT seq = pa->head.seq;
			//pslink->SeqReadMax = pa->head.seq;
			//报告序号,此时带有时间值
			pa->head.type = SEQ_REPORT;
			pa->head.seq = (pslink->SeqRead == 0 ? MAX_PACKET_SEQ -1 : pslink->SeqRead-1);
			SendMsg(pslink, pa, pa->len);

			pa->head.seq = seq;
		}

		//SeqReport(pslink, pa->len);
		//sem_post(&g_pSock[pslink->socket]->semListen);
		//if (pslink->paArrayRecv[pslink->SeqRead % MAX_PACKET_NUMBER].head.seq == pslink->SeqRead
		//	&& pslink->paArrayRecv[pslink->SeqRead % MAX_PACKET_NUMBER].len != 0)
		{
			if (sts->msgid != -1)
			{
				//g_pSock[pslink->socket]->idRecvStart = id;
				add_listRead(sts, pslink);
			}
			//SendToMsgQueue(pslink->id);
			sem_post(&pslink->semRecvList);
			//return 0;
		}
		
		return EX_UDP_OK;
	}

	//乱序会造成报告异常的错误
	printf_debug2("Error: seqCheck=%d, SeqRead=%d, SeqMaxSend=%d\r\n", pa->head.seqCheck, pslink->SeqRead, pa->head.seq);

	return EX_UDP_ERROR;

// 	//sem_post(&pslink->semRecvList);
// 	SeqLack(id, pa);
	
	return EX_UDP_OK;
}

//处理接收到的USER_DATA正常数据报文
static int UserDataRecv(st_sock* sts, s_link* pslink, s_packet_all* pa)
{
	if(pslink->linkState < 2)
	{
		return EX_UDP_ERROR;
	}
// 	pslink->RecvRepeatCount++;
// 	if (pslink->RecvRepeatCount % 20000 == 0)
// 	{
// 		LOG_WRITE_POS(LOG_ERR, "repeated2 conut=%d, seq=%d, seq=%d\n", pslink->RecvRepeatCount, pslink->paArrayRecv[pa->head.seq % MAX_PACKET_NUMBER].head.seq, pa->head.seq);
// 	}

	//判断是否是需要的新包
	if(CMP_SEQ(pa->head.seq, pslink->SeqRead) || pa->head.seq == pslink->SeqRead)
	{
#if CHECKSNUMMAT_ENABLE
		//比较校验和
		if (pa->head.checksum != checksummat(pa))
		{
			printf_debug2("pa->head.checksum=%d  checksummat(pa)=%d \n", pa->head.checksum, checksummat(pa));
			return EX_UDP_ERROR;
		}
#endif
		//判断此序号内是否有未读报文
		if (pslink->paArrayRecv[pa->head.seq % MAX_PACKET_NUMBER].head.seq != pa->head.seq
			|| pslink->paArrayRecv[pa->head.seq % MAX_PACKET_NUMBER].len == 0)
		{
			//将报文写入
			static int i = 0;
			if (i == 0)
			{
				i++;
				printf_debug6("UserDataRecv pa.head.seq=%d, pa.head.seqCheck=%d\r\n", pa->head.seq, pa->head.seqCheck);
			}
			pslink->paArrayRecv[pa->head.seq % MAX_PACKET_NUMBER] = *pa;
			if (pa->head.seq == pslink->SeqRead)
			{
				if (sts->msgid != -1)
				{
					//g_pSock[pslink->socket]->idRecvStart = id;
					add_listRead(sts, pslink);
				}
				//SendToMsgQueue(pslink->id);
				sem_post(&pslink->semRecvList);
			}
			//pslink->RecvCount++;
		}
// 		else
// 		{
// 			pslink->RecvRepeatCount++;
// 			if (pslink->RecvRepeatCount % 10000 == 0)
// 			{
// 				LOG_WRITE_POS(LOG_ERR, "repeated2 conut=%d, seq=%d, seq=%d\n", pslink->RecvRepeatCount, pslink->paArrayRecv[pa->head.seq % MAX_PACKET_NUMBER].head.seq, pa->head.seq);
// 			}
// 		}

		return EX_UDP_OK;
	}
// 	else
// 	{
// 		LOG_WRITE_POS(LOG_ERR, "[UserDataRecv] pa->head.seq=%d, pslink->SeqRead=%d\r\n", pa->head.seq, pslink->SeqRead);
// 	}
	return EX_UDP_ERROR;
}

//进程退出时会自动调用此函数
static void sigroutine(int dunno)
{ //其中dunno将会得到信号的值
//  	s_link* pslink = (s_link*)sts->pLinkIDArray[1];
// 	if (printf_RecvMsg && pslink)
// 	{
// 		LOG_WRITE_POS(LOG_ERR, "SeqCheckSend=%d, SeqRead=%d, SeqMaxSend=%d\r\n", pslink->SeqCheckSend, pslink->SeqRead, pslink->SeqMaxSend);
// 		LOG_WRITE_POS(LOG_ERR, "Get a signal -- %d\r\n ", dunno);
// 	}
// 	while (pslink != (s_link*)list_end(&g_list_head))
// 	{
// 		LOG_WRITE_POS(LOG_ERR, "SeqCheckSend=%d, SeqRead=%d, SeqMaxSend=%d\r\n", pslink->SeqCheckSend, pslink->SeqRead, pslink->SeqMaxSend);
// 		SeqReport(pslink->id, LEN_PACKET_HEAD);
// 		pslink = (s_link*)pslink->list.next;
// 	}
	exit(1);
}

#endif

#ifndef THREAD_FUNC
#define THREAD_FUNC
//接收线程
#ifdef WIN32
static DWORD WINAPI thread_recv(void* p)
#else
static void* thread_recv(void* p)
#endif
{
	st_sock* sts = (st_sock*)p;
	const int sock = sts->socket;
	int ret = 0;
	int id = 0;
	const int recvLen = MAX_PACKET_SIZE;
	s_link* pslink = NULL; 
	struct sockaddr_in addrRecv ={0};
	s_packet_all pa;
	memset(&pa, 0, sizeof(pa));
	sts->threadState++;

	while(1)
	{
THREAD_RECV_WORK_START:
		memset(&pa, 0, LEN_PACKET);
		
		ret = recvfrom(sock, (char*)&pa, recvLen, 0, (struct sockaddr*)&addrRecv, &g_lensockaddr);
		if (sts->linkState != 1)
		{
			//LOG_WRITE_POS(LOG_ERR, "ERROR: sock=%d\r\n", sock);
			sts->threadState--;
			return 0;
		}
		//LOG_WRITE_POS(LOG_ERR, "type=%s\r\n", PACKET_TYPE[pa.head.type]);
		if (ret == SOCKET_ERROR)
		{
			printf_debug2("thread_recv:recvfrom: type=%d, len=%d, seq=%d\n", pa.head.type, pa.len, pa.head.seq);
			continue;
		}
		pa.len = ret;
		if (pa.len < LEN_PACKET_HEAD || pa.len > MAX_PACKET_SIZE 
			|| pa.head.type < LINK_SYN || pa.head.type > LINK_FIN
			|| pa.head.seq >= MAX_PACKET_SEQ)
		{
			printf_debug2("ERROR: thread_recv:recvfrom: type=%d, len=%d, seq=%d\n", pa.head.type, pa.len, pa.head.seq);
			continue;
		}//else 
		//printf_RecvMsg("thread_recv:recvfrom: type=%d, len=%d, seq=%d, port=%d\n", pa.head.type, ret, pa.head.seq, htons(addrRecv.sin_port));
		// 		static ULONG ul = 1;
		// 		if (ul++ % 10000 == 0)
		// 		{
		// 			LOG_WRITE_POS(LOG_ERR, "thread_recv:recvfrom: type=%d, len=%d, seq=%d\n", pa.head.type, ret, pa.head.seq);
		// 		}

// 		if(pa.head.type == LINK_SYN_ACK)
// 		LOG_WRITE_POS(LOG_ERR, "pa.head.checksum=%d, sts->port=%d\r\n", pa.head.checksum, sts->port);

		if (pa.head.type == LINK_SYN && pa.head.checksum != EX_UDP_VER)
		{
			LOG_WRITE_POS(LOG_ERR, "Version not matched! EX_UDP_VER: local=%d, from=%d, address=%s, port=%d\r\n", EX_UDP_VER, pa.head.checksum, inet_ntoa(addrRecv.sin_addr), htons(addrRecv.sin_port));
			continue;
		}

		//分配或者查找数据队列
// 		sem_wait(&sts->semLink);
// 		if (sts->linkState != 1)
// 		{
// 			return 0;
// 		}
		pslink = get_link(sts, addrRecv.sin_addr.s_addr, htons(addrRecv.sin_port), pa.head.type);
//		sem_post(&sts->semLink);

		if (NULL == pslink)
		{
			continue;
		}
		
#if printf_RecvMsg
		if (USER_DATA != pa.head.type)
		{
			log_write(LOG_DEBUG, "[RecvMsg]: sock=%d, id=%d, type=%s, len=%d, seqCheck=%d, pslink->seqRead=%d, seq=%d, Recv=%s, port=%d\n", 
				sock, pslink->id, PACKET_TYPE[pa.head.type], ret, pa.head.seqCheck, pslink->SeqRead, pa.head.seq, inet_ntoa(addrRecv.sin_addr), htons(addrRecv.sin_port));
		}
#endif			

		//本地为半连接状态,对端为连接状态
		if (pslink->linkState == 1 && pa.head.state == 2)
		{
			if (pslink->paArrayRecv[1].head.seq == 1)
			{
				memset(pslink->paArrayRecv, 0, sizeof(pslink->paArrayRecv));
			}
			pslink->paArrayRecv[0].head.seq = 1;
			pslink->SeqRead = pa.head.seqCheck;
			pslink->linkState = 2;
		}

		pslink->timeout.tv_sec = 0;

		switch(pa.head.type)
		{
		case LINK_SYN:
			//对端为非连接状态
			if (pa.head.state < 2)
			{
				if (pslink->paArrayRecv[1].head.seq == 1)
				{
					memset(pslink->paArrayRecv, 0, sizeof(pslink->paArrayRecv));
				}
				pslink->paArrayRecv[0].head.seq = 1;
				pslink->SeqRead = pa.head.seqCheck;
			}

			//本地为非连接状态
			if(pslink->linkState < 2)
			{
				printf_debug7("LINK_SYN pa.head.seq=%d, pa.head.seqCheck=%d\r\n", pa.head.seq, pa.head.seqCheck);
				if (pa.head.state == 2)
				{
					pslink->linkState = 0;
				}else
				{
					pslink->linkState = 1;
				}
				
				pslink->SeqRead = pa.head.seqCheck;
			}
			pa.head.type = LINK_SYN_ACK;
			pa.head.seqCheck = (pslink->SeqCheckSend + 1) % MAX_PACKET_SEQ;
			//pa.head.checksum = htons(addrRecv.sin_port);
			SendMsg(pslink, &pa, LEN_PACKET_HEAD);
			break;
		case LINK_SYN_ACK:
			//对端为非连接状态
			if (pa.head.state < 2)
			{
				if (pslink->paArrayRecv[1].head.seq == 1)
				{
					memset(pslink->paArrayRecv, 0, sizeof(pslink->paArrayRecv));
				}
				pslink->paArrayRecv[0].head.seq = 1;
				pslink->SeqRead = pa.head.seqCheck;
 				pa.head.type = LINK_ACK;
				pa.head.seqCheck = (pslink->SeqCheckSend + 1) % MAX_PACKET_SEQ;
 				SendMsg(pslink, &pa, LEN_PACKET_HEAD);
			}

			//本地为非连接状态
			if(pslink->linkState < 2)
			{
				printf_debug7("LINK_SYN_ACK pa.head.seq=%d\r\n", pa.head.seqCheck);
				pslink->linkState = 2;
				pslink->SeqRead = pa.head.seqCheck;
			}
			
			if (GET_MS_DIFF(sts->time_listen, pslink->timeout) >= LISTEN_LINK_INTERVAL)
			{
				sem_post(&sts->semListen);
			}
			sem_post(&pslink->semSendList);
			break;
		case LINK_ACK:
			//本地为初始化
			if(pslink->linkState < 2)
			{
				printf_debug7("LINK_ACK pa.head.seq=%d\r\n", pa.head.seq);
				pslink->linkState = 2;
			}
			break;
		case USER_DATA:
			UserDataRecv(sts, pslink, &pa);
			break;
		case SEQ_CHECK:
			SeqCheckAck(sts, pslink, &pa);
			break;
		case SEQ_REPORT:
			SeqReportAck(pslink, &pa);
			break;
		case SEQ_LACK:
			SeqLackAck(pslink, &pa);
			break;
		case LINK_FIN:
			printf_debug2("LINK_FIN id=%d\r\n", pslink->id);

			if (sts->msgid >= 0)
			{
                int ret = 0;
                st_msg_packet msg;
                msg.type = MSGTYPE_UNCONNECTED;
                msg.id = (pslink->id | pslink->socket << 16);
                ret = msgsnd(sts->msgid, &msg, sizeof(msg.id), 0);
#ifndef WIN32
                if (-1 == ret)
                {
                    LOG_WRITE_POS(LOG_CRIT, "%s!!!\r\n", strerror(errno));
                }
#endif
            }
			sem_wait(&sts->semLink);
			del_list(sts, pslink);
			sem_post(&sts->semLink);
			break;
		default:
			break;
		}
	}
	sts->threadState--;
	return 0;
}

//定时监听线程
#ifdef WIN32
static DWORD WINAPI thread_listen(void* p)
#else
static void* thread_listen(void* p)
#endif
{
	st_sock* sts = (st_sock*)p;
	const int sock = sts->socket;
	long timeVal;
	int iRet = 0;
	s_link* pslink = NULL;
	s_link* pslink_t = NULL;
	s_packet_all pa;

	sts->time_send.tv_sec = 0;
	sts->time_send.tv_usec = 0;
	memset(&pa, 0, sizeof(pa));

	while (1)
	{
		gettimeofday (&sts->time_listen , NULL);
		if (!list_empty(&sts->listFreeHead))
		{//延迟释放指针
			do{
				pslink = (s_link*)list_begin(&sts->listFreeHead);
				if (pslink == (s_link*)list_end(&sts->listFreeHead))
				{
					break;
				}
				if (GET_MS_DIFF(sts->time_listen, pslink->timeoutFree) > FREE_MEM_INTERVAL)
				{
					list_erase(&pslink->list);
					free(pslink);
				}else
				{
					break;
				}
			}while(1);
			timeVal = LISTEN_INTERVAL/1000;
		}else if (GET_MS_DIFF(sts->time_listen, sts->time_send) < LISTEN_LINK_INTERVAL)
		{
			timeVal = LISTEN_INTERVAL/1000;
		}else if (list_empty(&sts->list))
		{
			//sem_wait(&sts->semListen);
			//continue;
			timeVal = HOUR_TIME;
		}else
		{
			timeVal = LISTEN_LINK_INTERVAL;
		}

		//LOG_WRITE_POS(LOG_ERR, "timeVal=%ld\r\n", timeVal);
		//iRet = sem_timedwait(&sts->semListen, &timeVal);//-1是超时，0是通过
		SEM_TIMED_WAIT(sts->semListen, timeVal, iRet);
		if (sts->linkState != 1)
		{
			while (sts->threadState != 0)
			{
				usleep(100000);//等待其它线程退出
			}
			usleep(1000000);//等待用户的接收发送都返回
			//list_erase(&sts->list);
			do{
				pslink = (s_link*)list_begin(&sts->listFreeHead);
				if (pslink == (s_link*)list_end(&sts->listFreeHead))
				{
					break;
				}
				list_erase(&pslink->list);
				free(pslink);
			}while(1);
			if (sts != NULL)
			{
				free(sts);
				sts = NULL;
			}
			return 0;
 		}

		pslink = (s_link*)list_begin(&sts->list);
		while((s_link*)list_end(&sts->list) != pslink)
		{
			if (pslink == (s_link*)&sts->listFreeHead || pslink->linkState == -1)
			{
				pslink = sts->nextListenLink;
				continue;
			}
			//长时间无收发数据,就做心跳检测,看连接是否还正常
			if (pslink->timeout.tv_sec == 0)
			{
				//pslink->timeout = sts->time_listen;
				gettimeofday (&pslink->timeout , NULL);
				pslink->timeout_times = 1;
			}else
			{
				if (GET_MS_DIFF(sts->time_listen, pslink->timeout) >= LISTEN_LINK_INTERVAL * pslink->timeout_times)
				{
					printf_debug2("id=%d, sts->time_listen=%ld   pslink->timeout.tv_sec=%ld!\n", pslink->id, sts->time_listen.tv_sec, pslink->timeout.tv_sec);
					if (pslink->timeout_times > LISTEN_LINK_TIMES)
					{
					    if (sts->msgid >= 0)
					    {
                            int ret = 0;
                            st_msg_packet msg;
                            msg.type = MSGTYPE_UNCONNECTED;
                            msg.id = (pslink->id | pslink->socket << 16);
                            ret = msgsnd(sts->msgid, &msg, sizeof(msg.id), 0);
#ifndef WIN32
                            if (-1 == ret)
                            {
                                LOG_WRITE_POS(LOG_CRIT, "%s!!!\r\n", strerror(errno));
                            }
#endif
					    }
						log_write(LOG_NOTICE, "Connect timeout !  id = %d\n", pslink->id);
						printf_debug2("Connect timeout !  id = %d\n", pslink->id);
						printf_debug2("SeqRead=%d, SeqMaxSend=%d, SeqCheckSend=%d\n", pslink->SeqRead, pslink->SeqMaxSend, pslink->SeqCheckSend);

						//做断开清除连接数据操作
						pslink_t = (s_link*)pslink->list.next;
						sem_wait(&sts->semLink);
						del_list(sts, pslink);
						sem_post(&sts->semLink);

						
						pslink = pslink_t;
						continue;
						//break;
					}
					if (pslink->linkState == 2)
					{
						printf_debug7("thread_listen pslink->SeqCheckSend=%d, pslink->SeqMaxSend=%d\r\n", pslink->SeqCheckSend, pslink->SeqMaxSend);
						pa.head.type = LINK_SYN;
						pa.head.checksum = EX_UDP_VER;
						pa.head.seqCheck = (pslink->SeqCheckSend + 1) % MAX_PACKET_SEQ;
						pa.head.seq = pslink->SeqMaxSend;
						SendMsg(pslink, &pa, LEN_PACKET_HEAD);
					}
					pslink->timeout_times++;
				}
			}

			//发送检查序号
			SeqCheck(sts, pslink);
			pslink = (s_link*)pslink->list.next;
		}
	}

	return NULL;
}


//发报文到消息队列线程
#ifdef WIN32
static DWORD WINAPI thread_RecvToMsg(void* p)
#else
static void* thread_RecvToMsg(void* p)
#endif
{
	st_sock* sts = (st_sock*)p;
	//const int sock = sts->socket;
	st_idList *psIDList = NULL;
	int id = 0;
	s_link *pslink = NULL;
	sts->threadState++;

	while (1)
	{
		sem_wait(&sts->semRecvToMsg);
		if (sts->linkState != 1)
		{
			/*LOG_WRITE_POS(LOG_ERR, "ERROR: this socket is closed ! sock=%d\r\n", sock);*/
			sts->threadState--;
			return 0;
		}
		while (list_end(&sts->listIDMsgReadHead) != (clist*)(psIDList = (st_idList *)list_begin(&sts->listIDMsgReadHead)))
		{
			id = psIDList->id;
			pslink = sts->pLinkIDArray[id & MAX_LINK_COUNT];
			if(NULL == pslink)
			{
				continue;
			}
			SendToMsgQueue(sts, pslink);
		}
	}
	sts->threadState--;
	return NULL;
}
#endif

#ifndef EXTERN_INTERFACES
#define EXTERN_INTERFACES

//功  能: 设置监听连接情况的时间间隔和次数
//参  数: Milliseconds：是间隔时间(毫秒)
//		  times: 间隔次数
//返回值: 成功返回EX_UDP_OK
//		  错误返回EX_UDP_ERROR
int setListenTime(long Milliseconds, long times)
{
	if (Milliseconds < LISTEN_INTERVAL / 1000)
	{
		LOG_WRITE_POS(LOG_ERR, "[setListenTime] ERROR: Parameter error ! Milliseconds=%ld, min is %d\r\n", Milliseconds, LISTEN_INTERVAL/1000);
		return EX_UDP_ERROR;
	}
	if (times < 1)
	{
		LOG_WRITE_POS(LOG_ERR, "[setListenTime] ERROR: Parameter error ! times=%ld, min is %d\r\n", times, 1);
		return EX_UDP_ERROR;
	}
	LISTEN_LINK_INTERVAL = Milliseconds;
	LISTEN_LINK_TIMES = times;
	return EX_UDP_OK;
}

//功  能: 设置堵塞模式、异步、超时模式，以及堵塞超时时间
//参  数: Milliseconds：是毫秒
//		  Milliseconds < 0，为堵塞模式
//		  Milliseconds == 0，为非堵塞模式
//		  Milliseconds > 0，为超时等待模式
//返回值: 
void setDefaultWaitTime(long Milliseconds)
{
	g_tsWaitTime = Milliseconds;
}

//功  能: 申请socket，并指定本地接收端口
//参  数: recvPort：表示端口，如果本地端口指定为0，就为客户端，随机分配端口，必须主动先去连接,再开始通信收发数据	  
//返回值: 成功返回sock
//        错误返回EX_UDP_ERROR
int exSocket(USHORT recvPort)
{
	int err = 0;
	pthread_t ntid;
	static char flag = 0;
	int sock = SOCKET_ERROR, i = 0;
	st_sock* sts = NULL;

#ifdef WIN32
	{
		static WSADATA wsaData;
		err = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (err != 0) 
		{
			/* Tell the user that we could not find a usable */
			/* Winsock DLL.                                  */
			printf("WSAStartup failed with error: %d\n", err);
			exit(0);//return EX_UDP_ERROR;//
		}
	}
#endif

	if (flag == 0)
	{
		flag = 1;

		err = sem_init(&g_semLink, 0, 1);
		if(err != 0)  
		{
			LOG_WRITE_POS(LOG_ERR, "sem_init failed\n");  
			exit(EXIT_FAILURE);  
		}
// 		memset(&g_logExudp, 0, sizeof(S_LOG));
// 		log_new(&g_logExudp, EX_UDP_LOG_LEVEL, " EX_UDP "); // 创建日志模块变量。

// #ifdef linux
// 		signal(SIGHUP, sigroutine); //* 下面设置三个信号的处理方法
// 		signal(SIGINT, sigroutine);
// 		signal(SIGQUIT, sigroutine);
// 		signal(SIGKILL, sigroutine);
// 		signal(SIGTERM , sigroutine);
// 		signal(SIGSTOP , sigroutine);
// #endif
	}

	sem_wait(&g_semLink);

	sock=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == SOCKET_ERROR)
	{
		LOG_WRITE_POS(LOG_ERR, "can't create socket: %d\n", sock);
		exit(EXIT_FAILURE);
	}
	
	if (sock > MAX_SOCKET_NUM || g_pSock[sock] != NULL)
	{
		LOG_WRITE_POS(LOG_ERR, "ERROR: More than the maximum number of socket ! sock=%d, or exist.\r\n", sock);
#ifdef WIN32
		closesocket(sock);
#else
		close(sock);
#endif
		sem_post(&g_semLink);
		return EX_UDP_ERROR;
	}

	if (recvPort != 0)
	{
		struct sockaddr_in addrSrv = {0};
		addrSrv.sin_addr.s_addr = htonl(INADDR_ANY);
		addrSrv.sin_family=AF_INET;
		addrSrv.sin_port=htons(recvPort);
		err = bind(sock,(struct sockaddr*)&addrSrv,g_lensockaddr);
		if(SOCKET_ERROR == err)
		{
			LOG_WRITE_POS(LOG_ERR, "bind failed!");
			//sem_post(&g_semLink);
			exit(EXIT_FAILURE);//return EX_UDP_ERROR;
		}
	}
	
	sts = g_pSock[sock] = (st_sock*)malloc(LEN_S_SOCK);
	if (NULL == sts)
	{
		LOG_WRITE_POS(LOG_ERR, "ERROR: malloc failed -> s_link !\n");
		exit(1);
	}
	memset(sts, 0, LEN_S_SOCK);
	sts->socket = sock;
	sts->tsWaitTime = g_tsWaitTime;
	sts->connectPriority = 0;
	sts->msgid = -1;
	sts->linkState = 1;
	list_init(&sts->list);
	list_init(&sts->listFreeHead);
	sts->nextListenLink = (s_link*)&sts->list;

	list_init(&sts->listIDAllocationHead);
	for (i = 0; i <= MAX_LINK_COUNT; i++)
	{
		sts->stIDListAllocation[i].id = i;
		list_push_back(&sts->listIDAllocationHead, &sts->stIDListAllocation[i].list);
	}

	err = sem_init(&sts->semLink, 0, 1);
	if(err != 0)  
	{
		LOG_WRITE_POS(LOG_ERR, "sem_init failed\n");  
		exit(EXIT_FAILURE);  
	}

	err = sem_init(&sts->semAccept, 0, 0);
	if(err != 0)  
	{
		LOG_WRITE_POS(LOG_ERR, "sem_init failed\n");  
		exit(EXIT_FAILURE);  
	} 
	
	err = sem_init(&sts->semListen, 0, 0);
	if(err != 0)  
	{
		LOG_WRITE_POS(LOG_ERR, "sem_init failed\n");  
		exit(EXIT_FAILURE);  
	}

	err = pthread_create(&ntid, NULL, thread_recv, (void*)sts);
	if (err != 0)
	{
		LOG_WRITE_POS(LOG_ERR, "can't create thread: %s\n", strerror(err));
		exit(EXIT_FAILURE);
	}

	err = pthread_create(&ntid, NULL, thread_listen, (void*)sts);
	if (err != 0)
	{
		LOG_WRITE_POS(LOG_ERR, "can't create thread: %s\n", strerror(err));
		exit(EXIT_FAILURE);
	}
	
	log_write(LOG_NOTICE, "EX_UDP_VER=%d, VER_TIME=%s %s\n", EX_UDP_VER, __DATE__, __TIME__);

	sem_post(&g_semLink);
	return sock;
}


//功  能: 设置消息队列模式，此模式下，不需要exAccept，接收的所有报文在消息队列中获取
//参  数: sock：是exSocket的返回值
//        msgid: 要接收消息的消息队列ID
//返回值: 成功返回EX_UDP_OK
//		  错误返回EX_UDP_ERROR
int setMsgQueue(int sock, int msgid)
{
	st_sock* sts = g_pSock[sock & MAX_SOCKET_NUM];
	int err = 0, i = 0;
	pthread_t ntid;

	ASSERT_SOCK;
	if (msgid < 0)
	{
		return EX_UDP_ERROR;
	}
	//消息队列模式时，必须为非堵塞模式，这样接收的同时可以发送，如果堵塞会导致接收的消息队列满，而写入消息队列时会堵塞，导致内部核心接收线程堵塞，无法再继续通信，接收任何报文
	//sts->tsWaitTime = 0;
	sts->msgid = msgid;

	list_init(&sts->listIDMsgReadHead);
	for (i = 0; i <= MAX_LINK_COUNT; i++)
	{
		list_init(&sts->stIDListMsgRead[i].list);
		sts->stIDListMsgRead[i].id = i;
	}

	err = sem_init(&sts->semRecvToMsg, 0, 0);
	if(err != 0)  
	{
		LOG_WRITE_POS(LOG_ERR, "sem_init failed\n");  
		exit(EXIT_FAILURE);  
	} 

	err = sem_init(&sts->semListRead, 0, 1);
	if(err != 0)  
	{
		LOG_WRITE_POS(LOG_ERR, "sem_init failed\n");  
		exit(EXIT_FAILURE);  
	} 

	err = pthread_create(&ntid, NULL, thread_RecvToMsg, (void*)sts);
	if (err != 0)
	{
		LOG_WRITE_POS(LOG_ERR, "can't create thread: %s\n", strerror(err));
		exit(EXIT_FAILURE);
	}

	return EX_UDP_OK;
}

//功  能: 被动等待连接函数
//参  数: sock：是exSocket的返回值
//返回值: 成功返回id
//		  错误返回EX_UDP_ERROR
int exAccept(int sock)
{
	s_link* pslink = NULL;
	st_sock* sts = g_pSock[sock & MAX_SOCKET_NUM];
	ASSERT_SOCK;
	while (NULL == (pslink = GetLinkAccept(sts)))
	{
		sem_wait(&sts->semAccept);
		ASSERT_SOCK;
		while (sts->connectPriority == 1)
		{
			sem_post(&sts->semAccept);
			sem_wait(&sts->semAccept);
			ASSERT_SOCK;
		}
	}
	printf_debug2("pslink=%x, pslink->SeqRead=%d\n", pslink, pslink->SeqRead);
	return (pslink->id | sock << 16);
}


//功  能: 主动连接目地端函数
//参  数: sock：是exSocket的返回值
//		  strDstIP：目地端IP地址
//		  DstPort：目地端端口
//返回值: 成功返回id
//		  错误返回EX_UDP_ERROR
int exConnect(int sock, const char* strDstIP, USHORT DstPort)
{
	int iRet = 0;
	s_link* pslink = NULL;
	int times = MAX_LINK_TIMES;
	struct sockaddr_in addrSrv = {0};
	long timeVal = MAX_LINK_INTERVAL;
	s_packet_all pa;
	st_sock* sts = g_pSock[sock & MAX_SOCKET_NUM];
	ASSERT_SOCK;
	memset(&pa, 0, sizeof(pa));
	pa.head.type = LINK_SYN;
	pa.head.checksum = EX_UDP_VER;
	pa.head.seq = 1;
	pa.head.seqCheck = 1;
	pa.len = LEN_PACKET_HEAD;
	addrSrv.sin_family=AF_INET;
	addrSrv.sin_port=htons(DstPort);
	if(-1 == (addrSrv.sin_addr.s_addr = inet_addr(strDstIP)))
	{
		return EX_UDP_ERROR;
	}
	
	sts->connectPriority = 1;
	while (times-- > 0)
	{
		sendto(sock,(const char*)&pa, LEN_PACKET_HEAD, 0, (struct sockaddr*)&addrSrv, g_lensockaddr);
		SEM_TIMED_WAIT(sts->semAccept, timeVal, iRet);
		ASSERT_SOCK;
		printf_debug2("SEM_TIMED_WAIT sock=%d, iRet=%d\r\n", sock, iRet);
		if (iRet == 0)
		{
			sem_wait(&sts->semLink);
			//pslink = get_connect(sock, addrSrv.sin_addr.s_addr, DstPort);
			pslink = (s_link*)list_begin(&sts->list);
			while ((s_link*)list_end(&sts->list) != pslink)
			{
				if (/*pslink->accept == 0 && */pslink->ip == addrSrv.sin_addr.s_addr && pslink->port == DstPort)
				{
					pslink->accept = 1;
					printf_debug2("pslink=%x, pslink->SeqRead=%d\n", pslink, pslink->SeqRead);
					sem_post(&sts->semLink);
					sts->connectPriority = 0;
					sem_post(&sts->semAccept);
					return (pslink->id | sock << 16);
				}
				pslink = (s_link*)pslink->list.next;
			}
			sem_post(&sts->semLink);
		}
	}
	sts->connectPriority = 0;
	sem_post(&sts->semAccept);

	return EX_UDP_ERROR;
}

//功  能: 设置单个ID的堵塞、异步、超时模式，以及堵塞超时时间
//参  数: id：代表一个连接，是exConnect或者exAccept的返回值
//        Milliseconds：是毫秒
//		  Milliseconds < 0，为堵塞模式
//		  Milliseconds == 0，为非堵塞模式
//		  Milliseconds > 0，为超时等待模式
//返回值: 错误返回EX_UDP_ERROR
int setWaitTime(int id, long Milliseconds)
{
	GET_LINK_FOR_ID;
	pslink->tsWaitTime = Milliseconds;
	return EX_UDP_OK;
}

//功  能: 接收报文函数
//参  数: id：代表一个连接，是exConnect或者exAccept的返回值
//        buf：接收数据的指针
//返回值: 成功：返回接收到的数据长度
//		        异步和超时模式时，返回值为0，表示无数据
//        错误：返回EX_UDP_ERROR，表示连接断开或者出错，必须做处理
int exRecv(int id, char* buf)
{
	int iRet = 0;
	USHORT len = 0;
	USHORT usRead_t = 0;
	GET_LINK_FOR_ID;
	if (NULL == buf)
	{
		LOG_WRITE_POS(LOG_ERR, "Parameter error ! buf=%x\r\n", buf);
		return EX_UDP_ERROR;
	}

// 	LOG_WRITE_POS(LOG_DEBUG, "[exRecv] start id=%d, SeqRead=%d, pslink->SeqRead=%d, len=%d, pslink->tsWaitTime=%ld\r\n", 
// 		id, pslink->paArrayRecv[pslink->SeqRead % MAX_PACKET_NUMBER].head.seq, pslink->SeqRead, pslink->paArrayRecv[pslink->SeqRead % MAX_PACKET_NUMBER].len, pslink->tsWaitTime);
	sem_wait(&pslink->semRecvListMutex);

	while (pslink->paArrayRecv[pslink->SeqRead % MAX_PACKET_NUMBER].head.seq != pslink->SeqRead
		|| pslink->paArrayRecv[pslink->SeqRead % MAX_PACKET_NUMBER].len == 0)
	{
		//printf_debug2("exRecv:1 SeqRead=%d, head.seq=%d\n", pslink->SeqRead, pslink->paArrayRecv[pslink->SeqRead % MAX_PACKET_NUMBER].head.seq);
		SeqReport(pslink);

		if (pslink->tsWaitTime < 0)
		{
			sem_wait(&pslink->semRecvList);
			ASSERT_ID_RECV;
		}else if(pslink->tsWaitTime == 0)
		{
			sem_post(&pslink->semRecvListMutex);
			return 0;
		}else
		{
			SEM_TIMED_WAIT(pslink->semRecvList, pslink->tsWaitTime, iRet);
			ASSERT_ID_RECV;
			if (iRet == -1)
			{//没有报文，而且超时就返回0
				sem_post(&pslink->semRecvListMutex);
				return 0;
			}
		}
		//printf_debug2("exRecv:2 SeqRead=%d\n", pslink->SeqRead);
	}

	len = pslink->paArrayRecv[pslink->SeqRead % MAX_PACKET_NUMBER].len-LEN_PACKET_HEAD;
	if (len > MAX_PACKET_DATA_SIZE)
	{
		LOG_WRITE_POS(LOG_ERR, "[exRecv] ERROR: len=%d, SeqRead=%d\n", len, pslink->SeqRead);
		sem_post(&pslink->semRecvListMutex);
		//return 0;
		exit(0);
	}

	memcpy(buf, pslink->paArrayRecv[pslink->SeqRead % MAX_PACKET_NUMBER].buf, len);

	usRead_t = pslink->SeqRead;
	pslink->paArrayRecv[pslink->SeqRead % MAX_PACKET_NUMBER].len = 0;
	//pslink->paArrayRecv[pslink->SeqRead % MAX_PACKET_NUMBER].head.seq++;
	//最后读取序号+1
	pslink->SeqRead = (pslink->SeqRead+1) % MAX_PACKET_SEQ;
	sem_post(&pslink->semRecvListMutex);

	return len;
}

//功  能: 发送报文函数
//参  数: id：代表一个连接，是exConnect或者exAccept的返回值
//        buf：接收数据的指针
//        len：发送数据的长度
//返回值: 成功：返回发送出的数据长度
//              堵塞模式时，返回值为0，表示系统sendto函数返回错误，对端可能已经断开，为了继续接收其它连接的报文，不要在向此连接发包
//		        异步和超时模式时，返回值为0，表示数据没有发送成功，可以重新发送
//        错误：返回EX_UDP_ERROR，表示连接断开或者出错，必须做处理
int exSend(int id, char* buf, int len)
{
	int iRet = 0;
	s_packet_all* pa = NULL;
	int len_send = len;
	int len_t = len_send;
	GET_LINK_FOR_ID;

	if (NULL == buf)
	{
		LOG_WRITE_POS(LOG_ERR, "Parameter error ! buf=%x\r\n", buf);
		return EX_UDP_ERROR;
	}

	if (len <= 0 || len > MAX_PACKET_DATA_SIZE)
	{
		LOG_WRITE_POS(LOG_ERR, "ERROR: Send length=%d, Should be between 1-%d !\r\n", len, MAX_PACKET_DATA_SIZE);
		return EX_UDP_ERROR;
	}

	//if (pslink->SeqMaxSend == 4094 && pslink->SeqCheckSend == 0)usleep(99999999);
	sem_wait(&pslink->semSendListMutex);

	while (len_send > 0)
	{
		if(pslink->PrioritySendLack == 0 
			&& ((pslink->SeqMaxSend+1) % MAX_PACKET_NUMBER) != (pslink->SeqCheckSend % MAX_PACKET_NUMBER))
		{
			if (len_send >= MAX_PACKET_DATA_SIZE)
			{
				len_t = MAX_PACKET_DATA_SIZE;
			}else
			{
				len_t = len_send;
			}
			
			pa = &pslink->paArraySend[pslink->SeqMaxSend % MAX_PACKET_NUMBER];
			pa->head.seq = pslink->SeqMaxSend;
			pa->head.seqCheck = (pslink->SeqCheckSend + 1) % MAX_PACKET_SEQ;
			pa->head.type = USER_DATA;

			pa->len = LEN_PACKET_HEAD+len_t;
			memcpy(pa->buf, buf, len_t);

#if CHECKSNUMMAT_ENABLE
			pa->head.checksum = checksummat(pa);
#endif

			printf_debug6("exSend: id=%d, sock=%d, len=%d, seq=%d, pslink->SeqCheckSend=%d\r\n", pslink->id, pslink->socket, pa->len, pa->head.seq, pslink->SeqCheckSend);
			iRet = SendMsg(pslink, pa, pa->len);
			if (iRet == EX_UDP_ERROR)
			{
				sem_post(&pslink->semSendListMutex);
				return 0;
			}
			len_send-=len_t;
			buf+=len_t;

			//最后发送序号+1
			pslink->SeqMaxSend = (pslink->SeqMaxSend+1) % MAX_PACKET_SEQ;
			
			ctlIntervalTime(sts);
			//pslink->SendTimeout = 0;
		}else
		{
			printf_debug2("send failed 2: pslink->SeqMaxSend=%d, pslink->SeqCheckSend=%d, pslink->PrioritySendLack=%d\n  ", 
				pslink->SeqMaxSend, pslink->SeqCheckSend, pslink->PrioritySendLack);

			if (pslink->tsWaitTime < 0)
			{
//  				if (GET_MS_DIFF(g_pSock[pslink->socket]->time_listen, pslink->timeout) >= LISTEN_LINK_INTERVAL 
// 					&& pslink->SeqCheckSend == (pslink->SeqMaxSend + 1) % MAX_PACKET_SEQ)
// 				//if (pslink->SendTimeout == 1)
// 				{
// 					return 0;
// 				}
// 				SEM_TIMED_WAIT(pslink->semSendList, LISTEN_LINK_INTERVAL, iRet);
// 				if (iRet == -1)
// 				{//不能发送报文，而且超时就返回0
// 					ASSERT_ID;
// 					//del_list(pslink->socket, pslink);
// 					pslink->SendTimeout = 1;
// 					exit(1);return 0;
// 				}
				sem_wait(&pslink->semSendList);
				ASSERT_ID_SEND;
			}else if(pslink->tsWaitTime == 0)
			{
				sem_post(&pslink->semSendListMutex);
				return 0;
			}else
			{
				SEM_TIMED_WAIT(pslink->semSendList, pslink->tsWaitTime, iRet);
				ASSERT_ID_SEND;
				if (iRet == -1)
				{//不能发送报文，而且超时就返回0
					sem_post(&pslink->semSendListMutex);
					return 0;
				}
			}
		}
	}
	sem_post(&pslink->semSendListMutex);
	return len;
}

//功  能: 关闭socket
//参  数: sock：是exSocket的返回值
//返回值: 错误返回EX_UDP_ERROR
int exClose(int sock)
{
	st_sock* sts = NULL;
	sem_wait(&g_semLink);
	if (sock > MAX_SOCKET_NUM || sock < 0 || g_pSock[sock] == NULL || g_pSock[sock]->linkState == -1)
	{
		sem_post(&g_semLink);
		LOG_WRITE_POS(LOG_ERR, "ERROR: [exClose] this socket is closed ! sock=%d\r\n", sock);
		return EX_UDP_ERROR;
	}
	sts = g_pSock[sock & MAX_SOCKET_NUM];
	sem_wait(&sts->semLink);
	//list_init(&sts->listIDMsgReadHead);
	sts->linkState = -1;
	g_pSock[sock] = NULL;
	
	while(!list_empty(&sts->list))
	{
		del_list(sts, (s_link*)list_begin(&sts->list));
	}
	//list_push_front(&sts->listFreeHead, (clist*)sts);

	if (sts->msgid != -1)
	{
		sem_post(&sts->semRecvToMsg);
		sem_destroy(&sts->semRecvToMsg);
		sem_post(&sts->semListRead);
		sem_destroy(&sts->semListRead);
	}
	sem_post(&sts->semAccept);
	sem_post(&sts->semListen);
	sem_post(&sts->semLink);
	sem_destroy(&sts->semAccept);
	sem_destroy(&sts->semListen);
	sem_destroy(&sts->semLink);

#ifdef WIN32
	closesocket(sock);
#else
	close(sock);
#endif

	sem_post(&g_semLink);
	return EX_UDP_OK;
}
#endif

#ifdef WIN32
//windosw下用的消息队列
int msgget(int key, int chmod)
{
	if(EX_UDP_ERROR == sem_init(&g_msgQueue.semRead, 0, 0) 
		|| EX_UDP_ERROR == sem_init(&g_msgQueue.semWrite, 0, 0)
		|| EX_UDP_ERROR == sem_init(&g_msgQueue.semReadOnly, 0, 1) 
		|| EX_UDP_ERROR == sem_init(&g_msgQueue.semWriteOnly, 0, 1))
	return EX_UDP_ERROR;
}

int msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg)
{
	sem_wait(&g_msgQueue.semWriteOnly);
	while (1)
	{
		if((g_msgQueue.write+1)%MAX_MSG_QUEUE != g_msgQueue.read)
		{
			memcpy(&g_msgQueue.msgQueue[g_msgQueue.write], msgp, msgsz+sizeof(long));
			g_msgQueue.write = (g_msgQueue.write+1)%MAX_MSG_QUEUE;
			sem_post(&g_msgQueue.semRead);
			break;
		}else if(msgflg == IPC_NOWAIT)
		{
			sem_post(&g_msgQueue.semWriteOnly);
			return -1;
		}
		sem_wait(&g_msgQueue.semWrite);
	}
	sem_post(&g_msgQueue.semWriteOnly);
	return msgsz;
}

int msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg)
{
	int ret = 0;
	if (msgsz <= 0)
	{
		return -1;
	}
	sem_wait(&g_msgQueue.semReadOnly);
	while(1)
	{
		if(g_msgQueue.write != g_msgQueue.read)
		{
			msgsz += sizeof(long);
			ret = sizeof(st_msg_packet) - MAX_PACKET_DATA_SIZE + g_msgQueue.msgQueue[g_msgQueue.read].len;
			if (ret > msgsz)
			{
				ret = msgsz;
			}
			memcpy(msgp, &g_msgQueue.msgQueue[g_msgQueue.read], ret);
			//g_msgQueue.msgQueue[g_msgQueue.read].type = 0;
			g_msgQueue.read = (g_msgQueue.read+1) % MAX_MSG_QUEUE;
			sem_post(&g_msgQueue.semWrite);
			break;
		}else if(msgflg == IPC_NOWAIT)
		{
			sem_post(&g_msgQueue.semReadOnly);
			return -1;
		}
		sem_wait(&g_msgQueue.semRead);
	}
	sem_post(&g_msgQueue.semReadOnly);
	return ret - sizeof(long);
}

#endif

#ifdef __cplusplus
}
#endif

