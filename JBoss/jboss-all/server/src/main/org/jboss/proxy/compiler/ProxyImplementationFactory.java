/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.proxy.compiler;

import org.apache.bcel.Constants;
import org.apache.bcel.classfile.Field;
import org.apache.bcel.classfile.Method;

import org.apache.bcel.generic.ArrayType;
import org.apache.bcel.generic.BasicType;
import org.apache.bcel.generic.ConstantPoolGen;
import org.apache.bcel.generic.ClassGen;
import org.apache.bcel.generic.FieldGen;
import org.apache.bcel.generic.Instruction;
import org.apache.bcel.generic.InstructionFactory;
import org.apache.bcel.generic.InstructionList;
import org.apache.bcel.generic.MethodGen;
import org.apache.bcel.generic.ObjectType;
import org.apache.bcel.generic.PUSH;
import org.apache.bcel.generic.ReferenceType;
import org.apache.bcel.generic.Type;

/**
 * Factory to create the bytecode implementation of various methods
 * required by the ProxyCompiler.
 *
 * @version <tt>$Revision: 1.2 $</tt>
 * @author <a href="mailto:neale@isismanor.co.uk">Neale Swinnerton</a>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class ProxyImplementationFactory 
{
   // Class Names
   private final static String RUNTIME_CN            = Runtime.class.getName();
   private final static String INVOCATION_HANDLER_CN = InvocationHandler.class.getName();
   private final static String STRING_BUFFER_CN      = StringBuffer.class.getName();

   // Types
   private final static ObjectType RUNTIME_T             = (ObjectType)Utility.getType(Runtime.class);
   private final static ObjectType INVOCATION_HANDLER_T  = (ObjectType)Utility.getType(InvocationHandler.class);
   private final static ArrayType  ARRAY_OF_CLASS_T      = new ArrayType("java.lang.Class", 1);
   private final static ObjectType OBJECT_T              = new ObjectType("java.lang.Object");
   private final static ArrayType  ARRAY_OF_OBJECT_T     = new ArrayType("java.lang.Object", 1);
   private final static ObjectType STRING_T              = new ObjectType("java.lang.String");
   private final static ObjectType STRING_BUFFER_T       = new ObjectType("java.lang.StringBuffer");
   private final static ObjectType PROXY_TARGET_T        = new ObjectType(Proxies.ProxyTarget.class.getName());
   private final static Type[]     INVOKE_ARGS           = { INVOCATION_HANDLER_T,
                                                             Type.INT,
                                                             ARRAY_OF_OBJECT_T };
   // Method Names
   private final static String GET_INVOCATION_HANDLER_MN = "getInvocationHandler";
   private final static String GET_TARGET_TYPES_MN       = "getTargetTypes";
   private final static String TO_STRING_MN              = "toString";
   private final static String APPEND_MN                 = "append";
   private final static String CTOR_MN                   = "<init>";

   // Field Names
   private final static String INVOCATION_HANDLER_FN     = "invocationHandler";

   /** The proxy class type (assigned in the ctor) */
   private static Type PROXY_CLASS_T;
   
   private InstructionList il = new InstructionList();
   private String proxyClassName;
   private String superClassName;
   private ConstantPoolGen constPool;
   private InstructionFactory iFactory;

   /**
    * Creates a new <code>ProxyImplementationFactory</code> instance.
    *
    * @param superClassName    a <code>String</code> value
    * @param proxyClassName    a <code>String</code> value
    * @param cg                a <code>ClassGen</code> value
    */
   public ProxyImplementationFactory(final String superClassName, 
                                     final String proxyClassName, 
                                     final ClassGen cg) 
   {
      this.superClassName = superClassName;
      this.proxyClassName = proxyClassName;

      PROXY_CLASS_T = new ObjectType(proxyClassName);
      constPool = cg.getConstantPool();
      iFactory = new InstructionFactory(cg, constPool);
   }
   
   /**
    * generate an implementation of
    * <pre>
    *
    * <code>
    *    public InvocationHandler getInvocationHandler() {
    *       return this.invocationHandler;
    *    }
    * </code>
    *
    * </pre>
    */
   public Method createGetInvocationHandler() 
   {
      MethodGen mg = new MethodGen(Constants.ACC_PUBLIC,
                                   INVOCATION_HANDLER_T,
                                   Type.NO_ARGS,
                                   null, GET_INVOCATION_HANDLER_MN, proxyClassName, il, constPool);
      
      il.append(iFactory.createLoad(PROXY_CLASS_T, 0));
      il.append(iFactory.createGetField(proxyClassName, INVOCATION_HANDLER_FN, INVOCATION_HANDLER_T));
      il.append(iFactory.createReturn(INVOCATION_HANDLER_T));

      mg.stripAttributes(true);
      mg.setMaxStack();
      mg.setMaxLocals();
      
      return getMethodAndTidyup(mg);
   }

   /**
    * generate an implementation of
    * <pre>
    *
    * <code>
    *   public Class[] getTargetTypes {
    *      return this.invocationHandler.copyTargetTypes();
    *   }
    * </code>
    *
    * </pre>
    *
    * @return the method
    * 
    */
   public Method createGetTargetTypes() 
   {
      MethodGen mg = new MethodGen(Constants.ACC_PUBLIC,
                                   ARRAY_OF_CLASS_T,
                                   Type.NO_ARGS,
                                   null,
                                   GET_TARGET_TYPES_MN,
                                   proxyClassName,
                                   il,
                                   constPool);
      
      il.append(iFactory.createLoad(PROXY_CLASS_T, 0));         
      il.append(iFactory.createGetField(proxyClassName, Runtime.RUNTIME_FN, RUNTIME_T));
      il.append(iFactory.createInvoke(RUNTIME_CN,
                                      "copyTargetTypes",
                                      ARRAY_OF_CLASS_T,
                                      Type.NO_ARGS,
                                      Constants.INVOKEVIRTUAL));
      
      il.append(iFactory.createReturn(ARRAY_OF_CLASS_T));
      
      mg.stripAttributes(true);
      mg.setMaxStack(1);
      mg.setMaxLocals();

      return getMethodAndTidyup(mg);
   }

   /**
    * generate an implementation of
    * <pre>
    *
    * <code>
    *    public String toString() {
    *       return "ProxyTarget[" + invocationHandler + "]";
    *    }
    * </code>
    *
    * </pre>
    *
    * @return the method
    * 
    */
   public Method createToString() 
   {
      MethodGen mg = new MethodGen(Constants.ACC_PUBLIC, STRING_T,
                                   Type.NO_ARGS,
                                   null, TO_STRING_MN, proxyClassName, il, constPool);

      il.append(iFactory.createNew(STRING_BUFFER_T));
      il.append(iFactory.createDup(1));
      il.append(iFactory.createInvoke(STRING_BUFFER_CN,
                                      CTOR_MN,
                                      Type.VOID,
                                      Type.NO_ARGS,
                                      Constants.INVOKESPECIAL));
      il.append(new PUSH(constPool, "ProxyTarget["));
      il.append(iFactory.createInvoke(STRING_BUFFER_CN,
                                      APPEND_MN,
                                      STRING_BUFFER_T,
                                      new Type[]{STRING_T},
                                      Constants.INVOKEVIRTUAL));
      il.append(iFactory.createLoad(PROXY_CLASS_T, 0));
      il.append(iFactory.createGetField(proxyClassName, INVOCATION_HANDLER_FN, INVOCATION_HANDLER_T));
      il.append(iFactory.createInvoke(STRING_BUFFER_CN,
                                      APPEND_MN,
                                      STRING_BUFFER_T,
                                      new Type[]{OBJECT_T},
                                      Constants.INVOKEVIRTUAL));
      il.append(new PUSH(constPool, "]"));
      il.append(iFactory.createInvoke(STRING_BUFFER_CN,
                                      APPEND_MN,
                                      STRING_BUFFER_T,
                                      new Type[]{STRING_T},
                                      Constants.INVOKEVIRTUAL));
      il.append(iFactory.createInvoke(STRING_BUFFER_CN,
                                      TO_STRING_MN,
                                      STRING_T,
                                      Type.NO_ARGS,
                                      Constants.INVOKEVIRTUAL));
      il.append(iFactory.createReturn(STRING_T));

      mg.stripAttributes(true);
      mg.setMaxStack();
      mg.setMaxLocals();

      return getMethodAndTidyup(mg);
   }
   
   /**
    * generate an implementation of
    * <pre>
    *
    * <xmp>
    *   public <proxyClassName> (InvocationHandler h) {
    *      this.invocationHandler = h;
    *   }
    * </xmp>
    *
    * </pre>
    *
    * @return the method
    * 
    */
   public Method createConstructor() 
   {
      MethodGen mg = new MethodGen(Constants.ACC_PUBLIC,
                                   Type.VOID,
                                   new Type[]{INVOCATION_HANDLER_T},
                                   null, 
                                   CTOR_MN,
                                   proxyClassName,
                                   il,
                                   constPool);

      il.append(iFactory.createLoad(INVOCATION_HANDLER_T, 0));
      il.append(iFactory.createInvoke(superClassName,
                                      CTOR_MN,
                                      Type.VOID,
                                      Type.NO_ARGS,
                                      Constants.INVOKESPECIAL));
      il.append(iFactory.createLoad(PROXY_CLASS_T, 0));
      il.append(iFactory.createLoad(INVOCATION_HANDLER_T, 1));
      il.append(iFactory.createPutField(proxyClassName, INVOCATION_HANDLER_FN, INVOCATION_HANDLER_T));
      il.append(iFactory.createReturn(Type.VOID));
      
      mg.stripAttributes(true);
      mg.setMaxStack();
      mg.setMaxLocals();

      return getMethodAndTidyup(mg);
   }

   /**
    * generate an implementation of...
    * <pre>
    *
    * <xmp>
    *   public <return type> <method name>(<p0 type> p0, <p1 type> p1, ...)
    *      throws e0, e1 ... 
    *   {
    *      return runtme.invoke(invocatioHandler, <method index>,
    *                           new Object[]{boxed p0, boxed p1, ...)};
    *   }                 
    * </xmp>
    *
    * </pre>
    *
    * @return the method
    */
   public Method createProxyMethod(String name,
                                   int methodNum,
                                   Type rType,
                                   Type[] pTypes,
                                   String[] exceptionNames) 
   {
      MethodGen mg = new MethodGen(Constants.ACC_PUBLIC,
                                   rType,
                                   pTypes,
                                   null, // argNames
                                   name,
                                   proxyClassName,
                                   il,
                                   constPool);
      
      for (int j = 0; j < exceptionNames.length; j++) {
         mg.addException(exceptionNames[j]);
      }

      // implementation of this.invocationHandler.invoke<Type>(InvocationHandler, i, new Object[]{ ... })
      il.append(iFactory.createGetStatic(proxyClassName, Runtime.RUNTIME_FN, RUNTIME_T));
      il.append(iFactory.createLoad(RUNTIME_T, 0));

      // load the first method param (the ih)
      il.append(iFactory.createGetField(proxyClassName, INVOCATION_HANDLER_FN, INVOCATION_HANDLER_T));
         
      // load the second method param (the method id)
      il.append(new PUSH(constPool, methodNum)); 

      // create a new array to hold param values
      il.append(new PUSH(constPool, pTypes.length));
      il.append((Instruction)iFactory.createNewArray(OBJECT_T, (short)1));
      
      if (pTypes.length > 0) {
         // the register index
         int i = 1; // register 0 loaded with runtime ?
         
         for (int j = 0; j < pTypes.length; j++) {
            Type t = pTypes[j];
            
            // not sure what this does
            il.append(iFactory.createDup(1));

            // load the index of the array element
            il.append(new PUSH(constPool, j));

            // box basic types into wrapped versions
            if (t instanceof BasicType) {
               // do a e.g new Boolean(b)
               String wrappedClassName = Utility.getObjectEquivalentClassName((BasicType)t);
               ObjectType wrappedType = new ObjectType(wrappedClassName);
               il.append(iFactory.createNew(wrappedType));

               // again, what does this do?
               il.append(iFactory.createDup(1));

               // load the parameter value from the register index
               il.append(iFactory.createLoad(t, i));
               il.append(iFactory.createInvoke(wrappedClassName,
                                               CTOR_MN,
                                               Type.VOID,
                                               new Type[] { t },
                                               Constants.INVOKESPECIAL));

               // increment register index for long & double
               switch (t.getType()) {
               case Constants.T_DOUBLE: // 7
               case Constants.T_LONG: // 11
                  i++;
               }

               // type is now wrapped type
               t = wrappedType;
            } 
            else {
               // just load the value in to the register slot
               il.append(iFactory.createLoad(t, i));
            }
               
            // increment register index for everything 
            // (makes += 2 for long & double) with above ++
            i++;

            // store the value into the array
            il.append(iFactory.createArrayStore(t)); 
         }
      }
            
      il.append(iFactory.createInvoke(RUNTIME_CN,
                                      "invoke",
                                      Type.OBJECT,
                                      INVOKE_ARGS,
                                      Constants.INVOKEVIRTUAL));
      
      // handle the return value
      if (rType instanceof ReferenceType) {
         il.append(iFactory.createCheckCast((ReferenceType)rType));
      } 
      else if (rType instanceof BasicType) {
         if (rType == Type.VOID) {
            // Chuck away returned value if it's void
            il.append(iFactory.createPop(1));
         }
         else {
            // unbox the return value of a primitive wrapper...
            // we've got an Object and need the equivalent primitive
            // do a e.g. (Boolean)obj.booleanValue();
            String wrappedClassName = Utility.getObjectEquivalentClassName((BasicType)rType);
            ObjectType wrappedType = new ObjectType(wrappedClassName);
            il.append(iFactory.createCheckCast((ReferenceType)wrappedType));
            
            String methodName = Utility.signatureToString(rType.getSignature()) + "Value";
            
            il.append(iFactory.createInvoke(wrappedClassName,
                                            methodName,
                                            rType,
                                            Type.NO_ARGS,
                                            Constants.INVOKEVIRTUAL));
         } 
      }
      
      il.append(iFactory.createReturn(rType));
      
      mg.stripAttributes(true);
      mg.setMaxStack();
      mg.setMaxLocals();
      
      return getMethodAndTidyup(mg);
   }

   /**
    * generate a field declaration of the form...
    * <pre>
    *
    * <code>
    *   private InvocationHandler invocationHandler;
    * </code>
    *
    * </pre>
    *
    * @return the method
    *
    */
   public Field createInvocationHandlerField() 
   {
      FieldGen fg = new FieldGen(Constants.ACC_PRIVATE, 
                                 INVOCATION_HANDLER_T, 
                                 INVOCATION_HANDLER_FN, 
                                 constPool);
      return fg.getField();
   }
   
   /**
    * generate a field declaration of the form...
    * <pre>
    *
    * <code>
    *   public static Runtime runtime;
    * </code>
    *
    * </pre>
    *
    * @return the method
    *
    */
   public Field createRuntimeField() 
   {
      FieldGen fg = new FieldGen(Constants.ACC_PUBLIC | Constants.ACC_STATIC, 
                                 RUNTIME_T, 
                                 Runtime.RUNTIME_FN, 
                                 constPool);
      return fg.getField();
   }

   /**
    * A helper to return the method from MethodGen and clean up the 
    * instruction list.
    */
   private Method getMethodAndTidyup(final MethodGen mg) 
   {
      Method m = mg.getMethod();
      il.dispose();

      return m;
   }         
}
