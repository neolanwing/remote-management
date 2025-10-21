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
#define _REMOTE_MANAGEMENT_SEVICE_PRIVATE_GLOBALS_H_


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

/******************************************************************************
**枚举定义
******************************************************************************/

/******************************************************************************
**结构类型定义
******************************************************************************/
typedef void (*remote_management_event_cb_t)(u32 service_id, const char *msg);

typedef void (*remote_management_ulog_cb_t)(REMOTE_MANAGEMENT_ULOG_LEVEL level, const char *msg);

typedef void (*remote_management_protocol_message_cb_t)(u32 service_id,u32 dev_id,SERVICE_PROTOCOL_TYPE protocol_type, const char *msg,u16 len);

typedef void (*remote_management_ota_progress_cb_t)(u32 service_id, UPDATE_STATUS status, const char *msg);

typedef void (*remote_management_ota_inform_cb_t)(u32 service_id, const char *msg);
/******************************************************************************..............
**全局变量定义
******************************************************************************/

/******************************************************************************
**API函数实现
******************************************************************************/
REMOTE_MANAGEMENT_SEVICE_PRIVATE_EXT int remote_management_register_event_cb(remote_management_event_cb_t cb);
REMOTE_MANAGEMENT_SEVICE_PRIVATE_EXT int remote_management_register_ulog_cb(remote_management_ulog_cb_t cb);
REMOTE_MANAGEMENT_SEVICE_PRIVATE_EXT int remote_management_register_protocol_message_cb(remote_management_protocol_message_cb_t cb);
REMOTE_MANAGEMENT_SEVICE_PRIVATE_EXT int remote_management_register_ota_progress_cb(remote_management_ota_progress_cb_t cb);
REMOTE_MANAGEMENT_SEVICE_PRIVATE_EXT int remote_management_register_ota_inform_cb(remote_management_ota_inform_cb_t cb);

#ifdef __cplusplus
}
#endif
#endif   /* end */

