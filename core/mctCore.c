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

static void clearFrameList(StaticFrameList *list)
{
    list->size = 0;
    memset(list->frames, 0, sizeof(list->frames));

}

static void clearFrameExpectedFrameList(StaticFrameList *list)
{
    list->have_expected = false;
    memset(&list->frames_expected, 0, sizeof(list->frames_expected));
}

static void addFrame(StaticFrameList *list, uint16_t startOffset, uint16_t endOffset, uint16_t id, bool is_expected)
{
    if (is_expected)
    {
        list->frames_expected.startOffset = startOffset;
        list->frames_expected.endOffset = endOffset;
        list->frames_expected.length = (uint16_t)(endOffset - startOffset);
        list->frames_expected.tcmd_id = id;
        list->frames_expected.status = FRAME_NEW;
        list->have_expected = true;
    }
    else
    {
        if (list->size >= MAX_FRAMES)
        {
            return;
        }
        list->frames[list->size].startOffset = startOffset;
        list->frames[list->size].endOffset = endOffset;
        list->frames[list->size].length = (uint16_t)(endOffset - startOffset);
        list->frames[list->size].tcmd_id = id;
        list->frames[list->size].status = FRAME_NEW;
        list->size++;
    }
}

// 统一读取载荷并更新 remain_len 与 payload_size，返回本次读取长度
static int32_t read_payload_once(MctInstance *inst, int32_t *remain_len)
{
    int32_t single_len = 0;
    if (NULL == inst || NULL == inst->mct_read || NULL == inst->payload_cache)
    {
        return 0;
    }

    /* mct_read 原型返回 uint32_t，这里以有符号 int32_t 承接（实际读取量远小于 INT32_MAX） */
    single_len = (int32_t)inst->mct_read(inst->payload_cache + inst->payload_size, \
                                         inst->PAYLOAD_MAX_SIZE - inst->payload_size);
    if (single_len > 0)
    {
        inst->payload_size = (uint16_t)(inst->payload_size + single_len);
        if (remain_len)
        {
            /* 有符号累加：缓冲为静态有界，不会触及 int32_t 上限 */
            *remain_len = *remain_len + single_len;
        }
    }
    return single_len;
}

/*-------------------------------------------------------------------------------------*/

/*帧匹配--------------------------------------------------------------------------------*/
static frameMacheType frame_mache(MctInstance *inst, const tCmd *expected_cmd,bool is_expected,uint16_t id,StaticFrameList *payloadlist,int32_t *remain_len)
{
    if(AscII == expected_cmd->format)
    {
        uint16_t PhaseOffset = 0;
        uint16_t SubphaseOffset = 0;

        // 如果指定了错误阶段且响应符合错误阶段，则返回错误等待状态
        if (NULL != expected_cmd->errorPhase && true == cmd_ComformRes(inst->payload_cache, inst->payload_size, expected_cmd->errorPhase, NULL, &PhaseOffset, &SubphaseOffset))
        {
            if (remain_len)
            {
                *remain_len = 0;
            }
            return match_error;
        }
        // 如果响应符合正确阶段
        if (true == cmd_ComformRes(inst->payload_cache, inst->payload_size, expected_cmd->rightPhase, expected_cmd->SubRightPhase, &PhaseOffset, &SubphaseOffset))
        {
            // 确保偏移有序，防止下溢
            if (SubphaseOffset >= PhaseOffset)
            {
                addFrame(payloadlist, PhaseOffset, SubphaseOffset, id, is_expected);
                if (remain_len)
                {
                    /* 有符号扣减已匹配帧长，不足则归零，杜绝无符号下溢回绕 */
                    int32_t left = *remain_len - (int32_t)(SubphaseOffset - PhaseOffset);
                    *remain_len = (left > 0) ? left : 0;
                }
                return match_sucess;
            }
            else
            {
                // 非法偏移，视为无匹配
                return match_null;
            }
        }
        // 没查询到
        return match_null;
    
    }
    else if(HeX == expected_cmd->format)
    {
        uint16_t PhaseOffset = 0;
        uint16_t SubphaseOffset = 0;

        if (NULL != expected_cmd->errorPhase && true == cmd_ComformResUint8(inst->payload_cache, inst->payload_size, expected_cmd->errorPhase,expected_cmd->errorPhaseLen, NULL, 0,&PhaseOffset, &SubphaseOffset))
        {
            if (remain_len) 
            {
                *remain_len = 0;
            }
            return match_error;
        }
        if (true == cmd_ComformResUint8(inst->payload_cache, inst->payload_size, expected_cmd->rightPhase,expected_cmd->rightPhaseLen, expected_cmd->SubRightPhase,expected_cmd->SubRightPhaseLen, &PhaseOffset, &SubphaseOffset))
        {
            if (SubphaseOffset >= PhaseOffset)
            {
                addFrame(payloadlist, PhaseOffset, SubphaseOffset, id, is_expected);
                if (remain_len)
                {
                    /* 有符号扣减已匹配帧长，不足则归零，杜绝无符号下溢回绕 */
                    int32_t left = *remain_len - (int32_t)(SubphaseOffset - PhaseOffset);
                    *remain_len = (left > 0) ? left : 0;
                }
                return match_sucess;
            }
            else
            {
                return match_null;
            }
        }
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


/* 粘帧切分-----------------------------------*/

/**
 * @fn   sticky_frame_locate
 * @brief 在接收窗口内定位一条命令对应的完整帧。
 *
 *        仅在窗口 payload_cache[window_start, window_start+window_len) 内搜索，
 *        按命令格式(AscII/HeX)复用既有匹配逻辑，并把“窗口内相对偏移”换算为相对
 *        整段 payload_cache 的绝对偏移。本函数只做定位，不修改 remain_len 等任何状态。
 *
 * @param[in]  inst         协议实例
 * @param[in]  cmd          待匹配命令
 * @param[in]  window_start 窗口起点（相对 payload_cache 的绝对偏移）
 * @param[in]  window_len   窗口长度
 * @param[out] frame_start  帧起始绝对偏移
 * @param[out] frame_end    帧结束绝对偏移
 * @return true 找到且偏移合法；false 未匹配 / 偏移非法 / 越界
 */
static bool sticky_frame_locate(MctInstance *inst, const tCmd *cmd, \
                                uint16_t window_start, uint16_t window_len, \
                                uint16_t *frame_start, uint16_t *frame_end)
{
    uint16_t header_in_window = 0;   /* 帧头(rightPhase)在“窗口内”的相对偏移 */
    uint16_t frame_end_in_window = 0;/* 帧尾(SubRightPhase结束)在“窗口内”的相对偏移 */
    bool matched = false;

    if (NULL == inst || NULL == cmd || NULL == inst->payload_cache || \
        NULL == frame_start || NULL == frame_end)
    {
        return false;
    }

    /* 按帧格式复用既有匹配函数；搜索基址为“窗口起点”，故不会命中窗口之前的旧帧 */
    if (AscII == cmd->format)
    {
        matched = cmd_ComformRes(inst->payload_cache + window_start, window_len, \
                                 (const char *)cmd->rightPhase, (const char *)cmd->SubRightPhase, \
                                 &header_in_window, &frame_end_in_window);
    }
    else if (HeX == cmd->format)
    {
        matched = cmd_ComformResUint8(inst->payload_cache + window_start, window_len, \
                                      (const uint8_t *)cmd->rightPhase, cmd->rightPhaseLen, \
                                      (const uint8_t *)cmd->SubRightPhase, cmd->SubRightPhaseLen, \
                                      &header_in_window, &frame_end_in_window);
    }
    else
    {
        matched = false;
    }

    /* 未匹配，或得到非法偏移（帧尾早于帧头） */
    if (false == matched || frame_end_in_window < header_in_window)
    {
        return false;
    }
    /* 防止换算后越过已接收数据边界 */
    if ((uint32_t)window_start + frame_end_in_window > inst->payload_size)
    {
        return false;
    }

    /* 把“窗口内相对偏移”换算为“整段缓冲区绝对偏移”后输出 */
    *frame_start = (uint16_t)(window_start + header_in_window);
    *frame_end = (uint16_t)(window_start + frame_end_in_window);
    return true;
}

/**
 * @fn   sticky_overlap_expected
 * @brief 判断一条 RecvSend 帧是否与已匹配的预期帧在区间上重叠。
 *
 *        半开区间重叠条件：frame_start < expected_end && expected_start < frame_end。
 *        用于避免预期帧被粘帧逻辑重复切割，同时不影响同 id 但位于别处的帧。
 *
 * @param[in] list        帧链表（含预期帧信息）
 * @param[in] frame_start 该 RecvSend 帧的起始偏移
 * @param[in] frame_end   该 RecvSend 帧的结束偏移
 * @return true 与预期帧重叠；false 无预期帧或不重叠
 */
static bool sticky_overlap_expected(StaticFrameList *list, uint16_t frame_start, uint16_t frame_end)
{
    uint16_t expected_start = 0;
    uint16_t expected_end = 0;

    if (NULL == list || false == list->have_expected)
    {
        return false; /* 本次没有预期帧，不存在冲突 */
    }

    expected_start = list->frames_expected.startOffset;
    expected_end = list->frames_expected.endOffset;
    return (frame_start < expected_end && expected_start < frame_end);
}

/**
 * @fn   sticky_frames_extract
 * @brief 游标式贪心切分缓冲区中的所有粘帧。
 *
 *        每轮在窗口 payload_cache[cursor, 末尾) 内，从所有 RecvSend 命令中选出
 *        起始偏移最早的一帧加入链表，并将游标推进到该帧结尾；重复直至窗口内无
 *        任何匹配（剩余为脏数据或残缺尾帧）、链表满或切分轮数达到 MAX_FRAMES。
 *
 *        特性：同类型重复帧可全部切出；顺序严格遵循字节流；帧间脏数据自动跳过；
 *        游标每轮严格增加，保证收敛、不会死循环。
 *
 * @param[in]    inst        协议实例
 * @param[in]    cmdList     命令表
 * @param[in]    cmdListNum  命令表条数
 * @param[inout] payloadlist 帧链表，切出的帧追加到其 frames[]
 */
static void sticky_frames_extract(MctInstance *inst, tCmd const *cmdList, uint16_t cmdListNum, \
                                  StaticFrameList *payloadlist)
{
    uint16_t cursor = 0;    /* 游标：此前所有字节已处理完毕，只增不减 */
    uint8_t round_count = 0;/* 已切分轮数，与游标构成双重防死循环保障 */

    if (NULL == inst || NULL == cmdList || NULL == payloadlist || 0 == inst->payload_size)
    {
        return;
    }

    /* 循环三道条件：尚有未处理字节；链表未满；切分轮数未越限 */
    while (cursor < inst->payload_size && payloadlist->size < MAX_FRAMES && round_count < MAX_FRAMES)
    {
        bool recv_frame_found = false;    /* 本轮是否已扫描到 RecvSend 帧 */
        uint16_t earliest_start = 0;      /* 本轮已扫到的 RecvSend 帧中，起始最早者的绝对起点 */
        uint16_t earliest_end = 0;        /* 对应绝对终点 */
        uint16_t earliest_cmd_id = 0;     /* 对应命令id */
        uint16_t window_len = (uint16_t)(inst->payload_size - cursor); /* 当前窗口长度 */

        round_count++;

        /* ---- 第一步：扫描所有 RecvSend 命令，保留窗口内起始最早者 ---- */
        for (uint16_t i = 0; i < cmdListNum; i++)
        {
            uint16_t candidate_start = 0;
            uint16_t candidate_end = 0;

            /* 只切设备主动上报帧；SendRev 等其它类型不参与粘帧切分 */
            if (RecvSend != cmdList[i].Type)
            {
                continue;
            }
            /* 在当前窗口内定位该命令的一帧 */
            if (false == sticky_frame_locate(inst, &cmdList[i], cursor, window_len, \
                                            &candidate_start, &candidate_end))
            {
                continue;
            }
            /* 防御性校验：该 RecvSend 帧必须位于窗口内且为正长度，确保游标能严格向前 */
            if (candidate_start < cursor || candidate_end <= candidate_start)
            {
                continue;
            }
            /* 该帧若落在预期帧区间内则跳过（预期帧已单独处理） */
            if (sticky_overlap_expected(payloadlist, candidate_start, candidate_end))
            {
                continue;
            }

            /* 本轮还没扫到 RecvSend 帧时，先把这一帧记下；
             * 已经扫到过，则只在这一帧起始更早时替换 */
            if (false == recv_frame_found || candidate_start < earliest_start)
            {
                recv_frame_found = true;
                earliest_start = candidate_start;
                earliest_end = candidate_end;
                earliest_cmd_id = cmdList[i].id;
            }
        }

        /* ---- 第二步：本轮没扫到任何 RecvSend 帧 -> 剩余为脏数据或残缺尾帧，停止扫描 ---- */
        if (false == recv_frame_found)
        {
            break;
        }

        /* ---- 第三步：切出选中帧 ----
         * [cursor, earliest_start) 是帧起点之前未被任何帧覆盖的字节，即帧间脏数据，
         * 不加入链表、随游标推进一并丢弃；这里只把有效帧 [earliest_start,earliest_end) 入表。 */
        addFrame(payloadlist, earliest_start, earliest_end, earliest_cmd_id, false);

        /* 游标推进到该帧结尾：因 earliest_end > earliest_start >= cursor，游标严格增加，
         * 保证算法必然收敛（同类型的下一帧只会在新窗口中被再次命中） */
        cursor = earliest_end;
    }
}
/*=====================================================================================*/

static bool expected_cmd_seek(MctInstance *inst, tCmd const *cmdList,uint16_t cmdListNum, \
                                    int32_t expected_tcmd_id, \
                                    StaticFrameList *payloadlist,uint16_t cmdlist_seq_i)
{
    uint32_t cnt = 0;
    int32_t remain_len = 0;
    bool result = false;

    /* 保护性检查：避免除以零 */
    uint32_t wait_ms = (WAIT_SCHEDULE_TIME_MS == 0) ? 1U : (uint32_t)WAIT_SCHEDULE_TIME_MS;
    uint32_t loop_max = 0;
    if (cmdList[cmdlist_seq_i].timeout > 0) 
    {
        uint32_t t_ms = (uint32_t)cmdList[cmdlist_seq_i].timeout * 1000U;
        loop_max = t_ms / wait_ms;
    } 
    else 
    {
        loop_max = 0;
    }

    do
    {
        int32_t single_len = 0;

        // 收帧
        single_len = read_payload_once(inst, &remain_len);

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
        MCT_DELAY(wait_ms);
        cnt++;
    } while (cnt < loop_max);

    /* 匹配结束后，若还有未被预期帧覆盖的字节且开启了粘帧处理，则切分粘帧。
     * remain_len 在命中预期帧时只扣减了预期帧自身长度，故此处：
     *   remain_len = 帧前字节数 + 帧后字节数
     * 任一侧存在粘帧即 >0 -> 预期帧之前/之后的粘帧都会进入切分。 */
    if(remain_len > 0 && inst->sticky_frame_enable)
    {
        /* 游标式贪心切分：同类型重复帧(AAB/ABA)可全部按字节流顺序切出，
         * 自动跳过帧间无匹配的脏数据，并保证收敛、不死循环 */
        sticky_frames_extract(inst, cmdList, cmdListNum, payloadlist);
    }
    else
    {
        //帧处理完毕
    }
    return result;
}

static bool all_cmd_seek(MctInstance *inst, tCmd const *cmdList, uint16_t cmdListNum, StaticFrameList *payloadlist)
{
    int32_t single_len = 0;
    int32_t remain_len = 0;
    bool result = false;

    // 收帧
    single_len = read_payload_once(inst, &remain_len);

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

static bool cmd_seek_with_pecify(MctInstance *inst, tCmd const *cmdList, uint16_t cmdListNum, \
                                                                            int32_t expected_tcmd_id, \
                                                                            StaticFrameList *payloadlist)
{
    int32_t single_len = 0;
    int32_t remain_len = 0;
    bool result = false;

    // 收帧
    single_len = read_payload_once(inst, &remain_len);

    // 有数据更新，则进入一次判断
    if (single_len > 0) 
    {
        for (uint16_t i = 0; i < cmdListNum; i++)
        {
            //此机制下，同一帧重复命令出现多次会被过滤，只处理第一次出现的重复命令
            //只匹配预期
            if((RecvSend == cmdList[i].Type)&&(expected_tcmd_id == cmdList[i].id))
            {
                frameMacheType matcheResult = frame_mache(inst, &cmdList[i], true,cmdList[i].id, payloadlist, &remain_len);
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
                    bool is_send_rev,int32_t expected_tcmd_id,uint16_t cmdlist_seq_i)
{
    if(true == is_send_rev)
    {
        //预期帧寻找
        return expected_cmd_seek(inst,cmdList,cmdListNum,expected_tcmd_id,payloadlist,cmdlist_seq_i); 
    }
    else
    {
        if(NULL_CMD_SEEK == expected_tcmd_id)
        {
            return all_cmd_seek(inst, cmdList,cmdListNum,payloadlist);  
        }
        else
        {
            return cmd_seek_with_pecify(inst, cmdList,cmdListNum,expected_tcmd_id,payloadlist);  
        }

    }

}
/**************************************************************************************** */
static dealprocess singleframeListDeal_expected(MctInstance *inst, StaticFrameList *payloadlist, \
                                       tCmd const *cmdList, uint16_t cmdListNum, void *para)
{
    bool commandFound = false;
    dealprocess status = FRAME_FAILED;

    for (uint8_t i = 0; i < cmdListNum; i++)
    {
        // 检查当前命令项是否与目标ID匹配
        if (payloadlist->frames_expected.tcmd_id == cmdList[i].id)
        {
            commandFound = true;

            if (!cmdList[i].analyze(inst->payload_cache + payloadlist->frames_expected.startOffset,payloadlist->frames_expected.length, para))
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
    return status;
}
static dealprocess singleframeListDeal(MctInstance *inst, StaticFrameList *payloadlist, \
                                       tCmd const *cmdList, uint16_t payloadlist_id, uint16_t cmdListNum)
{
    bool commandFound = false;
    dealprocess status = FRAME_FAILED;

    for (uint8_t i = 0; i < cmdListNum; i++)
    {
        // 检查当前命令项是否与目标ID匹配
        if (payloadlist->frames[payloadlist_id].tcmd_id == cmdList[i].id)
        {
            commandFound = true;
            void *para = NULL;
            if(USE_VAR == cmdList[i].stickytype)
            {
                para = cmdList[i].para;
            }
            else if(USE_CB == cmdList[i].stickytype)
            {
                if (cmdList[i].get_para)
                {
                    para = cmdList[i].get_para();
                }
            }
            else
            {
                //do nothing
            }
            if (!cmdList[i].analyze(inst->payload_cache + payloadlist->frames[payloadlist_id].startOffset,
                                    payloadlist->frames[payloadlist_id].length, para))
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

static dealprocess frameListDeal(MctInstance *inst,StaticFrameList *payloadlist,tCmd const *cmdList,uint16_t cmdListNum)
{
  dealprocess status = FRAME_FAILED;
  if(payloadlist->size > 0)
  { 
    for(uint8_t i = 0;i < payloadlist->size;i++)
    {
        if(payloadlist->frames[i].status == FRAME_NEW)
        {
            if(FRAME_SUCCEED == singleframeListDeal(inst,payloadlist,cmdList,i,cmdListNum))
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
    bool is_send_rev = false;
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
                is_send_rev = true;
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
    if(is_send_rev)
    {
        if(expected_cmd_send(inst,&inst->payload_list,List,cmdlist_seq_i,expected_tcmd_id,para))
        {
            if(payload_scan(inst,&inst->payload_list,List,cmdNum,is_send_rev,expected_tcmd_id,cmdlist_seq_i))
            {
                result = expectframeDeal(inst,&inst->payload_list,List,cmdNum,para); 
                
            }
            frameListDeal(inst,&inst->payload_list,List,cmdNum);
        }
    }
    else
    {
        if(payload_scan(inst,&inst->payload_list,List,cmdNum,is_send_rev,expected_tcmd_id,cmdlist_seq_i))
        {
            if(inst->payload_list.have_expected)
            {
                if(FRAME_SUCCEED == singleframeListDeal_expected(inst,&inst->payload_list,List,cmdNum,para))
                {
                    result = true;
                }
            }
            if(inst->payload_list.size > 0)
            {
                if(FRAME_SUCCEED == frameListDeal(inst,&inst->payload_list,List,cmdNum))
                {
                    result = true;
                }
            }
        }
    }
    clearFrameList(&inst->payload_list);
    clearFrameExpectedFrameList(&inst->payload_list);

    return result;
}
