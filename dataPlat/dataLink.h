#ifndef DATA_LINK_H
#define DATA_LINK_H

#ifdef __cplusplus
extern "C" {
#endif
#include "../../../Inc/projDefine.h"
#include "../../../Inc/typedef.h"



#define MCT(NAME) MCT_DATA_##NAME
#undef X
#define X(name, var, len) name,

// 生成枚举（接口模式和实现模式共用）
typedef enum {
    #include "dataLinkDef.h"  // 包含定义文件
    MCT_DATA_MAX_COUNT,
} mctParaIndex;

// 恢复X宏定义
#undef X


extern void *mctParaGetPtr(mctParaIndex id);
extern uint32_t mctParaGet(mctParaIndex id, void* outBuf, uint32_t bufLen);
extern uint32_t mctParaSet(mctParaIndex id, const void* inData, uint32_t dataLen);

#define MCT_PTR(ID) mctParaGetPtr(MCT(ID))
#define MCT_GET(ID,OUT_BUF,BUF_LEN) mctParaGet(MCT(ID),OUT_BUF,BUF_LEN)
#define MCT_SET(ID,IN_DATA,DATA_LEN) mctParaSet(MCT(ID),IN_DATA,DATA_LEN)
#ifdef __cplusplus
}
#endif
#endif
