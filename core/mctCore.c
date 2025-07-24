#include "mctCore.h"


void mct_data_reset(MctInstance* pInstance)
{
    if(NULL != pInstance->cmd_cache)
    {
        memset(pInstance->cmd_cache,0,pInstance->CMD_MAX_SIZE);
    }
    if(NULL != pInstance->payload_cache)
    {
        memset(pInstance->payload_cache,0,pInstance->PAYLOAD_MAX_SIZE);
    }
    pInstance->cmd_size = 0;
    pInstance->payload_size = 0;
}
/*-------------------------------------------------------------------------------------*/

/*响应列表-----------------------------------------------------------------------------*/
void initStaticFrameList(StaticFrameList *list)
{
    memset(list, 0, sizeof(StaticFrameList));
}

static void addFrameToExpectedFrameList(StaticFrameList *list, uint16_t startOffset, uint16_t endOffset,uint16_t id)
{

    list->frames_expected.startOffset = startOffset;
    list->frames_expected.endOffset = endOffset;
    list->frames_expected.length = endOffset - startOffset;
    list->frames_expected.tcmd_id = id;
    list->frames_expected.status = FRAME_NEW;

    list->have_expected = true;
}
static void addFrameToFrameList(StaticFrameList *list, uint16_t startOffset, uint16_t endOffset,uint16_t id)
{
    if (list->size >= MAX_FRAMES) 
    {
        return;
    }
    list->frames[list->size].startOffset = startOffset;
    list->frames[list->size].endOffset = endOffset;
    list->frames[list->size].length = endOffset - startOffset;
    list->frames[list->size].tcmd_id = id;
    list->frames[list->size].status = FRAME_NEW;

    list->size++;
}


/*-------------------------------------------------------------------------------------*/

/*帧匹配--------------------------------------------------------------------------------*/
static frameMacheType frame_mache(MctInstance *inst, const tCmd *expected_cmd,bool is_expected,uint16_t id,StaticFrameList *payloadlist,uint16_t *remain_len)
{
    if(AscII == expected_cmd->format)
    {
        uint16_t PhaseOffset = 0;
        uint16_t SubphaseOffset = 0;


        // 如果指定了错误阶段且响应符合错误阶段，则返回错误等待状态
        if (NULL != expected_cmd->errorPhase && true == cmd_ComformRes(inst->payload_cache, inst->payload_size, expected_cmd->errorPhase, NULL, &PhaseOffset, &SubphaseOffset))
        {
            *remain_len = 0;
            return match_error;
        }
        // 如果响应符合正确阶段，则返回成功等待状态  如果没有指定rightPhasefmct_data_reset 也视为正确响应
        if (true == cmd_ComformRes(inst->payload_cache, inst->payload_size, expected_cmd->rightPhase, expected_cmd->SubRightPhase, &PhaseOffset, &SubphaseOffset))
        {
            if(is_expected)
            {
                addFrameToExpectedFrameList(payloadlist, PhaseOffset, SubphaseOffset,id);
            }
            else
            {
                addFrameToFrameList(payloadlist, PhaseOffset, SubphaseOffset,id);
            }

            *remain_len -= (SubphaseOffset - PhaseOffset);
            return match_sucess;
        }
        //什么都没查询到 
        *remain_len -= (SubphaseOffset - PhaseOffset);
        return match_null;
    
    }
    else if(HeX == expected_cmd->format)
    {
        uint16_t PhaseOffset = 0;
        uint16_t SubphaseOffset = 0;


        // 如果指定了错误阶段且响应符合错误阶段，则返回错误等待状态
        if (NULL != expected_cmd->errorPhase && true == cmd_ComformResUint8(inst->payload_cache, inst->payload_size, expected_cmd->errorPhase,expected_cmd->errorPhaseLen, NULL, 0,&PhaseOffset, &SubphaseOffset))
        {
            *remain_len = 0;
            return match_error;
        }
        // 如果响应符合正确阶段，则返回成功等待状态  如果没有指定rightPhase 也视为正确响应
        if (true == cmd_ComformResUint8(inst->payload_cache, inst->payload_size, expected_cmd->rightPhase,expected_cmd->rightPhaseLen, expected_cmd->SubRightPhase,expected_cmd->SubRightPhaseLen, &PhaseOffset, &SubphaseOffset))
        {
            if(is_expected)
            {
                addFrameToExpectedFrameList(payloadlist, PhaseOffset, SubphaseOffset,id);
            }
            else
            {
                addFrameToFrameList(payloadlist, PhaseOffset, SubphaseOffset,id);
            }

            *remain_len -= (SubphaseOffset - PhaseOffset);
            return match_sucess;
        }
        //什么都没查询到 
        *remain_len -= (SubphaseOffset - PhaseOffset);
        return match_null;
    }
    else
    {
        return match_null;
    }
    
}
/*-------------------------------------------------------------------------------------*/

/*预期帧流程-----------------------------------------------------------------------------*/
static bool expected_cmd_send(MctInstance *inst,StaticFrameList *payloadlist,tCmd const *List, uint16_t cmdlist_seq_i, int32_t expected_tcmd_id,void *para)
{
    // 打包命令，如果失败则返回false
    if (false == List[cmdlist_seq_i].pack(inst->cmd_cache,&inst->cmd_size, para))
    {
        return false;
    }
    if(0 > inst->mct_write(inst->cmd_cache,inst->cmd_size))
    {
        return false;
    }
    return true;

}



static bool expected_cmd_seek(MctInstance *inst, tCmd const *cmdList,uint16_t cmdListNum, \
                                    int32_t expected_tcmd_id, \
                                    StaticFrameList *payloadlist,uint16_t cmdlist_seq_i)
{
    uint32_t cnt = 0;
    uint16_t remain_len = 0;
    bool result = false;

    do
    {
        uint16_t single_len = 0;

        //收帧
        single_len = inst->mct_read(inst->payload_cache + inst->payload_size, inst->PAYLOAD_MAX_SIZE - inst->payload_size);
        inst->payload_size += single_len;
        remain_len += single_len;
        //有数据更新，则进入一次判断
        if (single_len > 0)
        {

            frameMacheType matcheResult = frame_mache(inst,&cmdList[cmdlist_seq_i],true,expected_tcmd_id,payloadlist,&remain_len);
            //匹配到正确字段和匹配到错误字段都直接退出
            if(match_sucess == matcheResult)
            {
                result = true;
                break;
            }
            else if(match_error == matcheResult)
            {
                result = false;
                break;
            }
        }
        MCT_DELAY(WAIT_SCHEDULE_TIME_MS);
        cnt++;
    } while (cnt < (cmdList[cmdlist_seq_i].timeout * 1000 / WAIT_SCHEDULE_TIME_MS));

    if(remain_len > 0)
    {
        //处理沾帧，加入链表
        for(uint8_t i = 0;i < cmdListNum;i++)
        {
            //此机制下，同一帧重复命令出现多次会被过滤，只处理第一次出现的重复命令

            if( RecvSend == cmdList[i].Type)
            {
                if(cmdList[i].id == expected_tcmd_id) //跳过预期帧
                continue;

                frame_mache(inst,&cmdList[i],false,cmdList[i].id,payloadlist,&remain_len); 
                //处理完毕，无需继续遍历
                if(remain_len  == 0) 
                {
                    break; 
                }
            }
            //只会扫描一次，如果扫描不到，也认为帧处理完毕
        }
    }
    else
    {
        //帧处理完毕
    }
    return result;
}

static bool unexpected_cmd_seek(MctInstance *inst, tCmd const *cmdList, uint16_t cmdListNum, StaticFrameList *payloadlist)
{
    uint16_t single_len = 0;
    uint16_t remain_len = 0;
    bool result = false;

    // 收帧
    single_len = inst->mct_read(inst->payload_cache + inst->payload_size, inst->PAYLOAD_MAX_SIZE - inst->payload_size);
    inst->payload_size += single_len;
    remain_len += single_len;
    // 有数据更新，则进入一次判断
    if (single_len > 0) 
    {
        for (uint16_t i = 0; i < cmdListNum; i++)
        {
            //此机制下，同一帧重复命令出现多次会被过滤，只处理第一次出现的重复命令

            if( RecvSend == cmdList[i].Type)
            {
                frameMacheType matcheResult = frame_mache(inst, &cmdList[i], false,cmdList[i].id, payloadlist, &remain_len);
                // 如果匹配到，记录结果
                if(match_sucess == matcheResult)
                {
                    result = true;
                }
                else if(match_error == matcheResult)
                {
                    result = false;
                }


                // 处理完毕，无需继续遍历
                if (remain_len == 0) 
                {
                    break; 
                }
            }
            //只会扫描一次，如果扫描不到，也认为帧处理完毕
        }
    }

    // 返回最终结果
    return result;
}

static bool unexpected_cmd_seek_with_pecify(MctInstance *inst, tCmd const *cmdList, uint16_t cmdListNum, \
                                                                            int32_t expected_tcmd_id, \
                                                                            StaticFrameList *payloadlist)
{
    uint16_t single_len = 0;
    uint16_t remain_len = 0;
    bool result = false;

    // 收帧
    single_len = inst->mct_read(inst->payload_cache + inst->payload_size, inst->PAYLOAD_MAX_SIZE - inst->payload_size);
    inst->payload_size += single_len;
    remain_len += single_len;
    // 有数据更新，则进入一次判断
    if (single_len > 0) 
    {
        for (uint16_t i = 0; i < cmdListNum; i++)
        {
            //此机制下，同一帧重复命令出现多次会被过滤，只处理第一次出现的重复命令
            //只匹配预期
            if((RecvSend == cmdList[i].Type)&&(expected_tcmd_id == cmdList[i].id))
            {
                frameMacheType matcheResult = frame_mache(inst, &cmdList[i], false,cmdList[i].id, payloadlist, &remain_len);
                // 如果匹配到，记录结果
                if(match_sucess == matcheResult)
                {
                    result = true;
                }
                else if(match_error == matcheResult)
                {
                    result = false;
                }
                
                // 处理完毕，无需继续遍历
                if (remain_len == 0) 
                {
                    break; 
                }
            }
            //只会扫描一次，如果扫描不到，也认为帧处理完毕
        }
    }

    // 返回最终结果
    return result;
}
/**
 * @fn payload_scan

 * @retval 是否有数据
 */
static bool payload_scan(MctInstance *inst,StaticFrameList *payloadlist, \
                    tCmd const *cmdList,uint16_t cmdListNum, \
                    bool is_expected,int32_t expected_tcmd_id,uint16_t cmdlist_seq_i)
{
    if(true == is_expected)//有预期帧
    {
        //预期帧寻找
        return expected_cmd_seek(inst,cmdList,cmdListNum,expected_tcmd_id,payloadlist,cmdlist_seq_i); 
    }
    else
    {
        if(NULL_CMD_SEEK == expected_tcmd_id)
        {
            return unexpected_cmd_seek(inst, cmdList,cmdListNum,payloadlist);  
        }
        else
        {
            return unexpected_cmd_seek_with_pecify(inst, cmdList,cmdListNum,expected_tcmd_id,payloadlist);  
        }

    }

}
/**************************************************************************************** */

static dealprocess singleframeListDeal(MctInstance *inst, StaticFrameList *payloadlist, \
                                       tCmd const *cmdList, uint16_t payloadlist_id, uint16_t cmdListNum, void *para)
{
    bool commandFound = false;
    dealprocess status = FRAME_FAILED;

    for (uint8_t i = 0; i < cmdListNum; i++)
    {
        // 检查当前命令项是否与目标ID匹配
        if (payloadlist->frames[payloadlist_id].tcmd_id == cmdList[i].id)
        {
            commandFound = true;

            if (!cmdList[i].analyze(inst->payload_cache + payloadlist->frames[payloadlist_id].startOffset,payloadlist->frames[payloadlist_id].length, para))
            {
                break;
            }

            if (!cmdList[i].pack(inst->cmd_cache,&inst->cmd_size,para))
            {
                break;
            }

            if (0 > inst->mct_write(inst->cmd_cache, inst->cmd_size))
            {
                break;
            }

            status = FRAME_SUCCEED;
            break;
        }
    }

    if (!commandFound)
    {
        status = FRAME_FAILED;
    }

    payloadlist->frames[payloadlist_id].status = status;
    return status;
}

static dealprocess frameListDeal(MctInstance *inst,StaticFrameList *payloadlist,tCmd const *cmdList,uint16_t cmdListNum,void *para)
{
  dealprocess status = FRAME_FAILED;
  if(payloadlist->size > 0)
  { 
    for(uint8_t i = 0;i < payloadlist->size;i++)
    {
        if(payloadlist->frames[i].status ==    FRAME_NEW)
        {
            if(FRAME_SUCCEED == singleframeListDeal(inst,payloadlist,cmdList,i,cmdListNum,para))
            {
                status = FRAME_SUCCEED;
            }
        }
    }
  }
  return status;
}



static bool expectframeDeal(MctInstance *inst, StaticFrameList *payloadlist, tCmd const *cmdList, uint16_t cmdListNum, void *para)
{
    // 如果没有预期帧，直接返回 false
    if (!payloadlist->have_expected)
    {
        return false;
    }

    // 遍历命令列表
    for (uint8_t i = 0; i < cmdListNum; i++)
    {
        // 检查当前命令项是否与目标ID匹配
        if (payloadlist->frames_expected.tcmd_id == cmdList[i].id)
        {
            // 调用 analyze 函数
            if (cmdList[i].analyze(inst->payload_cache + payloadlist->frames_expected.startOffset,payloadlist->frames_expected.length, para))
            {
                // 分析成功，设置状态为成功并返回 true
                payloadlist->frames_expected.status = FRAME_SUCCEED;
                return true;
            }
            else
            {
                // 分析失败，设置状态为失败并返回 false
                payloadlist->frames_expected.status = FRAME_FAILED;
                return false;
            }
        }
    }

    // 没有找到匹配的命令项，返回 false
    return false;
}





bool CMD_Execute(MctInstance *inst, \
                int32_t expected_tcmd_id, \
                tCmd const *List,uint16_t cmdNum,void *para)
{

    {
        initStaticFrameList(&inst->payload_list);
        mct_data_reset(inst);
    }
    uint16_t i = 0;
    bool is_expected = false;
    bool result = false;
    uint16_t cmdlist_seq_i = 0;

    // 遍历命令列表，寻找匹配的命令
    for (i = 0; i < cmdNum; i++)
    {
        // 检查当前命令项是否与目标ID匹配
        if (expected_tcmd_id == List[i].id)
        {
            cmdlist_seq_i = i;
            if(List[i].Type == SendRev)
            {
                is_expected = true;
            }
            break;
        }
    }
    if((i == cmdNum)&&(expected_tcmd_id != NULL_CMD_SEEK))
    {
        //未定义命令
        return false;
    }
    
    //判断是否是SendRev类型的命令
    if(is_expected)
    {
        if(expected_cmd_send(inst,&inst->payload_list,List,cmdlist_seq_i,expected_tcmd_id,para))
        {
            if(payload_scan(inst,&inst->payload_list,List,cmdNum,is_expected,expected_tcmd_id,cmdlist_seq_i))
            {
                result = expectframeDeal(inst,&inst->payload_list,List,cmdNum,para); 
                
            }
            frameListDeal(inst,&inst->payload_list,List,cmdNum,para);
        }

    
    }
    else
    {
        payload_scan(inst,&inst->payload_list,List,cmdNum,is_expected,expected_tcmd_id,cmdlist_seq_i);
        if(FRAME_SUCCEED == frameListDeal(inst,&inst->payload_list,List,cmdNum,para))
        {
            result = true;
        }
    }

    return result;
}
