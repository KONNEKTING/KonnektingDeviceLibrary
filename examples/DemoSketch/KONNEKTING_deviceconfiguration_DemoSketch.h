#define MANUFACTURER_ID 57005
#define DEVICE_ID 255
#define REVISION 0

#define COMOBJ_commObj1 0
#define COMOBJ_commObj2 1
#define PARAM_blinkDelay 0
        
KnxComObject KnxDevice::_comObjectsList[] = {
    /* Index 0 - commObj1 */ KnxComObject(KNX_DPT_1_001, 0x2a),
    /* Index 1 - commObj2 */ KnxComObject(KNX_DPT_1_001, 0x34)
};
const byte KnxDevice::_numberOfComObjects = sizeof (_comObjectsList) / sizeof (KnxComObject); // do not change this code
       
byte KonnektingDevice::_paramSizeList[] = {
    /* Index 0 - initialDelay */ PARAM_UINT16
};
const byte KonnektingDevice::_numberOfParams = sizeof (_paramSizeList); // do not change this code
