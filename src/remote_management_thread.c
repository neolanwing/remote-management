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
#define ADDRESS   "10.30.103.27:1883"
#define CLIENTID  "syncReThread"
#define TOPIC     "demo/status"
#define QOS       1
#define KEEPALIVE 20
#define version  "1.0"

/******************************************************************************
**枚举定义
******************************************************************************/

/******************************************************************************
**结构类型定义
******************************************************************************/

/******************************************************************************
**静态变量定义
******************************************************************************/
static MQTTClient remote_management_client;
static u8 remote_management_online_flag = 0;
static u32 remote_management_device_attribute_cyc_tick = 0;
static u32 remote_management_pub_num = 0;
static volatile MQTTClient_deliveryToken remote_management_deliveredtoken;

static u8  remote_management_event_switch = 1;
static u8  remote_management_log_switch = 0;
static u8  remote_management_protocol_switch = 0;
//发布主题
char remote_management_device_attribute_topic[64]="/sys/%s/iot/post";
char remote_management_device_event_topic[64]="/sys/%s/iot/event/post";
char remote_management_device_protocolmessage_topic[64]="/sys/%s/protocolmessage/post";
char remote_management_device_log_topic[64]="/sys/%s/log/post";
char remote_management_device_ota_progress_topic[64]="/ota/device/progress/%s";
char remote_management_device_ota_inform_topic[64]="/ota/device/inform/%s";
char remote_management_device_ota_upgrade_topic[64]="/ota/device/upgrade/%s";


/******************************************************************************
**API函数实现
******************************************************************************/
extern char *get_terminal_id ( void );
extern int system_init_flag(void);


static void get_hw_version(const char *xml_file,char *out)
{
    if (!out) return;
    FILE    *fp = fopen(xml_file, "r");
    if (!fp) return;
    mxml_node_t *tree = mxmlLoadFile(NULL, fp, MXML_TEXT_CALLBACK);
    fclose(fp);
    if (!tree) return;


    mxml_node_t *inf  = mxmlFindElement(tree, tree, "inf",  NULL, NULL, MXML_DESCEND_FIRST);
    mxml_node_t *comm = mxmlFindElement(inf,  inf,  "comm", NULL, NULL, MXML_DESCEND_FIRST);
    mxml_node_t *hw   = mxmlFindElement(comm, comm, "Hardware", NULL, NULL, MXML_DESCEND_FIRST);

    const char *ret = NULL;
    for (mxml_node_t *txt = mxmlGetFirstChild(hw);
         txt;
         txt = mxmlGetNextSibling(txt))
    {
        if (mxmlGetType(txt) == MXML_TEXT &&
            mxmlGetText(txt, NULL) != NULL &&
            mxmlGetText(txt, NULL)[0] != '\n' &&
            mxmlGetText(txt, NULL)[0] != '\0')
        {
            ret = mxmlGetText(txt, NULL);
            break;
        }
    }
    printf("yjbbh:%s\n",ret);
    strcpy(out,ret);
    mxmlDelete(tree);
}


static void get_cal_crc(const char *xml_file,char *out)
{
    if (!out) return;
    FILE *fp = fopen(xml_file, "r");
    if (!fp) return;

    mxml_node_t *tree = mxmlLoadFile(NULL, fp, MXML_TEXT_CALLBACK);
    fclose(fp);
    if (!tree) return;

    mxml_node_t *inf  = mxmlFindElement(tree, tree, "inf",  NULL, NULL, MXML_DESCEND_FIRST);
    mxml_node_t *comm = mxmlFindElement(inf,  inf,  "comm", NULL, NULL, MXML_DESCEND_FIRST);
    mxml_node_t *cal  = mxmlFindElement(comm, comm, "CAL",  NULL, NULL, MXML_DESCEND_FIRST);

    const char *ret = cal ? mxmlGetText(cal, NULL) : NULL;
    strcpy(out,ret);
    mxmlDelete(tree);
}


static void get_version(const char *xml_file,char *out)
{
    if (!out) return;
    FILE *fp = fopen(xml_file, "r");
    if (!fp) return;

    mxml_node_t *tree = mxmlLoadFile(NULL, fp, MXML_TEXT_CALLBACK);
    fclose(fp);
    if (!tree) return;

    mxml_node_t *inf  = mxmlFindElement(tree, tree, "inf",  NULL, NULL, MXML_DESCEND_FIRST);
    mxml_node_t *comm = mxmlFindElement(inf,  inf,  "comm", NULL, NULL, MXML_DESCEND_FIRST);
    mxml_node_t *ver  = mxmlFindElement(comm, comm, "Version", NULL, NULL, MXML_DESCEND_FIRST);

    const char *ret = ver ? mxmlGetText(ver, NULL) : NULL;
    printf("rjversion:%s\n",ret);
    strcpy(out,ret);
    mxmlDelete(tree);

}

void remote_management_event_handler(u32 service_id, const char *msg)
{
    printf("into event handler:%d,%s\n",service_id,msg);

    if(remote_management_online_flag==0)
    	return;
    if(!msg)
    	return;

    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;

    char *ptr;
    time_t rawtime;

            char topic_tmp[128];
            sprintf(topic_tmp,remote_management_device_event_topic,get_terminal_id());
            printf("remote_management_device_event_topic:%s\n",topic_tmp);
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
                        	sprintf(msg_id_string,"%u",remote_management_pub_num);
                            cJSON_AddStringToObject(json_obj, "id", msg_id_string);
                            cJSON_AddStringToObject(json_obj, "version", version);
                            cJSON_AddStringToObject(json_obj, "sn", get_terminal_id());
                            cJSON_AddItemToObject(json_obj, "sys", json_obj_sys);
                            cJSON_AddNumberToObject(json_obj_sys,"ack",0);
                            time(&rawtime);
                            cJSON_AddNumberToObject(json_obj,"time",rawtime);
                            cJSON_AddItemToObject(json_obj, "params", json_obj_params);
                            cJSON_AddNumberToObject(json_obj_params,"serviceid",service_id);
                            cJSON_AddStringToObject(json_obj_params, "eventmsg", msg);
                            ptr = cJSON_Print(json_obj);
                            if(ptr)
                            {
                                pubmsg.payload = (void *)ptr;
                                pubmsg.payloadlen = strlen(pubmsg.payload);
                                pubmsg.qos = 0;
                                pubmsg.retained = 0;
                                if (MQTTClient_publishMessage(remote_management_client, topic_tmp, &pubmsg, &token) != MQTTCLIENT_SUCCESS)
                                	remote_management_online_flag=0;
                                remote_management_pub_num++;
                                free(ptr);
                                usleep(10000);
                            }
                        }
                        cJSON_Delete(json_obj);
                    }
            }
}



void remote_management_ulog_handler(REMOTE_MANAGEMENT_ULOG_LEVEL level, const char *msg)
{
    printf("into ulog handler:%d,%s\n",level,msg);

    if(remote_management_online_flag==0)
    	return;
    if(!msg)
    	return;
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;

    char *ptr;
    time_t rawtime;

            char topic_tmp[128];
            sprintf(topic_tmp,remote_management_device_log_topic,get_terminal_id());
            printf("remote_management_device_ulog_topic:%s\n",topic_tmp);
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
                        	sprintf(msg_id_string,"%u",remote_management_pub_num);
                            cJSON_AddStringToObject(json_obj, "id", msg_id_string);
                            cJSON_AddStringToObject(json_obj, "version", version);
                            cJSON_AddStringToObject(json_obj, "sn", get_terminal_id());
                            cJSON_AddItemToObject(json_obj, "sys", json_obj_sys);
                            cJSON_AddNumberToObject(json_obj_sys,"ack",0);
                            cJSON_AddItemToObject(json_obj, "params", json_obj_params);
                            time(&rawtime);
                            cJSON_AddNumberToObject(json_obj_params,"time",rawtime);
                            char log_level[32]={0};
                            if(level == REMOTE_MANAGEMENT_DEBUG)
                                strcpy(log_level,"DEBUG");
                            else if(level == REMOTE_MANAGEMENT_INFO)
                                strcpy(log_level,"INFO");
                            else if(level == REMOTE_MANAGEMENT_WARN)
                                strcpy(log_level,"WARN");
                            else if(level == REMOTE_MANAGEMENT_ERROR)
                                strcpy(log_level,"ERROR");
                            else if(level == REMOTE_MANAGEMENT_FATAL)
                                strcpy(log_level,"FATAL");
                            else
                                strcpy(log_level,"DEBUG");
                            cJSON_AddStringToObject(json_obj_params,"logLevel",log_level);
                            cJSON_AddStringToObject(json_obj_params, "code", "0");
                            cJSON_AddStringToObject(json_obj_params, "logContent", msg);
                            ptr = cJSON_Print(json_obj);
                            if(ptr)
                            {
                                pubmsg.payload = (void *)ptr;
                                pubmsg.payloadlen = strlen(pubmsg.payload);
                                pubmsg.qos = 0;
                                pubmsg.retained = 0;
                                if (MQTTClient_publishMessage(remote_management_client, topic_tmp, &pubmsg, &token) != MQTTCLIENT_SUCCESS)
                                	remote_management_online_flag=0;
                                remote_management_pub_num++;
                                free(ptr);
                                usleep(10000);
                            }
                        }
                        cJSON_Delete(json_obj);
                    }
            }
}


void remote_management_protocol_message_handler(u32 service_id,u32 dev_id,SERVICE_PROTOCOL_TYPE protocol_type, const char *msg,u16 len)
{
    printf("into protocol_message handler,service_id:%d,dev_id:%d\n",service_id,dev_id);

    if(remote_management_online_flag==0)
    	return;
    if(!msg)
    	return;
    if((len>=1024)||(len == 0))
    	return;

    char protocol_type_buf[32]={0};
    switch (protocol_type)
    {
		case SERVICE_PROTOCOL_TYPE_IEC104_UP:
            strcpy(protocol_type_buf,"IEC104_UP");
			break;
		case SERVICE_PROTOCOL_TYPE_IEC101_UP:
            strcpy(protocol_type_buf,"IEC101_UP");
			break;
		case SERVICE_PROTOCOL_TYPE_IEC103_UP:
            strcpy(protocol_type_buf,"IEC103_UP");
			break;
		case SERVICE_PROTOCOL_TYPE_CDT_UP:
            strcpy(protocol_type_buf,"CDT_UP");
			break;
		case SERVICE_PROTOCOL_TYPE_DNP_UP:
            strcpy(protocol_type_buf,"DNP_UP");
			break;
		case SERVICE_PROTOCOL_TYPE_MODBUS_SLAVE:
            strcpy(protocol_type_buf,"MODBUS_SLAVE");
			break;
		case SERVICE_PROTOCOL_TYPE_DLT645_UP:
            strcpy(protocol_type_buf,"DLT645_UP");
			break;
		case SERVICE_PROTOCOL_TYPE_IEC61850_UP:
            strcpy(protocol_type_buf,"IEC61850_UP");
			break;
		case SERVICE_PROTOCOL_TYPE_DEBUG_UP:
            strcpy(protocol_type_buf,"DEBUG_UP");
			break;
		case SERVICE_PROTOCOL_TYPE_XJ104_UP:
            strcpy(protocol_type_buf,"XJ104_UP");
			break;
		case SERVICE_PROTOCOL_TYPE_SEMS8000_UP:
            strcpy(protocol_type_buf,"SEMS8000_UP");
			break;
		case SERVICE_PROTOCOL_TYPE_MQTT_UP:
            strcpy(protocol_type_buf,"MQTT_UP");
			break;
		case SERVICE_PROTOCOL_TYPE_OPCUA_UP:
            strcpy(protocol_type_buf,"OPCUA_UP");
			break;
		case SERVICE_PROTOCOL_TYPE_IEC104_DOWN:
            strcpy(protocol_type_buf,"IEC104_DOWN");
			break;
		case SERVICE_PROTOCOL_TYPE_IEC101_DOWN:
            strcpy(protocol_type_buf,"IEC101_DOWN");
			break;
		case SERVICE_PROTOCOL_TYPE_IEC103_DOWN:
            strcpy(protocol_type_buf,"IEC103_DOWN");
			break;
		case SERVICE_PROTOCOL_TYPE_CDT_DOWN:
            strcpy(protocol_type_buf,"CDT_DOWN");
			break;
		case SERVICE_PROTOCOL_TYPE_DNP_DOWN:
            strcpy(protocol_type_buf,"DNP_DOWN");
			break;
		case SERVICE_PROTOCOL_TYPE_MODBUS_MASTER:
            strcpy(protocol_type_buf,"MODBUS_MASTER");
			break;
		case SERVICE_PROTOCOL_TYPE_DLT645_DOWN:
            strcpy(protocol_type_buf,"DLT645_DOWN");
			break;
		case SERVICE_PROTOCOL_TYPE_WFXJ104_DOWN:
            strcpy(protocol_type_buf,"WFXJ104_DOWN");
			break;
		case SERVICE_PROTOCOL_TYPE_CJT188_DOWN:
            strcpy(protocol_type_buf,"CJT188_DOWN");
			break;
		case SERVICE_PROTOCOL_TYPE_XJ104_DOWN:
            strcpy(protocol_type_buf,"XJ104_DOWN");
			break;
		case SERVICE_PROTOCOL_TYPE_COMBINED_DOWN:
            strcpy(protocol_type_buf,"COMBINED_DOWN");
			break;
		default:
			printf("unknow PROTOCOL TYPE\n");
    }
    if(strlen(protocol_type_buf) == 0)
    	return;

    char message_buf[2048]={0};
    printf("recvlen:%d\n",len);
    if(protocol_type == SERVICE_PROTOCOL_TYPE_MQTT_UP)
    {
    	strcpy(message_buf,msg);
    }
    else
    {
    	int i=0;
        for(i = 0; i < len; i++)
        {
        	printf("msg[%d]:%02x\n",i,msg[i]);
        	sprintf(message_buf+i*2,"%02x",msg[i]);
        }
    }


    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;

    char *ptr;
    time_t rawtime;

            char topic_tmp[128];
            sprintf(topic_tmp,remote_management_device_protocolmessage_topic,get_terminal_id());
            printf("remote_management_device_protocol_message_topic:%s\n",topic_tmp);
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
                        	sprintf(msg_id_string,"%u",remote_management_pub_num);
                            cJSON_AddStringToObject(json_obj, "id", msg_id_string);
                            cJSON_AddStringToObject(json_obj, "version", version);
                            cJSON_AddStringToObject(json_obj, "sn", get_terminal_id());
                            cJSON_AddItemToObject(json_obj, "sys", json_obj_sys);
                            cJSON_AddNumberToObject(json_obj_sys,"ack",0);
                            cJSON_AddItemToObject(json_obj, "params", json_obj_params);
                            time(&rawtime);
                            cJSON_AddNumberToObject(json_obj_params,"time",rawtime);

                            cJSON_AddStringToObject(json_obj_params,"protocolType",protocol_type_buf);
                            cJSON_AddNumberToObject(json_obj_params, "serviceId",service_id);
                            cJSON_AddNumberToObject(json_obj_params, "deviceId", dev_id);
                            cJSON_AddStringToObject(json_obj_params, "protocolMessage", message_buf);
                            ptr = cJSON_Print(json_obj);
                            if(ptr)
                            {
                                pubmsg.payload = (void *)ptr;
                                pubmsg.payloadlen = strlen(pubmsg.payload);
                                pubmsg.qos = 0;
                                pubmsg.retained = 0;
                                if (MQTTClient_publishMessage(remote_management_client, topic_tmp, &pubmsg, &token) != MQTTCLIENT_SUCCESS)
                                	remote_management_online_flag=0;
                                remote_management_pub_num++;
                                free(ptr);
                                usleep(10000);
                            }
                        }
                        cJSON_Delete(json_obj);
                    }
            }
}

void remote_management_ota_progress_handler(const char *service_id, UPDATE_STATUS status, const char *msg)
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

void remote_management_ota_inform_handler(const char *service_id, const char *msg)
{
    printf("into ota_inform handler,service_id:%d,msg:%s\n",service_id,msg);

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

static void device_attribute_cyc_msg_pub()
{
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;

    char *ptr;
    time_t rawtime;
    u32 tick;

    tick = get_tick_count();
    if(tick - remote_management_device_attribute_cyc_tick > 300000)    //
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
                        	sprintf(msg_id_string,"%u",remote_management_pub_num);
                            cJSON_AddStringToObject(json_obj, "id", msg_id_string);
                            cJSON_AddStringToObject(json_obj, "version", version);
                            cJSON_AddStringToObject(json_obj, "sn", get_terminal_id());
                            cJSON_AddItemToObject(json_obj, "sys", json_obj_sys);
                            cJSON_AddNumberToObject(json_obj_sys,"ack",0);
                            time(&rawtime);
                            cJSON_AddNumberToObject(json_obj,"time",rawtime);
                            cJSON_AddItemToObject(json_obj, "params", json_obj_params);
                            char devicetype[32]={0};
                            get_hw_version("/opt/updata/inf",devicetype);
                            if(strlen(devicetype)<1)
                            {
                                cJSON_Delete(json_obj);
                                return;
                            }
                            char dev_devicetype[32]={0};
                            sprintf(dev_devicetype,"%s%s","PMF406-",devicetype);
                            cJSON_AddStringToObject(json_obj_params, "devicetype", dev_devicetype);
                            char softcrc[32]={0};
                            get_cal_crc("/opt/updata/inf",softcrc);
                            if(strlen(softcrc)<1)
                            {
                                cJSON_Delete(json_obj);
                                return;
                            }
                            cJSON_AddStringToObject(json_obj_params, "softcrc", softcrc);

                            char softversion[32]={0};
                            get_version("/opt/updata/inf",softversion);
                            if(strlen(softversion)<1)
                            {
                                cJSON_Delete(json_obj);
                                return;
                            }
                            cJSON_AddStringToObject(json_obj_params, "softversion", softversion);
                            cJSON_AddNumberToObject(json_obj_params,"event_switch",remote_management_event_switch);
                            cJSON_AddNumberToObject(json_obj_params,"log_switch",remote_management_log_switch);
                            cJSON_AddNumberToObject(json_obj_params,"protocol_switch",remote_management_protocol_switch);

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
    conn_opts.username="xczn";
    conn_opts.password="xczn";

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
                cJSON *sign_item = cJSON_GetObjectItem(data_item, "sign");
                cJSON *size_item = cJSON_GetObjectItem(data_item, "size");
                
                if (url_item && cJSON_IsString(url_item)) strncpy(cmd.data.url, url_item->valuestring, sizeof(cmd.data.url) - 1);
                if (version_item && cJSON_IsString(version_item)) strncpy(cmd.data.version, version_item->valuestring, sizeof(cmd.data.version) - 1);
                if (md5_item && cJSON_IsString(md5_item)) strncpy(cmd.data.md5, md5_item->valuestring, sizeof(cmd.data.md5) - 1);
                if (sign_item && cJSON_IsString(sign_item)) strncpy(cmd.data.sign, sign_item->valuestring, sizeof(cmd.data.sign) - 1);
                if (size_item && cJSON_IsNumber(size_item)) cmd.data.size = size_item->valueint;
            }
            
            cJSON_Delete(root);
            
            // 检查关键参数（防止崩溃）
            if (strlen(cmd.data.url) > 0 && cmd.data.size > 0) { 
                ota_upgrade_handler(&cmd);
            } else {
                 // 参数不全，报升级失败
                remote_management_fire_ota_progress(cmd.id, UPDATE_FAILED, "-1升级失败: Missing parameters in command");
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

    while(system_init_flag())
    {
        sleep ( 1 );
    }

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

    // 【全局初始化】: 在线程开始时进行 libcurl 全局初始化
    CURLcode init_res = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (init_res != CURLE_OK) {
        printf("Fatal Error: libcurl global init failed: %s\n", curl_easy_strerror(init_res));
    }

    char *sn = get_terminal_id();
    char ota_topic[128];
    snprintf(ota_topic, sizeof(ota_topic), remote_management_device_ota_upgrade_topic, sn);


    // ---- 步骤 8. 启动后状态判断与上报 (检查 upgrade.txt) ----
    ota_reboot_status_t *status = check_ota_finish_status();
    if (status != NULL)
    {
        // 升级成功，调用 ota_inform 上报成功消息
        remote_management_fire_ota_inform(status->id, status->version); // msg为下发的version字段值
        printf("OTA upgrade success reported for version: %s (ID: %s)\n", status->version, status->id);
        
        // 清除标志文件
        clear_ota_finish_status();
    }
    remote_management_register_event_cb(remote_management_event_handler);
    remote_management_register_ulog_cb(remote_management_ulog_handler);
    remote_management_register_protocol_message_cb(remote_management_protocol_message_handler);
    remote_management_register_ota_progress_cb(remote_management_ota_progress_handler);
    remote_management_register_ota_inform_cb(remote_management_ota_inform_handler);
    while (1)
    {
        if (mqtt_connect(&remote_management_client) == MQTTCLIENT_SUCCESS)
        {
            printf("remote management connect success\n");

            MQTTClient_subscribe(remote_management_client, TOPIC, QOS);
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
    // 当程序/线程即将终止时调用，释放所有 libcurl 内部资源
    curl_global_cleanup();
    return NULL;
}


void remote_management_thread_init(void)
{
    pthread_t remote_management_thread;
    if (pthread_create(&remote_management_thread, NULL, remote_management_thread_entry, NULL) != 0)
    {
        printf("remote_management_thread error\n");
        return;
    }
    return;
}


/******************************************************************************
  End File
******************************************************************************/
