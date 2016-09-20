//
//  IOHIDAccelerationAlgorithm.hpp
//  IOHIDFamily
//
//  Created by YG on 10/29/15.
//
//

#ifndef IOHIDAccelerationAlgorithm_hpp
#define IOHIDAccelerationAlgorithm_hpp

#include <CoreFoundation/CoreFoundation.h>
#include "IOHIDAccelerationTable.hpp"
#include <vector>
#include <set>
#include <ostream>
#include <iomanip>
#include "CF.h"
#include "IOHIDDebug.h"

#define FIXED_TO_DOUBLE(x) ((x)/65536.0)
#define DOUBLE_TO_FIXED(x) (uint64_t)((x)*65536.0)
#define MAX_DEVICE_THRESHOLD   FIXED_TO_DOUBLE(0x7fffffff)
#define kCursorScale (96.0/67.0)

class IOHIDAccelerationAlgorithm {
   
public:

    enum Type {
      Table,
      Parametric,
      Unknown
    };
  
    IOHIDAccelerationAlgorithm () {}
  
    virtual ~IOHIDAccelerationAlgorithm() {}
    virtual double multiplier (double value) {
      return value;
    }
    virtual IOHIDAccelerationAlgorithm::Type getType () const {
      return IOHIDAccelerationAlgorithm::Unknown;
    };
  
    virtual void serialize (CFMutableDictionaryRef dict) const = 0;
  
};



class IOHIDParametricAcceleration : public  IOHIDAccelerationAlgorithm {

public:

    typedef struct {
        double  Index;
        double  GainLinear;
        double  GainParabolic;
        double  GainCubic;
        double  GainQudratic;
        double  TangentSpeedLinear;
        double  TangentSpeedParabolicRoot;
        bool isValid () {
          return GainLinear || GainParabolic  || GainCubic || GainQudratic ;
        }
    } ACCELL_CURVE;

    double        tangent [2];
    double        m [2];
    double        b [2];
    
    ACCELL_CURVE  accel;
    
    double        resolution_;
    double        rate_;
    double        accelIndex_;

    static double GetCurveParameter (CFDictionaryRef curve, CFStringRef parameter);
    static ACCELL_CURVE GetCurve (CFDictionaryRef curve);
  
public:
  
    static  IOHIDParametricAcceleration * CreateWithParameters (CFArrayRef curves, double acceleration, double resolution, double rate);
    
    virtual ~IOHIDParametricAcceleration() {}
  
    virtual double multiplier (double value);

    virtual IOHIDAccelerationAlgorithm::Type getType () const {
        return IOHIDAccelerationAlgorithm::Parametric;
    };

    virtual void serialize (CFMutableDictionaryRef dict) const;

protected:

    IOHIDParametricAcceleration () {};
};


class IOHIDTableAcceleration : public  IOHIDAccelerationAlgorithm {
  
protected:
  
    std::vector <ACCEL_SEGMENT> segments_;
    
    double resolution_;
    double rate_;
    
    ACCEL_POINT  InterpolatePoint (const ACCEL_POINT &p, const ACCEL_POINT &p0, const ACCEL_POINT &p1, double scale);
    static ACCEL_POINT  InterpolatePoint (const ACCEL_POINT &p, const ACCEL_POINT &p0, const ACCEL_POINT &p1,  double scale , boolean_t isLower);
   
    void  InterpolateFunction (const ACCEL_TABLE_ENTRY *lo, const ACCEL_TABLE_ENTRY *hi, double scale, std::set<ACCEL_POINT> &result);
  
public:
  
    static  IOHIDTableAcceleration * CreateWithTable (CFDataRef table, double acceleration, double resolution, double rate);
    static  IOHIDTableAcceleration * CreateOriginalWithTable (CFDataRef table, double acceleration, double resolution, double rate);
    
    virtual ~IOHIDTableAcceleration() {}
    
    virtual double multiplier (double value);

    virtual IOHIDAccelerationAlgorithm::Type getType () const {
        return IOHIDAccelerationAlgorithm::Table;
    };

    virtual void serialize (CFMutableDictionaryRef dict) const;

protected:
  
  IOHIDTableAcceleration () {};
};

#endif /* IOHIDAccelerationAlgorithm_hpp */
