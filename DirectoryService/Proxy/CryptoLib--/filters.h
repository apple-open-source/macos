/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifndef CRYPTOPP_FILTERS_H
#define CRYPTOPP_FILTERS_H

#include "cryptopp.h"
#include "cryptopp_misc.h"
#include "smartptr.h"
#include "queue.h"

NAMESPACE_BEGIN(CryptoPP)

/// provides an implementation of BufferedTransformation's attachment interface
class Filter : virtual public BufferedTransformation
{
public:
	Filter(BufferedTransformation *outQ);

	bool Attachable() {return true;}
	BufferedTransformation *AttachedTransformation() {return m_outQueue.get();}
	const BufferedTransformation *AttachedTransformation() const {return m_outQueue.get();}
	void Detach(BufferedTransformation *newOut = NULL);

protected:
	virtual void NotifyAttachmentChange() {}
	void Insert(Filter *nextFilter);	// insert filter after this one

private:
	void operator=(const Filter &); // assignment not allowed

	member_ptr<BufferedTransformation> m_outQueue;
};

//! measure how many byte and messages pass through, also serves as valve
class MeterFilter : public Filter
{
public:
	MeterFilter(BufferedTransformation *outQ=NULL, bool transparent=true)
		: Filter(outQ), m_transparent(transparent) {ResetMeter();}

	void SetTransparent(bool transparent) {m_transparent = transparent;}
	void ResetMeter() {m_currentMessageBytes = m_totalBytes = m_currentSeriesMessages = m_totalMessages = m_totalMessageSeries = 0;}

	unsigned long GetCurrentMessageBytes() const {return m_currentMessageBytes;}
	unsigned long GetTotalBytes() {return m_totalBytes;}
	unsigned int GetCurrentSeriesMessages() {return m_currentSeriesMessages;}
	unsigned int GetTotalMessages() {return m_totalMessages;}
	unsigned int GetTotalMessageSeries() {return m_totalMessageSeries;}

	void Put(byte inByte);
	void Put(const byte *inString, unsigned int length);
	void MessageEnd(int propagation=-1);
	void MessageSeriesEnd(int propagation=-1);

private:
	bool m_transparent;
	unsigned long m_currentMessageBytes, m_totalBytes;
	unsigned int m_currentSeriesMessages, m_totalMessages, m_totalMessageSeries;
};

//! .
class TransparentFilter : public MeterFilter
{
public:
	TransparentFilter(BufferedTransformation *outQ=NULL) : MeterFilter(outQ, true) {}
};

//! .
class OpaqueFilter : public MeterFilter
{
public:
	OpaqueFilter(BufferedTransformation *outQ=NULL) : MeterFilter(outQ, false) {}
};

/*! FilterWithBufferedInput divides up the input stream into
	a first block, a number of middle blocks, and a last block.
	First and last blocks are optional, and middle blocks may
	be a stream instead (i.e. blockSize == 1).
*/
class FilterWithBufferedInput : public Filter
{
public:
	/// firstSize and lastSize may be 0, blockSize must be at least 1
	FilterWithBufferedInput(unsigned int firstSize, unsigned int blockSize, unsigned int lastSize, BufferedTransformation *outQ);
	void Put(byte inByte);
	void Put(const byte *inString, unsigned int length);
	void MessageEnd(int propagation=-1);

	/*! the input buffer may contain more than blockSize bytes if lastSize != 0
		ForceNextPut() forces a call to NextPut() if this is the case 
	*/
	void ForceNextPut();

protected:
	bool DidFirstPut() {return m_firstInputDone;}

	// FirstPut() is called if (firstSize != 0 and totalLength >= firstSize)
	// or (firstSize == 0 and (totalLength > 0 or a MessageEnd() is received))
	virtual void FirstPut(const byte *inString) =0;
	// NextPut() is called if totalLength >= firstSize+blockSize+lastSize
	// length parameter is always blockSize unless blockSize == 1
	virtual void NextPut(const byte *inString, unsigned int length) =0;
	// LastPut() is always called
	// if totalLength < firstSize then length == totalLength
	// else if totalLength <= firstSize+lastSize then length == totalLength-firstSize
	// else lastSize <= length < lastSize+blockSize
	virtual void LastPut(const byte *inString, unsigned int length) =0;

private:
	class BlockQueue
	{
	public:
		BlockQueue(unsigned int blockSize, unsigned int maxBlocks);
		void ResetQueue(unsigned int blockSize, unsigned int maxBlocks);
		const byte *GetBlock();
		const byte *GetContigousBlocks(unsigned int &numberOfBlocks);
		unsigned int GetAll(byte *outString);
		void Put(const byte *inString, unsigned int length);
		unsigned int CurrentSize() const {return m_size;}
		unsigned int MaxSize() const {return m_buffer.size;}

	private:
		SecByteBlock m_buffer;
		unsigned int m_blockSize, m_maxBlocks, m_size;
		byte *m_begin;
	};

	unsigned int m_firstSize, m_blockSize, m_lastSize;
	bool m_firstInputDone;
	BlockQueue m_queue;
};

//! .
class FilterWithInputQueue : public Filter
{
public:
	FilterWithInputQueue(BufferedTransformation *attachment) : Filter(attachment) {}
	void Put(byte inByte) {m_inQueue.Put(inByte);}
	void Put(const byte *inString, unsigned int length) {m_inQueue.Put(inString, length);}

protected:
	ByteQueue m_inQueue;
};

//! Filter Wrapper for StreamCipher
class StreamCipherFilter : public Filter
{
public:
	StreamCipherFilter(StreamCipher &c,
					   BufferedTransformation *outQueue = NULL)
		: Filter(outQueue), cipher(c) {}

	void Put(byte inByte)
		{AttachedTransformation()->Put(cipher.ProcessByte(inByte));}

	void Put(const byte *inString, unsigned int length);

private:
	StreamCipher &cipher;
};

//! Filter Wrapper for HashModule
class HashFilter : public Filter
{
public:
	HashFilter(HashModule &hm, BufferedTransformation *outQueue = NULL, bool putMessage=false)
		: Filter(outQueue), m_hashModule(hm), m_putMessage(putMessage) {}

	void MessageEnd(int propagation=-1);

	void Put(byte inByte);
	void Put(const byte *inString, unsigned int length);

private:
	HashModule &m_hashModule;
	bool m_putMessage;
};

//! Filter Wrapper for HashModule
class HashVerifier : public FilterWithBufferedInput
{
public:
	class HashVerificationFailed : public BufferedTransformation::Err
	{
	public:
		HashVerificationFailed()
			: BufferedTransformation::Err(DATA_INTEGRITY_CHECK_FAILED, "HashVerifier: message hash not correct") {}
	};

	enum Flags {HASH_AT_BEGIN=1, PUT_MESSAGE=2, PUT_HASH=4, PUT_RESULT=8, THROW_EXCEPTION=16};
	HashVerifier(HashModule &hm, BufferedTransformation *outQueue = NULL, word32 flags = HASH_AT_BEGIN | PUT_RESULT);

	bool GetLastResult() const {return m_verified;}

protected:
	void FirstPut(const byte *inString);
	void NextPut(const byte *inString, unsigned int length);
	void LastPut(const byte *inString, unsigned int length);

private:
	HashModule &m_hashModule;
	word32 m_flags;
	SecByteBlock m_expectedHash;
	bool m_verified;
};

//! Filter Wrapper for PK_Signer
class SignerFilter : public Filter
{
public:
	SignerFilter(RandomNumberGenerator &rng, const PK_Signer &signer, BufferedTransformation *outQueue = NULL)
		: Filter(outQueue), m_rng(rng), m_signer(signer), m_messageAccumulator(signer.NewMessageAccumulator()) {}

	void MessageEnd(int propagation);

	void Put(byte inByte)
		{m_messageAccumulator->Update(&inByte, 1);}

	void Put(const byte *inString, unsigned int length)
		{m_messageAccumulator->Update(inString, length);}

private:
	RandomNumberGenerator &m_rng;
	const PK_Signer &m_signer;
	member_ptr<HashModule> m_messageAccumulator;
};

//! Filter Wrapper for PK_Verifier
class VerifierFilter : public Filter
{
public:
	VerifierFilter(const PK_Verifier &verifier, BufferedTransformation *outQueue = NULL)
		: Filter(outQueue), m_verifier(verifier), m_messageAccumulator(verifier.NewMessageAccumulator())
		, m_signature(verifier.SignatureLength()) {}

	// this function must be called before MessageEnd()
	void PutSignature(const byte *sig);

	void MessageEnd(int propagation);

	void Put(byte inByte)
		{m_messageAccumulator->Update(&inByte, 1);}

	void Put(const byte *inString, unsigned int length)
		{m_messageAccumulator->Update(inString, length);}

private:
	const PK_Verifier &m_verifier;
	member_ptr<HashModule> m_messageAccumulator;
	SecByteBlock m_signature;
};

//! A BufferedTransformation that doesn't produce any retrievable output
class Sink : public BufferedTransformation
{
};

//! .
class BitBucket : public Sink
{
public:
	void Put(byte) {}
	void Put(const byte *, unsigned int) {}
};

extern BitBucket g_bitBucket;

//! Redirect input to another BufferedTransformation without owning it
class Redirector : public Sink
{
public:
	Redirector() : m_target(NULL), m_passSignal(true) {}
	Redirector(BufferedTransformation &target, bool passSignal=true) : m_target(&target), m_passSignal(passSignal) {}

	void Redirect(BufferedTransformation &target) {m_target = &target;}
	void StopRedirect() {m_target = NULL;}
	bool GetPassSignal() const {return m_passSignal;}
	void SetPassSignal(bool passSignal) {m_passSignal = passSignal;}

	void Put(byte b) 
		{if (m_target) m_target->Put(b);}
	void Put(const byte *string, unsigned int len) 
		{if (m_target) m_target->Put(string, len);}
	void Flush(bool completeFlush, int propagation=-1) 
		{if (m_target && m_passSignal) m_target->Flush(completeFlush, propagation);}
	void MessageEnd(int propagation=-1)
		{if (m_target && m_passSignal) m_target->MessageEnd(propagation);}
	void MessageSeriesEnd(int propagation=-1) 
		{if (m_target && m_passSignal) m_target->MessageSeriesEnd(propagation);}

	void ChannelPut(const std::string &channel, byte b) 
		{if (m_target) m_target->ChannelPut(channel, b);}
	void ChannelPut(const std::string &channel, const byte *string, unsigned int len) 
		{if (m_target) m_target->ChannelPut(channel, string, len);}
	void ChannelFlush(const std::string &channel, bool completeFlush, int propagation=-1) 
		{if (m_target && m_passSignal) m_target->ChannelFlush(channel, completeFlush, propagation);}
	void ChannelMessageEnd(const std::string &channel, int propagation=-1)
		{if (m_target && m_passSignal) m_target->ChannelMessageEnd(channel, propagation);}
	void ChannelMessageSeriesEnd(const std::string &channel, int propagation=-1) 
		{if (m_target && m_passSignal) m_target->ChannelMessageSeriesEnd(channel, propagation);}

private:
	BufferedTransformation *m_target;
	bool m_passSignal;
};

// Used By ProxyFilter
class OutputProxy : public Sink
{
public:
	OutputProxy(BufferedTransformation &owner, bool passSignal) : m_owner(owner), m_passSignal(passSignal) {}

	bool GetPassSignal() const {return m_passSignal;}
	void SetPassSignal(bool passSignal) {m_passSignal = passSignal;}

	void Put(byte b) 
		{m_owner.AttachedTransformation()->Put(b);}
	void Put(const byte *string, unsigned int len) 
		{m_owner.AttachedTransformation()->Put(string, len);}
	void Flush(bool completeFlush, int propagation=-1) 
		{if (m_passSignal) m_owner.AttachedTransformation()->Flush(completeFlush, propagation);}
	void MessageEnd(int propagation=-1)
		{if (m_passSignal) m_owner.AttachedTransformation()->MessageEnd(propagation);}
	void MessageSeriesEnd(int propagation=-1) 
		{if (m_passSignal) m_owner.AttachedTransformation()->MessageSeriesEnd(propagation);}

	void ChannelPut(const std::string &channel, byte b) 
		{m_owner.AttachedTransformation()->ChannelPut(channel, b);}
	void ChannelPut(const std::string &channel, const byte *string, unsigned int len) 
		{m_owner.AttachedTransformation()->ChannelPut(channel, string, len);}
	void ChannelFlush(const std::string &channel, bool completeFlush, int propagation=-1) 
		{if (m_passSignal) m_owner.AttachedTransformation()->ChannelFlush(channel, completeFlush, propagation);}
	void ChannelMessageEnd(const std::string &channel, int propagation=-1)
		{if (m_passSignal) m_owner.AttachedTransformation()->ChannelMessageEnd(channel, propagation);}
	void ChannelMessageSeriesEnd(const std::string &channel, int propagation=-1) 
		{if (m_passSignal) m_owner.AttachedTransformation()->ChannelMessageSeriesEnd(channel, propagation);}

private:
	BufferedTransformation &m_owner;
	bool m_passSignal;
};

//! Base class for Filter classes that are proxies for a chain of other filters.
class ProxyFilter : public FilterWithBufferedInput
{
public:
	ProxyFilter(Filter *filter, unsigned int firstSize, unsigned int lastSize, BufferedTransformation *outQ);

	void Flush(bool completeFlush, int propagation=-1);

	void SetFilter(Filter *filter);
	void NextPut(const byte *s, unsigned int len);

protected:
	member_ptr<Filter> m_filter;
	OutputProxy *m_proxy;
};

//! Append input to a string object
template <class T>
class StringSinkTemplate : public Sink
{
public:
	// VC60 workaround: no T::char_type
	typedef typename T::traits_type::char_type char_type;

	StringSinkTemplate(T &output)
		: m_output(output) {assert(sizeof(output[0])==1);}
	void Put(byte b)
		{m_output += (char_type)b;}
	void Put(const byte *str, unsigned int bc)
		{m_output.append((const char_type *)str, bc);}

private:	
	T &m_output;
};

//! Append input to an std::string
typedef StringSinkTemplate<std::string> StringSink;

//! Copy input to a memory buffer
class ArraySink : public Sink
{
public:
	ArraySink(byte *buf, unsigned int size) : m_buf(buf), m_size(size), m_total(0) {}

	unsigned int AvailableSize() {return m_size - STDMIN(m_total, (unsigned long)m_size);}
	unsigned long TotalPutLength() {return m_total;}

	void Put(byte b)
	{
		if (m_total < m_size)
			m_buf[m_total] = b;
		m_total++;
	}

	void Put(const byte *str, unsigned int len)
	{
		if (m_total < m_size)
			memcpy(m_buf+m_total, str, STDMIN(len, (unsigned int)(m_size-m_total)));
		m_total += len;
	}

protected:
	byte *m_buf;
	unsigned int m_size;
	unsigned long m_total;
};

//! Xor input to a memory buffer
class ArrayXorSink : public ArraySink
{
public:
	ArrayXorSink(byte *buf, unsigned int size)
		: ArraySink(buf, size) {}

	void Put(byte b)
	{
		if (m_total < m_size)
			m_buf[m_total] ^= b;
		m_total++;
	}

	void Put(const byte *str, unsigned int len)
	{
		if (m_total < m_size)
			xorbuf(m_buf+m_total, str, STDMIN(len, (unsigned int)(m_size-m_total)));
		m_total += len;
	}
};

//! Provide implementation of SetAutoSignalPropagation and GetAutoSignalPropagation
class BufferedTransformationWithAutoSignal : virtual public BufferedTransformation
{
public:
	BufferedTransformationWithAutoSignal(int propagation=-1) : m_autoSignalPropagation(propagation) {}

	void SetAutoSignalPropagation(int propagation)
		{m_autoSignalPropagation = propagation;}
	int GetAutoSignalPropagation() const
		{return m_autoSignalPropagation;}

private:
	int m_autoSignalPropagation;
};

//! A BufferedTransformation that only contains pre-existing output
class Store : public BufferedTransformationWithAutoSignal
{
public:
	Store() : m_messageEnd(false) {}

	void Put(byte)
		{}
	void Put(const byte *, unsigned int length)
		{}

	virtual unsigned long TransferTo(BufferedTransformation &target, unsigned long transferMax=ULONG_MAX) =0;
	virtual unsigned long CopyTo(BufferedTransformation &target, unsigned long copyMax=ULONG_MAX) const =0;

	unsigned int NumberOfMessages() const {return m_messageEnd ? 0 : 1;}
	bool GetNextMessage();
	unsigned int CopyMessagesTo(BufferedTransformation &target, unsigned int count=UINT_MAX) const;

private:
	bool m_messageEnd;
};

//! .
class StringStore : public Store
{
public:
	StringStore(const char *string)
		: m_store((const byte *)string), m_length(strlen(string)), m_count(0) {}
	StringStore(const byte *string, unsigned int length)
		: m_store(string), m_length(length), m_count(0) {}
	template <class T> StringStore(const T &string)
		: m_store((const byte *)string.data()), m_length(string.length()), m_count(0) {assert(sizeof(string[0])==1);}

	unsigned long TransferTo(BufferedTransformation &target, unsigned long transferMax=ULONG_MAX);
	unsigned long CopyTo(BufferedTransformation &target, unsigned long copyMax=ULONG_MAX) const;

private:
	const byte *m_store;
	unsigned int m_length, m_count;
};

//! .
class RandomNumberStore : public Store
{
public:
	RandomNumberStore(RandomNumberGenerator &rng, unsigned long length)
		: m_rng(rng), m_length(length), m_count(0) {}

	unsigned long TransferTo(BufferedTransformation &target, unsigned long transferMax=ULONG_MAX);
	unsigned long CopyTo(BufferedTransformation &target, unsigned long copyMax=ULONG_MAX) const;

private:
	RandomNumberGenerator &m_rng;
	unsigned long m_length, m_count;
};

//! .
class NullStore : public Store
{
public:
	unsigned long MaxRetrievable() const {return ULONG_MAX;}
	unsigned long TransferTo(BufferedTransformation &target, unsigned long transferMax=ULONG_MAX);
	unsigned long CopyTo(BufferedTransformation &target, unsigned long copyMax=ULONG_MAX) const;
};

//! A Filter that pumps data into its attachment as input
class Source : public Filter
{
public:
	Source(BufferedTransformation *outQ)
		: Filter(outQ) {}

	virtual unsigned long Pump(unsigned long pumpMax=ULONG_MAX) =0;
	virtual unsigned int PumpMessages(unsigned int count=UINT_MAX) {return 0;}
	void PumpAll();

	void Put(byte)
		{Pump(1);}
	void Put(const byte *, unsigned int length)
		{Pump(length);}
	void MessageEnd(int propagation=-1)
		{PumpAll();}
};

//! Turn a Store into a Source
class GeneralSource : public Source
{
public:
	GeneralSource(BufferedTransformation &store, bool pumpAll, BufferedTransformation *outQueue = NULL)
		: Source(outQueue), m_store(store)
	{
		if (pumpAll) PumpAll();
	}

	unsigned long Pump(unsigned long pumpMax=ULONG_MAX)
		{return m_store.TransferTo(*AttachedTransformation(), pumpMax);}
	unsigned int PumpMessages(unsigned int count=UINT_MAX)
		{return m_store.TransferMessagesTo(*AttachedTransformation(), count);}

private:
	BufferedTransformation &m_store;
};

//! .
class StringSource : public Source
{
public:
	StringSource(const char *string, bool pumpAll, BufferedTransformation *outQueue = NULL);
	StringSource(const byte *string, unsigned int length, bool pumpAll, BufferedTransformation *outQueue = NULL);

#ifdef __MWERKS__	// CW60 workaround
	StringSource(const std::string &string, bool pumpAll, BufferedTransformation *outQueue = NULL)
#else
	template <class T> StringSource(const T &string, bool pumpAll, BufferedTransformation *outQueue = NULL)
#endif
		: Source(outQueue), m_store(string)
	{
		if (pumpAll)
			PumpAll();
	}

	unsigned long Pump(unsigned long pumpMax=ULONG_MAX)
		{return m_store.TransferTo(*AttachedTransformation(), pumpMax);}
	unsigned int PumpMessages(unsigned int count=UINT_MAX)
		{return m_store.TransferMessagesTo(*AttachedTransformation(), count);}

private:
	StringStore m_store;
};

//! .
class RandomNumberSource : public Source
{
public:
	RandomNumberSource(RandomNumberGenerator &rng, unsigned int length, bool pumpAll, BufferedTransformation *outQueue = NULL);

	unsigned long Pump(unsigned long pumpMax=ULONG_MAX)
		{return m_store.TransferTo(*AttachedTransformation(), pumpMax);}
	unsigned int PumpMessages(unsigned int count=UINT_MAX)
		{return m_store.TransferMessagesTo(*AttachedTransformation(), count);}

private:
	RandomNumberStore m_store;
};

NAMESPACE_END

#endif
