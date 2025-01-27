/*
 * Copyright (c) 2011-2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */
#ifndef _H_EVALUATIONMANAGER
#define _H_EVALUATIONMANAGER

#include "policydb.h"
#include <security_utilities/cfutilities.h>

namespace Security {
namespace CodeSigning {


class PolicyEngine;
class EvaluationTask; /* an opaque type */

//
// EvaluationManager manages a list of concurrent evaluation tasks (each of
// which is wrapped within an EvaluationTask object).
//
class EvaluationManager
{
public:
    static EvaluationManager *globalManager();

    EvaluationTask *evaluationTask(PolicyEngine *engine, CFURLRef path, AuthorityType type, SecAssessmentFlags flags, CFDictionaryRef context, CFMutableDictionaryRef result);
    void finalizeTask(EvaluationTask *task, SecAssessmentFlags flags, CFMutableDictionaryRef result);

    void kickTask(CFStringRef key);

private:
    CFCopyRef<CFMutableDictionaryRef> mCurrentEvaluations;

    EvaluationManager();
    ~EvaluationManager();

    void removeTask(EvaluationTask *task);

    dispatch_queue_t                  mListLockQueue;
};



} // end namespace CodeSigning
} // end namespace Security

#endif //_H_EVALUATIONMANAGER

