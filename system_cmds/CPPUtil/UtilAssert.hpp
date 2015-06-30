//
//  Assert.hpp
//  CPPUtil
//
//  Created by James McIlree on 4/7/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef CPPUtil_Assert_hpp
#define CPPUtil_Assert_hpp

#if !defined(NDEBUG) && !defined(NS_BLOCK_ASSERTIONS)

        #define DEBUG_ONLY( statement ) statement

        #define	ASSERT(e, d)												\
        {														\
                if (__builtin_expect(!(e), 0)) {									\
                        ::printf("ASSERT(%s) %s %d, %s\n", #e, util::Path::basename((char*)__FILE__).c_str(), __LINE__, d);	\
                        std::abort();											\
                }													\
        }

        #define SHOULD_NOT_REACH_HERE(d)										\
        {														\
                ::printf("SHOULD_NOT_REACH_HERE %s %d, %s\n", util::Path::basename((char*)__FILE__).c_str(), __LINE__, d);	\
		std::abort();												\
        }

        #define TrueInDebug true

#else

        #define DEBUG_ONLY( statement )
        #define	ASSERT(e, d)
        #define SHOULD_NOT_REACH_HERE(d)

        #define TrueInDebug false

#endif

#define	GUARANTEE(e)												\
{														\
	if (__builtin_expect(!(e), 0)) {									\
		::printf("ASSERT(%s) %s %d\n", #e, util::Path::basename((char*)__FILE__).c_str(), __LINE__);	\
		std::abort();											\
	}													\
}

#endif
