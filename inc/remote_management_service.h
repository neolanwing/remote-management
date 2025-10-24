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

#ifndef _REMOTE_MANAGEMENT_SEVICE_H_
#define _REMOTE_MANAGEMENT_SEVICE_GLOBALS_H_


#ifdef  REMOTE_MANAGEMENT_SEVICE_GLOBALS
#define REMOTE_MANAGEMENT_SEVICE_EXT
#else
#define  REMOTE_MANAGEMENT_SEVICE_EXT  extern
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
} ota_reboot_status_t;
// 用于升级指令的结构体
typedef struct
{
    char id[32]; // 命令ID
    int code;
    struct {
        int size;
        char sign[64];
        char version[64];
        int isDiff;
        char url[MAX_PATH_LEN];
        char signMethod[32];
        char md5[64];
    } data;
} ota_upgrade_cmd_t;
/******************************************************************************
**全局变量定义
******************************************************************************/
typedef enum
{
	REMOTE_MANAGEMENT_DEBUG,        //调试信息
	REMOTE_MANAGEMENT_INFO,         //运行信息
	REMOTE_MANAGEMENT_WARN,         //告警信息
	REMOTE_MANAGEMENT_ERROR,        //可恢复类错误信息
	REMOTE_MANAGEMENT_FATAL,        //致命错误信息
}REMOTE_MANAGEMENT_ULOG_LEVEL;

typedef enum
{
	UPDATE_FAILED  = -1,    // 升级失败
	DOWNLOAD_FAILED = -2,   // 下载失败
	VERIFY_FAILED   = -3,   // 校验失败
	FLASH_FAILED    = -4,   // 烧写失败
} UPDATE_STATUS;

/******************************************************************************
**API函数实现
******************************************************************************/
REMOTE_MANAGEMENT_SEVICE_EXT void remote_management_fire_event(u32 service_id, const char *msg);
REMOTE_MANAGEMENT_SEVICE_EXT void remote_management_fire_ulog(REMOTE_MANAGEMENT_ULOG_LEVEL level, const char *msg);
REMOTE_MANAGEMENT_SEVICE_EXT void remote_management_fire_protocol_message(u32 service_id,u32 dev_id,SERVICE_PROTOCOL_TYPE protocol_type, const char *msg,u16 len);
REMOTE_MANAGEMENT_SEVICE_EXT void remote_management_fire_ota_progress(const char *service_id, UPDATE_STATUS status, const char *msg);
REMOTE_MANAGEMENT_SEVICE_EXT void remote_management_fire_ota_inform(const char *service_id, const char *msg);
REMOTE_MANAGEMENT_SEVICE_EXT void remote_management_ota_progress_handler(const char *service_id, UPDATE_STATUS status, const char *msg)
REMOTE_MANAGEMENT_SEVICE_EXT void remote_management_ota_inform_handler(const char *service_id, const char *msg)

/**
 * OTA升级相关函数声明
 */

// MD5 文件计算函数 (OpenSSL 实现)
REMOTE_MANAGEMENT_SEVICE_EXT int md5_file(const char *filePath, char *md5_out);

// 文件下载函数 (libcurl 实现)
// REMOTE_MANAGEMENT_SEVICE_EXT int http_download_file(const char *url, const char *savePath, int overwrite);
// 文件下载函数 (命令 实现)
REMOTE_MANAGEMENT_SEVICE_EXT int http_download_file(const char *url, const char *savePath);

// ZIP 解压函数 (system/unzip 实现)
REMOTE_MANAGEMENT_SEVICE_EXT int unzip_file(const char *zipPath, const char *destDir);

// 获取文件大小 (POSIX stat 实现)
REMOTE_MANAGEMENT_SEVICE_EXT long get_file_size(const char *filePath);

// OTA 升级核心处理函数
REMOTE_MANAGEMENT_SEVICE_EXT int ota_upgrade_handler(const ota_upgrade_cmd_t *cmd);

// 升级状态文件操作
REMOTE_MANAGEMENT_SEVICE_EXT int write_ota_reboot_status(const char *version, const char *id);
REMOTE_MANAGEMENT_SEVICE_EXT ota_reboot_status_t *check_ota_finish_status(void);
REMOTE_MANAGEMENT_SEVICE_EXT void clear_ota_finish_status(void);

#ifdef __cplusplus
}
#endif
#endif   /* end */

                                                      