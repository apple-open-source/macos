//
//  IOHIDPointingAccelerationAlgorythm.hpp
//  IOHIDFamily
//
//  Created by local on 9/24/15.
//
//

#ifndef IOHIDAcceleration_hpp
#define IOHIDAcceleration_hpp

#include <CoreFoundation/CoreFoundation.h>
#include <iostream>
#include <vector>
#include <mach/mach_time.h>
#include <set>
#include "IOHIDAccelerationAlgorithm.hpp"



#define SCROLL_EVENT_AVARAGE_LENGHT     9
#define SCROLL_CLEAR_THRESHOLD_MS       500.00
#define SCROLL_EVENT_THRESHOLD_MS       150.00
#define SCROLL_MULTIPLIER_A             FIXED_TO_DOUBLE(0x00000002)
#define SCROLL_MULTIPLIER_B             FIXED_TO_DOUBLE(0x000003bb)
#define SCROLL_MULTIPLIER_C             FIXED_TO_DOUBLE(0x00018041)
#define SCROLL_PIXEL_TO_WHEEL_SCALE     FIXED_TO_DOUBLE(0x0000199a)


#define FRAME_RATE                      (67.0)
#define SCREEN_RESOLUTION               (96.0)


class IOHIDAccelerator {
public:
  IOHIDAccelerator () {};
  virtual ~IOHIDAccelerator () {};
  virtual bool accelerate (double * values, size_t length, uint64_t timestamp = 0) = 0;
  virtual void serialize (CFMutableDictionaryRef dict) const = 0;
};

class IOHIDSimpleAccelerator : public IOHIDAccelerator {
  double _multiplier;
public:
  IOHIDSimpleAccelerator (double multiplier):_multiplier(multiplier) {};
  virtual ~IOHIDSimpleAccelerator () {};
  virtual bool accelerate (double * values, size_t length, uint64_t timestamp = 0);
  virtual void serialize (CFMutableDictionaryRef dict) const;
};


class IOHIDScrollAccelerator : public IOHIDAccelerator {
private:
  typedef struct {
    double    deltaTime;
    double    scroll;
  } SCROL_EVENT;
  
  IOHIDAccelerationAlgorithm *_algorithm;
  int                         _tail;
  int                         _head;
  bool                        _direction;
  uint64_t                    _lastTimeStamp;
  double                      _resolution;
  double                      _rate;
  double                      _fixedMultiplier;
  
  static mach_timebase_info_data_t    _timebase;
  
  SCROL_EVENT       events[SCROLL_EVENT_AVARAGE_LENGHT];

public:
  
  IOHIDScrollAccelerator (IOHIDAccelerationAlgorithm *algorithm, double resolution, double rate, double fixedMultiplier):
    _algorithm(algorithm),
    _tail (0),
    _head (0),
    _resolution(resolution),
    _rate(rate),
    _fixedMultiplier(fixedMultiplier) {
    if ( _timebase.denom == 0 ) {
      (void) mach_timebase_info(&_timebase);
    }
  };
  
  virtual ~IOHIDScrollAccelerator () {
    if (_algorithm) {
      delete _algorithm;
    }
  }
  virtual bool accelerate (double * values, size_t length, uint64_t timestamp = 0);
  virtual void serialize (CFMutableDictionaryRef dict) const;
};

class IOHIDPointerAccelerator : public IOHIDAccelerator {

private:
  
  IOHIDAccelerationAlgorithm *_algorithm;
  uint64_t                    _lastTimeStamp;
  double                      _resolution;
  double                      _rate;
  double                      _fixedMultiplier;
  
  static mach_timebase_info_data_t    _timebase;
public:
  
  IOHIDPointerAccelerator (IOHIDAccelerationAlgorithm *algorithm, double resolution, double rate, double fixedMultiplier):
    _algorithm(algorithm),
    _resolution (resolution),
    _rate (rate),
    _fixedMultiplier(fixedMultiplier) {
      
    if ( _timebase.denom == 0 ) {
       (void) mach_timebase_info(&_timebase);
    }
  };
  virtual ~IOHIDPointerAccelerator() {
    if (_algorithm) {
      delete _algorithm;
    }
  }
  virtual bool accelerate (double * values, size_t length, uint64_t timestamp = 0);
  virtual void serialize (CFMutableDictionaryRef dict) const;
  
};



#endif /* IOHIDAcceleration_hpp */
