#ifndef __MCTSTRUCT_H__
#define __MCTSTRUCT_H__

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#define DATA_TIME_LEN  12
typedef union
{
    struct
    {
        uint8_t year_h;
        uint8_t year_l;
        uint8_t month;
        uint8_t day;
        uint8_t week;
        uint8_t hour;
        uint8_t min;
        uint8_t sec;
        uint8_t hund;
        uint8_t dev_h;
        uint8_t dev_l;
        uint8_t status;
    };
    uint8_t str[DATA_TIME_LEN];
}wanClock_u;

typedef struct
{
    wanClock_u clock;
    int       zone;
}wanClock_t;

typedef struct 
{
    char*    Topic;
    int      QOS;
    uint32_t PublishLen;
    char*    PublishData;
}mqttPulish_t;

typedef enum
{
    registerNone        = (uint8_t)0,
    registerHomeNet,
    resistering,
    registerDenied,
    registerUnknow,
    registerRoaming, 
    registerMax,
}regStatus_e;

typedef struct
{
  uint8_t SignalStrength; // 公网信号强度
  regStatus_e REGstatus;  // 组网进度
}networkPara_t;

typedef struct 
{
    char* ip;
    uint16_t port;
}httpURL;

typedef struct
{
    uint16_t BlockNum;
    uint32_t fileLen;
    char UpdatePath[128];		  //远程刷机绝对地址
    uint8_t *fileData;
} http_t;

typedef struct
{
    char      *ip;                // http地址
    uint16_t  port;               // http端口号
    char      *file_path;         // http文件地址
    uint32_t  file_path_len;      // http文件地址长度 
    uint32_t  block_len;          // 单包获取长度


    uint16_t  block_num;          // 传输序号
    uint8_t   *p_file_data;       // 单包数据指针
    uint32_t  file_len;           // 实际获取长度


}httpAction_t;
/***************************************************************
*Command API
***************************************************************/
#define CMD_TEST                    (int32_t)0
// USER DEFINED COMMAND ID
#define CMD_MODBUS_RTU_MASTER      (1 + CMD_TEST)
#define CMD_MODBUS_RTU_SLAVE       (1 + CMD_MODBUS_RTU_MASTER)
#define CMD_MAX                    (1 + CMD_MODBUS_RTU_SLAVE)

#ifdef __cplusplus
}
#endif
#endif

