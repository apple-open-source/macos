#pragma once

template <class T>
class StRawPointer {
public:
	StRawPointer (
		T*		inPointer):
		mPointer (inPointer) {}
	
	T& operator * () const { return *mPointer; }
private:
	T*		mPointer;
};

template <>
class StRawPointer  <void> {
public:
	StRawPointer (
		void*	inPointer):
		mPointer (inPointer) {}
	
	void* operator * () const { return mPointer; }
private:
	void*		mPointer;
};

template <>
class StRawPointer  <const void> {
public:
	StRawPointer (
		const void*	inPointer):
		mPointer (inPointer) {}
	
	const void* operator * () const { return mPointer; }
private:
	const void*		mPointer;
};

typedef	StRawPointer <KClientSession>	StRawKClientSession;
typedef	StRawPointer <KClientPrincipal>	StRawKClientPrincipal;
typedef	StRawPointer <KClientAddress>	StRawKClientAddress;
typedef	StRawPointer <KClientKey>		StRawKClientKey;
typedef	StRawPointer <UInt32>			StRawUInt32;
typedef	StRawPointer <void>				StRawBuffer;
typedef	StRawPointer <const void>		StRawConstBuffer;
