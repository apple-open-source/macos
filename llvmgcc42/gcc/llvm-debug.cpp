/* LLVM LOCAL begin (ENTIRE FILE!)  */
/* High-level LLVM backend interface 
Copyright (C) 2005 Free Software Foundation, Inc.
Contributed by Jim Laskey (jlaskey@apple.com)

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
// This is a C++ source file that implements the debug information gathering.
//===----------------------------------------------------------------------===//

#include "llvm-debug.h"

#include "llvm-abi.h"
#include "llvm-internal.h"
#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Instructions.h"
#include "llvm/Intrinsics.h"
#include "llvm/Module.h"
#include "llvm/Support/Dwarf.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/SmallVector.h"

extern "C" {
#include "langhooks.h"
#include "toplev.h"
#include "tree.h"
#include "version.h"
}

using namespace llvm;
using namespace llvm::dwarf;

#ifndef LLVMTESTDEBUG
#define DEBUGASSERT(S) ((void)0)
#else
#define DEBUGASSERT(S) assert(S)
#endif


/// DirectoryAndFile - Extract the directory and file name from a path.  If no
/// directory is specified, then use the source working directory.
static void DirectoryAndFile(const std::string &FullPath,
                             std::string &Directory, std::string &FileName) {
  // Look for the directory slash.
  size_t Slash = FullPath.rfind('/');
  
  // If no slash
  if (Slash == std::string::npos) {
    // The entire path is the file name.
    Directory = "";
    FileName = FullPath;
  } else {
    // Separate the directory from the file name.
    Directory = FullPath.substr(0, Slash);
    FileName = FullPath.substr(Slash + 1);
  }
  
  // If no directory present then use source working directory.
  if (Directory.empty() || Directory[0] != '/') {
    Directory = std::string(get_src_pwd()) + "/" + Directory;
  }
}

/// NodeSizeInBits - Returns the size in bits stored in a tree node regardless
/// of whether the node is a TYPE or DECL.
static uint64_t NodeSizeInBits(tree Node) {
  if (TREE_CODE(Node) == ERROR_MARK) {
    return BITS_PER_WORD;
  } else if (TYPE_P(Node)) {
    if (TYPE_SIZE(Node) == NULL_TREE)
      return 0;
    else if (isInt64(TYPE_SIZE(Node), 1))
      return getINTEGER_CSTVal(TYPE_SIZE(Node));
    else
      return TYPE_ALIGN(Node);
  } else if (DECL_P(Node)) {
    if (DECL_SIZE(Node) == NULL_TREE)
      return 0;
    else if (isInt64(DECL_SIZE(Node), 1))
      return getINTEGER_CSTVal(DECL_SIZE(Node));
    else
      return DECL_ALIGN(Node);
  }
  
  return 0;
}

/// NodeAlignInBits - Returns the alignment in bits stored in a tree node
/// regardless of whether the node is a TYPE or DECL.
static uint64_t NodeAlignInBits(tree Node) {
  if (TREE_CODE(Node) == ERROR_MARK) return BITS_PER_WORD;
  if (TYPE_P(Node)) return TYPE_ALIGN(Node);
  if (DECL_P(Node)) return DECL_ALIGN(Node);
  return BITS_PER_WORD;
}

/// FieldType - Returns the type node of a structure member field.
///
static tree FieldType(tree Field) {
  if (TREE_CODE (Field) == ERROR_MARK) return integer_type_node;
  return getDeclaredType(Field);
}

/// GetNodeName - Returns the name stored in a node regardless of whether the
/// node is a TYPE or DECL.
static const char *GetNodeName(tree Node) {
  tree Name = NULL;
  
  if (DECL_P(Node)) {
    Name = DECL_NAME(Node);
  } else if (TYPE_P(Node)) {
    Name = TYPE_NAME(Node);
  }

  if (Name) {
    if (TREE_CODE(Name) == IDENTIFIER_NODE) {
      return IDENTIFIER_POINTER(Name);
    } else if (TREE_CODE(Name) == TYPE_DECL && DECL_NAME(Name) &&
               !DECL_IGNORED_P(Name)) {
      return IDENTIFIER_POINTER(DECL_NAME(Name));
    }
  }
  
  return "";
}

/// GetNodeLocation - Returns the location stored in a node  regardless of
/// whether the node is a TYPE or DECL.  UseStub is true if we should consider
/// the type stub as the actually location (ignored in struct/unions/enums.)
static expanded_location GetNodeLocation(tree Node, bool UseStub = true) {
  expanded_location Location = { NULL, 0 };

  if (Node == NULL_TREE)
    return Location;

  tree Name = NULL;
  
  if (DECL_P(Node)) {
    Name = DECL_NAME(Node);
  } else if (TYPE_P(Node)) {
    Name = TYPE_NAME(Node);
  }
  
  if (Name) {
    if (TYPE_STUB_DECL(Name)) {
      tree Stub = TYPE_STUB_DECL(Name);
      Location = expand_location(DECL_SOURCE_LOCATION(Stub));
    } else if (DECL_P(Name)) {
      Location = expand_location(DECL_SOURCE_LOCATION(Name));
    }
  }
  
  if (!Location.line) {
    if (UseStub && TYPE_STUB_DECL(Node)) {
      tree Stub = TYPE_STUB_DECL(Node);
      Location = expand_location(DECL_SOURCE_LOCATION(Stub));
    } else if (DECL_P(Node)) {
      Location = expand_location(DECL_SOURCE_LOCATION(Node));
    }
  }
  
  return Location;
}

static const char *getLinkageName(tree Node) {

  // Use llvm value name as linkage name if it is available.
  if (DECL_LLVM_SET_P(Node)) {
    Value *V = DECL_LLVM(Node);
    return V->getNameStart();
  }

  tree decl_name = DECL_NAME(Node);
  if (decl_name != NULL && IDENTIFIER_POINTER (decl_name) != NULL) {
    if (TREE_PUBLIC(Node) &&
        DECL_ASSEMBLER_NAME(Node) != DECL_NAME(Node) && 
        !DECL_ABSTRACT(Node)) {
      return IDENTIFIER_POINTER(DECL_ASSEMBLER_NAME(Node));
    } 
  }
  return "";
}

DebugInfo::DebugInfo(Module *m)
: M(m)
, DebugFactory(*m)
, CurFullPath("")
, CurLineNo(0)
, PrevFullPath("")
, PrevLineNo(0)
, PrevBB(NULL)
, RegionStack()
{}

/// EmitFunctionStart - Constructs the debug code for entering a function -
/// "llvm.dbg.func.start."
void DebugInfo::EmitFunctionStart(tree FnDecl, Function *Fn,
                                  BasicBlock *CurBB) {
  // Gather location information.
  expanded_location Loc = GetNodeLocation(FnDecl, false);
  const char *LinkageName = getLinkageName(FnDecl);

  DISubprogram SP = 
    DebugFactory.CreateSubprogram(findRegion(FnDecl),
                                  lang_hooks.dwarf_name(FnDecl, 0),
                                  lang_hooks.dwarf_name(FnDecl, 0),
                                  LinkageName,
                                  getOrCreateCompileUnit(Loc.file), CurLineNo,
                                  getOrCreateType(TREE_TYPE(FnDecl)),
                                  Fn->hasInternalLinkage(),
                                  true /*definition*/);

  DebugFactory.InsertSubprogramStart(SP, CurBB);

  // Push function on region stack.
  RegionStack.push_back(SP);
  RegionMap[FnDecl] = SP;
}

  /// findRegion - Find tree_node N's region.
DIDescriptor DebugInfo::findRegion(tree Node) {
  if (Node == NULL_TREE)
    return getOrCreateCompileUnit(main_input_filename);

  std::map<tree_node *, DIDescriptor>::iterator I = RegionMap.find(Node);
  if (I != RegionMap.end())
    return I->second;

  if (TYPE_P (Node)) {
    if (TYPE_CONTEXT (Node))
      return findRegion (TYPE_CONTEXT(Node));
  } else if (DECL_P (Node)) {
    tree decl = Node;
    tree context = NULL_TREE;
    if (TREE_CODE (decl) != FUNCTION_DECL || ! DECL_VINDEX (decl))
      context = DECL_CONTEXT (decl);
    else
      context = TYPE_MAIN_VARIANT
        (TREE_TYPE (TREE_VALUE (TYPE_ARG_TYPES (TREE_TYPE (decl)))));
    
    if (context && !TYPE_P (context))
      context = NULL_TREE;
    if (context != NULL_TREE)
      return findRegion(context);
  }

  // Otherwise main compile unit covers everything.
  return getOrCreateCompileUnit(main_input_filename);
}

/// EmitRegionStart- Constructs the debug code for entering a declarative
/// region - "llvm.dbg.region.start."
void DebugInfo::EmitRegionStart(BasicBlock *CurBB) {
  llvm::DIDescriptor D;
  if (!RegionStack.empty())
    D = RegionStack.back();
  D = DebugFactory.CreateBlock(D);
  RegionStack.push_back(D);
  DebugFactory.InsertRegionStart(D, CurBB);
}

/// EmitRegionEnd - Constructs the debug code for exiting a declarative
/// region - "llvm.dbg.region.end."
void DebugInfo::EmitRegionEnd(BasicBlock *CurBB, bool EndFunction) {
  assert(!RegionStack.empty() && "Region stack mismatch, stack empty!");
  DebugFactory.InsertRegionEnd(RegionStack.back(), CurBB);
  RegionStack.pop_back();
  // Blocks get erased; clearing these is needed for determinism, and also
  // a good idea if the next function gets inlined.
  if (EndFunction) {
    PrevBB = NULL;
    PrevLineNo = 0;
    PrevFullPath = NULL;
  }
}

/// EmitDeclare - Constructs the debug code for allocation of a new variable.
/// region - "llvm.dbg.declare."
void DebugInfo::EmitDeclare(tree decl, unsigned Tag, const char *Name,
                            tree type, Value *AI, BasicBlock *CurBB) {

  // Do not emit variable declaration info, for now.
  if (optimize)
    return;

  // Ignore compiler generated temporaries.
  if (DECL_IGNORED_P(decl))
    return;

  assert(!RegionStack.empty() && "Region stack mismatch, stack empty!");

  expanded_location Loc = GetNodeLocation(decl, false);

  // Construct variable.
  llvm::DIVariable D =
    DebugFactory.CreateVariable(Tag, RegionStack.back(), Name, 
                                getOrCreateCompileUnit(Loc.file),
                                Loc.line, getOrCreateType(type));

  // Insert an llvm.dbg.declare into the current block.
  DebugFactory.InsertDeclare(AI, D, CurBB);
}

/// EmitStopPoint - Emit a call to llvm.dbg.stoppoint to indicate a change of 
/// source line - "llvm.dbg.stoppoint."  Now enabled at -O.
void DebugInfo::EmitStopPoint(Function *Fn, BasicBlock *CurBB) {

  // Don't bother if things are the same as last time.
  if (PrevLineNo == CurLineNo &&
      PrevBB == CurBB &&
      (PrevFullPath == CurFullPath ||
       !strcmp(PrevFullPath, CurFullPath))) return;
  if (!CurFullPath[0] || CurLineNo == 0) return;
  
  // Update last state.
  PrevFullPath = CurFullPath;
  PrevLineNo = CurLineNo;
  PrevBB = CurBB;
  
  DebugFactory.InsertStopPoint(getOrCreateCompileUnit(CurFullPath), 
                               CurLineNo, 0 /*column no. */,
                               CurBB);
}

/// EmitGlobalVariable - Emit information about a global variable.
///
void DebugInfo::EmitGlobalVariable(GlobalVariable *GV, tree decl) {
  // Gather location information.
  expanded_location Loc = expand_location(DECL_SOURCE_LOCATION(decl));
  DIType TyD = getOrCreateType(TREE_TYPE(decl));
  std::string DispName = GV->getNameStr();
  if (DECL_NAME(decl)) {
    if (IDENTIFIER_POINTER(DECL_NAME(decl)))
      DispName = IDENTIFIER_POINTER(DECL_NAME(decl));
  }
    
  DebugFactory.CreateGlobalVariable(getOrCreateCompileUnit(Loc.file), 
                                    GV->getNameStr(), 
                                    DispName,
                                    getLinkageName(decl), 
                                    getOrCreateCompileUnit(Loc.file), Loc.line,
                                    TyD, GV->hasInternalLinkage(),
                                    true/*definition*/, GV);
}

/// createBasicType - Create BasicType.
DIType DebugInfo::createBasicType(tree type) {

  const char *TypeName = GetNodeName(type);
  uint64_t Size = NodeSizeInBits(type);
  uint64_t Align = NodeAlignInBits(type);

  unsigned Encoding = 0;
  
  switch (TREE_CODE(type)) {
  case INTEGER_TYPE:
    if (TYPE_STRING_FLAG (type)) {
      if (TYPE_UNSIGNED (type))
        Encoding = DW_ATE_unsigned_char;
      else
        Encoding = DW_ATE_signed_char;
    }
    else if (TYPE_UNSIGNED (type))
      Encoding = DW_ATE_unsigned;
    else
      Encoding = DW_ATE_signed;
    break;
  case REAL_TYPE:
    Encoding = DW_ATE_float;
    break;
  case COMPLEX_TYPE:
    Encoding = TREE_CODE(TREE_TYPE(type)) == REAL_TYPE ?
      DW_ATE_complex_float : DW_ATE_lo_user;
    break;
  case BOOLEAN_TYPE:
    Encoding = DW_ATE_boolean;
    break;
  default: { 
    DEBUGASSERT(0 && "Basic type case missing");
    Encoding = DW_ATE_signed;
    Size = BITS_PER_WORD;
    Align = BITS_PER_WORD;
    break;
  }
  }
  return 
    DebugFactory.CreateBasicType(getOrCreateCompileUnit(main_input_filename),
                                 TypeName, 
                                 getOrCreateCompileUnit(main_input_filename),
                                 0, Size, Align,
                                 0, 0, Encoding);
}

/// createMethodType - Create MethodType.
DIType DebugInfo::createMethodType(tree type) {

  llvm::SmallVector<llvm::DIDescriptor, 16> EltTys;
  
  // Add the result type at least.
  EltTys.push_back(getOrCreateType(TREE_TYPE(type)));
  
  // Set up remainder of arguments.
  for (tree arg = TYPE_ARG_TYPES(type); arg; arg = TREE_CHAIN(arg)) {
    tree formal_type = TREE_VALUE(arg);
    if (formal_type == void_type_node) break;
    EltTys.push_back(getOrCreateType(formal_type));
  }
  
  llvm::DIArray EltTypeArray =
    DebugFactory.GetOrCreateArray(&EltTys[0], EltTys.size());

  return DebugFactory.CreateCompositeType(llvm::dwarf::DW_TAG_subroutine_type,
                                          findRegion(type), "", 
                                          getOrCreateCompileUnit(NULL), 
                                          0, 0, 0, 0, 0,
                                          llvm::DIType(), EltTypeArray);
}

/// createPointerType - Create PointerType.
DIType DebugInfo::createPointerType(tree type) {

  DIType FromTy = getOrCreateType(TREE_TYPE(type));
  // type* and type&
  // FIXME: Should BLOCK_POINTER_TYP have its own DW_TAG?
  unsigned Tag = (TREE_CODE(type) == POINTER_TYPE ||
                  TREE_CODE(type) == BLOCK_POINTER_TYPE) ?
    DW_TAG_pointer_type :
    DW_TAG_reference_type;
  expanded_location Loc = GetNodeLocation(type);
  return  DebugFactory.CreateDerivedType(Tag, findRegion(type), "", 
                                         getOrCreateCompileUnit(NULL), 
                                         0 /*line no*/, 
                                         NodeSizeInBits(type),
                                         NodeAlignInBits(type),
                                         0 /*offset */, 
                                         0 /* flags */, 
                                         FromTy);
}

/// createArrayType - Create ArrayType.
DIType DebugInfo::createArrayType(tree type) {

  // type[n][m]...[p]
  if (TYPE_STRING_FLAG(type) && TREE_CODE(TREE_TYPE(type)) == INTEGER_TYPE){
    DEBUGASSERT(0 && "Don't support pascal strings");
    return DIType();
  }
  
  unsigned Tag = 0;
  
  if (TREE_CODE(type) == VECTOR_TYPE) 
    Tag = DW_TAG_vector_type;
  else
    Tag = DW_TAG_array_type;
  
  // Add the dimensions of the array.  FIXME: This loses CV qualifiers from
  // interior arrays, do we care?  Why aren't nested arrays represented the
  // obvious/recursive way?
  llvm::SmallVector<llvm::DIDescriptor, 8> Subscripts;
  
  // There will be ARRAY_TYPE nodes for each rank.  Followed by the derived
  // type.
  tree atype = type;
  tree EltTy = TREE_TYPE(atype);
  for (; TREE_CODE(atype) == ARRAY_TYPE; atype = TREE_TYPE(atype)) {
    tree Domain = TYPE_DOMAIN(atype);
    if (Domain) {
      // FIXME - handle dynamic ranges
      tree MinValue = TYPE_MIN_VALUE(Domain);
      tree MaxValue = TYPE_MAX_VALUE(Domain);
      if (MinValue && MaxValue &&
          isInt64(MinValue, 0) && isInt64(MaxValue, 0)) {
        uint64_t Low = getINTEGER_CSTVal(MinValue);
        uint64_t Hi = getINTEGER_CSTVal(MaxValue);
        Subscripts.push_back(DebugFactory.GetOrCreateSubrange(Low, Hi));
      }
    }
    EltTy = TREE_TYPE(atype);
  }
  
  llvm::DIArray SubscriptArray =
    DebugFactory.GetOrCreateArray(&Subscripts[0], Subscripts.size());
  expanded_location Loc = GetNodeLocation(type);
  return DebugFactory.CreateCompositeType(llvm::dwarf::DW_TAG_array_type,
                                          findRegion(type), "", 
                                          getOrCreateCompileUnit(Loc.file), 0, 
                                          NodeSizeInBits(type), 
                                          NodeAlignInBits(type), 0, 0,
                                          getOrCreateType(EltTy),
                                          SubscriptArray);
}

/// createEnumType - Create EnumType.
DIType DebugInfo::createEnumType(tree type) {
  // enum { a, b, ..., z };
  llvm::SmallVector<llvm::DIDescriptor, 32> Elements;
  
  if (TYPE_SIZE(type)) {
    for (tree Link = TYPE_VALUES(type); Link; Link = TREE_CHAIN(Link)) {
      tree EnumValue = TREE_VALUE(Link);
      int64_t Value = getINTEGER_CSTVal(EnumValue);
      const char *EnumName = IDENTIFIER_POINTER(TREE_PURPOSE(Link));
      Elements.push_back(DebugFactory.CreateEnumerator(EnumName, Value));
    }
  }
  
  llvm::DIArray EltArray =
    DebugFactory.GetOrCreateArray(&Elements[0], Elements.size());
  
  expanded_location Loc = { NULL, 0 };
  if (TYPE_SIZE(type)) 
    // Incomplete enums do not  have any location info.
    Loc = GetNodeLocation(TREE_CHAIN(type), false);

  return DebugFactory.CreateCompositeType(llvm::dwarf::DW_TAG_enumeration_type,
                                          findRegion(type), GetNodeName(type), 
                                          getOrCreateCompileUnit(Loc.file), 
                                          Loc.line,
                                          NodeSizeInBits(type), 
                                          NodeAlignInBits(type), 0, 0,
                                          llvm::DIType(), EltArray);
}

/// createStructType - Create StructType for struct or union or class.
DIType DebugInfo::createStructType(tree type) {

  // struct { a; b; ... z; }; | union { a; b; ... z; };
  unsigned Tag = TREE_CODE(type) == RECORD_TYPE ? DW_TAG_structure_type :
    DW_TAG_union_type;
  
  unsigned RunTimeLang = 0;
  if (TYPE_LANG_SPECIFIC (type)
      && lang_hooks.types.is_runtime_specific_type (type))
    {
      DICompileUnit CU = getOrCreateCompileUnit(main_input_filename);
      unsigned CULang = CU.getLanguage();
      switch (CULang) {
      case DW_LANG_ObjC_plus_plus :
        RunTimeLang = DW_LANG_ObjC_plus_plus;
        break;
      case DW_LANG_ObjC :
        RunTimeLang = DW_LANG_ObjC;
        break;
      case DW_LANG_C_plus_plus :
        RunTimeLang = DW_LANG_C_plus_plus;
        break;
      default:
        break;
      }
    }
    
  // Records and classes and unions can all be recursive.  To handle them,
  // we first generate a debug descriptor for the struct as a forward 
  // declaration. Then (if it is a definition) we go through and get debug 
  // info for all of its members.  Finally, we create a descriptor for the
  // complete type (which may refer to the forward decl if the struct is 
  // recursive) and replace all  uses of the forward declaration with the 
  // final definition. 
  expanded_location Loc = GetNodeLocation(TREE_CHAIN(type), false);
  llvm::DIType FwdDecl =
    DebugFactory.CreateCompositeType(Tag, 
                                     findRegion(type),
                                     GetNodeName(type),
                                     getOrCreateCompileUnit(Loc.file), 
                                     Loc.line, 
                                     0, 0, 0, llvm::DIType::FlagFwdDecl,
                                     llvm::DIType(), llvm::DIArray(),
                                     RunTimeLang);
  
  // forward declaration, 
  if (TYPE_SIZE(type) == 0) 
    return FwdDecl;
  
  // Insert into the TypeCache so that recursive uses will find it.
  TypeCache[type] =  FwdDecl;
  
  // Convert all the elements.
  llvm::SmallVector<llvm::DIDescriptor, 16> EltTys;
  
  if (tree binfo = TYPE_BINFO(type)) {
    VEC (tree, gc) *accesses = BINFO_BASE_ACCESSES (binfo);
    
    for (unsigned i = 0, e = BINFO_N_BASE_BINFOS(binfo); i != e; ++i) {
      tree BInfo = BINFO_BASE_BINFO(binfo, i);
      tree BInfoType = BINFO_TYPE (BInfo);
      DIType BaseClass = getOrCreateType(BInfoType);
      
      expanded_location loc = GetNodeLocation(type);
      // FIXME : name, size, align etc...
      DIType DTy = 
        DebugFactory.CreateDerivedType(DW_TAG_inheritance, 
                                       findRegion(type), "",
                                       llvm::DICompileUnit(), 0,0,0, 
                                       getINTEGER_CSTVal(BINFO_OFFSET(BInfo)),
                                       0, BaseClass);
      EltTys.push_back(DTy);
    }
  }
  
  // Now add members of this class.
  for (tree Member = TYPE_FIELDS(type); Member;
       Member = TREE_CHAIN(Member)) {
    // Should we skip.
    if (DECL_P(Member) && DECL_IGNORED_P(Member)) continue;

    if (TREE_CODE(Member) == FIELD_DECL) {
      
      if (DECL_FIELD_OFFSET(Member) == 0 ||
          TREE_CODE(DECL_FIELD_OFFSET(Member)) != INTEGER_CST)
        // FIXME: field with variable position, skip it for now.
        continue;
      
      /* Ignore nameless fields.  */
      if (DECL_NAME (Member) == NULL_TREE)
        continue;
      
      // Get the location of the member.
      expanded_location MemLoc = GetNodeLocation(Member, false);
      
      // Field type is the declared type of the field.
      tree FieldNodeType = FieldType(Member);
      DIType MemberType = getOrCreateType(FieldNodeType);
      const char *MemberName = GetNodeName(Member);
      unsigned Flags = 0;
      if (TREE_PROTECTED(Member))
        Flags = llvm::DIType::FlagProtected;
      else if (TREE_PRIVATE(Member))
        Flags = llvm::DIType::FlagPrivate;
      
      DIType DTy =
        DebugFactory.CreateDerivedType(DW_TAG_member, findRegion(Member),
                                       MemberName, 
                                       getOrCreateCompileUnit(MemLoc.file),
                                       MemLoc.line, NodeSizeInBits(Member),
                                       NodeAlignInBits(FieldNodeType),
                                       int_bit_position(Member), 
                                       Flags, MemberType);
      EltTys.push_back(DTy);
    } else {
      DEBUGASSERT(0 && "Unsupported member tree code!");
    }
  }
  
  for (tree Member = TYPE_METHODS(type); Member;
       Member = TREE_CHAIN(Member)) {
    
    if (DECL_ABSTRACT_ORIGIN (Member)) continue;
    if (DECL_ARTIFICIAL (Member)) continue;
    // In C++, TEMPLATE_DECLs are marked Ignored, and should be.
    if (DECL_P (Member) && DECL_IGNORED_P (Member)) continue;

    // Get the location of the member.
    expanded_location MemLoc = GetNodeLocation(Member, false);
    
    const char *MemberName = lang_hooks.dwarf_name(Member, 0);        
    const char *LinkageName = getLinkageName(Member);
    DIType SPTy = getOrCreateType(TREE_TYPE(Member));
    DISubprogram SP = 
      DebugFactory.CreateSubprogram(findRegion(Member), MemberName, MemberName,
                                    LinkageName, 
                                    getOrCreateCompileUnit(MemLoc.file),
                                    MemLoc.line, SPTy, false, false);
    EltTys.push_back(SP);
  }
  
  llvm::DIArray Elements =
    DebugFactory.GetOrCreateArray(&EltTys[0], EltTys.size());
  
  llvm::DIType RealDecl =
    DebugFactory.CreateCompositeType(Tag, findRegion(type),
                                     GetNodeName(type),
                                     getOrCreateCompileUnit(Loc.file),
                                     Loc.line, 
                                     NodeSizeInBits(type), NodeAlignInBits(type),
                                     0, 0, llvm::DIType(), Elements,
                                     RunTimeLang);
  
  // Now that we have a real decl for the struct, replace anything using the
  // old decl with the new one.  This will recursively update the debug info.
  FwdDecl.getGV()->replaceAllUsesWith(RealDecl.getGV());
  FwdDecl.getGV()->eraseFromParent();
  return RealDecl;
}

/// createVarinatType - Create variant type or return MainTy.
DIType DebugInfo::createVariantType(tree type, DIType MainTy) {
  
  DIType Ty;
  if (tree Name = TYPE_NAME(type)) {
    if (TREE_CODE(Name) == TYPE_DECL &&  DECL_ORIGINAL_TYPE(Name)) {
      expanded_location TypeDefLoc = GetNodeLocation(Name);
      Ty = DebugFactory.CreateDerivedType(DW_TAG_typedef, findRegion(type),
                                          GetNodeName(Name), 
                                          getOrCreateCompileUnit(TypeDefLoc.file),
                                          TypeDefLoc.line,
                                          0 /*size*/,
                                          0 /*align*/,
                                          0 /*offset */, 
                                          0 /*flags*/, 
                                          MainTy);
      // Set the slot early to prevent recursion difficulties.
      TypeCache[type] = Ty;
      return Ty;
    }
  }

  if (TYPE_VOLATILE(type)) {
    Ty = DebugFactory.CreateDerivedType(DW_TAG_volatile_type, 
                                        findRegion(type), "", 
                                        getOrCreateCompileUnit(NULL), 
                                        0 /*line no*/, 
                                        NodeSizeInBits(type),
                                        NodeAlignInBits(type),
                                        0 /*offset */, 
                                        0 /* flags */, 
                                        MainTy);
    MainTy = Ty;
  }

  if (TYPE_READONLY(type)) 
    Ty =  DebugFactory.CreateDerivedType(DW_TAG_const_type, 
                                         findRegion(type), "", 
                                         getOrCreateCompileUnit(NULL), 
                                         0 /*line no*/, 
                                         NodeSizeInBits(type),
                                         NodeAlignInBits(type),
                                         0 /*offset */, 
                                         0 /* flags */, 
                                         MainTy);
  
  if (TYPE_VOLATILE(type) || TYPE_READONLY(type)) {
    TypeCache[type] = Ty;
    return Ty;
  }

  // If, for some reason, main type varaint type is seen then use it.
  return MainTy;
}

/// getOrCreateType - Get the type from the cache or create a new type if
/// necessary.
DIType DebugInfo::getOrCreateType(tree type) {
  DEBUGASSERT(type != NULL_TREE && type != error_mark_node &&
              "Not a type.");
  if (type == NULL_TREE || type == error_mark_node) return DIType();

  // Should only be void if a pointer/reference/return type.  Returning NULL
  // allows the caller to produce a non-derived type.
  if (TREE_CODE(type) == VOID_TYPE) return DIType();
  
  // Check to see if the compile unit already has created this type.
  DIType &Slot = TypeCache[type];
  if (!Slot.isNull())
    return Slot;
  
  DIType MainTy;
  if (type != TYPE_MAIN_VARIANT(type)) {
    if (TYPE_NEXT_VARIANT(type) && type != TYPE_NEXT_VARIANT(type))
      MainTy = getOrCreateType(TYPE_NEXT_VARIANT(type));
    else if (TYPE_MAIN_VARIANT(type))
      MainTy = getOrCreateType(TYPE_MAIN_VARIANT(type));
  }

  DIType Ty = createVariantType(type, MainTy);
  if (!Ty.isNull())
    return Ty;

  // Work out details of type.
  switch (TREE_CODE(type)) {
    case ERROR_MARK:
    case LANG_TYPE:
    case TRANSLATION_UNIT_DECL:
    default: {
      DEBUGASSERT(0 && "Unsupported type");
      return DIType();
    }
    
    case POINTER_TYPE:
    case REFERENCE_TYPE:
    case BLOCK_POINTER_TYPE:
      Ty = createPointerType(type);
      break;
    
    case OFFSET_TYPE: {
      // gen_type_die(TYPE_OFFSET_BASETYPE(type), context_die);
      // gen_type_die(TREE_TYPE(type), context_die);
      // gen_ptr_to_mbr_type_die(type, context_die);
      break;
    }

    case FUNCTION_TYPE:
    case METHOD_TYPE: 
      Ty = createMethodType(type);
      break;
      
    case VECTOR_TYPE:
    case ARRAY_TYPE: 
      Ty = createArrayType(type);
      break;
    
    case ENUMERAL_TYPE: 
      Ty = createEnumType(type);
      break;
    
    case RECORD_TYPE:
    case QUAL_UNION_TYPE:
    case UNION_TYPE: 
      Ty = createStructType(type);
      break;

    case INTEGER_TYPE:
    case REAL_TYPE:   
    case COMPLEX_TYPE:
    case BOOLEAN_TYPE:
      Ty = createBasicType(type);
      break;
  }
  TypeCache[type] = Ty;
  return Ty;
}

/// Initialize - Initialize debug info by creating compile unit for
/// main_input_filename. This must be invoked after language dependent
/// initialization is done.
void DebugInfo::Initialize() {

  // Each input file is encoded as a separate compile unit in LLVM
  // debugging information output. However, many target specific tool chains
  // prefer to encode only one compile unit in an object file. In this 
  // situation, the LLVM code generator will include  debugging information
  // entities in the compile unit that is marked as main compile unit. The 
  // code generator accepts maximum one main compile unit per module. If a
  // module does not contain any main compile unit then the code generator 
  // will emit multiple compile units in the output object file.
  getOrCreateCompileUnit(main_input_filename, true);
}

/// getOrCreateCompileUnit - Get the compile unit from the cache or 
/// create a new one if necessary.
DICompileUnit DebugInfo::getOrCreateCompileUnit(const char *FullPath,
                                                bool isMain) {
  if (!FullPath)
    FullPath = main_input_filename;
  GlobalVariable *&CU = CUCache[FullPath];
  if (CU)
    return DICompileUnit(CU);

  // Get source file information.
  std::string Directory;
  std::string FileName;
  DirectoryAndFile(FullPath, Directory, FileName);
  
  // Set up Language number.
  unsigned LangTag;
  const std::string LanguageName(lang_hooks.name);
  if (LanguageName == "GNU C")
    LangTag = DW_LANG_C89;
  else if (LanguageName == "GNU C++")
    LangTag = DW_LANG_C_plus_plus;
  else if (LanguageName == "GNU Ada")
    LangTag = DW_LANG_Ada95;
  else if (LanguageName == "GNU F77")
    LangTag = DW_LANG_Fortran77;
  else if (LanguageName == "GNU Pascal")
    LangTag = DW_LANG_Pascal83;
  else if (LanguageName == "GNU Java")
    LangTag = DW_LANG_Java;
  else if (LanguageName == "GNU Objective-C")
    LangTag = DW_LANG_ObjC;
  else if (LanguageName == "GNU Objective-C++") 
    LangTag = DW_LANG_ObjC_plus_plus;
  else
    LangTag = DW_LANG_C89;

   const char *Flags = "";
   // Do this only when RC_DEBUG_OPTIONS environment variable is set to
   // a nonempty string. This is intended only for internal Apple use.
   char * debugopt = getenv("RC_DEBUG_OPTIONS");
   if (debugopt && debugopt[0])
     Flags = get_arguments();

   // flag_objc_abi represents Objective-C runtime version number. It is zero
   // for all other language.
   unsigned ObjcRunTimeVer = 0;
   if (flag_objc_abi != 0 && flag_objc_abi != -1)
     ObjcRunTimeVer = flag_objc_abi;
   DICompileUnit NewCU = DebugFactory.CreateCompileUnit(LangTag, FileName, 
                                                        Directory, 
                                                        version_string, isMain,
                                                        optimize, Flags,
                                                        ObjcRunTimeVer);
  CU = NewCU.getGV();
  return NewCU;
}

/* LLVM LOCAL end (ENTIRE FILE!)  */
