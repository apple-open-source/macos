//
//  PrintBuffer.hpp
//  system_cmds
//
//  Created by James McIlree on 5/7/14.
//
//

#ifndef __system_cmds__PrintBuffer__
#define __system_cmds__PrintBuffer__

//
// Okay, here is how snprintf works.
//
// char buf[2];
//
// snprintf(buf, 0, "a"); // Returns 1, buf is unchanged.
// snprintf(buf, 1, "a"); // Returns 1, buf = \0
// snprintf(buf, 2, "a"); // Returns 1, buf = 'a', \0
//
// So... For a buffer of size N, each print is valid if and only if
// it consumes N-1 bytes.
//

class PrintBuffer {
    protected:
	char*	_buffer;
	size_t	_buffer_size;
	size_t	_buffer_capacity;
	size_t	_flush_boundary;
	int	_flush_fd;

    public:
	PrintBuffer(size_t capacity, size_t flush_boundary, int flush_fd) :
		_buffer((char*)malloc(capacity)),
		_buffer_size(0),
		_buffer_capacity(capacity),
		_flush_boundary(flush_boundary),
		_flush_fd(flush_fd)
	{
		ASSERT(capacity > 0, "Sanity");
		ASSERT(_buffer, "Sanity");
		ASSERT(flush_boundary < capacity, "Sanity");
		ASSERT(flush_fd != 0, "Must be a valid fd");
	}

	~PrintBuffer() {
		flush();
		free(_buffer);
	}

	void set_capacity(size_t capacity) {
		ASSERT(_buffer_size == 0, "Attempt to reallocate buffer while it still contains data");

		if (_buffer) {
			free(_buffer);
		}

		_buffer = (char*)malloc(capacity);
		_buffer_size = 0;
		_buffer_capacity = capacity;
	}

	void flush() {
		if (_buffer_size) {
			write(_flush_fd, _buffer, _buffer_size);
			_buffer_size = 0;
		}
	}

	void printf(const char* format, ...)  __attribute__((format(printf, 2, 3))) {
	    repeat:
		size_t remaining_bytes = _buffer_capacity - _buffer_size;

		va_list list;
		va_start(list, format);
		int bytes_needed = vsnprintf(&_buffer[_buffer_size], remaining_bytes, format, list);
		va_end(list);

		// There are three levels of "end" detection.
		//
		// 1) If bytes_needed is >= capacity, we must flush, grow capacity, and repeat.
		// 2) If bytes_needed is >= remaining_bytes, we must flush, and repeat.
		// 3) If bytes_needed + _buffer_size comes within _flush_boundary bytes of the end, flush.
		//
		// NOTE snprintf behavior, we need bytes_needed+1 bytes
		// to actually fully output all string characters.
		//
		// NOTE for any repeat condition, we do not commit the bytes that were written to the buffer.
		//

		// Condition 2
		if (bytes_needed >= remaining_bytes) {
			flush();

			// Save a common path if test by checking this only inside Condition 2
			//
			// Condition 1
			if (bytes_needed >= _buffer_capacity) {
				set_capacity(bytes_needed+1);
			}

			goto repeat;
		}

		// Commit the snprintf
		_buffer_size += bytes_needed;

		// Condition 3
		if (remaining_bytes - bytes_needed <= _flush_boundary) {
			flush();
		}
	}
};

#endif /* defined(__system_cmds__PrintBuffer__) */
