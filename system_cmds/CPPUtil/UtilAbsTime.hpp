//
//  UtilAbsTime.hpp
//  CPPUtil
//
//  Created by James McIlree on 4/14/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef __CPPUtil__UtilAbsTime__
#define __CPPUtil__UtilAbsTime__

class NanoTime;

class AbsTime {
    protected:
	uint64_t _time;

    public:
	// Minimum and Maximum possible values
	static const AbsTime BEGINNING_OF_TIME;
	static const AbsTime END_OF_TIME;

	static AbsTime now();

	AbsTime() : _time(0ULL) {}
	explicit AbsTime(uint64_t t) : _time(t) {}

	bool operator==(const AbsTime& rhs) const		{ return this->_time == rhs._time; }
	bool operator!=(const AbsTime &rhs) const 		{ return !(*this == rhs); }

	bool operator<(const AbsTime& rhs) const		{ return this->_time < rhs._time; }
	bool operator<=(const AbsTime& rhs) const		{ return this->_time <= rhs._time; }
	bool operator>(const AbsTime& rhs) const		{ return this->_time > rhs._time; }
	bool operator>=(const AbsTime& rhs) const		{ return this->_time >= rhs._time; }

	// We do not want to be able to mutate AbsTime(s)
	// without type enforcement, but it is useful to be able
	// to say "if (time == 0) {}", so we have value based
	// operators for comparison
	bool operator==(uint64_t value) const			{ return this->_time == value; }
	bool operator!=(uint64_t value) const	 		{ return !(*this == value); }

	bool operator<(uint64_t value) const			{ return this->_time < value; }
	bool operator<=(uint64_t value) const			{ return this->_time <= value; }
	bool operator>(uint64_t value) const			{ return this->_time > value; }
	bool operator>=(uint64_t value) const			{ return this->_time >= value; }

	AbsTime operator+(const AbsTime& rhs) const		{ return AbsTime(_time + rhs._time); }
	AbsTime operator-(const AbsTime& rhs) const		{ return AbsTime(_time - rhs._time); }
	AbsTime operator*(const AbsTime& rhs) const		{ return AbsTime(_time * rhs._time); }
	AbsTime operator/(const AbsTime& rhs) const		{ return AbsTime(_time / rhs._time); }

	AbsTime& operator+=(const AbsTime& rhs)			{ _time += rhs._time; return *this; }

	NanoTime nano_time() const; // NOTE! Uses system mach_timebase_info, potentially expensive conversion costs.
	NanoTime nano_time(mach_timebase_info_data_t timebase_info) const;

	uint64_t value() const					{ return _time; }
	double double_value() const				{ return (double)_time; }
};

#endif /* defined(__CPPUtil__UtilAbsTime__) */
