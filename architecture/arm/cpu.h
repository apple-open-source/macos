/*
 * Copyright (c) 2000-2007 Apple Inc. All rights reserved.
 */
y
/*
 * Control register 0
 */
 
typedef struct _cr0 {
    unsigned int	pe	:1,
    			mp	:1,
			em	:1,
			ts	:1,
				:1,
			ne	:1,
				:10,
			wp	:1,
				:1,
			am	:1,
				:10,
			nw	:1,
			cd	:1,
			pg	:1;
} cr0_t;

/*
 * Debugging register 6
 */

typedef struct _dr6 {
    unsigned int	b0	:1,
    			b1	:1,
			b2	:1,
			b3	:1,
				:9,
			bd	:1,
			bs	:1,
			bt	:1,
				:16;
} dr6_t;
