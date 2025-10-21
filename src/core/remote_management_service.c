/*********************************Copyright (c)**********************************
**
**                        许昌智能继电器股份有限公司
**                              www.xjpmf.com
**
**-----------------------------------文件信息-----------------------------------
**  项目名称    ：PMF406
**  文    件    ：remote_management_service.c
**  作    者    ：李鹏博
**  版    本    ：1.00
**  创建日期    ：2025-09-04
**  描    述    ：远程管理线程
**
**-----------------------------------历史版本-----------------------------------
**  作    者    ：
**  版    本    ：
**  修改日期    ：
**  描    述    ：
**
******************************************************************************/
#define REMOTE_MANAGEMENT_SEVICE_GLOBALS

/******************************************************************************
**C库头文件
******************************************************************************/
#include "../../inc/private/inc_private.h"
/******************************************************************************
**底层驱动平台头文件
******************************************************************************/

/******************************************************************************
**基础平台头文件
******************************************************************************/



/******************************************************************************
**应用头文件
******************************************************************************/

/******************************************************************************
**宏定义
******************************************************************************/
#define REMENTE_MANAGEMENT_QSIZE 16
/******************************************************************************
**枚举定义
******************************************************************************/

/******************************************************************************
**结构类型定义
******************************************************************************/

/******************************************************************************
**静态变量定义
******************************************************************************/
//实时事件
typedef struct
{
  u32 service_id;
  char msg[128];
} remote_management_event_qitem_t;
static remote_management_event_qitem_t remote_management_event_queue[REMENTE_MANAGEMENT_QSIZE];
static int remote_management_event_qhead = 0, remote_management_event_qtail = 0;
static pthread_mutex_t   remote_management_event_qmtx = PTHREAD_MUTEX_INITIALIZER;

//日志信息
typedef struct
{
  REMOTE_MANAGEMENT_ULOG_LEVEL level;
  char msg[128];
} remote_management_ulog_qitem_t;
static remote_management_ulog_qitem_t remote_management_ulog_queue[REMENTE_MANAGEMENT_QSIZE];
static int remote_management_ulog_qhead = 0, remote_management_ulog_qtail = 0;
static pthread_mutex_t   remote_management_ulog_qmtx = PTHREAD_MUTEX_INITIALIZER;


//报文信息

typedef struct
{
	u32 service_id;
	u32 dev_id;
	SERVICE_PROTOCOL_TYPE protocol_type;
    char msg[1024];
    u16 len;
} remote_management_protocol_message_qitem_t;
static remote_management_protocol_message_qitem_t remote_management_protocol_message_queue[REMENTE_MANAGEMENT_QSIZE];
static int remote_management_protocol_message_qhead = 0, remote_management_protocol_message_qtail = 0;
static pthread_mutex_t   remote_management_protocol_message_qmtx = PTHREAD_MUTEX_INITIALIZER;

//ota进度消息
typedef struct
{
  u32 service_id;
  UPDATE_STATUS status;
  char msg[128];
} remote_management_ota_progress_qitem_t;
static remote_management_ota_progress_qitem_t remote_management_ota_progress_queue[REMENTE_MANAGEMENT_QSIZE];
static int remote_management_ota_progress_qhead = 0, remote_management_ota_progress_qtail = 0;
static pthread_mutex_t   remote_management_ota_progress_qmtx = PTHREAD_MUTEX_INITIALIZER;

//ota升级结果
typedef struct
{
  u32 service_id;
  char msg[128];
} remote_management_ota_inform_qitem_t;
static remote_management_ota_inform_qitem_t remote_management_ota_inform_queue[REMENTE_MANAGEMENT_QSIZE];
static int remote_management_ota_inform_qhead = 0, remote_management_ota_inform_qtail = 0;
static pthread_mutex_t   remote_management_ota_inform_qmtx = PTHREAD_MUTEX_INITIALIZER;

static remote_management_event_cb_t   remote_management_event_cb = NULL;   /* 初始为空，表示未注册 */
static remote_management_ulog_cb_t    remote_management_ulog_cb = NULL;   /* 初始为空，表示未注册 */
static remote_management_protocol_message_cb_t remote_management_protocol_message_cb = NULL;   /* 初始为空，表示未注册 */
static remote_management_ota_progress_cb_t   remote_management_ota_progress_cb = NULL;   /* 初始为空，表示未注册 */
static remote_management_ota_inform_cb_t   remote_management_ota_inform_cb = NULL;   /* 初始为空，表示未注册 */
/******************************************************************************
**API函数实现
******************************************************************************/

/******************************************************************************
  End File
******************************************************************************/
  int remote_management_register_event_cb(remote_management_event_cb_t cb)
{
    if (cb == NULL)
    	return -1;
    remote_management_event_cb = cb;
    return 0;
}


int remote_management_register_ulog_cb(remote_management_ulog_cb_t cb)
{
    if (cb == NULL)
    	return -1;
    remote_management_ulog_cb = cb;
    return 0;
}


int remote_management_register_protocol_message_cb(remote_management_protocol_message_cb_t cb)
{
    if (cb == NULL)
    	return -1;
    remote_management_protocol_message_cb = cb;
    return 0;
}

int remote_management_register_ota_progress_cb(remote_management_ota_progress_cb_t cb)
{
    if (cb == NULL)
    	return -1;
    remote_management_ota_progress_cb = cb;
    return 0;
}

int remote_management_register_ota_inform_cb(remote_management_ota_inform_cb_t cb)
{
    if (cb == NULL)
    	return -1;
    remote_management_ota_inform_cb = cb;
    return 0;
}

/*
void remote_management_fire_event(int service_id, const char *msg)
{
    if (remote_management_event_cb)
    	remote_management_event_cb(service_id, msg);
    else
        printf("event_cb_unregistered\n");
}
*/
void remote_management_fire_event(u32 service_id, const char *msg)
{
    /* 1. 构造事件 */
	remote_management_event_qitem_t it;
    it.service_id = service_id;
    strncpy(it.msg, msg, sizeof(it.msg) - 1);
    it.msg[sizeof(it.msg) - 1] = '\0';

    pthread_mutex_lock(&remote_management_event_qmtx);

    /* 2. 入队 */
    int next = (remote_management_event_qtail + 1) % REMENTE_MANAGEMENT_QSIZE;
    while (next == remote_management_event_qhead)
    {               /* 满队列 */
        struct timespec ts = {0, 1000000}; /* 1 ms */
        pthread_mutex_unlock(&remote_management_event_qmtx);      /* 先放锁再睡 */
        nanosleep(&ts, NULL);
        pthread_mutex_lock(&remote_management_event_qmtx);        /* 醒来再抢锁 */
        next = (remote_management_event_qtail + 1) % REMENTE_MANAGEMENT_QSIZE;
    }
    remote_management_event_queue[remote_management_event_qtail] = it;
    remote_management_event_qtail = next;

    /* 3. 立即顺序执行回调（仍持有锁，天然串行） */
    if (remote_management_event_cb)
    {
        int idx = remote_management_event_qhead;
        while (idx != remote_management_event_qtail)
        {
        	remote_management_event_qitem_t *e = &remote_management_event_queue[idx];
            remote_management_event_cb(e->service_id, e->msg);
            idx = (idx + 1) % REMENTE_MANAGEMENT_QSIZE;
        }
        remote_management_event_qhead = remote_management_event_qtail;   /* 全部处理完，队列清空 */
    }
    else
    {
        printf("event_cb_unregistered\n");
    }
    pthread_mutex_unlock(&remote_management_event_qmtx);
}

/*
void remote_management_fire_ulog(REMOTE_MANAGEMENT_ULOG_LEVEL level, const char *msg)
{
    if (remote_management_ulog_cb)
    	remote_management_ulog_cb(level, msg);
    else
        printf("ulog_cb_unregistered\n");
}
*/

void remote_management_fire_ulog(REMOTE_MANAGEMENT_ULOG_LEVEL level, const char *msg)
{
    /* 1. 构造事件 */
	remote_management_ulog_qitem_t it;
    it.level = level;
    strncpy(it.msg, msg, sizeof(it.msg) - 1);
    it.msg[sizeof(it.msg) - 1] = '\0';

    pthread_mutex_lock(&remote_management_ulog_qmtx);

    /* 2. 入队 */
    int next = (remote_management_ulog_qtail + 1) % REMENTE_MANAGEMENT_QSIZE;
    while (next == remote_management_ulog_qhead)
    {               /* 满队列 */
        struct timespec ts = {0, 1000000}; /* 1 ms */
        pthread_mutex_unlock(&remote_management_ulog_qmtx);      /* 先放锁再睡 */
        nanosleep(&ts, NULL);
        pthread_mutex_lock(&remote_management_ulog_qmtx);        /* 醒来再抢锁 */
        next = (remote_management_ulog_qtail + 1) % REMENTE_MANAGEMENT_QSIZE;
    }
    remote_management_ulog_queue[remote_management_ulog_qtail] = it;
    remote_management_ulog_qtail = next;

    /* 3. 立即顺序执行回调（仍持有锁，天然串行） */
    if (remote_management_ulog_cb)
    {
        int idx = remote_management_ulog_qhead;
        while (idx != remote_management_ulog_qtail)
        {
        	remote_management_ulog_qitem_t *e = &remote_management_ulog_queue[idx];
        	remote_management_ulog_cb(e->level, e->msg);
            idx = (idx + 1) % REMENTE_MANAGEMENT_QSIZE;
        }
        remote_management_ulog_qhead = remote_management_ulog_qtail;   /* 全部处理完，队列清空 */
    }
    else
    {
        printf("ulog_cb_unregistered\n");
    }
    pthread_mutex_unlock(&remote_management_ulog_qmtx);
}

void remote_management_fire_protocol_message(u32 service_id,u32 dev_id,SERVICE_PROTOCOL_TYPE protocol_type, const char *msg,u16 len)
{
    /* 1. 构造事件 */
	remote_management_protocol_message_qitem_t it;
    strncpy(it.msg, msg, len);
    if(len <= 1024)
        it.msg[len] = '\0';
    else
       return;
    int ss=0;
    printf("chuan ru bw:");
    for(ss = 0 ;ss < len ;ss++)
    {
    	if(ss==len-1)
    		printf("%02x\n",msg[ss]);
    	else
    		printf("%02x",msg[ss]);
    }

    it.service_id = service_id;
    it.dev_id=dev_id;
    it.len=len;
    it.protocol_type=protocol_type;
    pthread_mutex_lock(&remote_management_protocol_message_qmtx);

    /* 2. 入队 */
    int next = (remote_management_protocol_message_qtail + 1) % REMENTE_MANAGEMENT_QSIZE;
    while (next == remote_management_protocol_message_qhead)
    {               /* 满队列 */
        struct timespec ts = {0, 1000000}; /* 1 ms */
        pthread_mutex_unlock(&remote_management_protocol_message_qmtx);      /* 先放锁再睡 */
        nanosleep(&ts, NULL);
        pthread_mutex_lock(&remote_management_protocol_message_qmtx);        /* 醒来再抢锁 */
        next = (remote_management_protocol_message_qtail + 1) % REMENTE_MANAGEMENT_QSIZE;
    }
    remote_management_protocol_message_queue[remote_management_protocol_message_qtail] = it;
    remote_management_protocol_message_qtail = next;

    /* 3. 立即顺序执行回调（仍持有锁，天然串行） */
    if (remote_management_protocol_message_cb)
    {
        int idx = remote_management_protocol_message_qhead;
        while (idx != remote_management_protocol_message_qtail)
        {
        	remote_management_protocol_message_qitem_t *e = &remote_management_protocol_message_queue[idx];
        	remote_management_protocol_message_cb(e->service_id,e->dev_id,e->protocol_type, e->msg,e->len);
            idx = (idx + 1) % REMENTE_MANAGEMENT_QSIZE;
        }
        remote_management_protocol_message_qhead = remote_management_protocol_message_qtail;   /* 全部处理完，队列清空 */
    }
    else
    {
        printf("protocol_message_cb_unregistered\n");
    }
    pthread_mutex_unlock(&remote_management_protocol_message_qmtx);
}

void remote_management_fire_ota_progress(u32 service_id, UPDATE_STATUS status, const char *msg)
{
    /* 1. 构造事件 */
	remote_management_ota_progress_qitem_t it;
    it.service_id = service_id;
    it.status = status;
    strncpy(it.msg, msg, sizeof(it.msg) - 1);
    it.msg[sizeof(it.msg) - 1] = '\0';

    pthread_mutex_lock(&remote_management_ota_progress_qmtx);

    /* 2. 入队 */
    int next = (remote_management_ota_progress_qtail + 1) % REMENTE_MANAGEMENT_QSIZE;
    while (next == remote_management_ota_progress_qhead)
    {               /* 满队列 */
        struct timespec ts = {0, 1000000}; /* 1 ms */
        pthread_mutex_unlock(&remote_management_ota_progress_qmtx);      /* 先放锁再睡 */
        nanosleep(&ts, NULL);
        pthread_mutex_lock(&remote_management_ota_progress_qmtx);        /* 醒来再抢锁 */
        next = (remote_management_ota_progress_qtail + 1) % REMENTE_MANAGEMENT_QSIZE;
    }
    remote_management_ota_progress_queue[remote_management_ota_progress_qtail] = it;
    remote_management_ota_progress_qtail = next;

    /* 3. 立即顺序执行回调（仍持有锁，天然串行） */
    if (remote_management_ota_progress_cb)
    {
        int idx = remote_management_ota_progress_qhead;
        while (idx != remote_management_ota_progress_qtail)
        {
        	remote_management_ota_progress_qitem_t *e = &remote_management_ota_progress_queue[idx];
            remote_management_ota_progress_cb(e->service_id,e->status, e->msg);
            idx = (idx + 1) % REMENTE_MANAGEMENT_QSIZE;
        }
        remote_management_ota_progress_qhead = remote_management_ota_progress_qtail;   /* 全部处理完，队列清空 */
    }
    else
    {
        printf("ota_progress_cb_unregistered\n");
    }
    pthread_mutex_unlock(&remote_management_ota_progress_qmtx);
}

void remote_management_fire_ota_inform(u32 service_id, const char *msg)
{
    /* 1. 构造事件 */
	remote_management_ota_inform_qitem_t it;
    it.service_id = service_id;
    strncpy(it.msg, msg, sizeof(it.msg) - 1);
    it.msg[sizeof(it.msg) - 1] = '\0';

    pthread_mutex_lock(&remote_management_ota_inform_qmtx);

    /* 2. 入队 */
    int next = (remote_management_ota_inform_qtail + 1) % REMENTE_MANAGEMENT_QSIZE;
    while (next == remote_management_ota_inform_qhead)
    {               /* 满队列 */
        struct timespec ts = {0, 1000000}; /* 1 ms */
        pthread_mutex_unlock(&remote_management_ota_inform_qmtx);      /* 先放锁再睡 */
        nanosleep(&ts, NULL);
        pthread_mutex_lock(&remote_management_ota_inform_qmtx);        /* 醒来再抢锁 */
        next = (remote_management_ota_inform_qtail + 1) % REMENTE_MANAGEMENT_QSIZE;
    }
    remote_management_ota_inform_queue[remote_management_ota_inform_qtail] = it;
    remote_management_ota_inform_qtail = next;

    /* 3. 立即顺序执行回调（仍持有锁，天然串行） */
    if (remote_management_ota_inform_cb)
    {
        int idx = remote_management_ota_inform_qhead;
        while (idx != remote_management_ota_inform_qtail)
        {
        	remote_management_ota_inform_qitem_t *e = &remote_management_ota_inform_queue[idx];
            remote_management_ota_inform_cb(e->service_id, e->msg);
            idx = (idx + 1) % REMENTE_MANAGEMENT_QSIZE;
        }
        remote_management_ota_inform_qhead = remote_management_ota_inform_qtail;   /* 全部处理完，队列清空 */
    }
    else
    {
        printf("ota_inform_cb_unregistered\n");
    }
    pthread_mutex_unlock(&remote_management_ota_inform_qmtx);
}