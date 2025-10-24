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

/*
typedef struct
{
  char service_id[32];
  UPDATE_STATUS status;
  char msg[128];
} remote_management_ota_progress_qitem_t;
static remote_management_ota_progress_qitem_t remote_management_ota_progress_queue[REMENTE_MANAGEMENT_QSIZE];
static int remote_management_ota_progress_qhead = 0, remote_management_ota_progress_qtail = 0;
static pthread_mutex_t   remote_management_ota_progress_qmtx = PTHREAD_MUTEX_INITIALIZER;


typedef struct
{
  char service_id[32];
  char msg[128];
} remote_management_ota_inform_qitem_t;
static remote_management_ota_inform_qitem_t remote_management_ota_inform_queue[REMENTE_MANAGEMENT_QSIZE];
static int remote_management_ota_inform_qhead = 0, remote_management_ota_inform_qtail = 0;
static pthread_mutex_t   remote_management_ota_inform_qmtx = PTHREAD_MUTEX_INITIALIZER;
*/
static remote_management_event_cb_t   remote_management_event_cb = NULL;   /* 初始为空，表示未注册 */
static remote_management_ulog_cb_t    remote_management_ulog_cb = NULL;   /* 初始为空，表示未注册 */
static remote_management_protocol_message_cb_t remote_management_protocol_message_cb = NULL;   /* 初始为空，表示未注册 */
//static remote_management_ota_progress_cb_t   remote_management_ota_progress_cb = NULL;   /* 初始为空，表示未注册 */
//static remote_management_ota_inform_cb_t   remote_management_ota_inform_cb = NULL;   /* 初始为空，表示未注册 */
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
/*
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
*/
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
#if 0
void remote_management_fire_ota_progress(const char *service_id, UPDATE_STATUS status, const char *msg)
{
    /* 1. 构造事件 */
	remote_management_ota_progress_qitem_t it;
    strncpy(it.service_id, service_id, sizeof(it.service_id) - 1);
    it.service_id[sizeof(it.service_id) - 1] = '\0';
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

void remote_management_fire_ota_inform(const char *service_id, const char *msg)
{
    /* 1. 构造事件 */
	remote_management_ota_inform_qitem_t it;
    strncpy(it.service_id, service_id, sizeof(it.service_id) - 1);
    it.service_id[sizeof(it.service_id) - 1] = '\0';
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
#endif

/*
** OTA升级相关函数实现
*/
// MD5 文件计算函数 (OpenSSL 实现)
int md5_file(const char *filePath, char *md5_out)
{
    unsigned char c[MD5_DIGEST_LENGTH];
    FILE *inFile = NULL;
    MD5_CTX mdContext;
    int bytes;
    unsigned char data[1024];
    int i;

    inFile = fopen(filePath, "rb");
    if (inFile == NULL) {
        printf("Error: Cannot open file %s for MD5 calculation.\n", filePath);
        return -1;
    }

    MD5_Init(&mdContext);

    while ((bytes = fread(data, 1, 1024, inFile)) != 0) {
        MD5_Update(&mdContext, data, bytes);
    }

    MD5_Final(c, &mdContext);

    fclose(inFile);

    // 将结果转换为 32 位小写十六进制字符串
    for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(&md5_out[i * 2], "%02x", c[i]);
    }
    md5_out[MAX_MD5_LEN - 1] = '\0';

    return 0;
}

// HTTP 文件下载函数 (系统命令实现)
int http_download_file(const char *url, const char *savePath){
    if (!url || !savePath) return -1;

    // 确保下载目录存在
    if (access(OTA_DOWNLOAD_DIR, F_OK) != 0) {
        mkdir(OTA_DOWNLOAD_DIR, 0755);
    }

    // 检查 curl 命令是否存在
    if (access("/usr/bin/curl", X_OK) != 0 && access("/bin/curl", X_OK) != 0) {
        printf("Error: curl command not found.\n");
        return -1;
    }

    // 删除旧文件，防止残留
    unlink(savePath);

    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "curl --fail --silent --show-error -L \"%s\" -o \"%s\"", url, savePath);

    printf("Downloading via system: %s\n", cmd);
    int ret = system(cmd);

    if (ret == 0) {
        printf("Download success: %s\n", savePath);
        return 0;
    } else {
        printf("Download failed (code=%d)\n", ret);
        return -1;
    }
}
// ZIP 解压函数 (使用 system/unzip 实现强制覆盖到指定目录)
int unzip_file(const char *zipPath, const char *destDir) {
    char cmd[MAX_PATH_LEN * 2];
    int ret;

    // 1. 确保目标解压目录存在
    if (access(destDir, F_OK) != 0) {
        if (mkdir(destDir, 0755) != 0) {
            printf("Error: Failed to create directory %s, errno: %d\n", destDir, errno);
            return -1;
        }
    }

    // 2. 使用 unzip -o 强制覆盖解压到指定目录
    snprintf(cmd, sizeof(cmd), "unzip -o %s -d %s", zipPath, destDir);

    printf("Executing unzip command: %s (Forced Overwrite)\n", cmd);

    ret = system(cmd);

    if (ret != 0) {
        printf("Error: Unzip of %s failed, system returned %d.\n", zipPath, ret);
        return -1;
    }

    return 0;
}
// 文件大小获取 (POSIX stat 实现)
long get_file_size(const char *filePath) {
    struct stat st;
    if (stat(filePath, &st) == 0) {
        return st.st_size;
    }
    return -1;
}
// ----------------------------------------------------------------------
// OTA 重启状态文件 (upgrade.txt) 操作函数
// ----------------------------------------------------------------------

/**
 * @brief 写入重启状态文件 (version 和 id)
 */
int write_ota_reboot_status(const char *version, const char *id) {
    char status_path[MAX_PATH_LEN];
    snprintf(status_path, sizeof(status_path), "%s%s", OTA_DOWNLOAD_DIR, OTA_STATUS_FILE);

    // "w" 模式会创建或截断文件
    FILE *f = fopen(status_path, "w");
    if (f) {
        // 格式: version\nid
        fprintf(f, "%s\n", version);
        fprintf(f, "%s\n", id);
        fclose(f);
        return 0;
    }
    printf("Error: Failed to create OTA status file %s.\n", status_path);
    return -1;
}

/**
 * @brief 检查升级状态文件，读取 version 和 id
 */
ota_reboot_status_t *check_ota_finish_status(void) {
    static ota_reboot_status_t status = {0};
    char status_path[MAX_PATH_LEN];
    snprintf(status_path, sizeof(status_path), "%s%s", OTA_DOWNLOAD_DIR, OTA_STATUS_FILE);

    FILE *f = fopen(status_path, "r");
    if (f) {
        if (fgets(status.version, sizeof(status.version), f) != NULL &&
            fgets(status.id, sizeof(status.id), f) != NULL)
        {
            status.version[strcspn(status.version, "\n")] = 0;
            status.id[strcspn(status.id, "\n")] = 0;
            fclose(f);
            return &status;
        }
        fclose(f);
    }
    return NULL;
}

/**
 * @brief 清除升级状态文件
 */
void clear_ota_finish_status(void) {
    char status_path[MAX_PATH_LEN];
    snprintf(status_path, sizeof(status_path), "%s%s", OTA_DOWNLOAD_DIR, OTA_STATUS_FILE);

    if (remove(status_path) != 0) {
        printf("Warning: Failed to delete OTA status file %s.\n", status_path);
    }
}

