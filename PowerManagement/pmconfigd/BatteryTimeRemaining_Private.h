//
//  BatteryTimeRemaining_Private.h
//  PowerManagement
//
//  Created by Lawrence Cayton on 4/25/23.
//
//  This header was created so that:
//   * C files like CommonLib.c can continue importing BatteryTimeRemaining.h
//   * .. but we can start using NSDates in the signature of some functions.
//
#ifndef BatteryTimeRemaining_Private_h
#define BatteryTimeRemaining_Private_h
#import <Foundation/Foundation.h>
#import "BatteryTimeRemaining.h"

__private_extern__ NSDate *copyBatteryDateOfManufacture(void);


#endif /* BatteryTimeRemaining_Private_h */
