#include <IOKit/IOLib.h>
#include <IOKit/IOTimerEventSource.h>

#include "VACDevice.h"
#include "VACEngine.h"

#define ChunkSize       2048
#define NumChunk        32
#define NumChan         2
#define IOBufSize       (ChunkSize*NumChunk*NumChan*sizeof(SInt16))
#define FBufSize        (ChunkSize*NumChunk*NumChan*sizeof(float))

// Note that the I/O Buffers (IOBufSize) are actually unused.
// Probably it is not necessary to allocate them with full size, but who knows?

#define super IOAudioEngine

OSDefineMetaClassAndStructors(VACEngine, IOAudioEngine)

bool VACEngine::initHardware(IOService *provider)
{
    bool result = false;
    IOAudioSampleRate initialSampleRate;
    IOAudioStream *audioStream;
	IOWorkLoop* workLoop = NULL;
    
    if (!super::initHardware(provider))
        goto done;
    
    fAudioInterruptSource = IOTimerEventSource::timerEventSource(this, interruptOccured);
    if (!fAudioInterruptSource)
        return false;
   	
    workLoop = getWorkLoop();
	if (!workLoop)
		return false;

    if (workLoop->addEventSource(fAudioInterruptSource) != kIOReturnSuccess)
        return false;
    
    // Setup the initial sample rate for the audio engine
    initialSampleRate.whole = 48000;
    initialSampleRate.fraction = 0;
    
    setSampleRate(&initialSampleRate);
    NanosecPerBlock=((UInt64) ChunkSize * 1000000000) / initialSampleRate.whole;
    
    // Set the number of sample frames in each buffer
    setNumSampleFramesPerBuffer(ChunkSize*NumChunk);
    setInputSampleLatency(ChunkSize);
    setOutputSampleOffset(ChunkSize);
    
    IOBuffer = (SInt16 *)IOMalloc(IOBufSize);
    if (!IOBuffer)
        goto done;
    
    CopyBuffer = (float *) IOMalloc(FBufSize);
    if (!CopyBuffer) goto done;

    // Create an IOAudioStream for each buffer and add it to this audio engine
    audioStream = createNewAudioStream(kIOAudioStreamDirectionOutput, IOBuffer, IOBufSize);
    if (!audioStream)
        goto done;
    
    addAudioStream(audioStream);
    audioStream->release();
    
    audioStream = createNewAudioStream(kIOAudioStreamDirectionInput, IOBuffer, IOBufSize);
    if (!audioStream)
        goto done;
    
    addAudioStream(audioStream);
    audioStream->release();
    
    result = true;
    
done:
    return result;
}

void VACEngine::free()
{
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

IOAudioStream *VACEngine::createNewAudioStream(IOAudioStreamDirection direction, void *sampleBuffer, UInt32 sampleBufferSize)
{
    IOAudioStream *audioStream;
    
    audioStream = new IOAudioStream;
    if (audioStream) {
        if (!audioStream->initWithAudioEngine(this, direction, 0)) {
            audioStream->release();
        } else {
            // Note that the format is completely virtual since we are doing no
            // actual I/O in this "virtual cable" driver. We never leave the internal
            // "float" representation of CoreAudio.
            // The only parameter that matters here is the number of Channels and the
            // Sample rate.
            IOAudioSampleRate rate;
            IOAudioStreamFormat format = {
                NumChan,										// num channels
                kIOAudioStreamSampleFormatLinearPCM,			// sample format
                kIOAudioStreamNumericRepresentationSignedInt,	// numeric format
                16,             								// bit depth
                16,								                // bit width
                kIOAudioStreamAlignmentHighByte,				// high byte aligned -
                                                                // unused because bit depth == bit width
                kIOAudioStreamByteOrderBigEndian,				// big endian
                true,											// format is mixable
                0												// driver-defined tag - unused by this driver
            };
            /*
             * Some sample rates supported by this driver.
             * Can easily define define more if needed
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

void VACEngine::stop(IOService *provider)
{
    if (fAudioInterruptSource)
    {
        fAudioInterruptSource->cancelTimeout();
        getWorkLoop()->removeEventSource(fAudioInterruptSource);
    }
    super::stop(provider);
}

IOReturn VACEngine::performAudioEngineStart()
{
    UInt64  time, timeNS;

    ChunkCounter=0;
    takeTimeStamp(false);
    fAudioInterruptSource->setTimeout(NanosecPerBlock);
    
    clock_get_uptime(&time);
    absolutetime_to_nanoseconds(time, &timeNS);
    
    fNextTimeout = timeNS + NanosecPerBlock;
    return kIOReturnSuccess;
}

IOReturn VACEngine::performAudioEngineStop()
{
    fAudioInterruptSource->cancelTimeout();
    
    return kIOReturnSuccess;
}

UInt32 VACEngine::getCurrentSampleFrame()
{
    return ChunkCounter*ChunkSize;
}

IOReturn VACEngine::performFormatChange(IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate)
{
    // Since we only allow one format, we only need to be concerned with sample rate changes
    // If the sample has changed, we only need to re-calculate the delay between two interrupts

    if (newSampleRate) {
        NanosecPerBlock = ((UInt64) ChunkSize * 1000000000) / newSampleRate->whole;
    }
    
    return kIOReturnSuccess;
}

//
// This is the actual cable engine, "output" side.
// In principle there is nothing to clip, since we just
// move 32-bit floating point data to a temporary buffer
//
IOReturn VACEngine::clipOutputSamples(const void *mixBuf, void *sampleBuf,
                                          UInt32 firstSampleFrame, UInt32 numSampleFrames,
                                          const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
    UInt32 SampleSize=streamFormat->fNumChannels * sizeof(float);
    UInt32 StartByte = firstSampleFrame * SampleSize;
    UInt32 NumBytes = numSampleFrames * SampleSize;

    memcpy((UInt8 *) CopyBuffer+StartByte, (UInt8 *)mixBuf+ StartByte, NumBytes);

    return kIOReturnSuccess;
}

//
// This is the "input" side of the cable.
// Note that we copy to destbuf[0], not to destbuf[firstSampleFrame]
// We take the next slice from the temp. buffer and copy it to destbuf
//
IOReturn VACEngine::convertInputSamples(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
    UInt32 SampleSize=streamFormat->fNumChannels * sizeof(float);
    UInt32 StartByte = firstSampleFrame * SampleSize;
    UInt32 NumBytes = numSampleFrames * SampleSize;
    
    memcpy((UInt8 *)destBuf, (UInt8 *)CopyBuffer + StartByte, NumBytes);
    
    return kIOReturnSuccess;
}

void VACEngine::interruptOccured(OSObject* owner, IOTimerEventSource* sender)
{
    UInt64      thisTimeNS;
    uint64_t    time;
    SInt64      diff;
    
    VACEngine* audioEngine = (VACEngine*)owner;

    if (audioEngine) {
        // If there is no client that produces an output, clear the transfer buffer
        IOAudioStream *stream=audioEngine->getAudioStream(kIOAudioStreamDirectionOutput, 1);
        if (stream) {
            if (stream->numClients == 0 && audioEngine->CopyBuffer) memset((void *) (audioEngine->CopyBuffer), 0, FBufSize);
        }
        // If we have reached the last block of our buffer,
        // we wrap back to the beginning.
        if (audioEngine->ChunkCounter >= NumChunk-1) {
            audioEngine->ChunkCounter=0;
            audioEngine->takeTimeStamp();
        } else {
            audioEngine->ChunkCounter++;
        }
    }
    if (!sender)
        return;
    
    // Adjust the next "nap time" by comparing the current time to when the Interrupt should have occured
    clock_get_uptime(&time);
    absolutetime_to_nanoseconds(time, &thisTimeNS);
    diff = ((SInt64)audioEngine->fNextTimeout - (SInt64)thisTimeNS);
        
    sender->setTimeout(audioEngine->NanosecPerBlock + diff);
    audioEngine->fNextTimeout += audioEngine->NanosecPerBlock;
}
