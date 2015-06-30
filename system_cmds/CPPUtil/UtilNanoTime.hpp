//
//  UtilNanoTime.hpp
//  CPPUtil
//
//  Created by James McIlree on 4/14/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef __CPPUtil__UtilNanoTime__
#define __CPPUtil__UtilNanoTime__

class NanoTime {
    protected:
	uint64_t	_time;

    public:
	NanoTime() : _time(0ULL) {}
	NanoTime(uint64_t t) : _time(t) {}

	bool operator==(const NanoTime& rhs) const		{ return this->_time == rhs._time; }
	bool operator!=(const NanoTime &rhs) const 		{ return !(*this == rhs); }

	bool operator<(const NanoTime& rhs) const		{ return this->_time < rhs._time; }
	bool operator<=(const NanoTime& rhs) const		{ return this->_time <= rhs._time; }
	bool operator>(const NanoTime& rhs) const		{ return this->_time > rhs._time; }
	bool operator>=(const NanoTime& rhs) const		{ return this->_time >= rhs._time; }

	// We do not want to be able to mutate NanoTime(s)
	// without type enforcement, but it is useful to be able
	// to say "if (time == 0) {}", so we have value based
	// operators for comparison
	bool operator==(uint64_t value) const			{ return this->_time == value; }
	bool operator!=(uint64_t value) const	 		{ return !(*this == value); }

	bool operator<(uint64_t value) const			{ return this->_time < value; }
	bool operator<=(uint64_t value) const			{ return this->_time <= value; }
	bool operator>(uint64_t value) const			{ return this->_time > value; }
	bool operator>=(uint64_t value) const			{ return this->_time >= value; }

	NanoTime operator+(const NanoTime& rhs) const		{ return NanoTime(_time + rhs._time); }
	NanoTime operator-(const NanoTime& rhs) const		{ return NanoTime(_time - rhs._time); }
	NanoTime operator*(const NanoTime& rhs) const		{ return NanoTime(_time * rhs._time); }
	NanoTime operator/(const NanoTime& rhs) const		{ return NanoTime(_time / rhs._time); }

	NanoTime& operator+=(const NanoTime& rhs)		{ _time += rhs._time; return *this; }

	AbsTime abs_time() const; // NOTE! Uses system mach_timebase_info, potentially expensive conversion costs.
	AbsTime abs_time(mach_timebase_info_data_t timebase_info) const {
		return AbsTime(_time * timebase_info.denom / timebase_info.numer);
	}

	uint64_t value() const					{ return _time; }
	double double_value() const				{ return (double)_time; }
};


#endif /* defined(__CPPUtil__UtilNanoTime__) */
