//
//  Kernel.hpp
//  KDBG
//
//  Created by James McIlree on 4/17/13.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#ifndef __kdprof__Kernel__
#define __kdprof__Kernel__

enum class KernelSize { k32, k64 };

class Kernel {
    public:
	static bool is_64_bit();
	static uint32_t active_cpu_count();
};

#endif /* defined(__kdprof__Kernel__) */
