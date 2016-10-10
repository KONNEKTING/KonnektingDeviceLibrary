#define MANUFACTURER_ID 57005
#define DEVICE_ID 2
#define REVISION 0

#define COMOBJ_tempValue 0
#define COMOBJ_tempMin 1
#define COMOBJ_tempMax 2
#define PARAM_initialDelay 0
#define PARAM_tempSendUpdate 1
#define PARAM_tempPollingTime 2
#define PARAM_tempDiff 3
#define PARAM_tempMinValue 4
#define PARAM_tempMinLimit 5
#define PARAM_tempMaxValue 6
#define PARAM_tempMaxLimit 7
        
KnxComObject KnxDevice::_comObjectsList[] = {
    /* Index 0 - tempValue */ KnxComObject(KNX_DPT_9_001, 0x34),
    /* Index 1 - tempMin */ KnxComObject(KNX_DPT_1_001, 0x34),
    /* Index 2 - tempMax */ KnxComObject(KNX_DPT_1_001, 0x34)
};
const byte KnxDevice::_numberOfComObjects = sizeof (_comObjectsList) / sizeof (KnxComObject); // do not change this code
       
byte KonnektingDevice::_paramSizeList[] = {
    /* Index 0 - initialDelay */ PARAM_UINT16,
    /* Index 1 - tempSendUpdate */ PARAM_UINT8,
    /* Index 2 - tempPollingTime */ PARAM_UINT32,
    /* Index 3 - tempDiff */ PARAM_UINT8,
    /* Index 4 - tempMinValue */ PARAM_UINT8,
    /* Index 5 - tempMinLimit */ PARAM_INT16,
    /* Index 6 - tempMaxValue */ PARAM_UINT8,
    /* Index 7 - tempMaxLimit */ PARAM_INT16
};
const byte KonnektingDevice::_numberOfParams = sizeof (_paramSizeList); // do not change this code