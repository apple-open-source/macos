/*
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 2011 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _IOFIXEDPOINT64_H
#define _IOFIXEDPOINT64_H

#include <IOKit/IOTypes.h>
#include <IOKit/graphics/IOGraphicsTypes.h>
#include <IOKit/hidsystem/IOHIDTypes.h>
#include "IOFixed64.h"

//===========================================================================
class IOFixedPoint64 
{
private:
    IOFixed64 x;
    IOFixed64 y;
    
public:    
    IOFixedPoint64() {
        // zeroed by IOFixed64
    }
    
    IOFixedPoint64(IOFixedPoint32 p) {
        x.fromFixed24x8(p.x); 
        y.fromFixed24x8(p.y); 
    }
    
    operator const IOFixedPoint32() {
        IOFixedPoint32 r;
        r.x = x.asFixed24x8();
        r.y = y.asFixed24x8();
        return r;
    }
    
    operator const IOGPoint() {
        IOGPoint r;
        r.x = x.as32();
        r.y = y.as32();
        return r;
    }
    
    operator const bool() {
        return (x || y);
    }
    
    IOFixedPoint64& fromIntFloor(SInt64 x_in, SInt64 y_in) {
        x.fromIntFloor(x_in);
        y.fromIntFloor(y_in);
        return *this;
    }
    
    IOFixedPoint64& fromIntCeiling(SInt64 x_in, SInt64 y_in) {
        x.fromIntCeiling(x_in);
        y.fromIntCeiling(y_in);
        return *this;
    }
    
    IOFixedPoint64& fromFixed(IOFixed x_in, IOFixed y_in) {
        x.fromFixed(x_in);
        y.fromFixed(y_in);
        return *this;
    }
    
    IOFixedPoint64& fromFixed64(IOFixed64 x_in, IOFixed64 y_in) {
        x = x_in;
        y = y_in;
        return *this;
    }
    
    IOFixedPoint64& fromFixed24x8(int32_t x_in, int32_t y_in) {
        x.fromFixed24x8(x_in);
        y.fromFixed24x8(y_in);
        return *this;
    }
    
    bool inRect(volatile IOGBounds &rect) {
        return (x >= (SInt64)rect.minx) && (x < (SInt64)rect.maxx) && (y >= (SInt64)rect.miny) && (y < (SInt64)rect.maxy);
    }
    
    void clipToRect(volatile IOGBounds &rect) {
        IOFixed64 minx;
        minx.fromIntFloor(rect.minx);
        IOFixed64 maxx;
        maxx.fromIntCeiling(rect.maxx - 1);
        
        if (x < minx)
            x = minx;
        else if (x > maxx)
            x = maxx;
        
        IOFixed64 miny;
        miny.fromIntFloor(rect.miny);
        IOFixed64 maxy;
        maxy.fromIntCeiling(rect.maxy - 1);

        if (y < miny)
            y = miny;
        else if (y > maxy)
            y = maxy;
    }
    
    IOFixed64& xValue() {
        return x;
    }
    
    IOFixed64& yValue() {
        return y;
    }
    
#define ARITHMETIC_OPERATOR(OP) \
IOFixedPoint64& operator OP(IOFixedPoint64 p) { \
    x OP p.x; \
    y OP p.y; \
    return *this; \
} \
IOFixedPoint64& operator OP(IOFixed64 s) { \
    x OP s; \
    y OP s; \
    return *this; \
} \
IOFixedPoint64& operator OP(SInt64 s) { \
    IOFixed64 s_fixed; \
    return (*this OP s_fixed.fromIntFloor(s)); \
}
    
    ARITHMETIC_OPERATOR(+=)
    ARITHMETIC_OPERATOR(-=)
    ARITHMETIC_OPERATOR(*=)
    ARITHMETIC_OPERATOR(/=)
    
#undef ARITHMETIC_OPERATOR
    
#define BOOL_OPERATOR(OP) \
bool operator OP (const IOFixedPoint64 p) const { return (x OP p.x) && (y OP p.y); }
    
    BOOL_OPERATOR(>)
    BOOL_OPERATOR(>=)
    BOOL_OPERATOR(<)
    BOOL_OPERATOR(<=)
    BOOL_OPERATOR(==)
    BOOL_OPERATOR(!=)
    
#undef BOOL_OPERATOR
    
};

//===========================================================================
IOFixedPoint64 operator* (const IOFixedPoint64 a, const IOFixedPoint64 b);
IOFixedPoint64 operator* (const IOFixedPoint64 a, const IOFixed64 b);
IOFixedPoint64 operator* (const IOFixedPoint64 a, const SInt64 b);
IOFixedPoint64 operator/ (const IOFixedPoint64 a, const IOFixedPoint64 b);
IOFixedPoint64 operator/ (const IOFixedPoint64 a, const IOFixed64 b);
IOFixedPoint64 operator/ (const IOFixedPoint64 a, const SInt64 b);
IOFixedPoint64 operator+ (const IOFixedPoint64 a, const IOFixedPoint64 b);
IOFixedPoint64 operator- (const IOFixedPoint64 a, const IOFixedPoint64 b);

//===========================================================================
#endif // _IOFIXEDPOINT64_H
