/*
 MacOS virtual Audio cable driver, programmed by DL1YCF.
 This code is derived from the MyAudioDevice sample code
 from the book
 
 O. H. Halvorsen and D. Clarke,
 "iOS and Mac OS Kernel Programming",
 Apress, 2011
 
 The sample code has been downloaded from github
 
 */

#include <IOKit/audio/IOAudioControl.h>
#include <IOKit/audio/IOAudioLevelControl.h>
#include <IOKit/audio/IOAudioToggleControl.h>
#include <IOKit/audio/IOAudioDefines.h>

#include <IOKit/IOLib.h>

#include "VACDevice.h"
#include "VACEngine.h"

#define super IOAudioDevice

OSDefineMetaClassAndStructors(VACDevice, IOAudioDevice)

/*
 * The task of this function is to create the Audio engines.
 * Apart from that, there is nothing to do since we have
 * no "real" hardware here.
 * We create two audio engines, which will later be
 * called VAC1 and VAC2. Of course, if needed, we can
 * create any number of additional engines.
 */

bool VACDevice::initHardware(IOService *provider)
{
    bool result = false;
    
    if (!super::initHardware(provider))
        goto done;
        
    setDeviceName("Virtual Audio Cable");
    setDeviceShortName("VAC");
    setManufacturerName("DL1YCF");
        
    if (!createAudioEngine("SDR-RX")) goto done;
    if (!createAudioEngine("SDR-TX")) goto done;
    
    result = true;
    
done:
    return result;
}


bool VACDevice::createAudioEngine(const char *name)
{
    bool result = false;
    VACEngine *audioEngine = NULL;
       
    audioEngine = new VACEngine();
    if (!audioEngine) goto done;
    
    if (!audioEngine->init(NULL)) goto done;
    audioEngine->setDescription(name);
    
    activateAudioEngine(audioEngine);
   	audioEngine->release(); 
    result = true;
    
done:
   
    if (!result && (audioEngine != NULL)) {
        audioEngine->release();
    }

    return result;
}

