//
//  Heimdal
//
//

#ifndef krb5_stat_h
#define krb5_stat_h

#ifdef __OBJC__
#include <Foundation/Foundation.h>
#define KRB5_STAT_CONST(c)  extern NSString * c
#else
#include <CoreFoundation/CoreFoundation.h>
#define KRB5_STAT_CONST(c)  extern CFStringRef c
#endif


KRB5_STAT_CONST(kHeimStatisticCommand);
KRB5_STAT_CONST(kHeimStatisticValue);
KRB5_STAT_CONST(kHeimStatisticExtraValue);

KRB5_STAT_CONST(kHeimStatisticCommandAdd);
KRB5_STAT_CONST(kHeimStatisticCommandSet);

#endif /* krb5_stat_h */
