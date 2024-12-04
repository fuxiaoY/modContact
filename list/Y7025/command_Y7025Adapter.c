/**
 *******************************************************************************
 * @file     $command_Y7025.c$
 * @author
 * @version  ver 1
 * @date     July 18th 2019
 * @build 
 * @brief    command_Y7025 Operation
 * @attention
 *
 * @note
 *
 *
 *******************************************************************************
 */
#include "command_Y7025Adapter.h"
#include "GlobalData.h"
#include "ulog.h"
static bool cmd_GetZTZEU(MctInstance* inst,void *para)
{
    tWanClock *clock = (tWanClock*)para;
    if (true == mct_y7025_execute(inst, true, CMD_Y7025_BOOTZTZEU, clock))
    {
        return true;
    }
    else
    {
        memset(clock, 0, sizeof(tWanClock));
        ULOG_INFO("Y7025: GetZTZEU failed:");
        return false;
    }
}


static bool cmd_WorklockSet(MctInstance* inst,void *para)
{
    if (false == mct_y7025_execute(inst, true, CMD_Y7025_WORKLOCK, para))
    {
        ULOG_INFO("Y7025: CMD_Y7025_WORKLOCK failed:");
        return false;
    }
    else
    {
        return true;
    }
}

// 关闭回显
static bool cmd_EchoClose(MctInstance* inst,void *para)
{
    if (false == mct_y7025_execute(inst, true, CMD_Y7025_ECHO_OFF, NULL))
    {
        ULOG_INFO("Y7025: Cmd CMD_Y7025_ECHO_OFF failed:");
    }
    return true;
}

static bool cmd_ModemInfoGet(MctInstance* inst,void *para)
{
    memset(&g_RTStat.wan_imei, 0, sizeof(g_RTStat.wan_imei));
    if (false == mct_y7025_execute(inst, true, CMD_Y7025_IMEI_GET, g_RTStat.wan_imei))
    {
        ULOG_INFO("Y7025: CMD_Y7025_IMEI_GET failed:");
    }
    else
    {
        ULOG_INFO("Y7025: IMEI:%s", g_RTStat.wan_imei);

    }

    memset(&g_RTStat.wan_imsi, 0, sizeof(g_RTStat.wan_imsi));
    if (false == mct_y7025_execute(inst, true, CMD_Y7025_IMSI_GET, g_RTStat.wan_imsi))
    {
        ULOG_INFO("Y7025: CMD_Y7025_IMEI_GET failed:");
    }
    else
    {
        ULOG_INFO("Y7025: IMSI:%s", g_RTStat.wan_imsi);
    }

    memset(&g_RTStat.wan_ver, 0, sizeof(g_RTStat.wan_ver));
    if (false == mct_y7025_execute(inst, true, CMD_Y7025_VERSION_GET, g_RTStat.wan_ver))
    {
        ULOG_INFO("Y7025: CMD_Y7025_IMEI_GET failed:");
    }
    else
    {
        ULOG_INFO("Y7025: ver:%s", g_RTStat.wan_ver);
    }

    memset(&g_RTStat.wan_iccid, 0, sizeof(g_RTStat.wan_iccid));
    if (false == mct_y7025_execute(inst, true, CMD_Y7025_NCCID, g_RTStat.wan_iccid))
    {
        ULOG_INFO("Y7025: CMD_Y7025_IMEI_GET failed:");
    }
    else
    {
        ULOG_INFO("Y7025: nccid:%s", g_RTStat.wan_iccid);
    }
    return true;
}

static bool cmd_PincodeGet(MctInstance* inst,void *para)
{
    ePincode status;
    if (false == mct_y7025_execute(inst, true, CMD_Y7025_PINCODE_INQUIRE, (void *)&status))
    {
        ULOG_INFO("Y7025: CMD_Y7025_PINCODE_INQUIRE failed:");
        return false;
    }
    else
    {
        ULOG_INFO("Y7025: pincode:%d", status);
        return true;
    }
}
static bool cmd_PincodeSet(MctInstance* inst,void *para)
{
    ePincode status;
    tPincode code;

    if (false == mct_y7025_execute(inst, true, CMD_Y7025_PINCODE_INQUIRE, (void *)&status))
    {
        ULOG_INFO("Y7025: CMD_Y7025_PINCODE_INQUIRE failed:");
        return false;
    }
    if (PIN_ERROR == status)
    {
        return false;
    }
    if (PIN_INPUT == status)
    {
        // 设置值
        code.data = 1;
        return mct_y7025_execute(inst, true, CMD_Y7025_PINCODE_SET, (void *)&code.data);
    }
    return true;
}

// 主动上报网络注册消息
static bool cmd_CEREGset(MctInstance* inst,void *para)
{
    if (false == mct_y7025_execute(inst, true, CMD_Y7025_CEREG, para))
    {
        ULOG_INFO("Y7025: CMD_Y7025_CEREG failed:");
        return false;
    }
    else
    {
        return true;
    }
}
// 查看信号强度
static bool cmd_ModemUpdateSignal(MctInstance* inst,void *para)
{
    NetworkPara_t* networkPara = (NetworkPara_t*)para;

    tsignalQuality signal;

    memset((void *)&signal, 0x00, sizeof(tsignalQuality));
    if (false == mct_y7025_execute(inst, true, CMD_Y7025_SIGNAL_GET, &signal))
    {
        ULOG_INFO("Y7025: CMD_Y7025_SIGNAL_GET failed:");
        return false;
    }
    else
    {
        networkPara->SignalStrength = signal.rssi;
    }

    return true;
}
// 查看注网状态
static bool cmd_CheckCEREG(MctInstance* inst,void *para)
{
    NetworkPara_t* networkPara = (NetworkPara_t*)para;
    tRegStus reg;
    if (false == mct_y7025_execute(inst, true, CMD_Y7025_DATA_REGISTER, &reg.value))
    {
        ULOG_INFO("Y7025: CMD_Y7025_DATA_REGISTER failed:");
        return false;
    }
    else
    {
        networkPara->REGstatus = reg.value;
    }
    return true;
}
// 配置PSM模式下，终端设备会减少与网络的交互
// 设置PSM参数，此处将T3412设置为60小时
// 意思是只要在60小时内至少有一次上报，就不用重新注网。T3324设置为2秒，意思是解除DTR休眠锁后延迟2秒进入PSM休眠
static bool cmd_CPSMset(MctInstance* inst,void *para)
{

    if (false == mct_y7025_execute(inst, true, CMD_Y7025_CPSMS, NULL))
    {
        ULOG_INFO("Y7025: CMD_Y7025_CPSMS failed:");
        return false;
    }
    else
    {
        return true;
    }
}

static bool cmd_powerOff(MctInstance* inst,void *para)
{
    if (false == mct_y7025_execute(inst, true, CMD_Y7025_POWER_OFF, NULL))
    {
        ULOG_INFO("Y7025: CMD_Y7025_POWER_OFF failed:");
        return false;
    }
    else
    {
        return true;
    }
}

static bool cmd_autoConfig(MctInstance* inst,void *para)
{
    uint8_t MAX_RETRIES = 3;

    // 设置worklock
    uint16_t WORKLOCK_status = 1;
    bool worklockSuccess = false;
    for (int i = 0; i < MAX_RETRIES; i++) {
        worklockSuccess = cmd_WorklockSet(inst,&WORKLOCK_status);
        if (worklockSuccess) {
            break;
        }
        MCT_DELAY(WAIT_SCHEDULE_TIME_MS);
    }
    if (!worklockSuccess) {
        return false;
    }

    // 设置CPSM
    bool cpsmSuccess = false;
    for (int i = 0; i < MAX_RETRIES; i++) {
        cpsmSuccess = cmd_CPSMset(inst,NULL);
        if (cpsmSuccess) {
            break;
        }
        MCT_DELAY(WAIT_SCHEDULE_TIME_MS);
    }
    if (!cpsmSuccess) {
        return false;
    }

    // 关闭回显
    bool echoCloseSuccess = false;
    for (int i = 0; i < MAX_RETRIES; i++) {
        echoCloseSuccess = cmd_EchoClose(inst,NULL);
        if (echoCloseSuccess) {
            break;
        }
        MCT_DELAY(WAIT_SCHEDULE_TIME_MS);
    }
    if (!echoCloseSuccess) {
        return false;
    }

    // 获取PIN码状态
    bool pincodeGetSuccess = false;
    for (int i = 0; i < MAX_RETRIES; i++) {
        pincodeGetSuccess = cmd_PincodeGet(inst,NULL);
        if (pincodeGetSuccess) {
            break;
        }
        MCT_DELAY(WAIT_SCHEDULE_TIME_MS);
    }
    if (!pincodeGetSuccess) {
        return false;
    }

    // 禁止主动上报网络注册消息
    uint16 CEREG_status = 0;
    bool ceregSuccess = false;
    for (int i = 0; i < MAX_RETRIES; i++) {
        ceregSuccess = cmd_CEREGset(inst,&CEREG_status);
        if (ceregSuccess) {
            break;
        }
        MCT_DELAY(WAIT_SCHEDULE_TIME_MS);
    }
    if (!ceregSuccess) {
        return false;
    }

    // 获取模块信息
    //cmd_ModemInfoGet(inst,NULL);

    return true;
}

static bool cmd_NetworkCheck(MctInstance* inst,void *para)
{
    cmd_ModemUpdateSignal(inst,para);
    cmd_CheckCEREG(inst,para);
    return true;
}

static bool cmd_mqttflow(MctInstance* inst,void *para)
{
    mqttConnet_t mqttConnetpara;
    mqttConnetpara.Serverip = g_RTCfg.ServerURL;
    mqttConnetpara.ServerPort = g_RTCfg.ServerPort;
    mqttConnetpara.ConnectTimeOut = g_RTCfg.ConnectTimeOut;
    if (false == mct_y7025_execute(inst, true, CMD_Y7025_MQTTNEW, &mqttConnetpara))
    {
        ULOG_INFO("Y7025: CMD_Y7025_MQTTNEW failed:");
        return false;
    }
    mqttconnnack_t mqttconnnackpara;
    if (false == mct_y7025_execute(inst, true, CMD_Y7025_MQTTCON, &mqttconnnackpara))
    {
        ULOG_INFO("Y7025: CMD_Y7025_MQTTCON failed:");
        return false;
    }
    else
    {
        if (0 != mqttconnnackpara.return_code || 0 != mqttconnnackpara.socket_id)
        {
            ULOG_INFO("Y7025: CMD_Y7025_MQTTCON para failed: %d, %d", mqttconnnackpara.socket_id, mqttconnnackpara.return_code);
            return false;
        }
    }

    if (false == mct_y7025_execute(inst, true, CMD_Y7025_MQTTSUB, g_MQTopic.Recv))
    {
        ULOG_INFO("Y7025: CMD_Y7025_MQTTSUB g_MQTopic.Resp failed:");
        return false;
    }
    if (false == mct_y7025_execute(inst, true, CMD_Y7025_MQTTSUB, g_MQTopic.ConfigDn))
    {
        ULOG_INFO("Y7025: CMD_Y7025_MQTTSUB g_MQTopic.ConfigDn failed:");
        return false;
    }
    return true;
}
static bool cmd_mqttpublish(MctInstance* inst,void *para)
{
    
    if (false == mct_y7025_execute(inst, true, CMD_Y7025_MQTTPUB, para))
    {
        ULOG_INFO("Y7025: CMD_Y7025_MQTTPub failed:");
        return false;
    }
    return true;
}

static bool cmd_ClockGet(MctInstance* inst,void *para)
{
    tWanClock *clock = (tWanClock*)para;
    if (true == mct_y7025_execute(inst, true, CMD_Y7025_CLOCK_GET, clock))
    {
        return true;
    }
    else
    {
        memset(clock, 0, sizeof(tWanClock));
        ULOG_INFO("Y7025: CMD_Y7025_CLOCK_GET failed:");
        return false;
    }
}

static bool cmd_revHandle(MctInstance *inst,void *para)
{

    mct_y7025_execute(inst,false,CMD_Y7025_MQTTREV,NULL);
    return true;
}


static const tCmdApi funList[] =
    {
        {.id = CMD_BOOTUPCLOCK, .fun = cmd_GetZTZEU},
        {.id = CMD_ECHO_CLOSE, .fun = cmd_EchoClose},
        {.id = CMD_WORKLOCKSET, .fun = cmd_WorklockSet},
        {.id = CMD_MODEM_INFO, .fun = cmd_ModemInfoGet},
        {.id = CMD_PINCODE_SET, .fun = cmd_PincodeSet},
        {.id = CMD_POWER_OFF, .fun = cmd_powerOff},
        {.id = CMD_AUTO_CONFIG, .fun = cmd_autoConfig},
        {.id = CMD_MODEM_SIGNAL, .fun = cmd_ModemUpdateSignal},
        {.id = CMD_NETWORK_CHECK, .fun = cmd_NetworkCheck},
        {.id = CMD_COLDSTARTUP_CHECK, .fun = NULL},
        {.id = CMD_MQTTFLOW, .fun = cmd_mqttflow},
        {.id = CMD_MQTTPUBLISH, .fun = cmd_mqttpublish},
        {.id = CMD_MODEM_CLOCK_GET, .fun = cmd_ClockGet},
        {.id = CMD_REV_FLOW,   .fun = cmd_revHandle},
        
};

tCmdApi const *CMD_Y7025ApiGet(void)
{
    return funList;
}

/*********** End of file **/