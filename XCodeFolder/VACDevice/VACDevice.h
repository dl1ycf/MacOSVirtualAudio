#include <IOKit/audio/IOAudioDevice.h>

#define VACDevice com_osxkernel_VACDevice

class VACDevice : public IOAudioDevice
{    
    OSDeclareDefaultStructors(VACDevice);
    
    virtual bool initHardware(IOService *provider);
    bool createAudioEngine(const char *name);
};
