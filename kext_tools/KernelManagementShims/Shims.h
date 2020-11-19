/*
 *  kernelmanagement_shims.h
 *  kext_tools
 *
 *  Copyright 2020 Apple Inc. All rights reserved.
 *
 */

#ifndef _KERNELMANAGEMENT_SHIMS_H_
#define _KERNELMANAGEMENT_SHIMS_H_

bool isKernelManagementLinked();
int KernelManagementLoadKextsWithURLs(CFArrayRef urls);

#endif /* _KERNELMANAGEMENT_SHIMS_H_ */
