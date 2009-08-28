/* LLVM LOCAL begin (ENTIRE FILE!)  */
/* Internal interfaces between the LLVM backend components
Copyright (C) 2006 Free Software Foundation, Inc.
Contributed by Jim Laskey  (jlaskey@apple.com)

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

//===----------------------------------------------------------------------===//
// This is a C++ header file that defines the debug interfaces shared among
// the llvm-*.cpp files.
//===----------------------------------------------------------------------===//

#ifndef LLVM_DEBUG_H
#define LLVM_DEBUG_H

#include "llvm/Analysis/DebugInfo.h"
#include "llvm/Support/Dwarf.h"

extern "C" {
#include "llvm.h"
}  

#include <string>
#include <map>
#include <vector>

namespace llvm {

// Forward declarations
class AllocaInst;
class BasicBlock;
class CallInst;
class Function;
class Module;

/// DebugInfo - This class gathers all debug information during compilation and
/// is responsible for emitting to llvm globals or pass directly to the backend.
class DebugInfo {
private:
  Module *M;                            // The current module.
  DIFactory DebugFactory;               
  const char *CurFullPath;              // Previous location file encountered.
  int CurLineNo;                        // Previous location line# encountered.
  const char *PrevFullPath;             // Previous location file encountered.
  int PrevLineNo;                       // Previous location line# encountered.
  BasicBlock *PrevBB;                   // Last basic block encountered.
  std::map<std::string, GlobalVariable *> CUCache;
  std::map<tree_node *, DIType> TypeCache;
                                        // Cache of previously constructed 
                                        // Types.
  std::vector<DIDescriptor> RegionStack;
                                        // Stack to track declarative scopes.
  
  std::map<tree_node *, DIDescriptor> RegionMap;
public:
  DebugInfo(Module *m);

  /// Initialize - Initialize debug info by creating compile unit for
  /// main_input_filename. This must be invoked after language dependent
  /// initialization is done.
  void Initialize();

  // Accessors.
  void setLocationFile(const char *FullPath) { CurFullPath = FullPath; }
  void setLocationLine(int LineNo)           { CurLineNo = LineNo; }
  
  /// EmitFunctionStart - Constructs the debug code for entering a function -
  /// "llvm.dbg.func.start."
  void EmitFunctionStart(tree_node *FnDecl, Function *Fn, BasicBlock *CurBB);

  /// EmitRegionStart- Constructs the debug code for entering a declarative
  /// region - "llvm.dbg.region.start."
  void EmitRegionStart(BasicBlock *CurBB);

  /// EmitRegionEnd - Constructs the debug code for exiting a declarative
  /// region - "llvm.dbg.region.end."
  void EmitRegionEnd(BasicBlock *CurBB, bool EndFunction);

  /// EmitDeclare - Constructs the debug code for allocation of a new variable.
  /// region - "llvm.dbg.declare."
  void EmitDeclare(tree_node *decl, unsigned Tag, const char *Name,
                   tree_node *type, Value *AI,
                   BasicBlock *CurBB);

  /// EmitStopPoint - Emit a call to llvm.dbg.stoppoint to indicate a change of 
  /// source line.
  void EmitStopPoint(Function *Fn, BasicBlock *CurBB);
                     
  /// EmitGlobalVariable - Emit information about a global variable.
  ///
  void EmitGlobalVariable(GlobalVariable *GV, tree_node *decl);

  /// getOrCreateType - Get the type from the cache or create a new type if
  /// necessary.
  DIType getOrCreateType(tree_node *type);

  /// createBasicType - Create BasicType.
  DIType createBasicType(tree_node *type);

  /// createMethodType - Create MethodType.
  DIType createMethodType(tree_node *type);

  /// createPointerType - Create PointerType.
  DIType createPointerType(tree_node *type);

  /// createArrayType - Create ArrayType.
  DIType createArrayType(tree_node *type);

  /// createEnumType - Create EnumType.
  DIType createEnumType(tree_node *type);

  /// createStructType - Create StructType for struct or union or class.
  DIType createStructType(tree_node *type);

  /// createVarinatType - Create variant type or return MainTy.
  DIType createVariantType(tree_node *type, DIType MainTy);

  /// getOrCreateCompileUnit - Create a new compile unit.
  DICompileUnit getOrCreateCompileUnit(const char *FullPath,
                                       bool isMain = false);

  /// findRegion - Find tree_node N's region.
  DIDescriptor findRegion(tree_node *n);
};

} // end namespace llvm

#endif
/* LLVM LOCAL end (ENTIRE FILE!)  */
