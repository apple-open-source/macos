//
//  IOHIDPointingAccelerationAlgorythm.cpp
//  IOHIDFamily
//
//  Created by local on 9/24/15.
//
//

#include "IOHIDAcceleration.hpp"
#include "CF.h"
#include "IOHIDParameter.h"
#include "IOHIDDebug.h"
#include <sstream>
#include <cmath>



void IOHIDSimpleAccelerator::serialize(CFMutableDictionaryRef dict) const {
    CFMutableDictionaryRefWrap serializer (dict);
    serializer.SetValueForKey(CFSTR("Class"), CFSTR("IOHIDScrollAccelerator"));
    serializer.SetValueForKey(CFSTR("Multiplier"), DOUBLE_TO_FIXED(_multiplier));
}

bool IOHIDSimpleAccelerator::accelerate (double *values, size_t length,  uint64_t timestamp __unused) {
    if (_multiplier == 1.0) {
        return false;
    }
    for (size_t i = 0; i < length; i++) {
      values[i] *= _multiplier;
    }
    return true;
}

mach_timebase_info_data_t    IOHIDScrollAccelerator::_timebase {0,0};

bool IOHIDScrollAccelerator::accelerate (double *values, size_t length __unused,  uint64_t timestamp) {
 
  double mult = 1.0;
  double &scroll = *values;
  
  double deltaT = ((double)(timestamp - _lastTimeStamp) * _timebase.numer/ ((double)_timebase.denom)) / 1000000;
  
  _lastTimeStamp = timestamp;
  
  int head = _head;
  
  events[head].deltaTime = deltaT;
  events[head].scroll   = std::abs(scroll);
  
  _head = (++_head) % SCROLL_EVENT_AVARAGE_LENGHT;
  
  if (_head == _tail) {
    _tail = (++_tail) % SCROLL_EVENT_AVARAGE_LENGHT;
  }
  
  bool direction = scroll > 0 ? true : false;
  if (_direction != direction || deltaT > SCROLL_CLEAR_THRESHOLD_MS) {
    _tail = head;
    _direction = direction;
  }
  
  double    sumDeltaT   = 0;
  double    sumScroll   = 0;
  int       eventCount  = 0;
  
  int i = _head;
  do {
    i = (i ? i : SCROLL_EVENT_AVARAGE_LENGHT) - 1;
    sumScroll += events[i].scroll;
    ++eventCount;
    if (events[i].deltaTime > SCROLL_EVENT_THRESHOLD_MS) {
      sumDeltaT +=SCROLL_EVENT_THRESHOLD_MS;
      break;
    } else {
      sumDeltaT += events[i].deltaTime;
    }
    if (sumDeltaT >= SCROLL_CLEAR_THRESHOLD_MS) {
      break;
    }
  } while (i != _tail);
  
  double avargeDeltaTime;
  double avargeScroll;
  double velocity;
  
  double rateMultiplier = _rate / FRAME_RATE;
  avargeDeltaTime = (sumDeltaT/eventCount) * rateMultiplier;
  if (avargeDeltaTime > SCROLL_EVENT_THRESHOLD_MS) {
    avargeDeltaTime = SCROLL_EVENT_THRESHOLD_MS;
  } else if (avargeDeltaTime < 1) {
    avargeDeltaTime = 1;
  }
  
  avargeScroll = (double)sumScroll/eventCount;
  
  velocity =  (SCROLL_MULTIPLIER_A * avargeDeltaTime * avargeDeltaTime - SCROLL_MULTIPLIER_B * avargeDeltaTime + SCROLL_MULTIPLIER_C) * avargeScroll * rateMultiplier ;
  
  
  if (velocity < FIXED_TO_DOUBLE(0x1)) {
    velocity = FIXED_TO_DOUBLE(0x1);
  }
  
  if (_algorithm->getType() == IOHIDAccelerationAlgorithm::Table) {
    if  (eventCount > 2) {
      velocity *= sqrt(eventCount * 16);
      velocity /= 4;
    }
    mult = _algorithm->multiplier(velocity) / std::abs(scroll) ;
  } else {
    mult = _algorithm->multiplier(velocity) / velocity ;
  }
  
//  HIDLogDebug("NHIDSCROLL: ACC: mult:%9.4f velocity:%9.4f  avg_time:%9.4f  avg_axis:%9.4f  avg_count:%2d",
//             mult, velocity, avargeDeltaTime, avargeScroll, eventCount
//            );

  double scrollAccel = scroll * mult * SCROLL_PIXEL_TO_WHEEL_SCALE;
  scroll = scrollAccel;
  return (mult != 1);
}

void IOHIDScrollAccelerator::serialize(CFMutableDictionaryRef dict) const {
  CFMutableDictionaryRefWrap serializer (dict);
  serializer.SetValueForKey(CFSTR("Class"), CFSTR("IOHIDScrollAccelerator"));
  serializer.SetValueForKey(CFSTR("Resolution"), DOUBLE_TO_FIXED(_resolution));
  serializer.SetValueForKey(CFSTR("Rate"), DOUBLE_TO_FIXED(_rate));
  CFMutableDictionaryRefWrap accelerator;
  if (_algorithm) {
    _algorithm->serialize(accelerator);
  }
  serializer.SetValueForKey(CFSTR("Accelerator"), accelerator);
}


mach_timebase_info_data_t    IOHIDPointerAccelerator::_timebase {0,0};


bool IOHIDPointerAccelerator::accelerate (double *values, size_t length __unused,  uint64_t timestamp ) {
  double mult = 0.0;
  double &dx = values[0];
  double &dy = values[1];

  double rateMultiplier = 1;

  if (_rate != 0)
  {
    double deltaT = ((double)(timestamp - _lastTimeStamp) * _timebase.numer/ ((double)_timebase.denom)) / 1000000;
    if (deltaT != 0) {
      double period_ms = 1000/_rate;
      if (deltaT < period_ms)
        deltaT = period_ms;
      
      rateMultiplier = period_ms / deltaT;
    }
  }
  
  _lastTimeStamp = timestamp;

  double  velocity = rateMultiplier * floor(sqrt(pow(dx,2) + pow(dy,2)));
  mult = _algorithm->multiplier(velocity) / velocity;

//  HIDLogDebug("IOHIDPointerAccelerator: mult %x\n", mult);

  dx *= mult;
  dy *= mult;

  return (mult != 1);
}

void IOHIDPointerAccelerator::serialize(CFMutableDictionaryRef dict) const {
  CFMutableDictionaryRefWrap serializer (dict);
  serializer.SetValueForKey(CFSTR("Class"), CFSTR("IOHIDPointerAccelerator"));
  serializer.SetValueForKey(CFSTR("Resolution"), DOUBLE_TO_FIXED(_resolution));
  serializer.SetValueForKey(CFSTR("Rate"), DOUBLE_TO_FIXED(_rate));
  CFMutableDictionaryRefWrap accelerator;
  if (_algorithm) {
    _algorithm->serialize(accelerator);
  }
  serializer.SetValueForKey(CFSTR("Accelerator"), accelerator);
}
