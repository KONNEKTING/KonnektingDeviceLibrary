#define MANUFACTURER_ID 57005
#define DEVICE_ID 255
#define REVISION 0

#define COMOBJ_ledOnOff 0
#define COMOBJ_trigger 1
#define PARAM_blinkDelay 0
        
KnxComObject KnxDevice::_comObjectsList[] = {
    /* Index 0 - ledOnOff */ KnxComObject(KNX_DPT_1_001, 0x2a),
    /* Index 1 - trigger */ KnxComObject(KNX_DPT_1_001, 0x34)
};
const byte KnxDevice::_numberOfComObjects = sizeof (_comObjectsList) / sizeof (KnxComObject); // do not change this code
       
byte KonnektingDevice::_paramSizeList[] = {
    /* Index 0 - blinkDelay */ PARAM_UINT16
};
const int KonnektingDevice::_numberOfParams = sizeof (_paramSizeList); // do not change this code
