//
//  UtilMemoryBuffer.h
//  CPPUtil
//
//  Created by James McIlree on 4/20/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef __CPPUtil__UtilMemoryBuffer__
#define __CPPUtil__UtilMemoryBuffer__

template <typename T>
class MemoryBuffer {
    protected:
	T*			_data;
	size_t			_capacity;

	// No copying?
	MemoryBuffer(const MemoryBuffer& that) = delete;
	MemoryBuffer& operator=(const MemoryBuffer& other) = delete;

    public:
	// Capacity is in units of T!
	//
	// MemoryBuffer<char>(1); // 1 byte
	// MemoryBuffer<uint32_t>(1); // 4 bytes
	MemoryBuffer()		{ _data = NULL; _capacity = 0; }
	MemoryBuffer(size_t capacity);
	MemoryBuffer(MemoryBuffer&& rhs) noexcept :
		_data(rhs._data),
		_capacity(rhs._capacity)
	{
		rhs._data = NULL;
		rhs._capacity = 0;
	}

	~MemoryBuffer()					{ if (_data) { free(_data); } }

	MemoryBuffer& operator=(MemoryBuffer&& rhs)	{ std::swap(_data, rhs._data); std::swap(_capacity, rhs._capacity); return *this; }

	T* data()					{ return _data; }
	size_t capacity() const				{ return _capacity; }
	size_t capacity_in_bytes() const		{ return _capacity * sizeof(T); }
	// This always results in an allocation and copy.
	// If the new capacity is smaller, data is truncated.
	void set_capacity(size_t capacity);
};

template <typename T>
MemoryBuffer<T>::MemoryBuffer(size_t capacity) :
	_capacity(capacity)
{
	_data = capacity ? (T*)malloc(capacity * sizeof(T)) : (T*)NULL;
}

template <typename T>
void MemoryBuffer<T>::set_capacity(size_t capacity) {
	MemoryBuffer<T> newbuf(capacity);
	memcpy(newbuf.data(), _data, std::min(_capacity * sizeof(T), newbuf.capacity() * sizeof(T)));
	*this = std::move(newbuf);
}

#endif /* defined(__CPPUtil__UtilMemoryBuffer__) */
