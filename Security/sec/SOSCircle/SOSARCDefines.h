//
//  SOSARCDefines.h
//  sec
//
//  Created by John Hurley on 11/2/12.
//
//

#ifndef sec_SOSARCDefines_h
#define sec_SOSARCDefines_h

#ifndef __has_feature
#define __has_feature(x) 0
#endif
#ifndef __has_extension
#define __has_extension __has_feature // Compatibility with pre-3.0 compilers.
#endif

#if __has_feature(objc_arc) && __clang_major__ >= 3
#define ARC_ENABLED 1
#endif // __has_feature(objc_arc)

#if !ARC_ENABLED || !defined(__clang__) || __clang_major__ < 3

#ifndef __bridge
#define __bridge
#endif
#ifndef __bridge_retained
#define __bridge_retained
#endif
#ifndef __bridge_transfer
#define __bridge_transfer
#endif
#ifndef __autoreleasing
#define __autoreleasing
#endif
#ifndef __strong
#define __strong
#endif
#ifndef __weak
#define __weak
#endif
#ifndef __unsafe_unretained
#define __unsafe_unretained
#endif

#endif // __clang_major__ < 3

#endif
