/* LLVM LOCAL begin (ENTIRE FILE!)  */
/* LLVM Library Link Hack  
Copyright (C) 2005 Free Software Foundation, Inc.
Contributed by Chris Lattner (sabre@nondot.org)

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include "llvm/Module.h"
#include "llvm/ModuleProvider.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Analysis/DebugInfo.h"
#include "llvm/Assembly/PrintModulePass.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/CodeGen/SchedulerRegistry.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Streams.h"
#include "llvm/Target/TargetMachineRegistry.h"
#include "llvm/Target/TargetMachine.h"

/// dummy_function - This is used when linking the LLVM libraries into a dynamic
/// library, allowing the llvm backend to be shared across the various
/// cc1/cc1plus/... tools.  Symbol references are added to this function that
/// cause the required pieces to be correctly pulled out of LLVM .a files.
///
/// This function is never dynamically called.
///
void dummy_function() {
  llvm::ModuleProvider *MP = new llvm::ExistingModuleProvider(0);
  llvm::createVerifierPass();
  llvm::CreateBitcodeWriterPass(*llvm::cout);
  llvm::WriteBitcodeToFile(0, *llvm::cout);
  llvm::ParseBitcodeFile(NULL);
  llvm::MemoryBuffer::getNewMemBuffer(0);

  llvm::createInstructionCombiningPass();
  llvm::createScalarReplAggregatesPass();
  llvm::createCFGSimplificationPass();

  llvm::createDefaultScheduler(NULL, llvm::CodeGenOpt::Default);
  llvm::createLinearScanRegisterAllocator();
  llvm::createLocalRegisterAllocator();

  llvm::createGVNPass();
  llvm::createLoopRotatePass();
  llvm::createLICMPass();
  llvm::createSCCPPass();
  llvm::createPruneEHPass();
  llvm::createGlobalDCEPass();
  llvm::createLoopUnrollPass();
  llvm::createReassociatePass();
  llvm::createLoopUnswitchPass();
  llvm::createLoopIndexSplitPass();
  llvm::createAggressiveDCEPass();
  llvm::createConstantMergePass();
  llvm::createIndVarSimplifyPass();
  llvm::createPredicateSimplifierPass();
  llvm::createCondPropagationPass();
  llvm::createGlobalOptimizerPass();
  llvm::createJumpThreadingPass();
  llvm::createFunctionInliningPass();
  llvm::createAlwaysInlinerPass();
  llvm::createRaiseAllocationsPass();
  llvm::createSimplifyLibCallsPass();
  llvm::createArgumentPromotionPass();
  llvm::createDeadArgEliminationPass();
  llvm::createTailCallEliminationPass();
  llvm::createDeadStoreEliminationPass();
  llvm::createIPConstantPropagationPass();
  llvm::createStripDeadPrototypesPass();
  llvm::createMemCpyOptPass();
  llvm::createDeadTypeEliminationPass();
  llvm::createLoopDeletionPass();
  llvm::createFunctionAttrsPass();
  llvm::createPrintModulePass(0);

  llvm::PrettyStackTraceProgram::PrettyStackTraceProgram(0, 0);
  llvm::DIFactory::DIFactory(*MP->getModule());
  std::string Err;
  llvm::TargetMachineRegistry::getClosestStaticTargetForModule(*MP->getModule(),
                                                               Err);
}

/* LLVM LOCAL end (ENTIRE FILE!)  */
