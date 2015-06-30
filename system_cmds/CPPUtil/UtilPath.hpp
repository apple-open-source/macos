//
//  UtilPath.hpp
//  CPPUtil
//
//  Created by James McIlree on 4/8/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef CPPUtil_UtilPath_hpp
#define CPPUtil_UtilPath_hpp

class Path {
    public:
	/*
	 * INPUT		OUTPUT
	 *
	 * /tmp/scratch.tiff	scratch.tiff
	 * /tmp/scratch		scratch
	 * /tmp/		tmp
	 * scratch		scratch
	 * /mach_kernel		mach_kernel
	 * /			/
	 */
	static std::string basename(const char* path);
	static std::string basename(std::string& path);

	static bool exists(const char* path);
	static bool exists(std::string& path);

	static bool is_file(const char* path, bool should_resolve_symlinks);
	static bool is_file(std::string& path, bool should_resolve_symlinks);
};

#endif
