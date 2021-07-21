//
//  XCTest_FunctionDefinitions.h
//  PowerManagement
//
//  Created by Mahdi Hamzeh on 3/22/17.
//
//


#ifdef XCTEST
#define STATIC
#define XCT_UNSAFE_UNRETAINED __unsafe_unretained
#define XCTEST_PID 10
#ifdef ERROR_LOG
#undef ERROR_LOG
#define ERROR_LOG(args...)
#endif
#else // XCTEST
#define XCT_UNSAFE_UNRETAINED
#define STATIC static
#endif

