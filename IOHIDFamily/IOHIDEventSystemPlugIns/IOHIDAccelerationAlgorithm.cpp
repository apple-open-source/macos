//
//  IOHIDAccelerationAlgorithm.cpp
//  IOHIDFamily
//
//  Created by YG on 10/29/15.
//
//

#include "IOHIDAccelerationAlgorithm.hpp"
#include "IOHIDParameter.h"
#include <cmath>
#include <sstream>


bool operator< (const ACCEL_POINT &lhs, const ACCEL_POINT &rhs);
bool operator==(const ACCEL_POINT &lhs, const ACCEL_POINT &rhs);
/*

PA Acceleration algorythm

  Parametric acceleration curve in general form contain 3 segment
  
  1) first segment [0 .. HIDAccelTangentSpeedLinear]   
  
    use function  f_1(x) = GainLinear*x + GainParabolic * x^2 + GainCubic * x^3 + GainQudratic * x^4;
  
  2) second segment [HIDAccelTangentSpeedLinear .. HIDAccelTangentSpeedParabolicRoot]
  
    use function f_2(x) = m0*x + b0 (f_2 is the tangent line to the function f_1 at x == HIDAccelTangentSpeedLinear)
    
    m0 = f_1`(x) = GainLinear + 2 * x * GainParabolic + 3 * x^2 * curve.GainCubic + 4 * curve.GainQudratic * x^3 =
                   GainLinear + 2 * TangentSpeedLinear * GainParabolic  + 3 * TangentSpeedLinear ^ 2 * GainCubic + 4 * TangentSpeedLinear ^ 3 * curve.GainQudratic
    
    and 
    
    b0 = f_2(HIDAccelTangentSpeedLinear) -  m * HIDAccelTangentSpeedLinear =
         f_1(HIDAccelTangentSpeedLinear) -  m * HIDAccelTangentSpeedLinear
         f_1(HIDAccelTangentSpeedLinear) == f_2(HIDAccelTangentSpeedLinear) by definition
 
  3) third segment  [HIDAccelTangentSpeedParabolicRoot .. ]
 
   use function f_3(x) = sqrd (m1*x + b1) (f_2 is tangent line to f_3  at x == HIDAccelTangentSpeedParabolicRoot)
   
   m = f_3'(x) = m1/(2(SQRT(m1*HIDAccelTangentSpeedParabolicRoot + b1));
   f_2(HIDAccelTangentSpeedParabolicRoot) = SQRT(m1*HIDAccelTangentSpeedParabolicRoot + b1)   and m = m0 by definition
   
   so 
 
   m1 = 2 * f_2 (HIDAccelTangentSpeedParabolicRoot) * m0
 
   b1 = f_3(HIDAccelTangentSpeedParabolicRoot) ^ 2 - m1 * HIDAccelTangentSpeedParabolicRoot =
        f_2(HIDAccelTangentSpeedParabolicRoot) ^ 2 - m1 * HIDAccelTangentSpeedParabolicRoot
 
 
 if HIDAccelTangentSpeedLinear == 0 or unspecified we ignore f_2 and assume
 
    f_1 for [0 .. HIDAccelTangentSpeedParabolicRoot]
    f_3 for [HIDAccelTangentSpeedParabolicRoot .. ]
 
 if HIDAccelTangentSpeedParabolicRoot == 0 or unspecified we ignore f_3 and assume
 
    f_1 for [0 .. HIDAccelTangentSpeedLinear]
    f_2 for [HIDAccelTangentSpeedLinear .. ]

 if HIDAccelTangentSpeedLinear == 0 and HIDAccelTangentSpeedParabolicRoot == 0 or both unspecified we ignore f_2 and f_3
 
   f_1 for [0 .. ]
 
 
 PA acceleration data adjustmenet
  
  PA acceleration data adjusted base on request for desired acceleration index
  
  high = GetHighCurve (accell)
  low  = GetLowCurve  (accell)
 
  ration = (request - low.Index) / (high.Index - low.Index);
  
  curve.GainLinear = low.GainLinear + (high.GainLinear - low.GainLinear ) * ratio
  curve.GainParabolic = low.GainParabolic + (high.GainParabolic - low.GainParabolic ) * ratio
  curve.GainCubic = low.GainCubic + (high.GainCubic - low.GainCubic ) * ratio
  curve.GainQudratic = low.GainQudratic + (high.GainQudratic - low.GainQudratic ) * ratio

 
*/


IOHIDParametricAcceleration::ACCELL_CURVE IOHIDParametricAcceleration::GetCurve (CFDictionaryRef curve) {
  ACCELL_CURVE result;
  result.Index         = GetCurveParameter (curve, CFSTR(kHIDAccelIndexKey));
  result.GainLinear    = GetCurveParameter (curve, CFSTR(kHIDAccelGainLinearKey));
  result.GainParabolic = GetCurveParameter (curve, CFSTR(kHIDAccelGainParabolicKey));
  result.GainCubic     = GetCurveParameter (curve, CFSTR(kHIDAccelGainCubicKey));
  result.GainQudratic  = GetCurveParameter (curve, CFSTR(kHIDAccelGainQuarticKey));
  result.TangentSpeedLinear        = GetCurveParameter (curve, CFSTR(kHIDAccelTangentSpeedLinearKey));
  result.TangentSpeedParabolicRoot = GetCurveParameter (curve, CFSTR(kHIDAccelTangentSpeedParabolicRootKey));
  return result;
}


double IOHIDParametricAcceleration::GetCurveParameter (CFDictionaryRef curve, CFStringRef key) {
  CFDictionaryRefWrap curveWrap (curve);
  CFNumberRefWrap value = (CFNumberRef)curveWrap[key];
  if (value.Reference() == NULL) {
    return 0;
  }
  return FIXED_TO_DOUBLE((SInt64)value);
}

IOHIDParametricAcceleration * IOHIDParametricAcceleration::CreateWithParameters (CFArrayRef curves, double acceleration, double resolution, double rate) {
  
  IOHIDParametricAcceleration * self = NULL;

  double accelIndex = acceleration;
  HIDLogDebug("acceleration %f resolution %f rate %f", accelIndex, resolution, rate);
  
  if (curves == NULL ||  accelIndex < 0) {
    return NULL;
  }

  CFArrayRefWrap cfCurves (curves);
  
  size_t  currentIndex = 0;

  std::vector <ACCELL_CURVE> accelCurves;

  for (CFIndex index = 0; index < (CFIndex)cfCurves.Count(); index++) {
    ACCELL_CURVE curve = GetCurve ((CFDictionaryRef) cfCurves[index]);
    if (curve.isValid()) {
      accelCurves.push_back(curve);
      if (accelIndex >= curve.Index) {
        currentIndex = index;
      }
    }
  }
  
  if (accelCurves.size() == 0) {
    return self;
  }
  
  self = new IOHIDParametricAcceleration;
  
  if (!self) {
    return self;
  }
  
  self->resolution_ = resolution;
  self->rate_ = rate;
  self->accelIndex_ = accelIndex;
  
  HIDLogDebug("table index %zu", currentIndex);

  if (accelCurves[currentIndex].Index < accelIndex && (currentIndex + 1) < accelCurves.size() ) {
    double ratio = (accelIndex - accelCurves[currentIndex].Index) / (accelCurves[currentIndex + 1].Index - accelCurves[currentIndex ].Index);
    self->accel.Index         = accelCurves[currentIndex].Index + ratio * (accelCurves[currentIndex + 1].Index - accelCurves[currentIndex].Index);
    self->accel.GainLinear    = accelCurves[currentIndex].GainLinear + ratio * (accelCurves[currentIndex + 1].GainLinear - accelCurves[currentIndex].GainLinear);
    self->accel.GainParabolic = accelCurves[currentIndex].GainParabolic + ratio * (accelCurves[currentIndex + 1].GainParabolic - accelCurves[currentIndex].GainParabolic);
    self->accel.GainCubic     = accelCurves[currentIndex].GainCubic + ratio * (accelCurves[currentIndex + 1].GainCubic - accelCurves[currentIndex].GainCubic);
    self->accel.GainQudratic  = accelCurves[currentIndex].GainQudratic + ratio * (accelCurves[currentIndex + 1].GainQudratic - accelCurves[currentIndex].GainQudratic);
    self->accel.TangentSpeedLinear  = accelCurves[currentIndex].TangentSpeedLinear + ratio * (accelCurves[currentIndex + 1].TangentSpeedLinear - accelCurves[currentIndex].TangentSpeedLinear);
    self->accel.TangentSpeedParabolicRoot  = accelCurves[currentIndex].TangentSpeedParabolicRoot + ratio * (accelCurves[currentIndex + 1].TangentSpeedParabolicRoot - accelCurves[currentIndex].TangentSpeedParabolicRoot);
  } else {
    memcpy (&self->accel, &accelCurves[currentIndex], sizeof(self->accel));
  }
  
  double y0;
  
  self->tangent[0] =  std::numeric_limits<double>::max();
  self->tangent[1] =  std::numeric_limits<double>::max();

  if (self->accel.TangentSpeedLinear != 0) {
    y0 = self->accel.GainLinear * self->accel.TangentSpeedLinear +
        pow (self->accel.GainParabolic * self->accel.TangentSpeedLinear, 2) +
        pow (self->accel.GainCubic * self->accel.TangentSpeedLinear, 3) +
        pow (self->accel.GainQudratic * self->accel.TangentSpeedLinear, 4);
  
    self->m[0] = self->accel.GainLinear +
                 2 * self->accel.TangentSpeedLinear * pow(self->accel.GainParabolic,2) +
                 3 * pow(self->accel.TangentSpeedLinear,2) * pow(self->accel.GainCubic,3) +
                 4 * pow (self->accel.TangentSpeedLinear, 3) * pow(self->accel.GainQudratic, 4);

    self->b[0] = y0 - self->m[0] * self->accel.TangentSpeedLinear;
    
    self->tangent[0] = self->accel.TangentSpeedLinear;
    
    if (self->accel.TangentSpeedParabolicRoot != 0) {
      double y1 = (self->m[0] * self->accel.TangentSpeedParabolicRoot + self->b[0]);
      self->m[1] = 2 * y1 * self->m[0];
      self->b[1] = pow (y1,2) - self->m[1] * self->accel.TangentSpeedParabolicRoot;
      self->tangent[1] = self->accel.TangentSpeedParabolicRoot;
    }
  } else if ( self->accel.TangentSpeedParabolicRoot != 0) {
    y0  = self->accel.GainLinear * self->accel.TangentSpeedParabolicRoot +
          pow (self->accel.GainParabolic * self->accel.TangentSpeedParabolicRoot, 2) +
          pow (self->accel.GainCubic * self->accel.TangentSpeedParabolicRoot, 3) +
          pow (self->accel.GainQudratic * self->accel.TangentSpeedParabolicRoot, 4);
    
    self->m[1] = self->accel.GainLinear +
                 2 * self->accel.TangentSpeedParabolicRoot * pow(self->accel.GainParabolic,2) +
                 3 * pow(self->accel.TangentSpeedParabolicRoot,2) * pow(self->accel.GainCubic,3) +
                 4 * pow (self->accel.TangentSpeedParabolicRoot, 3) * pow(self->accel.GainQudratic, 4);
    self->b[1] = pow (y0,2) - self->m[1] *  self->accel.TangentSpeedParabolicRoot;
    self->tangent[0] = self->accel.TangentSpeedParabolicRoot;
  }
  
  
  return self;
}
  
double IOHIDParametricAcceleration::multiplier (double value) {
  double multiplier;
  double deviceScale = resolution_/rate_;
  value /= deviceScale;
  
  if ( value <= tangent[0]) {
    multiplier =  accel.GainLinear * value +
                  pow (accel.GainParabolic * value, 2) +
                  pow (accel.GainCubic * value, 3) +
                  pow (accel.GainQudratic * value, 4);

//    HIDLogDebug("NHIDPOINTER: PAR: standardized_speed %11.4f accelerated_speed %11.4f scale %11.4f",
//              value, multiplier, kCursorScale);
//
//    HIDLogDebug("NHIDPOINTER: PAR: accelIndex %11.4f gain[0] %11.4f gain[1] %11.4f gain[2] %11.4f gain[3] %11.4f",
//              accelIndex_,
//              accel.GainLinear,
//              accel.GainParabolic,
//              accel.GainCubic,
//              accel.GainQudratic
//          );
    
  } else if (value <= tangent[1] && tangent[0] == accel.TangentSpeedLinear) {
    multiplier = m[0] * value + b[0];
//    HIDLogDebug("NHIDPOINTER: PAR: standardized_speed %11.4f accelerated_speed %11.4f scale %11.4f",
//              value, multiplier, kCursorScale);
//
//    HIDLogDebug("NHIDPOINTER: PAR: accelIndex %11.4f m0:%11.4f b0:%11.4f",
//          accelIndex_,
//          m[0],
//          b[0]
//          );
  } else {
    multiplier = sqrt (m[1] * value + b[1]);
//    HIDLogDebug("NHIDPOINTER: PAR: standardized_speed %11.4f accelerated_speed %11.4f scale %11.4f",
//              value, multiplier, kCursorScale);
//
//    HIDLogDebug("NHIDPOINTER: PAR: accelIndex %11.4f m_root:%11.4f b_root:%11.4f",
//              accelIndex_,
//              m[1],
//              b[1]
//          );
  }

  return multiplier * kCursorScale;
}

void IOHIDParametricAcceleration::serialize(CFMutableDictionaryRef dict) const {

  CFMutableDictionaryRefWrap serializer (dict);
  serializer.SetValueForKey(CFSTR("Class"), CFSTR("IOHIDParametricAcceleration"));
  serializer.SetValueForKey(CFSTR("AccelIndex"), DOUBLE_TO_FIXED(accel.Index));
  serializer.SetValueForKey(CFSTR("GainLinear"), DOUBLE_TO_FIXED(accel.GainLinear));
  serializer.SetValueForKey(CFSTR("GainCubic"),  DOUBLE_TO_FIXED(accel.GainCubic));
  serializer.SetValueForKey(CFSTR("TangentSpeedLinear"), DOUBLE_TO_FIXED(accel.TangentSpeedLinear));
  serializer.SetValueForKey(CFSTR("TangentSpeedParabolicRoot"), DOUBLE_TO_FIXED(accel.TangentSpeedParabolicRoot));
  
}

bool operator< (const ACCEL_POINT &lhs, const ACCEL_POINT &rhs) {
    return (lhs.x < rhs.x);
}

bool operator==(const ACCEL_POINT &lhs, const ACCEL_POINT &rhs) {
    return (lhs.x == rhs.x);
}

ACCEL_POINT  IOHIDTableAcceleration::InterpolatePoint (const ACCEL_POINT &p, const ACCEL_POINT &p0, const ACCEL_POINT &p1, double ratio) {
    double m;
    double b;
    ACCEL_POINT result;
    m = (p1.y - p0.y ) / (p1.x - p0.x);
    b =  p1.y - m * (p1.x);
    result.x = p.x;
    result.y = p.x * m + b;
    result.y = std::min (p.y, result.y) + std::abs (p.y - result.y) * ratio;
    return result;
}

ACCEL_POINT IOHIDTableAcceleration::InterpolatePoint( const ACCEL_POINT &p, const ACCEL_POINT &p0, const ACCEL_POINT &p1,  double scale , boolean_t isLower) {
    double m;
    double b;
    ACCEL_POINT result;

    m = (p1.x == p0.x) ? 0 : (p1.y - p0.y) / (p1.x - p0.x);
    b = p0.y - m * p0.x;

    result.y = b + m * p.x;
    if( isLower) {
        result.y = p.y -  scale * (p.y - result.y);
    } else {
        result.y = result.y + scale * (p.y - result.y);
    }
    result.x = p.x;
    return result;
}

void  IOHIDTableAcceleration::InterpolateFunction (const ACCEL_TABLE_ENTRY *lo, const ACCEL_TABLE_ENTRY *hi, double ratio, std::set<ACCEL_POINT> &result) {
  uint32_t hindex = 0;
  ACCEL_POINT  ph0 = {0,0};
  ACCEL_POINT  ph1 = hi->point(hindex);
  for (uint32_t index = 0; index < lo->count(); index++) {
    ACCEL_POINT p = lo->point(index);
    while (p.x > ph1.x) {
      if (hindex < (hi->count() - 1)) {
        ph0 = ph1;
        ph1 = hi->point(++hindex);
      } else {
        break;
      }
    }
    result.insert(InterpolatePoint (p, ph0, ph1, ratio));
  }
}

IOHIDTableAcceleration * IOHIDTableAcceleration::CreateWithTable (CFDataRef data, double acceleration, double resolution, double rate) {
  
  ACCEL_TABLE       *table = (ACCEL_TABLE *)CFDataGetBytePtr(data);
  if (table == NULL) {
    HIDLogDebug("table in NULL");
    return NULL;
  }
  if (table->signature() != APPLE_ACCELERATION_DEFAULT_TABLE_SIGNATURE &&
      table->signature() != APPLE_ACCELERATION_MT_TABLE_SIGNATURE
      ) {
    HIDLogDebug("unsupported table signature  %d", table->signature());
    return NULL;
  }
  
  IOHIDTableAcceleration * self = new IOHIDTableAcceleration;
  if (!self) {
    return self;
  }

  HIDLogDebug("acceleration %f resolution %f rate %f", acceleration, resolution, rate);

  self->resolution_ = resolution;
  self->rate_ = rate;
  
  
  const ACCEL_TABLE_ENTRY *hi = table->entry(0);
  const ACCEL_TABLE_ENTRY *lo = hi;
  
  std::stringstream s;
  s << *table;
  
  HIDLogDebug("Acceleration table %s", s.str().c_str());
  
  std::set<ACCEL_POINT> f;

  for (unsigned int i = 0; i < table->count(); i++) {
    lo = hi;
    hi = table->entry(i);
    if (acceleration <= hi->acceleration<double>()) {
      if (acceleration == hi->acceleration<double>()) {
          lo = hi;
      };
      break;
    }
  }
  if (hi == lo || acceleration > hi->acceleration<double>()) {
    for (uint32_t index = 0; index < hi->count(); index++) {
        double scale = (acceleration + 1) / (hi->acceleration<double>() + 1);
        ACCEL_POINT point = hi->point(index);
        point.y *= scale;
        f.insert(point);
    }
  } else {
    // Interpolate two curves with ratio
    double ratio = (acceleration - lo->acceleration<double>()) / (hi->acceleration<double>() - lo->acceleration<double>());
    self->InterpolateFunction(lo, hi, ratio, f);
    self->InterpolateFunction(hi, lo, ratio, f);
  }
    
  // build segment function table
  ACCEL_POINT p0;
  ACCEL_POINT p1 {0,0};
  for (auto iter = f.begin(); iter != f.end(); iter++) {
    p0 = p1;
    p1 = *iter;
    p1.x *= resolution / rate;
    p1.y *= kCursorScale;
    double m;
    double b;
    m = (p1.y - p0.y ) / (p1.x - p0.x);
    b =  p1.y - m * (p1.x);
        self->segments_.push_back({m, b, p1.x});
  }
  return self;
}
  
IOHIDTableAcceleration * IOHIDTableAcceleration::CreateOriginalWithTable (CFDataRef table, double acceleration, const double resolution, const double rate)
{

    double	scale;
    UInt32	count;
    Boolean	isLower;

    ACCEL_POINT lower, upper, p1, p2, p3, prev, curveP1, curveP2;
    double      loAccel, hiAccel;
    const void *  loTable = NULL;
    const void *  hiTable = NULL;
    unsigned int loCount, hiCount;
    p1 = prev = curveP1 = curveP2 = {0,0};
    loCount = hiCount = 0;
    loAccel = 0;

    if( table == NULL || resolution == 0 || rate == 0) {
        return NULL;
    }

    IOHIDTableAcceleration * self = new IOHIDTableAcceleration;

    if (self == NULL) {
        return NULL;
    }
    self->resolution_ = resolution;
    self->rate_ = rate;

    hiTable = CFDataGetBytePtr(table);

    scale = FIXED_TO_DOUBLE(ACCEL_TABLE_CONSUME_INT32(&hiTable));
    ACCEL_TABLE_CONSUME_INT32(&hiTable);
    // normalize table's default (scale) to 0.5
    if( acceleration > 0.5) {
        acceleration =  (acceleration - 0.5) * (1 - scale ) * 2 + scale ;
    } else {
        acceleration = acceleration * scale ;
    }

    count = ACCEL_TABLE_CONSUME_INT16(&hiTable);
    scale = 1.0;

    // find curves bracketing the desired value
    do {
        hiAccel =  FIXED_TO_DOUBLE(ACCEL_TABLE_CONSUME_INT32(&hiTable));
        hiCount =  ACCEL_TABLE_CONSUME_INT16 (&hiTable);

        if( acceleration <= hiAccel) {
            break;
        }

        if( 0 == --count) {
            // this much over the highest table
            scale = (hiAccel) ? (acceleration / hiAccel ) : 0;
            loTable = NULL;
            break;
        }

        loTable    = hiTable;
        loAccel	   = hiAccel;
        loCount    = hiCount;
        hiTable    = (uint8_t*)hiTable + loCount * 8;

    } while (true);

    // scale between the two
    if( loTable) {
        scale = (hiAccel == loAccel) ? 0 : (acceleration - loAccel) / (hiAccel - loAccel);
    }
    // or take all the high one
    else {
        loTable  = hiTable;
        //loAccel	 = hiAccel;
        loCount  = 0;
    }

    lower = ACCELL_TABLE_CONSUME_POINT(&loTable);
    upper = ACCELL_TABLE_CONSUME_POINT(&hiTable);

    do {
        // consume next point from first X
        isLower = (loCount && (!hiCount || (lower.x <= upper.x)));

        if( isLower) {
            /* highline */
            p2 = upper;
            p3 = lower;
            if( loCount && (--loCount)) {
                lower = ACCELL_TABLE_CONSUME_POINT(&loTable);
            }
        } else  {
            /* lowline */
            p2 = lower;
            p3 = upper;
            if( hiCount && (--hiCount)) {
                upper  = ACCELL_TABLE_CONSUME_POINT(&hiTable);
            }
        }
        {

            curveP2 = InterpolatePoint(p3, p1, p2 , scale, isLower);
            curveP2.x *= (resolution/rate);
            curveP2.y *= kCursorScale;

            ACCEL_SEGMENT segment;

            segment.m = (curveP2.x == curveP1.x) ? 0 : (curveP2.y - curveP1.y) / (curveP2.x - curveP1.x),
                    segment.b = curveP2.y - segment.m * curveP2.x,
                            segment.x = (loCount || hiCount) ? curveP2.x : MAX_DEVICE_THRESHOLD;

            self->segments_.push_back ({
                segment
            });

            curveP1 = curveP2;
        }

        // continue on from last point
        if( loCount && hiCount) {
            if( lower.x > upper.x) {
                prev = p1;
            } else {
                prev = p1;
                p1 = p3;
            }
        } else {
            p2 = p1;
            p1 = prev;
            prev = p2;
        }

    } while( loCount || hiCount );

    return self;
}

double IOHIDTableAcceleration::multiplier (double value) {
  auto iter = segments_.begin();
  auto curve = iter;
  for (;iter != segments_.end(); iter++) {
    curve = iter;
    if (!(value > iter->x)) {
      break;
    }
  }
//  HIDLogDebug("IOHIDTableAcceleration: curve x:%x m:%x b:%x value:%x", (uint32_t)DOUBLE_TO_FIXED(curve->x) , (uint32_t)DOUBLE_TO_FIXED(curve->m), (uint32_t)DOUBLE_TO_FIXED(curve->b), (uint32_t)DOUBLE_TO_FIXED(value));
 
  return curve->m * value + curve->b;
}

void IOHIDTableAcceleration::serialize(CFMutableDictionaryRef dict) const {
  CFMutableDictionaryRefWrap serializer (dict);
  serializer.SetValueForKey(CFSTR("Class"), CFSTR("IOHIDTableAcceleration"));
  CFMutableArrayRefWrap curves;
  for (auto iter = segments_.begin() ; iter != segments_.end() ; ++iter) {
    curves.Append(CFDictionaryRefWrap(
                    {CFSTR("m"),CFSTR("b"),CFSTR("x")},
                    {CFNumberRefWrap(DOUBLE_TO_FIXED(iter->m)),CFNumberRefWrap(DOUBLE_TO_FIXED(iter->b)),CFNumberRefWrap(DOUBLE_TO_FIXED(iter->x))})
                  );
  }
  serializer.SetValueForKey(CFSTR("Curves"), curves);
}





