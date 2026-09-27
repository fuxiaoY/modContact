
#include "mctList.h"

const tModemList modemList[] =
{
    {.name = "MODBUS",      .api = mctModbusApiGet    },
    // user defined modem list
};
uint16_t mctModemLisNumGet(void)
{
    return (sizeof(modemList) / sizeof(tModemList));
}


const tModemList *mctModemListGet(void)
{
    return modemList;
}

