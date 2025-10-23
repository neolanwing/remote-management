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
// 下载和解压目录
#define OTA_DOWNLOAD_DIR        "/opt/update/"
// 目标程序安装目录
#define OTA_TARGET_DIR          "/usr/pmf406/"
#define MAX_PATH_LEN            256
#define MAX_MD5_LEN             33  // 32 字符 + '\0'

// 用于重启后检查的标志文件
#define OTA_STATUS_FILE         "upgrade.txt"
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
  char service_id[32];
  UPDATE_STATUS status;
  char msg[128];
} remote_management_ota_progress_qitem_t;
static remote_management_ota_progress_qitem_t remote_management_ota_progress_queue[REMENTE_MANAGEMENT_QSIZE];
static int remote_management_ota_progress_qhead = 0, remote_management_ota_progress_qtail = 0;
static pthread_mutex_t   remote_management_ota_progress_qmtx = PTHREAD_MUTEX_INITIALIZER;

//ota升级结果
typedef struct
{
  char service_id[32];
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

/*
** OTA升级相关函数实现
*/
// MD5 文件计算函数 (OpenSSL 实现)
int md5_file(const char *filePath, char *md5_out) {
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
// Libcurl 写入回调函数
static size_t write_data_callback(void *ptr, size_t size, size_t nmemb, void *stream) {
    size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
    return written;
}
// HTTP 文件下载函数 (libcurl 实现)
int http_download_file(const char *url, const char *savePath, int overwrite) {
    CURL *curl;
    CURLcode res;
    FILE *fp = NULL;

    // 1. 确保下载目录存在
    if (access(OTA_DOWNLOAD_DIR, F_OK) != 0) {
        mkdir(OTA_DOWNLOAD_DIR, 0755);
    }

    // 2. 处理强制覆盖要求：使用 "wb" 模式实现强制覆盖
    fp = fopen(savePath, "wb"); 
    
    if (fp == NULL) {
        printf("Error: Cannot open file %s for writing.\n", savePath);
        return -1;
    }

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L); 
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); 
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L); 
        
        printf("Downloading from %s to %s (Forced Overwrite)...\n", url, savePath);
        res = curl_easy_perform(curl);
        
        if (res != CURLE_OK) {
            printf("Error: libcurl download failed: %s\n", curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
    }
    
    fclose(fp);

    return (res == CURLE_OK) ? 0 : -1;
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
// ----------------------------------------------------------------------
// 核心逻辑: OTA 升级处理函数
// ----------------------------------------------------------------------

int ota_upgrade_handler(const ota_upgrade_cmd_t *cmd)
{
    int ret = -1;
    char zip_filename[MAX_PATH_LEN] = {0};
    char zip_path[MAX_PATH_LEN] = {0};
    char extract_dir[MAX_PATH_LEN] = {0};
    char base_filename[MAX_PATH_LEN] = {0};
    long downloaded_size;
    char calculated_md5[MAX_MD5_LEN] = {0};

    const char *target_dir = OTA_TARGET_DIR;
    const char *download_dir = OTA_DOWNLOAD_DIR;

    printf("Starting OTA upgrade for version: %s\n", cmd->data.version);

    // 1. 确定文件名和解压目录：使用版本号作为基准名
    snprintf(base_filename, sizeof(base_filename), "ota_%s", cmd->data.version);
    snprintf(zip_filename, sizeof(zip_filename), "%s.zip", base_filename);
    snprintf(zip_path, sizeof(zip_path), "%s%s", download_dir, zip_filename);
    snprintf(extract_dir, sizeof(extract_dir), "%s%s/", download_dir, base_filename);


    // ---- 步骤 1. 文件下载 (强制覆盖) ----
    if (http_download_file(cmd->data.url, zip_path, 1) != 0) {
        remote_management_fire_ota_progress(cmd->id, DOWNLOAD_FAILED, "-2下载失败");
        goto cleanup;
    }

    // ---- 步骤 2. 大小校验 ----
    downloaded_size = get_file_size(zip_path);
    if (downloaded_size == -1 || downloaded_size != cmd->data.size) {
        remote_management_fire_ota_progress(cmd->id, VERIFY_FAILED, "-3校验失败: Size mismatch");
        goto cleanup;
    }
    
    // ---- 步骤 3. 签名/MD5 校验 ----
    if (md5_file(zip_path, calculated_md5) != 0) {
        remote_management_fire_ota_progress(cmd->id, VERIFY_FAILED, "-3校验失败: MD5 calculation failed");
        goto cleanup;
    }
    
    // 3.1 md5 字段比对 (不区分大小写)
    if (strcasecmp(calculated_md5, cmd->data.md5) != 0) {
        remote_management_fire_ota_progress(cmd->id, VERIFY_FAILED, "-3校验失败: MD5 field mismatch");
        goto cleanup;
    }

    // 3.2 Sign 字段比对 (不区分大小写)
    if (strcasecmp(calculated_md5, cmd->data.sign) != 0) {
        remote_management_fire_ota_progress(cmd->id, VERIFY_FAILED, "-3校验失败: Sign field mismatch");
        goto cleanup;
    }
    
    printf("File integrity verification passed. MD5: %s\n", calculated_md5);

    // ---- 步骤 4. 文件解压 (强制覆盖到同名目录) ----
    if (unzip_file(zip_path, extract_dir) != 0) {
        remote_management_fire_ota_progress(cmd->id, FLASH_FAILED, "-4烧写失败: Unzip failed"); 
        goto cleanup;
    }

    // ---- 步骤 5 & 6. 文件清理、复制与权限设置 ----
    DIR *dir;
    struct dirent *entry;
    
    dir = opendir(extract_dir);
    if (dir == NULL) {
        remote_management_fire_ota_progress(cmd->id, FLASH_FAILED, "-4烧写失败: Cannot open extracted directory");
        goto cleanup;
    }

    while ((entry = readdir(dir)) != NULL) {
        // 跳过目录和特殊项
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || entry->d_type == DT_DIR) {
            continue; 
        }

        char old_target_path[MAX_PATH_LEN];
        char new_source_path[MAX_PATH_LEN];
        char copy_cmd[MAX_PATH_LEN * 2];
        
        // 5.2 删除 /usr/pmf406/ 目录下同名旧文件
        snprintf(old_target_path, sizeof(old_target_path), "%s%s", target_dir, entry->d_name);
        if (access(old_target_path, F_OK) == 0) {
            printf("Deleting old file: %s\n", old_target_path);
            remove(old_target_path); // 允许删除失败，但最好记录
        }
        
        // 5.3 复制新文件到 /usr/pmf406/ (强制覆盖)
        snprintf(new_source_path, sizeof(new_source_path), "%s%s", extract_dir, entry->d_name);
        
        // 使用 cp -f 强制覆盖复制
        snprintf(copy_cmd, sizeof(copy_cmd), "cp -f %s %s", new_source_path, target_dir);
        if (system(copy_cmd) != 0) {
            closedir(dir);
            remote_management_fire_ota_progress(cmd->id, FLASH_FAILED, "-4烧写失败: File copy failed");
            goto cleanup;
        }

        // 步骤 6. 权限设置 (设置为可执行 rwxrwxrwx)
        char final_target_path[MAX_PATH_LEN];
        snprintf(final_target_path, sizeof(final_target_path), "%s%s", target_dir, entry->d_name);
        if (chmod(final_target_path, S_IRWXU | S_IRWXG | S_IRWXO) != 0) {
            printf("Warning: Failed to set executable permission for %s\n", final_target_path);
        }
    }
    closedir(dir);
    
    // 在重启前，写入版本和ID到 upgrade.txt
    if (write_ota_reboot_status(cmd->data.version, cmd->id) != 0) {
        printf("Warning: Failed to write upgrade.txt status file.\n");
    }

    ret = 0;

    // ---- 步骤 7. 系统重启 ----
    printf("OTA files deployed successfully. Executing system reboot...\n");
    system("reboot"); 

cleanup:
    // 清理临时文件和目录
    char cleanup_cmd[MAX_PATH_LEN * 2];
    
    // 清理解压目录
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf %s", extract_dir);
    system(cleanup_cmd); 
    
    // 清理下载的ZIP文件
    remove(zip_path); 

    return ret; 
}