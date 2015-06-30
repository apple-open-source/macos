//
//  UtilMakeUnique.hpp
//  CPPUtil
//
//  Created by James McIlree on 4/15/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef CPPUtil_UtilMakeUnique_hpp
#define CPPUtil_UtilMakeUnique_hpp

/* Not needed in C++14 or later */
#if __cplusplus <= 201103

template<typename T, typename ...Args>
std::unique_ptr<T> make_unique( Args&& ...args )
{
	return std::unique_ptr<T>( new T( std::forward<Args>(args)... ) );
}

#endif

#endif
