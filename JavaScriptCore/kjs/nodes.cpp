// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 */

#include "nodes.h"

//#include <iostream>
#include <math.h>
#include <assert.h>
#ifdef KJS_DEBUG_MEM
#include <stdio.h>
#include <typeinfo>
#endif

#include "collector.h"
#include "context.h"
#include "debugger.h"
#include "function_object.h"
#include "internal.h"
#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"
#include "lexer.h"
#include "operations.h"
#include "ustring.h"

using namespace KJS;

#define KJS_BREAKPOINT \
  if (Debugger::debuggersPresent > 0 && !hitStatement(exec)) \
    return Completion(Normal);

#define KJS_ABORTPOINT \
  if (Debugger::debuggersPresent > 0 && \
      exec->dynamicInterpreter()->imp()->debugger() && \
      exec->dynamicInterpreter()->imp()->debugger()->imp()->aborted()) \
    return Completion(Normal);

#define KJS_CHECKEXCEPTION \
  if (exec->hadException()) { \
    setExceptionDetailsIfNeeded(exec); \
    return Completion(Throw, exec->exception()); \
  } \
  if (Collector::outOfMemory()) \
    return Completion(Throw, Error::create(exec,GeneralError,"Out of memory"));

#define KJS_CHECKEXCEPTIONVALUE \
  if (exec->hadException()) { \
    setExceptionDetailsIfNeeded(exec); \
    return exec->exception(); \
  } \
  if (Collector::outOfMemory()) \
    return Undefined(); // will be picked up by KJS_CHECKEXCEPTION

#define KJS_CHECKEXCEPTIONREFERENCE \
  if (exec->hadException()) { \
    setExceptionDetailsIfNeeded(exec); \
    return Reference::makeValueReference(Undefined());; \
  } \
  if (Collector::outOfMemory()) \
    return Reference::makeValueReference(Undefined()); // will be picked up by KJS_CHECKEXCEPTION

#define KJS_CHECKEXCEPTIONLIST \
  if (exec->hadException()) { \
    setExceptionDetailsIfNeeded(exec); \
    return List(); \
  } \
  if (Collector::outOfMemory()) \
    return List(); // will be picked up by KJS_CHECKEXCEPTION

#ifdef KJS_DEBUG_MEM
std::list<Node *> * Node::s_nodes = 0L;
#endif

// ------------------------------ Node -----------------------------------------

Node::Node()
{
  line = Lexer::curr()->lineNo();
  sourceURL = Lexer::curr()->sourceURL();
  m_refcount = 0;
}

Node::~Node()
{
}

Reference Node::evaluateReference(ExecState *exec)
{
  Value v = evaluate(exec);
  KJS_CHECKEXCEPTIONREFERENCE
  return Reference::makeValueReference(v);
}

#ifdef KJS_DEBUG_MEM
void Node::finalCheck()
{
  fprintf( stderr, "Node::finalCheck(): list count       : %d\n", (int)s_nodes.size() );
  std::list<Node *>::iterator it = s_nodes->begin();
  for ( uint i = 0; it != s_nodes->end() ; ++it, ++i )
    fprintf( stderr, "[%d] Still having node %p (%s) (refcount %d)\n", i, (void*)*it, typeid( **it ).name(), (*it)->refcount );
  delete s_nodes;
  s_nodes = 0L;
}
#endif

Value Node::throwError(ExecState *exec, ErrorType e, const char *msg)
{
  Object err = Error::create(exec, e, msg, lineNo(), sourceId(), &sourceURL);
  exec->setException(err);
  return err;
}

Value Node::throwError(ExecState *exec, ErrorType e, const char *msg, Value v, Node *expr)
{
  char *vStr = strdup(v.toString(exec).ascii());
  char *exprStr = strdup(expr->toString().ascii());
  
  int length =  strlen(msg) - 4 /* two %s */ + strlen(vStr) + strlen(exprStr) + 1 /* null terminator */;
  char *str = new char[length];
  sprintf(str, msg, vStr, exprStr);
  free(vStr);
  free(exprStr);

  Value result = throwError(exec, e, str);
  delete [] str;
  
  return result;
}


Value Node::throwError(ExecState *exec, ErrorType e, const char *msg, Identifier label)
{
  const char *l = label.ascii();
  int length = strlen(msg) - 2 /* %s */ + strlen(l) + 1 /* null terminator */;
  char *message = new char[length];
  sprintf(message, msg, l);

  Value result = throwError(exec, e, message);
  delete [] message;

  return result;
}

void Node::setExceptionDetailsIfNeeded(ExecState *exec)
{
    if (exec->exception().isA(ObjectType)) {
        ObjectImp *exception = static_cast<ObjectImp *>(exec->exception().imp());
        if (!exception->hasProperty(exec, "line") || !exception->hasProperty(exec, "sourceURL")) {
            exception->put(exec, "line", Number(line));
            exception->put(exec, "sourceURL", String(sourceURL));
        }
    }
}

// ------------------------------ StatementNode --------------------------------
StatementNode::StatementNode() : l0(-1), l1(-1), sid(-1), breakPoint(false)
{
}

void StatementNode::setLoc(int line0, int line1, int sourceId)
{
    l0 = line0;
    l1 = line1;
    sid = sourceId;
}

// return true if the debugger wants us to stop at this point
bool StatementNode::hitStatement(ExecState *exec)
{
  Debugger *dbg = exec->dynamicInterpreter()->imp()->debugger();
  if (dbg)
    return dbg->atStatement(exec,sid,l0,l1);
  else
    return true; // continue
}

// return true if the debugger wants us to stop at this point
bool StatementNode::abortStatement(ExecState *exec)
{
  Debugger *dbg = exec->dynamicInterpreter()->imp()->debugger();
  if (dbg)
    return dbg->imp()->aborted();
  else
    return false;
}

void StatementNode::processFuncDecl(ExecState *exec)
{
}

// ------------------------------ NullNode -------------------------------------

Value NullNode::evaluate(ExecState */*exec*/)
{
  return Null();
}

// ------------------------------ BooleanNode ----------------------------------

Value BooleanNode::evaluate(ExecState */*exec*/)
{
  return Value(value);
}

// ------------------------------ NumberNode -----------------------------------

Value NumberNode::evaluate(ExecState */*exec*/)
{
  return Value(value);
}

// ------------------------------ StringNode -----------------------------------

Value StringNode::evaluate(ExecState */*exec*/)
{
  return value;
}

// ------------------------------ RegExpNode -----------------------------------

Value RegExpNode::evaluate(ExecState *exec)
{
  List list;
  String p(pattern);
  String f(flags);
  list.append(p);
  list.append(f);

  Object reg = exec->lexicalInterpreter()->imp()->builtinRegExp();
  return reg.construct(exec,list);
}

// ------------------------------ ThisNode -------------------------------------

// ECMA 11.1.1
Value ThisNode::evaluate(ExecState *exec)
{
  return exec->context().imp()->thisValue();
}

// ------------------------------ ResolveNode ----------------------------------

// ECMA 11.1.2 & 10.1.4
Value ResolveNode::evaluate(ExecState *exec)
{
  return evaluateReference(exec).getValue(exec);
}

Reference ResolveNode::evaluateReference(ExecState *exec)
{
  ScopeChain chain = exec->context().imp()->scopeChain();

  while (!chain.isEmpty()) {
    ObjectImp *o = chain.top();

    //cout << "Resolve: looking at '" << ident.ascii() << "'"
    //     << " in " << (void*)o << " " << o->classInfo()->className << endl;
    if (o->hasProperty(exec,ident)) {
      //cout << "Resolve: FOUND '" << ident.ascii() << "'"
      //     << " in " << (void*)o << " " << o->classInfo()->className << endl;
      return Reference(o, ident);
    }
    
    chain.pop();
  }

  // identifier not found
  //cout << "Resolve: didn't find '" << ident.ascii() << "'" << endl;
  return Reference(Null(), ident);
}


// ------------------------------ GroupNode ------------------------------------

// ECMA 11.1.6
Value GroupNode::evaluate(ExecState *exec)
{
  return group->evaluate(exec);
}

Reference GroupNode::evaluateReference(ExecState *exec)
{
  return group->evaluateReference(exec);
}

// ------------------------------ ElementNode ----------------------------------

// ECMA 11.1.4
Value ElementNode::evaluate(ExecState *exec)
{
  Object array = exec->lexicalInterpreter()->builtinArray().construct(exec, List::empty());
  int length = 0;
  for (ElementNode *n = this; n; n = n->list.get()) {
    Value val = n->node->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    length += n->elision;
    array.put(exec, length++, val);
  }
  return array;
}

// ------------------------------ ArrayNode ------------------------------------

// ECMA 11.1.4
Value ArrayNode::evaluate(ExecState *exec)
{
  Object array;
  int length;

  if (element) {
    array = Object(static_cast<ObjectImp*>(element->evaluate(exec).imp()));
    KJS_CHECKEXCEPTIONVALUE
    length = opt ? array.get(exec,lengthPropertyName).toInt32(exec) : 0;
  } else {
    Value newArr = exec->lexicalInterpreter()->builtinArray().construct(exec,List::empty());
    array = Object(static_cast<ObjectImp*>(newArr.imp()));
    length = 0;
  }

  if (opt)
    array.put(exec,lengthPropertyName, Value(elision + length), DontEnum | DontDelete);

  return array;
}

// ------------------------------ ObjectLiteralNode ----------------------------

// ECMA 11.1.5
Value ObjectLiteralNode::evaluate(ExecState *exec)
{
  if (list)
    return list->evaluate(exec);

  return exec->lexicalInterpreter()->builtinObject().construct(exec,List::empty());
}

// ------------------------------ PropertyValueNode ----------------------------

// ECMA 11.1.5
Value PropertyValueNode::evaluate(ExecState *exec)
{
  Object obj = exec->lexicalInterpreter()->builtinObject().construct(exec, List::empty());
  
  for (PropertyValueNode *p = this; p; p = p->list.get()) {
    Value n = p->name->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    Value v = p->assign->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE

    obj.put(exec, Identifier(n.toString(exec)), v);
  }

  return obj;
}

// ------------------------------ PropertyNode ---------------------------------

// ECMA 11.1.5
Value PropertyNode::evaluate(ExecState */*exec*/)
{
  Value s;

  if (str.isNull()) {
    s = String(UString::from(numeric));
  } else {
    s = String(str.ustring());
  }

  return s;
}

// ------------------------------ AccessorNode1 --------------------------------

// ECMA 11.2.1a
Value AccessorNode1::evaluate(ExecState *exec)
{
  return evaluateReference(exec).getValue(exec);
}

Reference AccessorNode1::evaluateReference(ExecState *exec)
{
  Value v1 = expr1->evaluate(exec);
  KJS_CHECKEXCEPTIONREFERENCE
  Value v2 = expr2->evaluate(exec);
  KJS_CHECKEXCEPTIONREFERENCE
  Object o = v1.toObject(exec);
  unsigned i;
  if (v2.toUInt32(i))
    return Reference(o, i);
  String s = v2.toString(exec);
  return Reference(o, Identifier(s.value()));
}


// ------------------------------ AccessorNode2 --------------------------------

// ECMA 11.2.1b
Value AccessorNode2::evaluate(ExecState *exec)
{
  return evaluateReference(exec).getValue(exec);
}

Reference AccessorNode2::evaluateReference(ExecState *exec)
{
  Value v = expr->evaluate(exec);
  KJS_CHECKEXCEPTIONREFERENCE
  Object o = v.toObject(exec);
  return Reference(o, ident);
}

// ------------------------------ ArgumentListNode -----------------------------

Value ArgumentListNode::evaluate(ExecState */*exec*/)
{
  assert(0);
  return Value(); // dummy, see evaluateList()
}

// ECMA 11.2.4
List ArgumentListNode::evaluateList(ExecState *exec)
{
  List l;

  for (ArgumentListNode *n = this; n; n = n->list.get()) {
    Value v = n->expr->evaluate(exec);
    KJS_CHECKEXCEPTIONLIST
    l.append(v);
  }

  return l;
}

// ------------------------------ ArgumentsNode --------------------------------

Value ArgumentsNode::evaluate(ExecState */*exec*/)
{
  assert(0);
  return Value(); // dummy, see evaluateList()
}

// ECMA 11.2.4
List ArgumentsNode::evaluateList(ExecState *exec)
{
  if (!list)
    return List();

  return list->evaluateList(exec);
}

// ------------------------------ NewExprNode ----------------------------------

// ECMA 11.2.2

Value NewExprNode::evaluate(ExecState *exec)
{
  Value v = expr->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  List argList;
  if (args) {
    argList = args->evaluateList(exec);
    KJS_CHECKEXCEPTIONVALUE
  }

  if (v.type() != ObjectType) {
    return throwError(exec, TypeError, "Value %s (result of expression %s) is not an object. Cannot be used with new.", v, expr.get());
  }

  Object constr = Object(static_cast<ObjectImp*>(v.imp()));
  if (!constr.implementsConstruct()) {
    return throwError(exec, TypeError, "Value %s (result of expression %s) is not a constructor. Cannot be used with new.", v, expr.get());
  }

  Value res = constr.construct(exec,argList);

  return res;
}

// ------------------------------ FunctionCallNode -----------------------------

// ECMA 11.2.3
Value FunctionCallNode::evaluate(ExecState *exec)
{
  Reference ref = expr->evaluateReference(exec);
  KJS_CHECKEXCEPTIONVALUE

  List argList = args->evaluateList(exec);

  KJS_CHECKEXCEPTIONVALUE

  Value v = ref.getValue(exec);

  if (v.type() != ObjectType) {
    return throwError(exec, TypeError, "Value %s (result of expression %s) is not object.", v, expr.get());
  }

  ObjectImp *func = static_cast<ObjectImp*>(v.imp());

  if (!func->implementsCall()) {
    return throwError(exec, TypeError, "Object %s (result of expression %s) does not allow calls.", v, expr.get());
  }

  ObjectImp *thisObjImp = 0;
  ValueImp *thisValImp = ref.baseIfMutable();
  if (thisValImp && thisValImp->type() == ObjectType && !static_cast<ObjectImp *>(thisValImp)->inherits(&ActivationImp::info))
    thisObjImp = static_cast<ObjectImp *>(thisValImp);

  if (!thisObjImp) {
    // ECMA 11.2.3 says that in this situation the this value should be null.
    // However, section 10.2.3 says that in the case where the value provided
    // by the caller is null, the global object should be used. It also says
    // that the section does not apply to interal functions, but for simplicity
    // of implementation we use the global object anyway here. This guarantees
    // that in host objects you always get a valid object for this.
    thisObjImp = exec->dynamicInterpreter()->globalObject().imp();
  }

  Object thisObj(thisObjImp);
  return Object(func).call(exec, thisObj, argList);
}

// ------------------------------ PostfixNode ----------------------------------

// ECMA 11.3
Value PostfixNode::evaluate(ExecState *exec)
{
  Reference ref = expr->evaluateReference(exec);
  KJS_CHECKEXCEPTIONVALUE
  Value v = ref.getValue(exec);

  bool knownToBeInteger;
  double n = v.toNumber(exec, knownToBeInteger);

  double newValue = (oper == OpPlusPlus) ? n + 1 : n - 1;
  ref.putValue(exec, Value(newValue, knownToBeInteger));

  return Value(n, knownToBeInteger);
}

// ------------------------------ DeleteNode -----------------------------------

// ECMA 11.4.1
Value DeleteNode::evaluate(ExecState *exec)
{
  Reference ref = expr->evaluateReference(exec);
  KJS_CHECKEXCEPTIONVALUE
  return Value(ref.deleteValue(exec));
}

// ------------------------------ VoidNode -------------------------------------

// ECMA 11.4.2
Value VoidNode::evaluate(ExecState *exec)
{
  Value dummy1 = expr->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  return Undefined();
}

// ------------------------------ TypeOfNode -----------------------------------

// ECMA 11.4.3
Value TypeOfNode::evaluate(ExecState *exec)
{
  const char *s = 0L;
  Reference ref = expr->evaluateReference(exec);
  KJS_CHECKEXCEPTIONVALUE
  ValueImp *b = ref.baseIfMutable();
  if (b && b->dispatchType() == NullType)
    return Value("undefined");
  Value v = ref.getValue(exec);
  switch (v.type())
    {
    case UndefinedType:
      s = "undefined";
      break;
    case NullType:
      s = "object";
      break;
    case BooleanType:
      s = "boolean";
      break;
    case NumberType:
      s = "number";
      break;
    case StringType:
      s = "string";
      break;
    default:
      if (v.type() == ObjectType && static_cast<ObjectImp*>(v.imp())->implementsCall())
	s = "function";
      else
	s = "object";
      break;
    }

  return Value(s);
}

// ------------------------------ PrefixNode -----------------------------------

// ECMA 11.4.4 and 11.4.5
Value PrefixNode::evaluate(ExecState *exec)
{
  Reference ref = expr->evaluateReference(exec);
  KJS_CHECKEXCEPTIONVALUE
  Value v = ref.getValue(exec);

  bool knownToBeInteger;
  double n = v.toNumber(exec, knownToBeInteger);

  double newValue = (oper == OpPlusPlus) ? n + 1 : n - 1;
  Value n2(newValue, knownToBeInteger);

  ref.putValue(exec, n2);

  return n2;
}

// ------------------------------ UnaryPlusNode --------------------------------

// ECMA 11.4.6
Value UnaryPlusNode::evaluate(ExecState *exec)
{
  Value v = expr->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  return Value(v.toNumber(exec)); /* TODO: optimize */
}

// ------------------------------ NegateNode -----------------------------------

// ECMA 11.4.7
Value NegateNode::evaluate(ExecState *exec)
{
  Value v = expr->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  bool knownToBeInteger;
  double n = v.toNumber(exec, knownToBeInteger);
  return Value(-n, knownToBeInteger && n != 0);
}

// ------------------------------ BitwiseNotNode -------------------------------

// ECMA 11.4.8
Value BitwiseNotNode::evaluate(ExecState *exec)
{
  Value v = expr->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  return Value(~v.toInt32(exec));
}

// ------------------------------ LogicalNotNode -------------------------------

// ECMA 11.4.9
Value LogicalNotNode::evaluate(ExecState *exec)
{
  Value v = expr->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  return Value(!v.toBoolean(exec));
}

// ------------------------------ MultNode -------------------------------------

// ECMA 11.5
Value MultNode::evaluate(ExecState *exec)
{
  Value v1 = term1->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  Value v2 = term2->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  return mult(exec, v1, v2, oper);
}

// ------------------------------ AddNode --------------------------------------

// ECMA 11.6
Value AddNode::evaluate(ExecState *exec)
{
  Value v1 = term1->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  Value v2 = term2->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  return add(exec, v1, v2, oper);
}

// ------------------------------ ShiftNode ------------------------------------

// ECMA 11.7
Value ShiftNode::evaluate(ExecState *exec)
{
  Value v1 = term1->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  Value v2 = term2->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  unsigned int i2 = v2.toUInt32(exec);
  i2 &= 0x1f;

  switch (oper) {
  case OpLShift:
    return Value(v1.toInt32(exec) << i2);
  case OpRShift:
    return Value(v1.toInt32(exec) >> i2);
  case OpURShift:
    return Value(v1.toUInt32(exec) >> i2);
  default:
    assert(!"ShiftNode: unhandled switch case");
    return Undefined();
  }
}

// ------------------------------ RelationalNode -------------------------------

// ECMA 11.8
Value RelationalNode::evaluate(ExecState *exec)
{
  Value v1 = expr1->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  Value v2 = expr2->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  bool b;
  if (oper == OpLess || oper == OpGreaterEq) {
    int r = relation(exec, v1, v2);
    if (r < 0)
      b = false;
    else
      b = (oper == OpLess) ? (r == 1) : (r == 0);
  } else if (oper == OpGreater || oper == OpLessEq) {
    int r = relation(exec, v2, v1);
    if (r < 0)
      b = false;
    else
      b = (oper == OpGreater) ? (r == 1) : (r == 0);
  } else if (oper == OpIn) {
      // Is all of this OK for host objects?
      if (v2.type() != ObjectType)
          return throwError(exec,  TypeError,
                             "Value %s (result of expression %s) is not an object. Cannot be used with IN expression.", v2, expr2.get());
      Object o2(static_cast<ObjectImp*>(v2.imp()));
      b = o2.hasProperty(exec, Identifier(v1.toString(exec)));
  } else {
    if (v2.type() != ObjectType)
        return throwError(exec,  TypeError,
                           "Value %s (result of expression %s) is not an object. Cannot be used with instanceof operator.", v2, expr2.get());

    Object o2(static_cast<ObjectImp*>(v2.imp()));
    if (!o2.implementsHasInstance()) {
      // According to the spec, only some types of objects "imlement" the [[HasInstance]] property.
      // But we are supposed to throw an exception where the object does not "have" the [[HasInstance]]
      // property. It seems that all object have the property, but not all implement it, so in this
      // case we return false (consistent with mozilla)
      return Value(false);
      //      return throwError(exec, TypeError,
      //			"Object does not implement the [[HasInstance]] method." );
    }
    return o2.hasInstance(exec, v1);
  }

  return Value(b);
}

// ------------------------------ EqualNode ------------------------------------

// ECMA 11.9
Value EqualNode::evaluate(ExecState *exec)
{
  Value v1 = expr1->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  Value v2 = expr2->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  bool result;
  if (oper == OpEqEq || oper == OpNotEq) {
    // == and !=
    bool eq = equal(exec,v1, v2);
    result = oper == OpEqEq ? eq : !eq;
  } else {
    // === and !==
    bool eq = strictEqual(exec,v1, v2);
    result = oper == OpStrEq ? eq : !eq;
  }
  return Value(result);
}

// ------------------------------ BitOperNode ----------------------------------

// ECMA 11.10
Value BitOperNode::evaluate(ExecState *exec)
{
  Value v1 = expr1->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  Value v2 = expr2->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  int i1 = v1.toInt32(exec);
  int i2 = v2.toInt32(exec);
  int result;
  if (oper == OpBitAnd)
    result = i1 & i2;
  else if (oper == OpBitXOr)
    result = i1 ^ i2;
  else
    result = i1 | i2;

  return Value(result);
}

// ------------------------------ BinaryLogicalNode ----------------------------

// ECMA 11.11
Value BinaryLogicalNode::evaluate(ExecState *exec)
{
  Value v1 = expr1->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  bool b1 = v1.toBoolean(exec);
  if ((!b1 && oper == OpAnd) || (b1 && oper == OpOr))
    return v1;

  Value v2 = expr2->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  return v2;
}

// ------------------------------ ConditionalNode ------------------------------

// ECMA 11.12
Value ConditionalNode::evaluate(ExecState *exec)
{
  Value v = logical->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  bool b = v.toBoolean(exec);

  if (b)
    v = expr1->evaluate(exec);
  else
    v = expr2->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  return v;
}

// ------------------------------ AssignNode -----------------------------------

// ECMA 11.13
Value AssignNode::evaluate(ExecState *exec)
{
  Reference l = left->evaluateReference(exec);
  KJS_CHECKEXCEPTIONVALUE
  Value e, v;
  if (oper == OpEqual) {
    v = expr->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
  } else {
    Value v1 = l.getValue(exec);
    Value v2 = expr->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
    int i1;
    int i2;
    unsigned int ui;
    switch (oper) {
    case OpMultEq:
      v = mult(exec, v1, v2, '*');
      break;
    case OpDivEq:
      v = mult(exec, v1, v2, '/');
      break;
    case OpPlusEq:
      v = add(exec, v1, v2, '+');
      break;
    case OpMinusEq:
      v = add(exec, v1, v2, '-');
      break;
    case OpLShift:
      i1 = v1.toInt32(exec);
      i2 = v2.toInt32(exec);
      v = Value(i1 << i2);
      break;
    case OpRShift:
      i1 = v1.toInt32(exec);
      i2 = v2.toInt32(exec);
      v = Value(i1 >> i2);
      break;
    case OpURShift:
      ui = v1.toUInt32(exec);
      i2 = v2.toInt32(exec);
      v = Value(ui >> i2);
      break;
    case OpAndEq:
      i1 = v1.toInt32(exec);
      i2 = v2.toInt32(exec);
      v = Value(i1 & i2);
      break;
    case OpXOrEq:
      i1 = v1.toInt32(exec);
      i2 = v2.toInt32(exec);
      v = Value(i1 ^ i2);
      break;
    case OpOrEq:
      i1 = v1.toInt32(exec);
      i2 = v2.toInt32(exec);
      v = Value(i1 | i2);
      break;
    case OpModEq: {
      bool d1KnownToBeInteger;
      double d1 = v1.toNumber(exec, d1KnownToBeInteger);
      bool d2KnownToBeInteger;
      double d2 = v2.toNumber(exec, d2KnownToBeInteger);
      v = Value(fmod(d1, d2), d1KnownToBeInteger && d2KnownToBeInteger && d2 != 0);
    }
      break;
    default:
      v = Undefined();
    }
  };
  l.putValue(exec,v);

  KJS_CHECKEXCEPTIONVALUE

  return v;
}

// ------------------------------ CommaNode ------------------------------------

// ECMA 11.14
Value CommaNode::evaluate(ExecState *exec)
{
  Value dummy = expr1->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE
  Value v = expr2->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  return v;
}

// ------------------------------ StatListNode ---------------------------------

StatListNode::StatListNode(StatementNode *s)
  : statement(s), list(this)
{
  setLoc(s->firstLine(), s->lastLine(), s->sourceId());
}
 
StatListNode::StatListNode(StatListNode *l, StatementNode *s)
  : statement(s), list(l->list)
{
  l->list = this;
  setLoc(l->firstLine(), s->lastLine(), l->sourceId());
}

// ECMA 12.1
Completion StatListNode::execute(ExecState *exec)
{
  Completion c = statement->execute(exec);
  KJS_ABORTPOINT
  if (exec->hadException()) {
    Value ex = exec->exception();
    exec->clearException();
    return Completion(Throw, ex);
  }

  if (c.complType() != Normal)
    return c;
  
  Value v = c.value();
  
  for (StatListNode *n = list.get(); n; n = n->list.get()) {
    Completion c2 = n->statement->execute(exec);
    KJS_ABORTPOINT
    if (c2.complType() != Normal)
      return c2;

    if (exec->hadException()) {
      Value ex = exec->exception();
      exec->clearException();
      return Completion(Throw, ex);
    }

    if (c2.isValueCompletion())
      v = c2.value();
    c = c2;
  }

  return Completion(c.complType(), v, c.target());
}

void StatListNode::processVarDecls(ExecState *exec)
{
  for (StatListNode *n = this; n; n = n->list.get())
    n->statement->processVarDecls(exec);
}

// ------------------------------ AssignExprNode -------------------------------

// ECMA 12.2
Value AssignExprNode::evaluate(ExecState *exec)
{
  return expr->evaluate(exec);
}

// ------------------------------ VarDeclNode ----------------------------------

VarDeclNode::VarDeclNode(const Identifier &id, AssignExprNode *in)
    : ident(id), init(in)
{
}

// ECMA 12.2
Value VarDeclNode::evaluate(ExecState *exec)
{
  Object variable = exec->context().imp()->variableObject();

  Value val;
  if (init) {
      val = init->evaluate(exec);
      KJS_CHECKEXCEPTIONVALUE
  } else {
      // already declared? - check with getDirect so you can override
      // built-in properties of the global object with var declarations.
      if ( variable.imp()->getDirect(ident) ) 
          return Value();
      val = Undefined();
  }

#ifdef KJS_VERBOSE
  printInfo(exec,(UString("new variable ")+ident).cstring().c_str(),val);
#endif
  // We use Internal to bypass all checks in derived objects, e.g. so that
  // "var location" creates a dynamic property instead of activating window.location.
  variable.put(exec, ident, val, DontDelete | Internal);

  return ident.ustring();
}

void VarDeclNode::processVarDecls(ExecState *exec)
{
  Object variable = exec->context().imp()->variableObject();

  // If a variable by this name already exists, don't clobber it -
  // it might be a function parameter
  if (!variable.hasProperty(exec, ident)) {
    variable.put(exec,ident, Undefined(), DontDelete);
  }
}

// ------------------------------ VarDeclListNode ------------------------------

// ECMA 12.2
Value VarDeclListNode::evaluate(ExecState *exec)
{
  for (VarDeclListNode *n = this; n; n = n->list.get()) {
    n->var->evaluate(exec);
    KJS_CHECKEXCEPTIONVALUE
  }
  return Undefined();
}

void VarDeclListNode::processVarDecls(ExecState *exec)
{
  for (VarDeclListNode *n = this; n; n = n->list.get())
    n->var->processVarDecls(exec);
}

// ------------------------------ VarStatementNode -----------------------------

// ECMA 12.2
Completion VarStatementNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  (void) list->evaluate(exec); // returns 0L
  KJS_CHECKEXCEPTION

  return Completion(Normal);
}

void VarStatementNode::processVarDecls(ExecState *exec)
{
  list->processVarDecls(exec);
}

// ------------------------------ BlockNode ------------------------------------

BlockNode::BlockNode(SourceElementsNode *s)
{
  if (s) {
    source = s->elements;
    s->elements = 0;
    setLoc(s->firstLine(), s->lastLine(), s->sourceId());
  } else {
    source = 0;
  }
}

// ECMA 12.1
Completion BlockNode::execute(ExecState *exec)
{
  if (!source)
    return Completion(Normal);

  source->processFuncDecl(exec);

  return source->execute(exec);
}

void BlockNode::processVarDecls(ExecState *exec)
{
  if (source)
    source->processVarDecls(exec);
}

// ------------------------------ EmptyStatementNode ---------------------------

// ECMA 12.3
Completion EmptyStatementNode::execute(ExecState */*exec*/)
{
  return Completion(Normal);
}

// ------------------------------ ExprStatementNode ----------------------------

// ECMA 12.4
Completion ExprStatementNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  Value v = expr->evaluate(exec);
  KJS_CHECKEXCEPTION

  return Completion(Normal, v);
}

// ------------------------------ IfNode ---------------------------------------

// ECMA 12.5
Completion IfNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  Value v = expr->evaluate(exec);
  KJS_CHECKEXCEPTION
  bool b = v.toBoolean(exec);

  // if ... then
  if (b)
    return statement1->execute(exec);

  // no else
  if (!statement2)
    return Completion(Normal);

  // else
  return statement2->execute(exec);
}

void IfNode::processVarDecls(ExecState *exec)
{
  statement1->processVarDecls(exec);

  if (statement2)
    statement2->processVarDecls(exec);
}

// ------------------------------ DoWhileNode ----------------------------------

// ECMA 12.6.1
Completion DoWhileNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  Value be, bv;
  Completion c;
  Value value;

  do {
    // bail out on error
    KJS_CHECKEXCEPTION

    c = statement->execute(exec);
    if (!((c.complType() == Continue) && ls.contains(c.target()))) {
      if ((c.complType() == Break) && ls.contains(c.target()))
        return Completion(Normal, value);
      if (c.complType() != Normal)
        return c;
    }
    bv = expr->evaluate(exec);
    KJS_CHECKEXCEPTION
  } while (bv.toBoolean(exec));

  return Completion(Normal, value);
}

void DoWhileNode::processVarDecls(ExecState *exec)
{
  statement->processVarDecls(exec);
}

// ------------------------------ WhileNode ------------------------------------

// ECMA 12.6.2
Completion WhileNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  Value be, bv;
  Completion c;
  bool b(false);
  Value value;

  while (1) {
    bv = expr->evaluate(exec);
    KJS_CHECKEXCEPTION
    b = bv.toBoolean(exec);

    // bail out on error
    KJS_CHECKEXCEPTION

    if (!b)
      return Completion(Normal, value);

    c = statement->execute(exec);
    if (c.isValueCompletion())
      value = c.value();

    if ((c.complType() == Continue) && ls.contains(c.target()))
      continue;
    if ((c.complType() == Break) && ls.contains(c.target()))
      return Completion(Normal, value);
    if (c.complType() != Normal)
      return c;
  }

  return Completion(); // work around gcc 4.0 bug
}

void WhileNode::processVarDecls(ExecState *exec)
{
  statement->processVarDecls(exec);
}

// ------------------------------ ForNode --------------------------------------

// ECMA 12.6.3
Completion ForNode::execute(ExecState *exec)
{
  Value v, cval;

  if (expr1) {
    v = expr1->evaluate(exec);
    KJS_CHECKEXCEPTION
  }
  while (1) {
    if (expr2) {
      v = expr2->evaluate(exec);
      KJS_CHECKEXCEPTION
      if (!v.toBoolean(exec))
	return Completion(Normal, cval);
    }
    // bail out on error
    KJS_CHECKEXCEPTION

    Completion c = statement->execute(exec);
    if (c.isValueCompletion())
      cval = c.value();
    if (!((c.complType() == Continue) && ls.contains(c.target()))) {
      if ((c.complType() == Break) && ls.contains(c.target()))
        return Completion(Normal, cval);
      if (c.complType() != Normal)
      return c;
    }
    if (expr3) {
      v = expr3->evaluate(exec);
      KJS_CHECKEXCEPTION
    }
  }
  
  return Completion(); // work around gcc 4.0 bug
}

void ForNode::processVarDecls(ExecState *exec)
{
  if (expr1)
    expr1->processVarDecls(exec);

  statement->processVarDecls(exec);
}

// ------------------------------ ForInNode ------------------------------------

ForInNode::ForInNode(Node *l, Node *e, StatementNode *s)
  : init(0L), lexpr(l), expr(e), varDecl(0L), statement(s)
{
}

ForInNode::ForInNode(const Identifier &i, AssignExprNode *in, Node *e, StatementNode *s)
  : ident(i), init(in), expr(e), statement(s)
{
  // for( var foo = bar in baz )
  varDecl = new VarDeclNode(ident, init.get());
  lexpr = new ResolveNode(ident);
}

// ECMA 12.6.4
Completion ForInNode::execute(ExecState *exec)
{
  Value e, retval;
  Object v;
  Completion c;
  ReferenceList propList;

  if ( varDecl ) {
    varDecl->evaluate(exec);
    KJS_CHECKEXCEPTION
  }

  e = expr->evaluate(exec);

  // for Null and Undefined, we want to make sure not to go through
  // the loop at all, because their object wrappers will have a
  // property list but will throw an exception if you attempt to
  // access any property.
  if (e.type() == UndefinedType || e.type() == NullType) {
    return Completion(Normal, retval);
  }

  KJS_CHECKEXCEPTION
  v = e.toObject(exec);
  propList = v.propList(exec);

  ReferenceListIterator propIt = propList.begin();

  while (propIt != propList.end()) {
    Identifier name = propIt->getPropertyName(exec);
    if (!v.hasProperty(exec,name)) {
      propIt++;
      continue;
    }

    Reference ref = lexpr->evaluateReference(exec);
    KJS_CHECKEXCEPTION
    ref.putValue(exec, String(name.ustring()));

    c = statement->execute(exec);
    if (c.isValueCompletion())
      retval = c.value();

    if (!((c.complType() == Continue) && ls.contains(c.target()))) {
      if ((c.complType() == Break) && ls.contains(c.target()))
        break;
      if (c.complType() != Normal) {
        return c;
      }
    }

    propIt++;
  }

  // bail out on error
  KJS_CHECKEXCEPTION

  return Completion(Normal, retval);
}

void ForInNode::processVarDecls(ExecState *exec)
{
  statement->processVarDecls(exec);
}

// ------------------------------ ContinueNode ---------------------------------

// ECMA 12.7
Completion ContinueNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  Value dummy;
  return exec->context().imp()->seenLabels()->contains(ident) ?
    Completion(Continue, dummy, ident) :
    Completion(Throw,
	       throwError(exec, SyntaxError, "Label %s not found in containing block. Can't continue.", ident));
}

// ------------------------------ BreakNode ------------------------------------

// ECMA 12.8
Completion BreakNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  Value dummy;
  return exec->context().imp()->seenLabels()->contains(ident) ?
    Completion(Break, dummy, ident) :
    Completion(Throw,
	       throwError(exec, SyntaxError, "Label %s not found in containing block. Can't break.", ident));
}

// ------------------------------ ReturnNode -----------------------------------

// ECMA 12.9
Completion ReturnNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  if (!value)
    return Completion(ReturnValue, Undefined());

  Value v = value->evaluate(exec);
  KJS_CHECKEXCEPTION

  return Completion(ReturnValue, v);
}

// ------------------------------ WithNode -------------------------------------

// ECMA 12.10
Completion WithNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  Value v = expr->evaluate(exec);
  KJS_CHECKEXCEPTION
  Object o = v.toObject(exec);
  KJS_CHECKEXCEPTION
  exec->context().imp()->pushScope(o);
  Completion res = statement->execute(exec);
  exec->context().imp()->popScope();

  return res;
}

void WithNode::processVarDecls(ExecState *exec)
{
  statement->processVarDecls(exec);
}

// ------------------------------ CaseClauseNode -------------------------------

// ECMA 12.11
Value CaseClauseNode::evaluate(ExecState *exec)
{
  Value v = expr->evaluate(exec);
  KJS_CHECKEXCEPTIONVALUE

  return v;
}

// ECMA 12.11
Completion CaseClauseNode::evalStatements(ExecState *exec)
{
  if (list)
    return list->execute(exec);
  else
    return Completion(Normal, Undefined());
}

void CaseClauseNode::processVarDecls(ExecState *exec)
{
  if (list)
    list->processVarDecls(exec);
}

// ------------------------------ ClauseListNode -------------------------------

Value ClauseListNode::evaluate(ExecState */*exec*/)
{
  /* should never be called */
  assert(false);
  return Value();
}

// ECMA 12.11
void ClauseListNode::processVarDecls(ExecState *exec)
{
  for (ClauseListNode *n = this; n; n = n->nx.get())
    if (n->cl)
      n->cl->processVarDecls(exec);
}

// ------------------------------ CaseBlockNode --------------------------------

CaseBlockNode::CaseBlockNode(ClauseListNode *l1, CaseClauseNode *d,
                             ClauseListNode *l2)
{
  if (l1) {
    list1 = l1->nx;
    l1->nx = 0;
  } else {
    list1 = 0;
  }

  def = d;

  if (l2) {
    list2 = l2->nx;
    l2->nx = 0;
  } else {
    list2 = 0;
  }
}
 
Value CaseBlockNode::evaluate(ExecState */*exec*/)
{
  /* should never be called */
  assert(false);
  return Value();
}

// ECMA 12.11
Completion CaseBlockNode::evalBlock(ExecState *exec, const Value& input)
{
  Value v;
  Completion res;
  ClauseListNode *a = list1.get();
  ClauseListNode *b = list2.get();
  CaseClauseNode *clause;

    while (a) {
      clause = a->clause();
      a = a->next();
      v = clause->evaluate(exec);
      KJS_CHECKEXCEPTION
      if (strictEqual(exec, input, v)) {
	res = clause->evalStatements(exec);
	if (res.complType() != Normal)
	  return res;
	while (a) {
	  res = a->clause()->evalStatements(exec);
	  if (res.complType() != Normal)
	    return res;
	  a = a->next();
	}
	break;
      }
    }

  while (b) {
    clause = b->clause();
    b = b->next();
    v = clause->evaluate(exec);
    KJS_CHECKEXCEPTION
    if (strictEqual(exec, input, v)) {
      res = clause->evalStatements(exec);
      if (res.complType() != Normal)
	return res;
      goto step18;
    }
  }

  // default clause
  if (def) {
    res = def->evalStatements(exec);
    if (res.complType() != Normal)
      return res;
  }
  b = list2.get();
 step18:
  while (b) {
    clause = b->clause();
    res = clause->evalStatements(exec);
    if (res.complType() != Normal)
      return res;
    b = b->next();
  }

  // bail out on error
  KJS_CHECKEXCEPTION

  return Completion(Normal);
}

void CaseBlockNode::processVarDecls(ExecState *exec)
{
  if (list1)
    list1->processVarDecls(exec);
  if (def)
    def->processVarDecls(exec);
  if (list2)
    list2->processVarDecls(exec);
}

// ------------------------------ SwitchNode -----------------------------------

// ECMA 12.11
Completion SwitchNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  Value v = expr->evaluate(exec);
  KJS_CHECKEXCEPTION
  Completion res = block->evalBlock(exec,v);

  if ((res.complType() == Break) && ls.contains(res.target()))
    return Completion(Normal, res.value());
  return res;
}

void SwitchNode::processVarDecls(ExecState *exec)
{
  block->processVarDecls(exec);
}

// ------------------------------ LabelNode ------------------------------------

// ECMA 12.12
Completion LabelNode::execute(ExecState *exec)
{
  Completion e;

  if (!exec->context().imp()->seenLabels()->push(label)) {
    return Completion( Throw,
		       throwError(exec, SyntaxError, "Duplicated label %s found.", label));
  };
  e = statement->execute(exec);
  exec->context().imp()->seenLabels()->pop();

  if ((e.complType() == Break) && (e.target() == label))
    return Completion(Normal, e.value());
  return e;
}

void LabelNode::processVarDecls(ExecState *exec)
{
  statement->processVarDecls(exec);
}

// ------------------------------ ThrowNode ------------------------------------

// ECMA 12.13
Completion ThrowNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  Value v = expr->evaluate(exec);
  KJS_CHECKEXCEPTION

  return Completion(Throw, v);
}

// ------------------------------ CatchNode ------------------------------------

Completion CatchNode::execute(ExecState */*exec*/)
{
  // should never be reached. execute(exec, arg) is used instead
  assert(0L);
  return Completion();
}

// ECMA 12.14
Completion CatchNode::execute(ExecState *exec, const Value &arg)
{
  /* TODO: correct ? Not part of the spec */

  exec->clearException();

  Object obj(new ObjectImp());
  obj.put(exec, ident, arg, DontDelete);
  exec->context().imp()->pushScope(obj);
  Completion c = block->execute(exec);
  exec->context().imp()->popScope();

  return c;
}

void CatchNode::processVarDecls(ExecState *exec)
{
  block->processVarDecls(exec);
}

// ------------------------------ FinallyNode ----------------------------------

// ECMA 12.14
Completion FinallyNode::execute(ExecState *exec)
{
  return block->execute(exec);
}

void FinallyNode::processVarDecls(ExecState *exec)
{
  block->processVarDecls(exec);
}

// ------------------------------ TryNode --------------------------------------

// ECMA 12.14
Completion TryNode::execute(ExecState *exec)
{
  KJS_BREAKPOINT;

  Completion c, c2;

  c = block->execute(exec);

  if (!_final) {
    if (c.complType() != Throw)
      return c;
    return _catch->execute(exec,c.value());
  }

  if (!_catch) {
    Value lastException = exec->exception();
    exec->clearException();
    
    c2 = _final->execute(exec);
    
    if (!exec->hadException())
      exec->setException(lastException);
    
    return (c2.complType() == Normal) ? c : c2;
  }

  if (c.complType() == Throw)
    c = _catch->execute(exec,c.value());

  c2 = _final->execute(exec);
  return (c2.complType() == Normal) ? c : c2;
}

void TryNode::processVarDecls(ExecState *exec)
{
  block->processVarDecls(exec);
  if (_final)
    _final->processVarDecls(exec);
  if (_catch)
    _catch->processVarDecls(exec);
}

// ------------------------------ ParameterNode --------------------------------

// ECMA 13
Value ParameterNode::evaluate(ExecState */*exec*/)
{
  return Undefined();
}

// ------------------------------ FunctionBodyNode -----------------------------

FunctionBodyNode::FunctionBodyNode(SourceElementsNode *s)
  : BlockNode(s)
{
  setLoc(-1, -1, -1);
  //fprintf(stderr,"FunctionBodyNode::FunctionBodyNode %p\n",this);
}

void FunctionBodyNode::processFuncDecl(ExecState *exec)
{
  if (source)
    source->processFuncDecl(exec);
}

// ------------------------------ FuncDeclNode ---------------------------------

// ECMA 13
void FuncDeclNode::processFuncDecl(ExecState *exec)
{
  // TODO: let this be an object with [[Class]] property "Function"
  FunctionImp *fimp = new DeclaredFunctionImp(exec, ident, body.get(), exec->context().imp()->scopeChain());
  Object func(fimp); // protect from GC

  //  Value proto = exec->lexicalInterpreter()->builtinObject().construct(exec,List::empty());
  List empty;
  Object proto = exec->lexicalInterpreter()->builtinObject().construct(exec,empty);
  proto.put(exec, constructorPropertyName, func, ReadOnly|DontDelete|DontEnum);
  func.put(exec, prototypePropertyName, proto, Internal|DontDelete);

  int plen = 0;
  for(ParameterNode *p = param.get(); p != 0L; p = p->nextParam(), plen++)
    fimp->addParameter(p->ident());

  func.put(exec, lengthPropertyName, Number(plen), ReadOnly|DontDelete|DontEnum);

  exec->context().imp()->variableObject().put(exec,ident,func);

  if (body) {
    // hack the scope so that the function gets put as a property of func, and it's scope
    // contains the func as well as our current scope
    Object oldVar = exec->context().imp()->variableObject();
    exec->context().imp()->setVariableObject(func);
    exec->context().imp()->pushScope(func);
    body->processFuncDecl(exec);
    exec->context().imp()->popScope();
    exec->context().imp()->setVariableObject(oldVar);
  }
}

// ------------------------------ FuncExprNode ---------------------------------

// ECMA 13
Value FuncExprNode::evaluate(ExecState *exec)
{
  FunctionImp *fimp = new DeclaredFunctionImp(exec, Identifier::null(), body.get(), exec->context().imp()->scopeChain());
  Value ret(fimp);
  List empty;
  Value proto = exec->lexicalInterpreter()->builtinObject().construct(exec,empty);
  fimp->put(exec, prototypePropertyName, proto, Internal|DontDelete);

  int plen = 0;
  for(ParameterNode *p = param.get(); p != 0L; p = p->nextParam(), plen++)
    fimp->addParameter(p->ident());

  return ret;
}

// ------------------------------ SourceElementsNode ---------------------------

SourceElementsNode::SourceElementsNode(StatementNode *s1)
  : element(s1), elements(this)
{
  setLoc(s1->firstLine(), s1->lastLine(), s1->sourceId());
}
 
SourceElementsNode::SourceElementsNode(SourceElementsNode *s1, StatementNode *s2)
  : element(s2), elements(s1->elements)
{
  s1->elements = this;
  setLoc(s1->firstLine(), s2->lastLine(), s1->sourceId());
}

// ECMA 14
Completion SourceElementsNode::execute(ExecState *exec)
{
  KJS_CHECKEXCEPTION

  Completion c1 = element->execute(exec);
  KJS_CHECKEXCEPTION;
  if (c1.complType() != Normal)
    return c1;
  
  for (SourceElementsNode *n = elements.get(); n; n = n->elements.get()) {
    Completion c2 = n->element->execute(exec);
    if (c2.complType() != Normal)
      return c2;
    // The spec says to return c2 here, but it seems that mozilla returns c1 if
    // c2 doesn't have a value
    if (!c2.value().isNull())
      c1 = c2;
  }
  
  return c1;
}

// ECMA 14
void SourceElementsNode::processFuncDecl(ExecState *exec)
{
  for (SourceElementsNode *n = this; n; n = n->elements.get())
    n->element->processFuncDecl(exec);
}

void SourceElementsNode::processVarDecls(ExecState *exec)
{
  for (SourceElementsNode *n = this; n; n = n->elements.get())
    n->element->processVarDecls(exec);
}

ProgramNode::ProgramNode(SourceElementsNode *s): FunctionBodyNode(s) {
    //fprintf(stderr,"ProgramNode::ProgramNode %p\n",this);
}
