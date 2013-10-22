/**********************************************************************************************************************************
*
*   OpenAL cross platform audio library
*   Copyright (c) 2004, Apple Computer, Inc. All rights reserved.
*
*   Redistribution and use in source and binary forms, with or without modification, are permitted provided 
*   that the following conditions are met:
*
*   1.  Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer. 
*   2.  Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following 
*       disclaimer in the documentation and/or other materials provided with the distribution. 
*   3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of its contributors may be used to endorse or promote 
*       products derived from this software without specific prior written permission. 
*
*   THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
*   THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS 
*   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED 
*   TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
*   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE 
*   USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
**********************************************************************************************************************************/

#include "oalBuffer.h"
#include "oalSource.h"
#include "oalImp.h"

#include "CAGuard.h"

#define LOG_VERBOSE         0

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark _____Support Methods_____
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// OALBuffers
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** OALBuffers *****
OALBuffer::OALBuffer (ALuint	inSelfToken)
	: 	mSelfToken (inSelfToken),
#if USE_SOURCE_LIST_MUTEX
		mSourceListGuard ("OALBuffer::SourceListGuard"),
#endif
		mBufferLock ("OALBuffer::EditLock"),
		mInUseFlag(0),
		mData(NULL),
		mAppOwnsBufferMemory(false),
		mDataSize (0),
		mPreConvertedDataSize(0),
		mDataHasBeenConverted(false),
		mAttachedSourceList(NULL),
        mIsInPostRenderMessageQueue(false)
{
#if LOG_VERBOSE
	DebugMessageN1("OALBuffer::OALBuffer() - OALBuffer = %ld", (long int) mSelfToken);
#endif
	mAttachedSourceList = new AttachedSourceList ();			// create a source list map
	mAttachedSourceList->Reserve(128);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OALBuffer::~OALBuffer()
{
#if LOG_VERBOSE
	DebugMessageN1("OALBuffer::~OALBuffer() - OALBuffer = %ld", (long int) mSelfToken);
#endif
	if (!mAppOwnsBufferMemory && (mData != NULL))
		free (mData);
		
// should never be deleted unless this object said it was ok
#if USE_SOURCE_LIST_MUTEX
	bool wasLocked = mSourceListGuard.Lock();
#endif

	if (mAttachedSourceList)
		delete mAttachedSourceList;

#if USE_SOURCE_LIST_MUTEX
	if (wasLocked) mSourceListGuard.Unlock();
#endif
}
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool		OALBuffer::CanBeRemovedFromBufferMap()
{
#if LOG_VERBOSE
	DebugMessageN1("OALBuffer::CanBeRemovedFromBufferMap() - OALBuffer = %ld", (long int) mSelfToken);
#endif
	bool	returnValue = true;
	
#if USE_SOURCE_LIST_MUTEX
	bool wasLocked = mSourceListGuard.Lock();
#endif

	// if the buffer object is not attached to a source, it's ok to remove
	if (mAttachedSourceList->Size() == 0)
		returnValue = true;
	else
	{
		// if all attached sources are transitioning to flush, it can be removed from buffer map,
		// but the OALBuffer object must be deleted at a later time
		for (UInt32	i = 0; i < mAttachedSourceList->Size(); i++)
		{
			OALSource	*curSource = mAttachedSourceList->GetSourceByIndex(i);
			if (curSource)
			{
				if (!curSource->IsSourceTransitioningToFlushQ())
				{
					returnValue = false; 
					goto Finished;
				}
			}
		}
	}
	
Finished:

#if USE_SOURCE_LIST_MUTEX
	if (wasLocked) mSourceListGuard.Unlock();
#endif

	return returnValue; // all attached sources are transitioning to flush
}								

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool	OALBuffer::IsPurgable()
{
#if LOG_VERBOSE
	DebugMessageN1("OALBuffer::IsPurgable() - OALBuffer = %ld", (long int) mSelfToken);
#endif
	bool returnValue = false;
	
#if USE_SOURCE_LIST_MUTEX
	bool wasLocked = mSourceListGuard.Lock();
#endif
	
	// make sure that no other source has attached this buffer, and that no other thread is editing it
	if ((mAttachedSourceList->Size() == 0) && mBufferLock.IsFree() && (mInUseFlag <= 0) && (!IsInPostRenderMessageQueue()))
		returnValue = true; 
	
#if USE_SOURCE_LIST_MUTEX
	if (wasLocked) mSourceListGuard.Unlock();
#endif

	return returnValue;
}
															
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus	OALBuffer::AddAudioDataStatic(char*	inAudioData, UInt32	inAudioDataSize, ALenum format, ALsizei freq)
{
#if LOG_VERBOSE
	DebugMessageN5("OALBuffer::AddAudioDataStatic() - OALBuffer:inAudioData:inAudioDataSize:format:freq = %ld:%p:%ld:%d:%d", (long int) mSelfToken, inAudioData, (long int) inAudioDataSize, format, freq);
#endif
    #if LOG_EXTRAS		
        DebugMessage("AddAudioDataStatic called: Converting Data Now");
    #endif

	CAGuard::Locker bufferLock(mBufferLock);

	try {
        
		if (!IsFormatSupported(format))
           throw ((OSStatus) AL_INVALID_VALUE);   // this is not a valid buffer token or is an invalid format
	
		// don't allow if the buffer is in a queue
        if (mAttachedSourceList->Size() > 0)
        {
            DebugMessage("AddAudioDataStatic ATTACHMENT > 0");
            throw ((OSStatus) AL_INVALID_OPERATION);   
        }
		
        mPreConvertedDataSize = (UInt32) inAudioDataSize;
        OSStatus    result = noErr;
		
		// if this buffer was using memory created by the library, free it now and initialize mData
		if (!mAppOwnsBufferMemory && (mData != NULL))
		{
			free (mData);
			mData = NULL;
		}
		
		mData = (UInt8*) inAudioData;
		mDataSize = (UInt32) inAudioDataSize;
				
		result = FillInASBD(mDataFormat, format, freq);
			THROW_RESULT
			
		mPreConvertedDataFormat.SetFrom(mDataFormat); //  make sure they are the same so original format info can be returned to caller
    }
    catch (OSStatus     result) {
		mData = NULL;
		mAppOwnsBufferMemory = false;
		DebugMessageN1("AddAudioDataStatic Failed - err = %ld\n", (long int) result);
        alSetError(result);
    }
    catch (...) {
		mData = NULL;
		mAppOwnsBufferMemory = false;
		DebugMessage("AddAudioDataStatic Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
	mAppOwnsBufferMemory = true;
	return noErr;
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus	OALBuffer::AddAudioData(char*	inAudioData, UInt32	inAudioDataSize, ALenum format, ALsizei freq, bool	inPreConvertToHalFormat)
{
#if LOG_VERBOSE
	DebugMessageN6("OALBuffer::AddAudioData() - OALBuffer:inAudioData:inAudioDataSize:format:freq:inPreConvertToHalFormat = %ld:%p:%ld:%d:%d:%d", (long int) mSelfToken, inAudioData, (long int) inAudioDataSize, format, freq, inPreConvertToHalFormat);
#endif
	// creates memory if needed
	// reallocs if needed
	// returns an error if buffer is in use

	CAGuard::Locker bufferLock(mBufferLock);
	
	try {
        if (!IsFormatSupported(format))
           throw ((OSStatus) AL_INVALID_VALUE);   // this is not a valid buffer token or is an invalid format
	
#if USE_SOURCE_LIST_MUTEX
	bool wasLocked = mSourceListGuard.Lock();
#endif
		// don't allow if the buffer is in a queue
		UInt32	attachedCount = mAttachedSourceList->Size();

#if USE_SOURCE_LIST_MUTEX
		if (wasLocked) mSourceListGuard.Unlock();
#endif

        if (attachedCount > 0)
        {
			DebugMessage("WAITING: AddAudioData ---> WaitOneRenderCycle");
			// Let a render cycle go by and try again

			WaitOneRenderCycle();

#if USE_SOURCE_LIST_MUTEX
			wasLocked = mSourceListGuard.Lock();
#endif
			attachedCount = mAttachedSourceList->Size();

#if USE_SOURCE_LIST_MUTEX
			if (wasLocked) mSourceListGuard.Unlock();
#endif
			
			if (attachedCount > 0){
				DebugMessageN2("OALBuffer::AddAudioData: buffer ATTACHMENT > 0 - mSelfToken:mAttachedSourceList->Size() = %ld:%ld", (long int) mSelfToken, (long int) mAttachedSourceList->Size());
				throw ((OSStatus) AL_INVALID_OPERATION);   
			}
        }

		if (mAppOwnsBufferMemory)
		{
			mData = NULL;	// we were using the apps memory before so just initialize mData incase we fail
			mAppOwnsBufferMemory = false;
		}
		
        mPreConvertedDataSize = (UInt32) inAudioDataSize;
        // do not pre-convert stereo sounds, let the AC do the deinterleaving	
        OSStatus    result = noErr;
        if (!inPreConvertToHalFormat || ((format == AL_FORMAT_STEREO16) || (format == AL_FORMAT_STEREO8)))
        {
            if (mData != NULL)
            {
                if (mDataSize != (UInt32) inAudioDataSize)
                {
                    mDataSize = (UInt32) inAudioDataSize;
                    void *newDataPtr = realloc(mData, mDataSize);
                    mData = (UInt8 *) newDataPtr;
                }		
            }
            else
            {
                mDataSize = (UInt32) inAudioDataSize;
                mData = (UInt8 *) malloc (mDataSize);
            }
            
            if (mData)
            {
                result = FillInASBD(mDataFormat, format, freq);
                    THROW_RESULT
                
                mPreConvertedDataFormat.SetFrom(mDataFormat); //  make sure they are the same so original format info can be returned to caller
                memcpy (mData, inAudioData, mDataSize);		
            }
        }
        else
        {
    #if LOG_EXTRAS		
        DebugMessage("alBufferData called: Converting Data Now");
    #endif

            result = ConvertDataForBuffer(inAudioData, inAudioDataSize, format, freq);	// convert the data to the mixer's format and copy to the buffer
                THROW_RESULT
        }
    }
    catch (OSStatus     result) {
		DebugMessageN1("OALBuffer::AddAudioData Failed - err = %ld\n", (long int) result);
        alSetError(result);
		throw result;
    }
    catch (...) {
		DebugMessage("OALBuffer::AddAudioData Failed");
        alSetError(AL_INVALID_OPERATION);
		throw -1;
	}
			
	return noErr;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// This currently will only work for cbr formats
OSStatus OALBuffer::ConvertDataForBuffer(void *inData, UInt32 inDataSize, UInt32	inDataFormat, UInt32 inDataSampleRate)
{
#if LOG_VERBOSE
	DebugMessageN5("OALBuffer::ConvertDataForBuffer() - OALBuffer:inData:inDataSize:inDataFormat:inDataSampleRate = %ld:%p:%ld:%ld:%ld", (long int) mSelfToken, inData, (long int) inDataSize, (long int) inDataFormat, (long int) inDataSampleRate);
#endif    
	OSStatus					result = noErr;

    try {

        AudioConverterRef			converter;
        CAStreamBasicDescription	destFormat;
        UInt32						framesOfSource = 0;

        if (inData == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        result = FillInASBD(mPreConvertedDataFormat, inDataFormat, inDataSampleRate);
            THROW_RESULT

        if (mPreConvertedDataFormat.NumberChannels() == 1)
            mPreConvertedDataFormat.mFormatFlags |= kAudioFormatFlagIsNonInterleaved; 
                    
        destFormat.mChannelsPerFrame = mPreConvertedDataFormat.NumberChannels();
        destFormat.mSampleRate = mPreConvertedDataFormat.mSampleRate;
        destFormat.mFormatID = kAudioFormatLinearPCM;
        
        if (mPreConvertedDataFormat.NumberChannels() == 1)
            destFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
        else
            destFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked; // leave stereo data interleaved, and an AC will be used for deinterleaving later on
        
        destFormat.mFramesPerPacket = 1;	
        destFormat.mBitsPerChannel = sizeof (Float32) * 8;	
        destFormat.mBytesPerPacket = sizeof (Float32) * destFormat.NumberChannels();	
        destFormat.mBytesPerFrame = sizeof (Float32) * destFormat.NumberChannels();
        
        result = FillInASBD(mDataFormat, inDataFormat, UInt32(destFormat.mSampleRate));
            THROW_RESULT
            
        result = AudioConverterNew(&mPreConvertedDataFormat, &destFormat, &converter);
            THROW_RESULT

		framesOfSource = inDataSize / mPreConvertedDataFormat.mBytesPerFrame; // THIS ONLY WORKS FOR CBR FORMATS
			
       	UInt32 dataSize = framesOfSource * sizeof(Float32) * destFormat.NumberChannels();
        
		mDataSize = (UInt32) dataSize;

        if (mData != NULL)
        {
            if (mDataSize != dataSize)
            {
                mDataSize = dataSize;
                void *newDataPtr = realloc(mData, mDataSize);
                if (newDataPtr == NULL)
                    throw ((OSStatus) AL_INVALID_OPERATION);
                    
                mData = (UInt8 *) newDataPtr;
            }		
        }
        else
        {
            mDataSize = dataSize;
            mData = (UInt8 *) malloc (mDataSize);
            if (mData == NULL)
                throw ((OSStatus) AL_INVALID_OPERATION);
        }

        if (mData != NULL)
        {
			result = AudioConverterConvertBuffer(converter, inDataSize, inData, &mDataSize, mData);
			if (result == noErr)
            {
                mDataFormat.SetFrom(destFormat);
				if (mPreConvertedDataFormat.NumberChannels() == 1)
					mDataHasBeenConverted = true;
				else
					mDataHasBeenConverted = false;
			}
        }
        
        AudioConverterDispose(converter);
    }
    catch (OSStatus     result) {
        return (result);
    }
    catch (...) {
        result = (OSStatus) AL_INVALID_OPERATION;
    }
    
	return (result);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Methods called from OALSource objects
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 	OALBuffer::GetFramesToBytes(UInt32	inOffsetInFrames) 
{
#if LOG_VERBOSE
	DebugMessageN2("OALBuffer::GetFramesToBytes() - OALBuffer:inOffsetInFrames = %ld:%ld", (long int) mSelfToken, (long int) inOffsetInFrames);
#endif
	UInt32		returnValue = 0;

	if (GetFramesPerPacket() == 1)
	{
		// pcm - it's easy
		returnValue = inOffsetInFrames * GetBytesPerPacket();			
	}
	else
	{
		// discover which packet conatins the frame we want to offset to
		// CURRENTLY UNNECESSARY BECAUSE WE ARE ONLY STORING PCM DATA
	}
	
	return returnValue;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32		OALBuffer::GetFrameCount()
{
#if LOG_VERBOSE
	DebugMessageN1("OALBuffer::GetFrameCount() - OALBuffer = %ld", (long int) mSelfToken);
#endif
	UInt32	totalPackets =  (mDataFormat.mBytesPerPacket == 0) ? 0 : mDataSize/mDataFormat.mBytesPerPacket;
	
	return totalPackets * mDataFormat.mFramesPerPacket;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool	OALBuffer::UseThisBuffer(OALSource*	inSource)
{
#if LOG_VERBOSE
	DebugMessageN1("OALBuffer::UseThisBuffer() - OALBuffer = %ld", (long int) mSelfToken);
#endif
#if USE_SOURCE_LIST_MUTEX
	bool wasLocked = mSourceListGuard.Lock();
#endif
	
	// see if this source is in list already
	if (mAttachedSourceList->SourceExists(inSource) == true)
		mAttachedSourceList->IncreaseAttachmentCount(inSource);
	else
	{
		SourceAttachedInfo		nuSourceInfo;
		
		nuSourceInfo.mSource = inSource;
		nuSourceInfo.mAttachedCount = 1;
		
		mAttachedSourceList->Add(nuSourceInfo);
	}

#if USE_SOURCE_LIST_MUTEX
	if (wasLocked) mSourceListGuard.Unlock();
#endif
	
#if LOG_EXTRAS		
	DebugMessageN2("OALBuffer::UseThisBuffer - BufferToken:SourceToken = %ld:%ld", (long int)mSelfToken, (long int)inSource->GetToken());
#endif
	
	return noErr;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Called by OALSource's BufferQueue::RemoveQueueEntryByIndex() class
bool	OALBuffer::ReleaseBuffer(OALSource*	inSource)
{
#if LOG_VERBOSE
	DebugMessageN2("OALBuffer::ReleaseBuffer() - OALBuffer:inSource = %ld:%ld", (long int) mSelfToken, (long int) inSource->GetToken());
#endif
	bool returnValue = false;
#if LOG_EXTRAS		
	DebugMessageN2("OALBuffer::ReleaseBuffer - BufferToken:SourceToken = %ld:%ld", mSelfToken, inSource->GetToken());
#endif

#if USE_SOURCE_LIST_MUTEX
	bool wasLocked = mSourceListGuard.Lock();
#endif

	// see if this source is in list already
	if (mAttachedSourceList->SourceExists(inSource) == true)
	{
		UInt32		attachmentCount = mAttachedSourceList->DecreaseAttachmentCount(inSource);
		if (attachmentCount == 0)
		{
			//DebugMessageN2("OALBuffer::ReleaseBuffer - BufferToken:SourceToken = %ld:%ld", (long int) mSelfToken, (long int) inSource->GetToken());
			mAttachedSourceList->Remove(inSource);
			returnValue = true;
		}
	}
	
#if USE_SOURCE_LIST_MUTEX
	if (wasLocked) mSourceListGuard.Unlock();
#endif

	return returnValue;
}