/*
 * Copyright (c) 2000-2007 Apple Inc. All rights reserved.
 */

/*
 * Segment selector.
 */

#ifndef __XNU_ARCH_ARM_SEL_H
#define __XNU_ARCH_ARM_SEL_H

typedef struct sel {
    unsigned short	rpl	:2,
#define KERN_PRIV	0
#define USER_PRIV	3
			ti	:1,
#define SEL_GDT		0
#define SEL_LDT		1
			index	:13;
} sel_t;

#define NULL_SEL	((sel_t) { 0, 0, 0 } )

#endif /* __XNU_ARCH_ARM_SEL_H */
