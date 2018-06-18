#include <IOKit/audio/IOAudioEngine.h>

#define VACEngine com_osxkernel_VACEngine

class VACEngine : public IOAudioEngine
{
    OSDeclareDefaultStructors(VACEngine)

public:
    virtual void free();                                // standard Method
    
    virtual bool initHardware(IOService *provider);     // standard Method
    virtual void stop(IOService *provider);
    
    virtual IOAudioStream *createNewAudioStream(IOAudioStreamDirection direction,
                                                void *sampleBuffer, UInt32 sampleBufferSize);
    
    virtual IOReturn performAudioEngineStart();
    virtual IOReturn performAudioEngineStop();
    
    virtual UInt32 getCurrentSampleFrame();
    
    virtual IOReturn performFormatChange(IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat,
                                         const IOAudioSampleRate *newSampleRate);
    
    virtual IOReturn clipOutputSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame,
                                       UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat,
                                       IOAudioStream *audioStream);
    virtual IOReturn convertInputSamples(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame,
                                         UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat,
                                         IOAudioStream *audioStream);
    
private:
    IOTimerEventSource*     fAudioInterruptSource;
    SInt16                  *IOBuffer;                  // actually unused, we use the same for input and output
    float                   *CopyBuffer;                // For moving sound data through the cable
    
    static void             interruptOccured(OSObject* owner, IOTimerEventSource* sender);
    
    SInt32                  ChunkCounter;               // determines actual chunk
    SInt64                  fNextTimeout;               // next time to fire interrupt
    UInt64                  NanosecPerBlock;            // nanosecs that fit into one chunk

};

