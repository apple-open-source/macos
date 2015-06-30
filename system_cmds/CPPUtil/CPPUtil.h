//
//  CPPUtil.h
//  CPPUtil
//
//  Created by James McIlree on 4/7/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef CPPUtil_CPPUtil_h
#define CPPUtil_CPPUtil_h

#include <unistd.h>
#include <limits.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <cstdlib>
#include <string>
#include <sstream>
#include <exception>
#include <vector>
#include <asl.h>

#include <math.h>

#include <mach/mach_time.h>

namespace util {
	
#include "UtilBase.hpp"
#include "UtilAssert.hpp"
#include "UtilException.hpp"
#include "UtilMakeUnique.hpp"
	
#include "UtilPath.hpp"

#include "UtilTRange.hpp"
#include "UtilTRangeValue.hpp"

#include "UtilPrettyPrinting.hpp"
#include "UtilTime.hpp"
#include "UtilAbsTime.hpp"
#include "UtilNanoTime.hpp"
#include "UtilAbsInterval.hpp"
#include "UtilNanoInterval.hpp"
#include "UtilTimer.hpp"
	
#include "UtilLog.hpp"

#include "UtilFileDescriptor.hpp"
#include "UtilString.hpp"

#include "UtilMappedFile.hpp"
#include "UtilMemoryBuffer.hpp"

#include "UtilTerminalColor.hpp"
	
}

#endif
