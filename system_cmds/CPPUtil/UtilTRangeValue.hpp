//
//  UtilTRangeValue.hpp
//  CPPUtil
//
//  Created by James McIlree on 12/10/07.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef CPPUtil_UtilTRangeValue_hpp
#define CPPUtil_UtilTRangeValue_hpp

template <typename T1, typename T2>
class TRangeValue : public TRange<T1> {
    protected:
	T2 _value;
    
    public:
	TRangeValue(T1 location, T1 length, T2 value) : TRange<T1>(location, length), _value(value) { };
    
	const T2 value() const	{ return _value; }

	// Sometimes we need to reference the value as a mutable reference (think std::vector types)
	T2& mutable_value()	{ return _value; }
};

#endif
