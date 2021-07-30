#include <IOKit/IOLib.h>
#include <IOKit/IOTimerEventSource.h>

#include "VACDevice.h"
#include "VACEngine.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// 12 chunks with 2048 samples each, that is about half a second
// and more than enough.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define ChunkSize       2048
#define NumChunk        12
#define NumChan         2
#define IOBufSize       (ChunkSize*NumChunk*NumChan*sizeof(float))  // used both for input and output
#define FBufSize        (ChunkSize*NumChunk*NumChan*sizeof(float))

// "parent" class
#define super IOAudioEngine

OSDefineMetaClassAndStructors(VACEngine, IOAudioEngine)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Init the engine.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool VACEngine::initHardware(IOService *provider)
{
    bool result = false;
    IOAudioSampleRate initialSampleRate;
    IOAudioStream *audioStream;
    IOWorkLoop* workLoop = NULL;
    
    if (!super::initHardware(provider)) goto done;
    
    if (!(fAudioInterruptSource = IOTimerEventSource::timerEventSource(this, interruptOccured))) goto done;
    if (!(workLoop = getWorkLoop())) goto done;
    if (workLoop->addEventSource(fAudioInterruptSource) != kIOReturnSuccess) goto done;
    
    // Setup the initial sample rate for the audio engine
    initialSampleRate.whole = 48000;
    initialSampleRate.fraction = 0;
    
    setSampleRate(&initialSampleRate);
    NanosecPerBlock=((UInt64) ChunkSize * 1000000000) / initialSampleRate.whole;
    
    // Set the number of sample frames in each buffer
    setNumSampleFramesPerBuffer(ChunkSize*NumChunk);
    //setInputSampleLatency(ChunkSize);
    //setOutputSampleOffset(ChunkSize);
    
    //
    // We do not really need the IOBuffers, but they are
    // used by CoreAudio (e.g. parts of them are cleared).
    // To this end, we have to provide dummy storage
    // (enough to store NumChan*ChunkSize*NumChunk samples),
    // but we can use the same storage for input and output
    // since THIS driver never works with the sample buffers.
    //
    if (!(IOBuffer = (float *)IOMalloc(IOBufSize))) goto done;
    if (!(CopyBuffer = (float *) IOMalloc(FBufSize))) goto done;


    // Create an IOAudioStream for both output and input, and add it to this audio engine
    if (!(audioStream = createNewAudioStream(kIOAudioStreamDirectionOutput, IOBuffer, IOBufSize))) goto done;
    addAudioStream(audioStream);
    audioStream->release();
    
    if (!(audioStream = createNewAudioStream(kIOAudioStreamDirectionInput, IOBuffer, IOBufSize))) goto done;
    addAudioStream(audioStream);
    audioStream->release();
    
    result = true;
    
done:
    return result;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// release resource occupied from this engine (buffers, etc.)
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void VACEngine::free()
{
    // The input and output buffers are the same.
    if (IOBuffer) {
        IOFree(IOBuffer, IOBufSize);
        IOBuffer = NULL;
    }

    if (CopyBuffer) {
        IOFree(CopyBuffer,FBufSize);
        CopyBuffer=NULL;
    }
    
    if (fAudioInterruptSource)
    {
        fAudioInterruptSource->release();
        fAudioInterruptSource = NULL;
    }
    
    super::free();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Create new audio stream (input or output) with fixed 32-bit float format, 2 channels
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
IOAudioStream *VACEngine::createNewAudioStream(IOAudioStreamDirection direction, void *sampleBuffer,
                                                UInt32 sampleBufferSize)
{
    IOAudioStream *audioStream;
    
    audioStream = new IOAudioStream;
    if (audioStream) {
        if (!audioStream->initWithAudioEngine(this, direction, 0)) {
            audioStream->release();
            audioStream=NULL;
        } else {
            // Note that the format is completely virtual since we are doing no
            // actual I/O in this "virtual cable" driver. We never leave the internal
            // "float" representation of CoreAudio.
            // The only parameter that matters here is the number of Channels and the
            // Sample rate.
            // To indicate that everything is done with floating point, we use
            // the 32-bit float format. The results are probably not different when
            // using signed 16-bit integers, which would save half of the I/O buffer storage.
            IOAudioSampleRate rate;
            IOAudioStreamFormat format = {
                NumChan,                                          // num channels
                kIOAudioStreamSampleFormatLinearPCM,              // sample format
                kIOAudioStreamNumericRepresentationIEEE754Float,  // numeric format
                32,                                               // bit depth
                32,                                               // bit width
                kIOAudioStreamAlignmentHighByte,                  // high byte aligned -
                                                                  // unused because bit depth == bit width
                kIOAudioStreamByteOrderBigEndian,                 // big endian
                true,                                             // format is mixable
                0                                                 // driver-defined tag - unused by this driver
            };
            /*
             * Some sample rates supported by this driver.
             * Can easily define more if needed
             */
            audioStream->setSampleBuffer(sampleBuffer, sampleBufferSize);
            rate.fraction = 0;
            rate.whole = 16000;
            audioStream->addAvailableFormat(&format, &rate, &rate);
            rate.whole = 44100;
            audioStream->addAvailableFormat(&format, &rate, &rate);
            rate.whole = 48000;
            audioStream->addAvailableFormat(&format, &rate, &rate);
            audioStream->setFormat(&format);
        }
    }
    return audioStream;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Stop periodic interrupts, remove them from the work loop, call "stop" in the upstream class
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void VACEngine::stop(IOService *provider)
{
    if (fAudioInterruptSource)
    {
        fAudioInterruptSource->cancelTimeout();
        getWorkLoop()->removeEventSource(fAudioInterruptSource);
    }
    super::stop(provider);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Init the buffers, start periodic interrupts
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
IOReturn VACEngine::performAudioEngineStart()
{
    UInt64  time, timeNS;

    ChunkCounter=0;
    NoSignal=1;   // initially mute.
    takeTimeStamp(false);
    fAudioInterruptSource->setTimeout(NanosecPerBlock);
    
    clock_get_uptime(&time);
    absolutetime_to_nanoseconds(time, &timeNS);
    
    fNextTimeout = timeNS + NanosecPerBlock;
    return kIOReturnSuccess;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Stop the periodic interrupts
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
IOReturn VACEngine::performAudioEngineStop()
{
    if (fAudioInterruptSource) {
        fAudioInterruptSource->cancelTimeout();
    }
    return kIOReturnSuccess;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Return "where are we now"? within the sample buffer
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
UInt32 VACEngine::getCurrentSampleFrame()
{
    return ChunkCounter*ChunkSize;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Simply adjust the interrupt period when the sample rate changes.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
IOReturn VACEngine::performFormatChange(IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate)
{
    // Since we only allow one format, we only need to be concerned with sample rate changes
    // If the sample has changed, we only need to re-calculate the delay between two interrupts

    if (newSampleRate) {
        NanosecPerBlock = ((UInt64) ChunkSize * 1000000000) / newSampleRate->whole;
    }
    
    return kIOReturnSuccess;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// This is the actual cable engine, "output" side.
// In principle there is nothing to clip, since we just
// move 32-bit floating point data to a temporary buffer
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
IOReturn VACEngine::clipOutputSamples(const void *mixBuf, void *sampleBuf,
                                          UInt32 firstSampleFrame, UInt32 numSampleFrames,
                                          const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
    UInt32 SampleSize=streamFormat->fNumChannels * sizeof(float);
    UInt32 StartByte = firstSampleFrame * SampleSize;
    UInt32 NumBytes = numSampleFrames * SampleSize;

    // sampleBuf not used
    memcpy((UInt8 *) CopyBuffer+StartByte, (UInt8 *)mixBuf+ StartByte, NumBytes);

    return kIOReturnSuccess;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// This is the "input" side of the cable.
// Note that we copy to destbuf[0], not to destbuf[firstSampleFrame]
// We take the next slice from the temp. buffer and copy it to destbuf
// while there is something at the other side of the cable. If NoSignal
// is set, just put silence through the cable. This avoids end-less
// "echos" of what has been put into the cable the last time the sender
// disconnected.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
IOReturn VACEngine::convertInputSamples(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
    UInt32 SampleSize=streamFormat->fNumChannels * sizeof(float);
    UInt32 StartByte = firstSampleFrame * SampleSize;
    UInt32 NumBytes = numSampleFrames * SampleSize;
    
    // sampleBuf not used
    if (NoSignal) {
        memset((UInt8 *)destBuf, 0, NumBytes);
    } else {
        memcpy((UInt8 *)destBuf, (UInt8 *)CopyBuffer + StartByte, NumBytes);
    }
    return kIOReturnSuccess;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// This is called periodically, and we have to be very careful about
// getting these interrupts on the very minute.
// Therefore we compare the "intended" interrupt time with the actual
// time (diff) and adjust the next time-out inverval accordingly.
//
// The actual contents is easy: let NoSignal reflect whether there is
// something put into this cable, adjust ChunkCounter and take a time
// stamp when wrapping around.
//
// If a client occurs which has not been there in the last interrupt,
// clear the whole CopyBuffer.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void VACEngine::interruptOccured(OSObject* owner, IOTimerEventSource* sender)
{
    UInt64      thisTimeNS;
    uint64_t    time;
    SInt64      diff;
    
    VACEngine* audioEngine = (VACEngine*)owner;

    // Both audioEngine and sender should be non-NULL, but in kernel
    // programming you better have much of paranoia
    if (audioEngine && sender) {
        IOAudioStream *stream=audioEngine->getAudioStream(kIOAudioStreamDirectionOutput, 1);
        if (stream) {
            if (stream->numClients == 0) {
                // If there is no source (client that produces an output), set NoSignal flag
                audioEngine->NoSignal=1;
            } else if (audioEngine->NoSignal == 1) {
                // If source re-appears, clear NoSignal flag and zero out the Buffer ONCE
		// This is also done the first time a source is connected
                memset((UInt8 *)audioEngine->CopyBuffer, 0, FBufSize);
                audioEngine->NoSignal=0;
            }
        }
        // If we have reached the last block of our buffer,
        // we wrap back to the beginning and take a time stamp
        if (audioEngine->ChunkCounter >= NumChunk-1) {
            audioEngine->ChunkCounter=0;
            audioEngine->takeTimeStamp();
        } else {
            audioEngine->ChunkCounter++;
        }
        // Adjust the time-to-next-interrupt by comparing the current time to when the Interrupt should have occured
        clock_get_uptime(&time);
        absolutetime_to_nanoseconds(time, &thisTimeNS);
        diff = ((SInt64)audioEngine->fNextTimeout - (SInt64)thisTimeNS);
        
        sender->setTimeout(audioEngine->NanosecPerBlock + diff);
        audioEngine->fNextTimeout += audioEngine->NanosecPerBlock;
    }
}
