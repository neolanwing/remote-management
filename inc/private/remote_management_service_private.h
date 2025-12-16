/*********************************Copyright (c)**********************************
**
**                        河南许继智能科技股份有限公司
**                              www.xjpmf.com
**
**-----------------------------------文件信息-----------------------------------
**  项目名称    ：remote_management_service通用库
**  文    件    ：remote_management_service.h
**  作    者    ：李鹏博
**  版    本    ：1.00
**  创建日期    ：2025-09-17
**  描    述    ：remote_management_service接口定义
**
**-----------------------------------历史版本-----------------------------------
**  作    者    ：
**  版    本    ：
**  修改日期    ：
**  描    述    ：
**
********************************************************************************/

#ifndef _REMOTE_MANAGEMENT_SEVICE_PRIVATE_H_
#define _REMOTE_MANAGEMENT_SEVICE_PRIVATE_H_


#ifdef  REMOTE_MANAGEMENT_SEVICE_PRIVATE_GLOBALS
#define REMOTE_MANAGEMENT_SEVICE_PRIVATE_EXT
#else
#define  REMOTE_MANAGEMENT_SEVICE_PRIVATE_EXT  extern
#endif

/******************************************************************************
**C库头文件
******************************************************************************/

/******************************************************************************
**底层驱动平台头文件
******************************************************************************/

/******************************************************************************
**基础平台头文件
******************************************************************************/

/******************************************************************************
**应用头文件
******************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************
**宏定义
******************************************************************************/
#define REMENTE_MANAGEMENT_QSIZE 16
// 下载和解压目录
#define OTA_DOWNLOAD_DIR        "/opt/update/"
// 目标程序安装目录
#define OTA_TARGET_DIR          "/usr/pmf406/"
// so文件安装目录
#define SO_TARGET_DIR           "/usr/lib/"
#define BACKUP_LIST_FILE        "/var/lib/backup_file"
#define BACKUP_DIR              "/opt/apps/backup"
#define MAX_PATH_LEN            256
#define MAX_MD5_LEN             33  // 32 字符 + '\0'
#define MAX_SHA1_LEN            41  // 40 字符 + '\0'
#define MAX_SHA256_LEN          65  // 64 字符 + '\0'

// 用于重启后检查的标志文件
#define OTA_STATUS_FILE         "upgrade.txt"
/******************************************************************************
**枚举定义
******************************************************************************/

/******************************************************************************
**结构类型定义
******************************************************************************/
// 用于存储重启时升级信息的结构体
typedef struct {
    char version[64];
    char id[32];
    char ota[32];
} ota_reboot_status_t;
// 用于升级指令的结构体
typedef struct
{
    char id[32]; // 命令ID
    int code;
    struct {
        int size;
        char sign[128];
        char version[64];
        int isDiff;
        char url[MAX_PATH_LEN];
        char signMethod[32];
        char md5[64];
    } data;
} ota_upgrade_cmd_t;

typedef enum
{
	UPDATE_FAILED  = -1,    // 升级失败
	DOWNLOAD_FAILED = -2,   // 下载失败
	VERIFY_FAILED   = -3,   // 校验失败
	FLASH_FAILED    = -4,   // 烧写失败
} UPDATE_STATUS;


/******************************************************************************..............
**全局变量定义
******************************************************************************/
extern volatile int g_ota_service_running;
/******************************************************************************
**API函数实现
******************************************************************************/

// MD5 文件计算函数 (OpenSSL 实现)
REMOTE_MANAGEMENT_SEVICE_PRIVATE_EXT int md5_file(const char *filePath, char *md5_out);
// SHA-1 文件计算函数 (OpenSSL 实现)
REMOTE_MANAGEMENT_SEVICE_PRIVATE_EXT int sha1_file(const char *filePath, char *sha1_out);
// SHA-256 文件计算函数 (OpenSSL 实现)
REMOTE_MANAGEMENT_SEVICE_PRIVATE_EXT int sha256_file(const char *filePath, char *sha256_out);

// 文件下载函数 (命令 实现)
REMOTE_MANAGEMENT_SEVICE_PRIVATE_EXT int http_download_file(const char *url, const char *savePath);

// ZIP 解压函数 (system/unzip 实现)
REMOTE_MANAGEMENT_SEVICE_PRIVATE_EXT int unzip_file(const char *zipPath, const char *destDir);

// 获取文件大小 (POSIX stat 实现)
REMOTE_MANAGEMENT_SEVICE_PRIVATE_EXT long get_file_size(const char *filePath);

// 判断文件名是否在备份列表中 
REMOTE_MANAGEMENT_SEVICE_PRIVATE_EXT int is_file_in_backup_list(const char *filename);

// 备份文件到 BACKUP_DIR（不检查是否已存在，直接覆盖） 
REMOTE_MANAGEMENT_SEVICE_PRIVATE_EXT void backup_file_if_needed(const char *file_path, const char *filename);

// OTA 升级核心处理函数
//REMOTE_MANAGEMENT_SEVICE_PRIVATE_EXT int ota_upgrade_handler(const ota_upgrade_cmd_t *cmd);

// 升级状态文件操作
REMOTE_MANAGEMENT_SEVICE_PRIVATE_EXT int write_ota_reboot_status(const char *version, const char *id, const char *ota);
REMOTE_MANAGEMENT_SEVICE_PRIVATE_EXT ota_reboot_status_t *check_ota_finish_status(void);
REMOTE_MANAGEMENT_SEVICE_PRIVATE_EXT void clear_ota_finish_status(void);



#ifdef __cplusplus
}
#endif
#endif   /* end */

