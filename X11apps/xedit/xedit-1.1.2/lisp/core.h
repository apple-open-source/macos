/*
 * Copyright (c) 2001 by The XFree86 Project, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *  
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the XFree86 Project shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from the
 * XFree86 Project.
 *
 * Author: Paulo César Pereira de Andrade
 */

/* $XFree86: xc/programs/xedit/lisp/core.h,v 1.35tsi Exp $ */

#ifndef Lisp_core_h
#define Lisp_core_h

#include "lisp/internal.h"

void LispCoreInit(void);

LispObj *Lisp_Acons(LispBuiltin*);
LispObj *Lisp_Adjoin(LispBuiltin*);
LispObj *Lisp_Append(LispBuiltin*);
LispObj *Lisp_And(LispBuiltin*);
LispObj *Lisp_Aref(LispBuiltin*);
LispObj *Lisp_Assoc(LispBuiltin*);
LispObj *Lisp_AssocIf(LispBuiltin*);
LispObj *Lisp_AssocIfNot(LispBuiltin*);
LispObj *Lisp_Apply(LispBuiltin*);
LispObj *Lisp_Atom(LispBuiltin*);
LispObj *Lisp_Block(LispBuiltin*);
LispObj *Lisp_Boundp(LispBuiltin*);
LispObj *Lisp_Butlast(LispBuiltin*);
LispObj *Lisp_Nbutlast(LispBuiltin*);
LispObj *Lisp_Car(LispBuiltin*);
LispObj *Lisp_Case(LispBuiltin*);
LispObj *Lisp_Catch(LispBuiltin*);
LispObj *Lisp_Cdr(LispBuiltin*);
LispObj *Lisp_C_r(LispBuiltin*);
LispObj *Lisp_Coerce(LispBuiltin*);
LispObj *Lisp_Cond(LispBuiltin*);
LispObj *Lisp_Cons(LispBuiltin*);
LispObj *Lisp_Consp(LispBuiltin*);
LispObj *Lisp_Constantp(LispBuiltin*);
LispObj *Lisp_CopyAlist(LispBuiltin*);
LispObj *Lisp_CopyList(LispBuiltin*);
LispObj *Lisp_CopyTree(LispBuiltin*);
LispObj *Lisp_Defconstant(LispBuiltin*);
LispObj *Lisp_Defmacro(LispBuiltin*);
LispObj *Lisp_Defun(LispBuiltin*);
LispObj *Lisp_Defsetf(LispBuiltin*);
LispObj *Lisp_Defparameter(LispBuiltin*);
LispObj *Lisp_Defvar(LispBuiltin*);
LispObj *Lisp_Delete(LispBuiltin*);
LispObj *Lisp_DeleteDuplicates(LispBuiltin*);
LispObj *Lisp_DeleteIf(LispBuiltin*);
LispObj *Lisp_DeleteIfNot(LispBuiltin*);
LispObj *Lisp_Do(LispBuiltin*);
LispObj *Lisp_DoP(LispBuiltin*);
LispObj *Lisp_Documentation(LispBuiltin*);
LispObj *Lisp_DoList(LispBuiltin*);
LispObj *Lisp_DoTimes(LispBuiltin*);
LispObj *Lisp_Elt(LispBuiltin*);
LispObj *Lisp_Endp(LispBuiltin*);
LispObj *Lisp_Eq(LispBuiltin*);
LispObj *Lisp_Eql(LispBuiltin*);
LispObj *Lisp_Equal(LispBuiltin*);
LispObj *Lisp_Equalp(LispBuiltin*);
LispObj *Lisp_Error(LispBuiltin*);
LispObj *Lisp_Eval(LispBuiltin*);
LispObj *Lisp_Every(LispBuiltin*);
LispObj *Lisp_Some(LispBuiltin*);
LispObj *Lisp_Notevery(LispBuiltin*);
LispObj *Lisp_Notany(LispBuiltin*);
LispObj *Lisp_Fboundp(LispBuiltin*);
LispObj *Lisp_Find(LispBuiltin*);
LispObj *Lisp_FindIf(LispBuiltin*);
LispObj *Lisp_FindIfNot(LispBuiltin*);
LispObj *Lisp_Fill(LispBuiltin*);
LispObj *Lisp_Fmakunbound(LispBuiltin*);
LispObj *Lisp_Functionp(LispBuiltin*);
LispObj *Lisp_Funcall(LispBuiltin*);
LispObj *Lisp_Gc(LispBuiltin*);
LispObj *Lisp_Gensym(LispBuiltin*);
LispObj *Lisp_Get(LispBuiltin*);
LispObj *Lisp_Getenv(LispBuiltin*);
LispObj *Lisp_Go(LispBuiltin*);
LispObj *Lisp_If(LispBuiltin*);
LispObj *Lisp_IgnoreErrors(LispBuiltin*);
LispObj *Lisp_Intersection(LispBuiltin*);
LispObj *Lisp_Nintersection(LispBuiltin*);
LispObj *Lisp_Keywordp(LispBuiltin*);
LispObj *Lisp_Lambda(LispBuiltin*);
LispObj *Lisp_Last(LispBuiltin*);
LispObj *Lisp_Let(LispBuiltin*);
LispObj *Lisp_Length(LispBuiltin*);
LispObj *Lisp_LetP(LispBuiltin*);
LispObj *Lisp_List(LispBuiltin*);
LispObj *Lisp_ListLength(LispBuiltin*);
LispObj *Lisp_ListP(LispBuiltin*);
LispObj *Lisp_Listp(LispBuiltin*);
LispObj *Lisp_Loop(LispBuiltin*);
LispObj *Lisp_MakeArray(LispBuiltin*);
LispObj *Lisp_MakeList(LispBuiltin*);
LispObj *Lisp_MakeSymbol(LispBuiltin*);
LispObj *Lisp_Makunbound(LispBuiltin*);
LispObj *Lisp_Mapc(LispBuiltin*);
LispObj *Lisp_Mapcar(LispBuiltin*);
LispObj *Lisp_Mapcan(LispBuiltin*);
LispObj *Lisp_Mapl(LispBuiltin*);
LispObj *Lisp_Maplist(LispBuiltin*);
LispObj *Lisp_Mapcon(LispBuiltin*);
LispObj *Lisp_Member(LispBuiltin*);
LispObj *Lisp_MemberIf(LispBuiltin*);
LispObj *Lisp_MemberIfNot(LispBuiltin*);
LispObj *Lisp_MultipleValueBind(LispBuiltin*);
LispObj *Lisp_MultipleValueCall(LispBuiltin*);
LispObj *Lisp_MultipleValueProg1(LispBuiltin*);
LispObj *Lisp_MultipleValueList(LispBuiltin*);
LispObj *Lisp_MultipleValueSetq(LispBuiltin*);
LispObj *Lisp_Nconc(LispBuiltin*);
LispObj *Lisp_Nreverse(LispBuiltin*);
LispObj *Lisp_NsetDifference(LispBuiltin*);
LispObj *Lisp_Nsubstitute(LispBuiltin*);
LispObj *Lisp_NsubstituteIf(LispBuiltin*);
LispObj *Lisp_NsubstituteIfNot(LispBuiltin*);
LispObj *Lisp_Nth(LispBuiltin*);
LispObj *Lisp_Nthcdr(LispBuiltin*);
LispObj *Lisp_NthValue(LispBuiltin*);
LispObj *Lisp_Null(LispBuiltin*);
LispObj *Lisp_Or(LispBuiltin*);
LispObj *Lisp_Pairlis(LispBuiltin*);
LispObj *Lisp_Pop(LispBuiltin*);
LispObj *Lisp_Position(LispBuiltin*);
LispObj *Lisp_PositionIf(LispBuiltin*);
LispObj *Lisp_PositionIfNot(LispBuiltin*);
LispObj *Lisp_Proclaim(LispBuiltin*);
LispObj *Lisp_Prog1(LispBuiltin*);
LispObj *Lisp_Prog2(LispBuiltin*);
LispObj *Lisp_Progn(LispBuiltin*);
LispObj *Lisp_Progv(LispBuiltin*);
LispObj *Lisp_Provide(LispBuiltin*);
LispObj *Lisp_Push(LispBuiltin*);
LispObj *Lisp_Pushnew(LispBuiltin*);
LispObj *Lisp_Quit(LispBuiltin*);
LispObj *Lisp_Quote(LispBuiltin*);
LispObj *Lisp_Remove(LispBuiltin*);
LispObj *Lisp_RemoveDuplicates(LispBuiltin*);
LispObj *Lisp_RemoveIf(LispBuiltin*);
LispObj *Lisp_RemoveIfNot(LispBuiltin*);
LispObj *Lisp_Remprop(LispBuiltin*);
LispObj *Lisp_Replace(LispBuiltin*);
LispObj *Lisp_Return(LispBuiltin*);
LispObj *Lisp_ReturnFrom(LispBuiltin*);
LispObj *Lisp_Reverse(LispBuiltin*);
LispObj *Lisp_Rplaca(LispBuiltin*);
LispObj *Lisp_Rplacd(LispBuiltin*);
LispObj *Lisp_Search(LispBuiltin*);
LispObj *Lisp_Setenv(LispBuiltin*);
LispObj *Lisp_Set(LispBuiltin*);
LispObj *Lisp_SetDifference(LispBuiltin*);
LispObj *Lisp_SetExclusiveOr(LispBuiltin*);
LispObj *Lisp_NsetExclusiveOr(LispBuiltin*);
LispObj *Lisp_Setf(LispBuiltin*);
LispObj *Lisp_Psetf(LispBuiltin*);
LispObj *Lisp_SetQ(LispBuiltin*);
LispObj *Lisp_Psetq(LispBuiltin*);
LispObj *Lisp_Sleep(LispBuiltin*);
LispObj *Lisp_Sort(LispBuiltin*);
LispObj *Lisp_Subseq(LispBuiltin*);
LispObj *Lisp_Subsetp(LispBuiltin*);
LispObj *Lisp_Substitute(LispBuiltin*);
LispObj *Lisp_SubstituteIf(LispBuiltin*);
LispObj *Lisp_SubstituteIfNot(LispBuiltin*);
LispObj *Lisp_Symbolp(LispBuiltin*);
LispObj *Lisp_SymbolFunction(LispBuiltin*);
LispObj *Lisp_SymbolName(LispBuiltin*);
LispObj *Lisp_SymbolPackage(LispBuiltin*);
LispObj *Lisp_SymbolPlist(LispBuiltin*);
LispObj *Lisp_SymbolValue(LispBuiltin*);
LispObj *Lisp_Tagbody(LispBuiltin*);
LispObj *Lisp_Throw(LispBuiltin*);
LispObj *Lisp_The(LispBuiltin*);
LispObj *Lisp_TreeEqual(LispBuiltin*);
LispObj *Lisp_Typep(LispBuiltin*);
LispObj *Lisp_Union(LispBuiltin*);
LispObj *Lisp_Nunion(LispBuiltin*);
LispObj *Lisp_Unless(LispBuiltin*);
LispObj *Lisp_Until(LispBuiltin*);
LispObj *Lisp_Unsetenv(LispBuiltin*);
LispObj *Lisp_UnwindProtect(LispBuiltin*);
LispObj *Lisp_Values(LispBuiltin*);
LispObj *Lisp_ValuesList(LispBuiltin*);
LispObj *Lisp_Vector(LispBuiltin*);
LispObj *Lisp_When(LispBuiltin*);
LispObj *Lisp_While(LispBuiltin*);
LispObj *Lisp_XeditEltStore(LispBuiltin*);
LispObj *Lisp_XeditPut(LispBuiltin*);
LispObj *Lisp_XeditSetSymbolPlist(LispBuiltin*);
LispObj *Lisp_XeditVectorStore(LispBuiltin*);
LispObj *Lisp_XeditDocumentationStore(LispBuiltin*);

#endif
