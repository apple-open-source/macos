/**********************************************************************************************************************************
*
*   OpenAL cross platform audio library
*    Copyright (c) 2004, Apple Computer, Inc. All rights reserved.
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

#ifndef __OAL_BUFFER__
#define __OAL_BUFFER__

#include <Carbon/Carbon.h>
#include <CoreAudio/CoreAudioTypes.h>
#include "CAStreamBasicDescription.h"
#include <map>
#include <vector>
#include "al.h"
#include <libkern/OSAtomic.h>

#define USE_SOURCE_LIST_MUTEX  1

#if USE_SOURCE_LIST_MUTEX
#include "CAGuard.h"
#endif

class OALSource;        // forward declaration

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark _____AttachedSourceList_____
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct	SourceAttachedInfo {
								OALSource*		mSource;
								UInt32			mAttachedCount;
};

class AttachedSourceList : std::vector<SourceAttachedInfo> {
public:
    
	void 	Add(SourceAttachedInfo&	inSourceInfo)
	{
		push_back(value_type (inSourceInfo));
	}

    void     Remove(OALSource*		inSource)  {
        iterator	it = begin();        
        while (it != end()) {
            if ((*it).mSource == inSource)
			{
				erase(it);
				return;
			}
			else
				++it;
        }
		return;
    }

    bool     SourceExists(OALSource*		inSource)  {
        iterator	it = begin();        
        while (it != end()) {
            if ((*it).mSource == inSource)
				return true;
			else 
				++it;
        }
		return false;
    }

    void     IncreaseAttachmentCount(OALSource*		inSource)  {
        iterator	it = begin();        
        while (it != end()) {
            if ((*it).mSource == inSource) {
				(*it).mAttachedCount++;
				return;
			}
			else 
				++it;
        }
		return;
    }

    UInt32     DecreaseAttachmentCount(OALSource*		inSource)  {
        iterator	it = begin();        
        while (it != end()) {
            if ((*it).mSource == inSource) {
				(*it).mAttachedCount--;
				return (*it).mAttachedCount;
			}
			else
				++it;
        }
		return 0;
    }

    UInt32     GetAttachmentCount(OALSource*		inSource)  {
        iterator	it = begin();        
        while (it != end()) {
            if ((*it).mSource == inSource)
				 return (*it).mAttachedCount;
			else
				++it;
        }
		return 0;
    }

    OALSource*     GetSourceByIndex(short		index)  {
        iterator	it = begin();        
        std::advance(it, index);
        if (it != end())
            return((*it).mSource);
        return (NULL);
    }

    UInt32 Size () const { return size(); }
    bool Empty () const { return empty(); }
	void Reserve(UInt32 reserveSize) {return reserve(reserveSize); }
};

typedef	AttachedSourceList AttachedSourceList;

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark _____OALBuffer_____
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class OALBuffer
{
	public:
	OALBuffer(ALuint	inSelfToken);
	~OALBuffer();
	OSStatus						AddAudioData(	char*		inAudioData, UInt32	inAudioDataSize, ALenum format, ALsizei freq, bool	inPreConvertToHalFormat);													
	OSStatus						AddAudioDataStatic(char*	inAudioData, UInt32	inAudioDataSize, ALenum format, ALsizei freq);

	void							SetInUseFlag()					{ OSAtomicIncrement32Barrier(&mInUseFlag); }
	void							ClearInUseFlag()				{ OSAtomicDecrement32Barrier(&mInUseFlag); }
	bool							IsPurgable();
	bool							HasBeenConverted(){return mDataHasBeenConverted;}

	ALuint							GetToken(){return mSelfToken;}
	UInt32							GetFrameCount();
	UInt32							GetFramesToBytes(UInt32	inOffsetInFrames) ;
	Float64							GetSampleRate(){return mDataFormat.mSampleRate;}
	UInt32							GetBytesPerPacket(){return mDataFormat.mBytesPerPacket;}
	UInt32							GetFramesPerPacket(){return mDataFormat.mFramesPerPacket;}
	UInt32							GetPreConvertedBitsPerChannel(){return mPreConvertedDataFormat.mBitsPerChannel;}
	UInt32							GetNumberChannels(){return mDataFormat.NumberChannels();}
	UInt32							GetPreConvertedDataSize(){return mPreConvertedDataSize;}
	UInt8*							GetDataPtr(){return mData;}
	UInt32							GetDataSize(){return mDataSize;}
	CAStreamBasicDescription*		GetFormat(){return &mDataFormat;}
	bool							CanBeRemovedFromBufferMap();

	// called from OAL Source object
	bool							UseThisBuffer(OALSource*	inSource);			// one entry per source regardless of how many uses in the Q there are
	bool							ReleaseBuffer(OALSource*	inSource);			// when source does not need this buffer, remove it from the source list
	
	void							PrintFormat(){mDataFormat.Print();}

private:
		
	ALuint							mSelfToken;
#if USE_SOURCE_LIST_MUTEX
	CAGuard							mSourceListGuard;
#endif
	CAGuard							mBufferLock;				// lock to serialize all buffer manipulations
	volatile int32_t				mInUseFlag;					// flag to indicate if the buffer is currently being edited by one or more threads
	UInt8							*mData;						// ptr to the actual audio data
	bool							mAppOwnsBufferMemory;		// true when data is passed in via the alBufferDtatStatic API (extension)
	UInt32							mDataSize;					// size in bytes of the audio data ptr
	CAStreamBasicDescription		mDataFormat;				// format of the data of the data as it sits in memory
	UInt32							mPreConvertedDataSize;		// if data gets converted on the way in, it is necessary to remember the original size
	CAStreamBasicDescription		mPreConvertedDataFormat;	// if data gets converted on the way in, it is necessary to remember the original format
	bool							mDataHasBeenConverted;		// was the data converted to the mixer format when handed to the library?
	AttachedSourceList*				mAttachedSourceList;		// all the OAL Source objects that use this buffer

	OSStatus						ConvertDataForBuffer (void *inData, UInt32 inDataSize, UInt32	inDataFormat, UInt32 inDataSampleRate);
};


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark _____OALBufferMap_____
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class OALBufferMap : std::multimap<ALuint, OALBuffer*, std::less<ALuint> > {
public:
    
    void Add (const	ALuint	inBufferToken, OALBuffer **inBuffer)  
	{
		iterator it = upper_bound(inBufferToken);
		insert(it, value_type (inBufferToken, *inBuffer));
	}


    OALBuffer* GetBufferByIndex(UInt32	inIndex) {
        iterator	it = begin();

		for (UInt32 i = 0; i < inIndex; i++){
            if (it != end())
                ++it;
            else
                i = inIndex;
        }
        
        if (it != end())
            return ((*it).second);		
		
		return (NULL);
    }

    OALBuffer* Get(ALuint	inBufferToken) {
        iterator	it = find(inBufferToken);
        if (it != end())
            return ((*it).second);
		return (NULL);
    }
    
    void Remove (const	ALuint inBufferToken) {
        iterator 	it = find(inBufferToken);
        if (it != end())
            erase(it);
    }

    OALBuffer* GetFirstItem() {
        iterator	it = begin();
        if (it != end())
            return ((*it).second);
		return (NULL);
    }

    UInt32 Size () const { return size(); }
    bool Empty () const { return empty(); }
};

#endif