/*********************************Copyright (c)**********************************
**
**                        许昌智能继电器股份有限公司
**                              www.xjpmf.com
**
**-----------------------------------文件信息-----------------------------------
**  项目名称    ：PMF406
**  文    件    ：remote_management_server.c
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


/******************************************************************************
**C库头文件
******************************************************************************/

/******************************************************************************
**底层驱动平台头文件
******************************************************************************/

/******************************************************************************
**基础平台头文件
******************************************************************************/
#include "../inc/private/inc_private.h"

/******************************************************************************
**应用头文件
******************************************************************************/

/******************************************************************************
**宏定义
******************************************************************************/
//#define ADDRESS   "mqtt.xjpmf.cloud:1883"
#define ADDRESS   "39.105.53.134:1883"
#define TOPIC     "demo/status"
#define QOS       1
#define KEEPALIVE 20
#define remote_management_version  "1.0"

#define OTA_FAIL_COUNT_FILE "/opt/files/ota_fail_count"
#define APP_FAIL_COUNT_FILE "/opt/files/app_fail_count"
#define BACKUP_DIR "/opt/apps/backup"
#define RUN_DIR    "/usr/pmf406"



/******************************************************************************
**枚举定义
******************************************************************************/

/******************************************************************************
**结构类型定义
******************************************************************************/
typedef struct {
    unsigned long user;
    unsigned long nice;
    unsigned long system;
    unsigned long idle;
    unsigned long iowait;
    unsigned long irq;
    unsigned long softirq;
    unsigned long steal;
} cpu_stats_t;
/******************************************************************************
**静态变量定义
******************************************************************************/
static MQTTClient remote_management_client;
static u8 remote_management_online_flag = 0;
static u32 remote_management_pub_num = 0;
static u32 remote_management_device_attribute_cyc_tick = 0;
static int g_app_fail_count = 0;
static int g_pid_no_change_count = 0;
static int g_last_app_pid = 0;
static volatile MQTTClient_deliveryToken remote_management_deliveredtoken;
static char CLIENTID[128]={0};
static char updated_ota_bin[2] = "0";   // 新增变量，记录是否更新了 ota 程序
static int ota_updated = 0;


//发布主题
char remote_management_device_attribute_topic[64]="/sys/%s/iot/post";
char remote_management_device_ota_progress_topic[64]="/ota/device/progress/%s";
char remote_management_device_ota_inform_topic[64]="/ota/device/inform/%s";
char remote_management_device_ota_upgrade_topic[64]="/ota/device/upgrade/%s";


/******************************************************************************
**API函数实现
******************************************************************************/
extern char *get_terminal_id ( void );

static int read_persist_fail_count(void)
{
    FILE *fp = fopen(APP_FAIL_COUNT_FILE, "r");
    int count = 0;

    if (fp) {
        if (fscanf(fp, "%d", &count) != 1 || count < 0) {
            count = 0;
        }
        fclose(fp);
    }
    return count;
}
static void write_persist_fail_count(int count)
{
    FILE *fp = fopen(APP_FAIL_COUNT_FILE, "w");
    if (fp) {
        fprintf(fp, "%d\n", count);
        fclose(fp);
    }
}
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>

#define BACKUP_DIR "/opt/apps/backup"
#define RUN_DIR    "/usr/pmf406"

static int rollback_app_files(void)
{
    DIR *dir;
    struct dirent *ent;
    char src[512];
    char dst[512];
    char cmd[1024];
    struct stat st;

    dir = opendir(BACKUP_DIR);
    if (!dir) {
        perror("opendir backup");
        return -1;
    }

    while ((ent = readdir(dir)) != NULL) {

        /* 跳过 . 和 .. */
        if (strcmp(ent->d_name, ".") == 0 ||
            strcmp(ent->d_name, "..") == 0)
            continue;

        snprintf(src, sizeof(src), "%s/%s", BACKUP_DIR, ent->d_name);
        snprintf(dst, sizeof(dst), "%s/%s", RUN_DIR, ent->d_name);

        /* 只处理普通文件 */
        if (stat(src, &st) != 0)
            continue;

        if (!S_ISREG(st.st_mode))
            continue;

        /* 1️⃣ 删除运行目录中的同名文件（存在才删） */
        snprintf(cmd, sizeof(cmd),
                 "wr rm -f '%s'", dst);
        system(cmd);

        /* 2️⃣ 复制 backup -> run */
        snprintf(cmd, sizeof(cmd),
                 "wr cp '%s' '%s'", src, dst);
        if (system(cmd) != 0) {
            fprintf(stderr,
                    "[ROLLBACK] copy failed: %s\n",
                    ent->d_name);
            continue;
        }

        /* 3️⃣ 设置为可执行 */
        snprintf(cmd, sizeof(cmd),
                 "wr chmod +x '%s'", dst);
        if (system(cmd) != 0) {
            fprintf(stderr,
                    "[ROLLBACK] chmod failed: %s\n",
                    ent->d_name);
        }
    }

    closedir(dir);
    return 0;
}

static void rollback_and_reboot(void)
{
    int reboot_count;

    /* 1. 读取当前重启次数 */
    reboot_count = read_persist_fail_count();

    /* 2. 累加并写回 */
    reboot_count++;
    write_persist_fail_count(reboot_count);

    /* 3. 重新读取，确认写入成功 */
    reboot_count = read_persist_fail_count();

    /* 4. 达到阈值，执行回滚 */
    if (reboot_count >= 3) {
        rollback_app_files();

        /* ★ 关键：回滚完成后立刻清零 */
        write_persist_fail_count(0);
    }
    sync();
    /* 5. 立刻重启 */
    system("reboot -f");
}

static pid_t get_pid_by_name(const char* process_name)
{
    DIR* dir;
    struct dirent* entry;
    char path[128];
    FILE* fp;
    char cmdline[256];  // 增大缓冲区
    pid_t pid = -1;
    size_t process_name_len = strlen(process_name);

    dir = opendir("/proc");
    if (dir == NULL) {
        perror("opendir");
        return -1;
    }

    // 获取当前进程PID，用于排除自身
    pid_t self_pid = getpid();

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            if (sscanf(entry->d_name, "%d", &pid) == 1) {
                // 跳过当前进程自身
                if (pid == self_pid) continue;

                snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
                fp = fopen(path, "r");
                if (fp != NULL) {
                    size_t n = fread(cmdline, 1, sizeof(cmdline)-1, fp);
                    if (n > 0) {
                        cmdline[n] = '\0';
                        // cmdline可能包含\0分隔的参数，取第一个参数
                        char* base_name = strrchr(cmdline, '/');
                        base_name = base_name ? base_name + 1 : cmdline;

                        // 精确匹配进程名（非子串匹配）
                        if (strcmp(base_name, process_name) == 0) {
                            //printf("PID of process '%s': %d\n", process_name, pid);
                            fclose(fp);
                            closedir(dir);
                            return pid;
                        }
                    }
                    fclose(fp);
                }
            }
        }
    }
    closedir(dir);
    return -1;
}
static int is_app_running()
{
    // 用 /proc 方式搜索 PID
    pid_t pid = get_pid_by_name("app");

     /* ---------- app 未运行 ---------- */
    if (pid < 0) {
        g_app_fail_count++;

        if (g_app_fail_count >= 10) {
            rollback_and_reboot();
            return 0;   // ★ 明确终止
        }
        return 0;
    }

    // ---- app 找到了 ----
    if (g_last_app_pid == 0) {
        // 第一次记录 PID
        g_last_app_pid = pid;
        g_pid_no_change_count = 0;
    } 
    else if (pid != g_last_app_pid) {
        // PID 改变 → app 重启了
        g_app_fail_count++;
        g_last_app_pid = pid;
        g_pid_no_change_count = 0;
    }
    else {
        /* PID 未变化 */
        g_pid_no_change_count++;

        if (g_pid_no_change_count >= 50) {
            g_app_fail_count = 0;
            write_persist_fail_count(0);
            g_pid_no_change_count = 0;
        }
    }

    // 超限重启
    if (g_app_fail_count >= 10) {
        rollback_and_reboot();
    }

    return 1;   // running
}

static float get_memory_usage_percent() {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) return -1;

    char line[256];
    unsigned long mem_total = 0, mem_free = 0, buffers = 0, cached = 0;

    while(fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "MemTotal: %lu kB", &mem_total) == 1) continue;
        if (sscanf(line, "MemFree: %lu kB", &mem_free) == 1) continue;
        if (sscanf(line, "Buffers: %lu kB", &buffers) == 1) continue;
        if (sscanf(line, "Cached: %lu kB", &cached) == 1) continue;
    }
    fclose(fp);

    if(mem_total == 0) return -1;

    unsigned long used = mem_total - mem_free - buffers - cached;
    return used * 100.0f / mem_total;
}

static float get_disk_usage_percent(const char *path) {
    struct statfs buf;
    if(statfs(path, &buf) != 0) return -1;

    unsigned long total = buf.f_blocks * buf.f_bsize;
    unsigned long free  = buf.f_bfree * buf.f_bsize;
    unsigned long used  = total - free;

    if(total == 0) return -1;
    return used * 100.0f / total;
}

// 读取 /proc/stat 的 cpu 行，兼容字段缺失
static int read_cpu_stats(cpu_stats_t *stats) {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return -1;

    char line[256];
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    // 初始化为0
    stats->user = stats->nice = stats->system = stats->idle = 0;
    stats->iowait = stats->irq = stats->softirq = stats->steal = 0;

    // 解析最多8个字段，少字段也可
    int n = sscanf(line, "cpu %lu %lu %lu %lu %lu %lu %lu %lu",
                   &stats->user, &stats->nice, &stats->system, &stats->idle,
                   &stats->iowait, &stats->irq, &stats->softirq, &stats->steal);

    if (n < 4) return -1; // 至少要有 user/nice/system/idle

    return 0;
}

// 计算 CPU 使用率百分比
static float get_cpu_usage_percent() {
    cpu_stats_t stat1, stat2;
    if (read_cpu_stats(&stat1) != 0) return -1;

    usleep(100000); // 100ms

    if (read_cpu_stats(&stat2) != 0) return -1;

    unsigned long idle1 = stat1.idle + stat1.iowait;
    unsigned long idle2 = stat2.idle + stat2.iowait;

    unsigned long total1 = stat1.user + stat1.nice + stat1.system + stat1.idle +
                           stat1.iowait + stat1.irq + stat1.softirq + stat1.steal;
    unsigned long total2 = stat2.user + stat2.nice + stat2.system + stat2.idle +
                           stat2.iowait + stat2.irq + stat2.softirq + stat2.steal;

    unsigned long total_delta = total2 - total1;
    unsigned long idle_delta  = idle2 - idle1;

    if (total_delta == 0) return 0.0f;

    return (total_delta - idle_delta) * 100.0f / total_delta;
}

static void device_attribute_cyc_msg_pub()
{
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;

    char *ptr;
    time_t rawtime;
    u32 tick;

    tick = get_tick_count();
    if(tick - remote_management_device_attribute_cyc_tick > 60000)    //
    {
            char topic_tmp[128];
            sprintf(topic_tmp,remote_management_device_attribute_topic,get_terminal_id());
            printf("device_attribute_cyc_msg_topic:%s\n",topic_tmp);
            if(topic_tmp != NULL)
            {
                    cJSON *json_obj = cJSON_CreateObject();
                    if(json_obj)
                    {
                        cJSON *json_obj_params = cJSON_CreateObject();
                        cJSON *json_obj_sys = cJSON_CreateObject();
                        if(json_obj_params&&json_obj_sys)
                        {
                        	char msg_id_string[32]={0};
                        	sprintf(msg_id_string,"ota-%u",remote_management_pub_num);
                            cJSON_AddStringToObject(json_obj, "id", msg_id_string);
                            cJSON_AddStringToObject(json_obj, "version", remote_management_version);
                            cJSON_AddStringToObject(json_obj, "sn", get_terminal_id());
                            cJSON_AddItemToObject(json_obj, "sys", json_obj_sys);
                            cJSON_AddNumberToObject(json_obj_sys,"ack",0);
                            time(&rawtime);
                            cJSON_AddNumberToObject(json_obj,"time", rawtime * 1000);
                            cJSON_AddItemToObject(json_obj, "params", json_obj_params);
                            cJSON_AddNumberToObject(json_obj_params, "appstatus", is_app_running());
                            cJSON_AddNumberToObject(json_obj_params, "memoryusage", get_memory_usage_percent());
                            cJSON_AddNumberToObject(json_obj_params, "diskusage", get_disk_usage_percent("/"));
                            cJSON_AddNumberToObject(json_obj_params, "cpuusage", get_cpu_usage_percent());

                            ptr = cJSON_Print(json_obj);
                            if(ptr)
                            {
                                pubmsg.payload = (void *)ptr;
                                pubmsg.payloadlen = strlen(pubmsg.payload);
                                pubmsg.qos = 0;
                                pubmsg.retained = 0;
                                if (MQTTClient_publishMessage(remote_management_client, topic_tmp, &pubmsg, &token) != MQTTCLIENT_SUCCESS)
                                {
                                	remote_management_online_flag=0;
                                }
                                remote_management_pub_num++;
                                free(ptr);
                                usleep(10000);
                            }
                        }
                        cJSON_Delete(json_obj);
                    }
            }
        remote_management_device_attribute_cyc_tick = get_tick_count();
    }
}
static void remote_management_ota_progress_handler(const char *service_id, UPDATE_STATUS status, const char *msg)
{
    printf("into ota_progress handler,service_id:%s,status:%d\n",service_id,status);

    if(remote_management_online_flag==0)
    	return;
    if(!msg)
    	return;

    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;

    char *ptr;
    time_t rawtime;

            char topic_tmp[128];
            sprintf(topic_tmp,remote_management_device_ota_progress_topic,get_terminal_id());
            printf("remote_management_device_ota_progress_topic:%s\n",topic_tmp);
            if(topic_tmp != NULL)
            {
                    cJSON *json_obj = cJSON_CreateObject();
                    if(json_obj)
                    {
                        cJSON *json_obj_params = cJSON_CreateObject();
                        if(json_obj_params)
                        {
                            char status_string[16];
                            sprintf(status_string,"%d",status);
                            cJSON_AddStringToObject(json_obj, "id", service_id);
                            cJSON_AddItemToObject(json_obj, "params", json_obj_params);
                            cJSON_AddStringToObject(json_obj_params,"step",status_string);
                            cJSON_AddStringToObject(json_obj_params,"desc",msg);
                            ptr = cJSON_Print(json_obj);
                            if(ptr)
                            {
                                pubmsg.payload = (void *)ptr;
                                pubmsg.payloadlen = strlen(pubmsg.payload);
                                pubmsg.qos = 0;
                                pubmsg.retained = 0;
                                if (MQTTClient_publishMessage(remote_management_client, topic_tmp, &pubmsg, &token) != MQTTCLIENT_SUCCESS)
                                	remote_management_online_flag=0;
                                free(ptr);
                                usleep(10000);
                            }
                        }
                        cJSON_Delete(json_obj);
                    }
            }
}

static void remote_management_ota_inform_handler(const char *service_id, const char *msg)
{
    printf("into ota_inform handler,service_id:%s,msg:%s\n",service_id,msg);

    if(remote_management_online_flag==0)
    	return;
    if(!msg)
    	return;

    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;

    char *ptr;
    time_t rawtime;

            char topic_tmp[128];
            sprintf(topic_tmp,remote_management_device_ota_inform_topic,get_terminal_id());
            printf("remote_management_device_ota_inform_topic:%s\n",topic_tmp);
            if(topic_tmp != NULL)
            {
                    cJSON *json_obj = cJSON_CreateObject();
                    if(json_obj)
                    {
                        cJSON *json_obj_params = cJSON_CreateObject();
                        if(json_obj_params)
                        {
                            cJSON_AddStringToObject(json_obj, "id", service_id);
                            cJSON_AddStringToObject(json_obj, "sn", get_terminal_id());
                            cJSON_AddItemToObject(json_obj, "params", json_obj_params);
                            cJSON_AddStringToObject(json_obj_params,"version",msg);
                            ptr = cJSON_Print(json_obj);
                            if(ptr)
                            {
                                pubmsg.payload = (void *)ptr;
                                pubmsg.payloadlen = strlen(pubmsg.payload);
                                pubmsg.qos = 0;
                                pubmsg.retained = 0;
                                if (MQTTClient_publishMessage(remote_management_client, topic_tmp, &pubmsg, &token) != MQTTCLIENT_SUCCESS)
                                	remote_management_online_flag=0;
                                free(ptr);
                                usleep(10000);
                            }
                        }
                        cJSON_Delete(json_obj);
                    }
            }
}

// ----------------------------------------------------------------------
// 核心逻辑: OTA 升级处理函数
// ----------------------------------------------------------------------

static int ota_upgrade_handler(const ota_upgrade_cmd_t *cmd)
{
    int ret = -1;
    char zip_filename[MAX_PATH_LEN] = {0};
    char zip_path[MAX_PATH_LEN] = {0};
    char extract_dir[MAX_PATH_LEN] = {0};
    char delete_dir[MAX_PATH_LEN] = {0};
    char base_filename[MAX_PATH_LEN] = {0};
    long downloaded_size;
    char calculated_md5[MAX_MD5_LEN] = {0};
    char calculated_sha1[MAX_SHA1_LEN] = {0};
    char calculated_sha256[MAX_SHA256_LEN] = {0};
    // 清理临时文件和目录
    char cleanup_cmd[MAX_PATH_LEN * 2]={0};

    const char *target_dir = OTA_TARGET_DIR;
    const char *download_dir = OTA_DOWNLOAD_DIR;
    const char *so_target_dir = SO_TARGET_DIR;

    printf("Starting OTA upgrade for version: %s\n", cmd->data.version);

    // 1. 确定文件名和解压目录：使用版本号作为基准名
    snprintf(base_filename, sizeof(base_filename), "ota_%s", cmd->data.version);
    snprintf(zip_filename, sizeof(zip_filename), "%s.zip", base_filename);
    snprintf(zip_path, sizeof(zip_path), "%s%s", download_dir, zip_filename);
    snprintf(extract_dir, sizeof(extract_dir), "%s%s/", download_dir, base_filename);
    snprintf(delete_dir, sizeof(delete_dir), "%s%s/", download_dir, base_filename);


    // ---- 步骤 1. 文件下载 (强制覆盖) ----
    if (http_download_file(cmd->data.url, zip_path) != 0) {
        remote_management_ota_progress_handler(cmd->id, DOWNLOAD_FAILED, "下载失败");
        goto cleanup;
    } else {
        remote_management_ota_progress_handler(cmd->id, 20, "下载成功");
    }

    // ---- 步骤 2. 大小校验 ----
    downloaded_size = get_file_size(zip_path);
    if (downloaded_size == -1 || downloaded_size != cmd->data.size) {
        remote_management_ota_progress_handler(cmd->id, VERIFY_FAILED, "大小校验失败");
        goto cleanup;
    } else {
        remote_management_ota_progress_handler(cmd->id, 30, "大小校验成功");
    }

    // ---- 步骤 3. 签名/MD5 校验 ----
    if (md5_file(zip_path, calculated_md5) != 0) {
        remote_management_ota_progress_handler(cmd->id, VERIFY_FAILED, "MD5签名校验失败");
        goto cleanup;
    }

    // 3.1 md5 字段比对 (不区分大小写)
    if (strcasecmp(calculated_md5, cmd->data.md5) != 0) {
        remote_management_ota_progress_handler(cmd->id, VERIFY_FAILED, "MD5字段校验失败");
        goto cleanup;
    } else {
        remote_management_ota_progress_handler(cmd->id, 40, "MD5字段校验成功");
    }

    // 3.2 Sign 字段比对 (不区分大小写)
    if (strcasecmp(cmd->data.signMethod, "md5") == 0 || strlen(cmd->data.signMethod) == 0) {
        if (strcasecmp(calculated_md5, cmd->data.sign) != 0) {
            remote_management_ota_progress_handler(cmd->id, VERIFY_FAILED, "SIGN字段md5校验失败");
            goto cleanup;
        } else {
            remote_management_ota_progress_handler(cmd->id, 70, "SIGN字段md5校验成功");
        }
    } else if (strcasecmp(cmd->data.signMethod, "sha-1") == 0) {
        if (sha1_file(zip_path, calculated_sha1) != 0) {
            remote_management_ota_progress_handler(cmd->id, VERIFY_FAILED, "SHA-1签名校验失败");
            goto cleanup;
        }
        if (strcasecmp(calculated_sha1, cmd->data.sign) != 0) {
            remote_management_ota_progress_handler(cmd->id, VERIFY_FAILED, "SIGN字段sha-1校验失败");
            goto cleanup;
        } else {
            remote_management_ota_progress_handler(cmd->id, 70, "SIGN字段sha-1校验成功");
        }    
    } else if (strcasecmp(cmd->data.signMethod, "sha-256") == 0) {
        if (sha256_file(zip_path, calculated_sha256) != 0) {
            remote_management_ota_progress_handler(cmd->id, VERIFY_FAILED, "SHA-256签名校验失败");
            goto cleanup;
        }
        if (strcasecmp(calculated_sha256, cmd->data.sign) != 0) {
            remote_management_ota_progress_handler(cmd->id, VERIFY_FAILED, "SIGN字段sha-256校验失败");
            goto cleanup;
        }  else {
            remote_management_ota_progress_handler(cmd->id, 70, "SIGN字段sha-256校验成功");
        }   
    }
    // ---- 步骤 4. 文件解压 (强制覆盖到同名目录) ----
    if (unzip_file(zip_path, extract_dir) != 0) {
        remote_management_ota_progress_handler(cmd->id, FLASH_FAILED, "解压失败");
        goto cleanup;
    }

    // ---- 步骤 5 & 6. 文件清理、复制与权限设置 ----
    DIR *dir;
    struct dirent *entry;

    dir = opendir(extract_dir);
    if (dir == NULL) {
        remote_management_ota_progress_handler(cmd->id, FLASH_FAILED, "进入解压文件失败");
        goto cleanup;
    }

    // 检测是否只有一个顶层子目录
    int dir_count = 0;
    char single_dir_name[MAX_PATH_LEN] = {0};

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char full_path[MAX_PATH_LEN];
        snprintf(full_path, sizeof(full_path), "%s%s", extract_dir, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0)
            continue;

        if (S_ISDIR(st.st_mode)) {
            dir_count++;
            strncpy(single_dir_name, full_path, sizeof(single_dir_name) - 1);
        }
    }
    closedir(dir);

    // 如果只有一个目录，把 extract_dir 更新为子目录
    if (dir_count == 1) {
    printf("Detected single nested dir: %s\n", single_dir_name);

    // 保证末尾有 '/'
    size_t l = strlen(single_dir_name);
    if (l > 0 && single_dir_name[l-1] != '/')
        single_dir_name[l] = '/', single_dir_name[l+1] = '\0';

    strncpy(extract_dir, single_dir_name, MAX_PATH_LEN - 1);
    }

    // ---- 遍历文件，执行删除/复制/权限设置 ----
    dir = opendir(extract_dir);
    if (dir == NULL) {
        remote_management_ota_progress_handler(cmd->id, FLASH_FAILED, "进入解压文件失败");
        goto cleanup;
    }

    while ((entry = readdir(dir)) != NULL) {
        // 跳过目录和特殊项
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || entry->d_type == DT_DIR) {
            continue;
        }

        const char *suffix = strrchr(entry->d_name, '.');

        // ---- 新增：处理 .so 文件，只放入 /usr/lib/ ----
        if (suffix && strcmp(suffix, ".so") == 0) {
            char old_lib_path[MAX_PATH_LEN];
            char new_lib_path[MAX_PATH_LEN];
            char copy_lib_cmd[MAX_PATH_LEN * 2];

            snprintf(old_lib_path, sizeof(old_lib_path), "%s%s", so_target_dir, entry->d_name);
            snprintf(new_lib_path, sizeof(new_lib_path), "%s%s", extract_dir, entry->d_name);

            // 删除旧的 .so
            if (access(old_lib_path, F_OK) == 0) {
                char cmd[512];
                snprintf(cmd, sizeof(cmd), "wr rm -rf '%s'", old_lib_path);
                printf("Force deleting old .sotarget: %s\n", old_lib_path);
                system(cmd);
            }

            // 复制新的 .so 文件到 /usr/lib/
            snprintf(copy_lib_cmd, sizeof(copy_lib_cmd), "wr cp -f %s %s", new_lib_path, so_target_dir);
            if (system(copy_lib_cmd) != 0) {
                printf("Error: Failed to copy %s to %s\n", entry->d_name, so_target_dir);
                closedir(dir);
                remote_management_ota_progress_handler(cmd->id, FLASH_FAILED, "lib烧写失败");
                goto cleanup;
            } else {
                remote_management_ota_progress_handler(cmd->id, 90, "lib烧写成功");
            }

            // 1. 设置权限
            char final_lib_path[MAX_PATH_LEN];
            snprintf(final_lib_path, sizeof(final_lib_path), "%s%s", so_target_dir, entry->d_name);

            // 2. 构造 wr chmod 命令
            char chmod_so_cmd[512];
            snprintf(chmod_so_cmd, sizeof(chmod_so_cmd), "wr chmod 777 '%s'", final_lib_path);

            // 3. 执行命令
            int ret = system(chmod_so_cmd);

            continue; // 直接跳过 /usr/pmf406/ 的逻辑
        }

        char old_target_path[MAX_PATH_LEN];
        char new_source_path[MAX_PATH_LEN];
        char copy_cmd[MAX_PATH_LEN * 2];

        // 5.1 备份文件
        backup_file_if_needed(old_target_path, entry->d_name);

        // 5.2 删除 /usr/pmf406/ 目录下同名旧文件
        snprintf(old_target_path, sizeof(old_target_path), "%s%s", target_dir, entry->d_name);
        if (access(old_target_path, F_OK) == 0) {
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "wr rm -rf '%s'", old_target_path);
            printf("Force deleting old target: %s\n", old_target_path);
            system(cmd);
        }

        // 5.3 复制新文件到 /usr/pmf406/ (强制覆盖)
        snprintf(new_source_path, sizeof(new_source_path), "%s%s", extract_dir, entry->d_name);

        // 使用 cp -f 强制覆盖复制
        snprintf(copy_cmd, sizeof(copy_cmd), "wr cp -f %s %s", new_source_path, target_dir);
        if (system(copy_cmd) != 0) {
            closedir(dir);
            remote_management_ota_progress_handler(cmd->id, FLASH_FAILED, "烧写失败");
            goto cleanup;
        } else {
            remote_management_ota_progress_handler(cmd->id, 90, "烧写成功");
            if (strcmp(entry->d_name, "remote_management_ota") == 0) {
                ota_updated = 1;
            }
        }

        // 步骤 6. 权限设置 (设置为可执行 rwxrwxrwx)
        char final_target_path[MAX_PATH_LEN];
        snprintf(final_target_path, sizeof(final_target_path), "%s%s", target_dir, entry->d_name);
        // 2. 构造 wr chmod 命令
        char chmod_cmd[512];
        snprintf(chmod_cmd, sizeof(chmod_cmd), "wr chmod 777 '%s'", final_target_path);

        // 执行命令并检查返回值
        int ret = system(chmod_cmd);
        if (ret != 0) {
            fprintf(stderr, "wr chmod failed for %s, ret=%d\n", final_target_path, ret);
        }
    }
    closedir(dir);
    // 判断是否更新过ota程序
    if (ota_updated) {
        updated_ota_bin[0] = '1';
        updated_ota_bin[1] = '\0';
    }
    // 在重启前，写入版本和ID到 upgrade.txt
    if (write_ota_reboot_status(cmd->data.version, cmd->id, updated_ota_bin) != 0) {
        printf("Warning: Failed to write upgrade.txt status file.\n");
    }

    ret = 0;

    // ---- 步骤 7. 系统重启 ----
    printf("OTA files deployed successfully. Executing system reboot...\n");
    system("sync; reboot &");

cleanup:


    // 清理解压目录
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf %s", delete_dir);
    system(cleanup_cmd);

    // 清理下载的ZIP文件
    remove(zip_path);

    return ret;
}



static int mqtt_connect(MQTTClient *client)
{
#if 0
    MQTTClient_connectOptions opts = MQTTClient_connectOptions_initializer;
    MQTTClient_SSLOptions      ssl  = MQTTClient_SSLOptions_initializer;

    ssl.trustStore   = "/etc/ssl/certs/ca-certificates.crt"; /* Linux 自带 CA */
    ssl.enableServerCertAuth = 1;

    opts.keepAliveInterval = KEEPALIVE;
    opts.cleansession        = 1;
    opts.ssl                 = &ssl;

    int rc = MQTTClient_connect(*client, &opts);
    if (rc == MQTTCLIENT_SUCCESS)
        printf("[I] SSL connected to %s\n", ADDRESS);
    else
        printf("[E] connect failed, rc=%d\n", rc);
    return rc;
#endif
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = 60;
    conn_opts.cleansession = 1;
    conn_opts.MQTTVersion = MQTTVERSION_3_1_1;
    conn_opts.connectTimeout = 60;
    conn_opts.username="406ota";
    conn_opts.password="406ota";

    int rc = MQTTClient_connect(*client, &conn_opts);
    if (rc == MQTTCLIENT_SUCCESS)
    {
    	remote_management_online_flag=1;
        printf("[I] SSL connected to %s\n", ADDRESS);
    }
    else
        printf("[E] connect failed, rc=%d\n", rc);
    return rc;
}


static void delivered(void *context, MQTTClient_deliveryToken dt)
{
    printf("remote_management Message with token value %d delivery confirmed\n", dt);
    remote_management_deliveredtoken = dt;
}

static int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    char* payloadptr;
    printf("remote_management Message arrived\n");

    char *sn = get_terminal_id();
    char expected_topic[128];
    snprintf(expected_topic, sizeof(expected_topic), remote_management_device_ota_upgrade_topic, sn);
    if (strcmp(topicName, expected_topic) == 0)
    {
        ota_upgrade_cmd_t cmd = {0};
        cJSON *root = cJSON_Parse((const char *)message->payload);

        if (root)
        {
            cJSON *id_item = cJSON_GetObjectItem(root, "id");
            cJSON *data_item = cJSON_GetObjectItem(root, "data");

            if (id_item && cJSON_IsString(id_item)) strncpy(cmd.id, id_item->valuestring, sizeof(cmd.id) - 1);

            if (data_item && cJSON_IsObject(data_item)) {
                cJSON *url_item = cJSON_GetObjectItem(data_item, "url");
                cJSON *version_item = cJSON_GetObjectItem(data_item, "version");
                cJSON *md5_item = cJSON_GetObjectItem(data_item, "md5");
                cJSON *signMethod_item = cJSON_GetObjectItem(data_item, "signMethod");
                cJSON *sign_item = cJSON_GetObjectItem(data_item, "sign");
                cJSON *size_item = cJSON_GetObjectItem(data_item, "size");

                if (url_item && cJSON_IsString(url_item)) strncpy(cmd.data.url, url_item->valuestring, sizeof(cmd.data.url) - 1);
                if (version_item && cJSON_IsString(version_item)) strncpy(cmd.data.version, version_item->valuestring, sizeof(cmd.data.version) - 1);
                if (md5_item && cJSON_IsString(md5_item)) strncpy(cmd.data.md5, md5_item->valuestring, sizeof(cmd.data.md5) - 1);
                if (signMethod_item && cJSON_IsString(signMethod_item)) strncpy(cmd.data.signMethod, signMethod_item->valuestring, sizeof(cmd.data.signMethod) - 1);
                if (sign_item && cJSON_IsString(sign_item)) strncpy(cmd.data.sign, sign_item->valuestring, sizeof(cmd.data.sign) - 1);
                if (size_item && cJSON_IsNumber(size_item)) cmd.data.size = size_item->valueint;
            }

            cJSON_Delete(root);

            // 检查关键参数（防止崩溃）
            if (strlen(cmd.data.url) > 0 && cmd.data.size > 0 && strlen(cmd.data.version) > 0) {
                ota_upgrade_handler(&cmd);
            } else {
                 // 参数不全，报升级失败
                remote_management_ota_progress_handler(cmd.id, UPDATE_FAILED, "升级失败: 参数缺失");
            }
        }

        return 1;
    }
    payloadptr = message->payload;
        cJSON* cjson_up = cJSON_Parse(payloadptr);//将JSON字符串转换成JSON结构体
    if(cjson_up == NULL)                       //判断转换是否成功
    {
         MQTTClient_freeMessage(&message);
         MQTTClient_free(topicName);
        return 1;
    }
    else
    {

    }
    cJSON_Delete(cjson_up);
    return 1;
}

static void connlost(void *context, char *cause)
{
	remote_management_online_flag=0;
    printf("\remote_management_Connection lost\n");
    printf("     cause: %s\n", cause);
}
/******************************************************************************
**  函    数:   remote_management_thread_entry()
**  功    能:   远程管理任务
**  参    数:   parameter     -   配置信息
**  返    回:   无
**  作    者:   李鹏博
**  创建日期:   2025-09-04
**  说    明:
**
**-----------------------------------历史版本-----------------------------------
**  作    者:
**  修改日期:
**  说    明:
**
******************************************************************************/
void *remote_management_thread_entry(void *parameter)
{

    char remote_management_client_name[64]={0};
    sprintf(remote_management_client_name,"%s%s","remote_management_",get_terminal_id());

    int rc = MQTTClient_create(&remote_management_client, ADDRESS, remote_management_client_name,
                               MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (rc != MQTTCLIENT_SUCCESS)
    {
        printf("remote management create failed\n");
        return NULL;
    }
    if (MQTTCLIENT_SUCCESS== MQTTClient_setCallbacks(remote_management_client, NULL, connlost, msgarrvd, delivered))
    {
        printf("mqttupClient_setCallbacks_success\n");
    }


    char *sn = get_terminal_id();
    printf("sn = %s\n", sn);
    char ota_topic[128];
    snprintf(ota_topic, sizeof(ota_topic), remote_management_device_ota_upgrade_topic, sn);

    while (1)
    {
        if (mqtt_connect(&remote_management_client) == MQTTCLIENT_SUCCESS)
        {
            printf("remote management connect success\n");

            // ---- 步骤 8. mqtt连接完成后状态判断与上报 (检查 upgrade.txt) ----
            ota_reboot_status_t *status = check_ota_finish_status();
            if (status != NULL)
            {
                // 升级成功，调用 ota_inform 上报成功消息
                remote_management_ota_progress_handler(status->id, 100, "升级成功");
                remote_management_ota_inform_handler(status->id, status->version);
                printf("OTA upgrade success reported for version: %s (ID: %s)\n", status->version, status->id);
                // 如果升级的文件中包含 OTA 主程序，清空 ota_fail_count
                if (strcmp(status->ota, "1") == 0) {
                    FILE *f = fopen(OTA_FAIL_COUNT_FILE, "w");
                    if (f) {
                        fprintf(f, "0\n");
                        fclose(f);
                        printf("OTA updated: reset ota_fail_count to 0.\n");
                    } else {
                        printf("Warning: Failed to reset ota_fail_count file.\n");
                    }
                }
                // 清除标志文件
                clear_ota_finish_status();
            }
            // 订阅 OTA 升级主题
            MQTTClient_subscribe(remote_management_client, ota_topic, QOS);

            while (1)
            {
                device_attribute_cyc_msg_pub();
            	if(remote_management_online_flag==0)
            		break;
                sleep(1);
            }
        }
            printf("remote management connect failed\n");
            MQTTClient_disconnect(remote_management_client, 0);
        	sleep(10);
    }

    MQTTClient_destroy(&remote_management_client);
    return NULL;
}


void remote_management_thread_init(void)
{
    pthread_t remote_management_thread;
    snprintf(CLIENTID, sizeof(CLIENTID), "%s_ota", get_terminal_id());
    if (pthread_create(&remote_management_thread, NULL, remote_management_thread_entry, NULL) != 0)
    {
        printf("remote_management_thread error\n");
        g_ota_service_running = 0;
        return;
    }
    return;
}


/******************************************************************************
  End File
******************************************************************************/
