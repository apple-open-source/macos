/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.capability;

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;

import java.io.ByteArrayOutputStream;
import java.io.BufferedOutputStream;

import java.net.URL;
import java.net.URLClassLoader;

import javax.management.IntrospectionException;
import javax.management.MBeanException;
import javax.management.MBeanInfo;
import javax.management.MBeanOperationInfo;
import javax.management.MBeanParameterInfo;
import javax.management.ReflectionException;

import javax.management.loading.DefaultLoaderRepository;

import org.apache.bcel.Constants;
import org.apache.bcel.generic.ClassGen;
import org.apache.bcel.generic.MethodGen;
import org.apache.bcel.generic.ConstantPoolGen;
import org.apache.bcel.generic.InstructionHandle;
import org.apache.bcel.generic.InstructionList;
import org.apache.bcel.generic.ObjectType;
import org.apache.bcel.generic.ArrayType;
import org.apache.bcel.generic.Type;

import org.apache.bcel.generic.ACONST_NULL;
import org.apache.bcel.generic.ALOAD;
import org.apache.bcel.generic.AALOAD;
import org.apache.bcel.generic.ASTORE;
import org.apache.bcel.generic.ARETURN;
import org.apache.bcel.generic.CHECKCAST;
import org.apache.bcel.generic.DLOAD;
import org.apache.bcel.generic.DSTORE;
import org.apache.bcel.generic.FLOAD;
import org.apache.bcel.generic.FSTORE;
import org.apache.bcel.generic.ICONST;
import org.apache.bcel.generic.IFEQ;
import org.apache.bcel.generic.IFNULL;
import org.apache.bcel.generic.IFNONNULL;
import org.apache.bcel.generic.ILOAD;
import org.apache.bcel.generic.IRETURN;
import org.apache.bcel.generic.ISTORE;
import org.apache.bcel.generic.INVOKESTATIC;
import org.apache.bcel.generic.INVOKESPECIAL;
import org.apache.bcel.generic.INVOKEVIRTUAL;
import org.apache.bcel.generic.GETFIELD;
import org.apache.bcel.generic.GETSTATIC;
import org.apache.bcel.generic.LDC;
import org.apache.bcel.generic.LLOAD;
import org.apache.bcel.generic.LSTORE;
import org.apache.bcel.generic.NEW;
import org.apache.bcel.generic.POP;
import org.apache.bcel.generic.PUSH;
import org.apache.bcel.generic.RETURN;

import org.apache.bcel.classfile.JavaClass;

import org.jboss.mx.loading.LoaderRepository;
import org.jboss.mx.loading.UnifiedClassLoader;

import org.jboss.mx.server.ServerConstants;

import org.jboss.mx.metadata.AttributeOperationResolver;


/**
 * Byte code optimized dispatcher for Standard MBeans. This dispatcher generates
 * an invoke implementation that handles the operation dispatching without
 * Java reflection.<p>
 *
 * The use of this dispatcher may be controlled by setting a
 * {@link org.jboss.mx.server.ServerConstants#OPTIMIZE_REFLECTED_DISPATCHER OPTIMIZE_REFLECTED_DISPATCHER}
 * property to either <tt>"true"</tt> or <tt>"false"</tt> string value.
 *
 * @see org.jboss.mx.capability.ReflectedMBeanDispatcher
 * @see org.jboss.mx.capability.DispatchClassLoader
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.4.6.2 $
 *
 */
public class OptimizedMBeanDispatcher implements ServerConstants
{
   // Constants -----------------------------------------------------
   final static Class SUPER_CLASS = ReflectedMBeanDispatcher.class;


   // Static --------------------------------------------------------
   public static ReflectedMBeanDispatcher create(MBeanInfo info, Object resource)
   {
      try
      {
         // construct class template
         String className     = resource.getClass().getName().replace('.', '_') + "_Dispatcher";
         String superClass    = SUPER_CLASS.getName();
         String fileName      = className + ".class";
         int modifiers        = Constants.ACC_PUBLIC;
         String[] interfaces  = new String[0];

         ClassGen clazz       = new ClassGen(className, superClass, fileName, modifiers, interfaces);
         ConstantPoolGen cp   = clazz.getConstantPool();

         clazz.addMethod(createConstructor(cp, className).getMethod());
         clazz.addMethod(createInvoke(cp, info, className, resource.getClass().getName()).getMethod());
         clazz.update();

         JavaClass c = clazz.getJavaClass();

         ByteArrayOutputStream baos = new ByteArrayOutputStream(2000);
         BufferedOutputStream bos = new BufferedOutputStream(baos);
         c.dump(bos);
         
         // FIXME: what about ctx cl?
         // FIXME: also I dont know if the parent is right here, have to check later
         ClassLoader ocl = new DispatchClassLoader(resource.getClass().getClassLoader(), className, baos.toByteArray());

         Class dispatcherClass = ocl.loadClass(className);
         Constructor constr = dispatcherClass.getConstructor(
               new Class[] { MBeanInfo.class, AttributeOperationResolver.class, Object.class }
         );

         Object o = constr.newInstance(new Object[] { info, new AttributeOperationResolver(info), resource });

         return (ReflectedMBeanDispatcher)o;
      }
      catch (Exception e)
      {
         e.printStackTrace();
         throw new Error();
      }
   }

   /**
    * Returns the signature of a MBean operation using the grammar required by
    * the class file format, excluding the method name. <p>
    *
    * <h4>The Java Virtual Machine Specification: 4.3.3 Method Descriptors</h4>
    *
    * A method descriptor represents the parameters that the method takes and the value that it returns:   <br><i><pre>
    *
    * MethodDescriptor:
    *    ( ParameterDescriptor* ) ReturnDescriptor
    * </pre></i>
    *
    * A parameter descriptor represents a parameter passed to a method:   <br><i><pre>
    *
    * ParameterDescriptor:
    *    FieldType
    * </pre></i>
    *
    * A return descriptor represents the type of the value returned from a method. It is a series of characters generated by the grammar:  <br><i><pre>
    *
    * ReturnDescriptor:
    *    FieldType
    *    V
    * </pre></i>
    *
    * The character V indicates that the method returns no value (its return type is void).  <p>
    *
    * For example, the method descriptor for the method <br>
    *
    * <pre>  Object mymethod(int i, double d, Thread t)  </pre>
    *
    * is <br>
    *
    * <b><pre>  (IDLjava/lang/Thread;)Ljava/lang/Object;  </pre></b>
    *
    * Note that internal forms of the fully qualified names of Thread and Object are used in the method descriptor.
    */
   public static String getMethodDescriptor(MBeanParameterInfo[] signature, String returnType)
   {

      StringBuffer sign = new StringBuffer(256);
      sign.append("(");

      for (int i = 0; i < signature.length; ++i)
         sign.append(getDescriptorForType(signature[i].getName()));

      sign.append(")" + getDescriptorForType(returnType));

      return sign.toString();
   }

   /**
   * Returns a descriptor for a given Java type. See {@link java.lang.Class#getName() Class.getName()}
   * for details on the grammar for arrays and primitive types. Note that the internal form of the fully
   * qualified name for class Object is used, so for example, the returned descriptor for
   * <tt>java.lang.Object</tt> is
   *
   * <b><pre>Ljava/lang/Object;</pre></b>
   *
   * See JVM spec §4.2 and §4.3 for detailed description of the internal class name format and grammar notation.
   *
   * @param name  fully qualified name of the Java type
   * @return descriptor string using the JVM grammar
   */
   public static String getDescriptorForType(String name)
   {
      if (name.equals(Byte.TYPE.getName()))               return "B";
      else if (name.equals(Character.TYPE.getName()))     return "C";
      else if (name.equals(Double.TYPE.getName()))        return "D";
      else if (name.equals(Float.TYPE.getName()))         return "F";
      else if (name.equals(Integer.TYPE.getName()))       return "I";
      else if (name.equals(Long.TYPE.getName()))          return "J";
      else if (name.equals(Short.TYPE.getName()))         return "S";
      else if (name.equals(Boolean.TYPE.getName()))       return "Z";
      else if (name.equals(Void.TYPE.getName()))          return "V";
      else if (name.startsWith("["))                      return name.replace('.', '/');
      else                                                return "L" + name.replace('.', '/') + ";";
   }

   /**
    * Checks if a given name matches the <tt>TYPE</tt> name of a primitive wrapper class.
    *
    * @see  java.lang.Integer#TYPE
    *
    * @param   name  TYPE.getName()
    * @return  true if is a primitive type name; false otherwise
    */
   public static boolean isPrimitive(String name)
   {
      if (name.equals(Byte.TYPE.getName())      ||
          name.equals(Character.TYPE.getName()) ||
          name.equals(Double.TYPE.getName())    ||
          name.equals(Float.TYPE.getName())     ||
          name.equals(Integer.TYPE.getName())   ||
          name.equals(Long.TYPE.getName())      ||
          name.equals(Short.TYPE.getName())     ||
          name.equals(Boolean.TYPE.getName()))

               return true;

      return false;
   }



   // Protected -----------------------------------------------------

   /**
    * creates constructor <tt>&lt;init&gt;(MBeanInfo info, AttributeOperationResolver resolver, Object resource)</tt>
    * that calls <tt>super(info, resolver, resource)</tt> in its implementation
    *
    * @param   cp          constant pool
    * @param   className   name of the class being generated
    */
   protected static MethodGen createConstructor(ConstantPoolGen cp, String className)
   {
      InstructionList constrInstructions = new InstructionList();

      int constrRefIndex   = cp.addMethodref(
            SUPER_CLASS.getName(),
            "<init>",
            "(" + getDescriptorForType(MBeanInfo.class.getName())
                + getDescriptorForType(AttributeOperationResolver.class.getName())
                + getDescriptorForType(Object.class.getName())
                + ")V"
      );

      constrInstructions.append(new ALOAD(0));                                   // Stack:  => ..., this
      constrInstructions.append(new ALOAD(1));                                   // Stack:  => ..., this, arg1 [MBeanInfo]
      constrInstructions.append(new ALOAD(2));                                   // Stack:  => ..., this, arg1 [MBeanInfo], arg2 [AttributeOperationResolver]
      constrInstructions.append(new ALOAD(3));                                   // Stack:  => ..., this, arg1 [MBeanInfo], arg2 [AttributeOperationResolver], arg3 [Object]
      constrInstructions.append(new INVOKESPECIAL(constrRefIndex));              // Stack:  => ...
      constrInstructions.append(new RETURN());                                   // Stack:  => <empty>

      MethodGen constrMethod = new MethodGen(
            Constants.ACC_PUBLIC,
            Type.VOID,
            new Type[] {
                  new ObjectType(MBeanInfo.class.getName()),
                  new ObjectType(AttributeOperationResolver.class.getName()),
                  new ObjectType(Object.class.getName())  },
            new String[] { "info", "resolver", "resource" },
            "<init>",
            className, constrInstructions, cp
      );
      constrMethod.setMaxStack(4);

      return constrMethod;
   }

   /**
    * Creates the implementation of the <tt>invoke(String actionName, Object[] args, String[] signature)</tt>
    * method. This implementation currently handles all non overloaded MBean operation invocations.
    * Overloaded operations still fall back to the default reflected invocations. <p>
    *
    * The Java equivalent of the implementation looks roughly as follows:<br><pre>
    *
    * public void invoke(String actionName, Object[] args, String[] signature)
    * {
    *    if (actionName != null)
    *    {
    *       try
    *       {
    *          if (actionName.equals(&lt;operName1&gt;))
    *             return ((&lt;resource type&gt;)super.getResourceObject()).&lt;operName1&gt;((&lt;arg1 type&gt;)arg1, (&lt;arg2 type&gt;)arg2, ...);
    *          else if (actionName.equals(&lt;operName2&gt;))
    *             return ((&lt;resource type&gt;)super.getResourceObject()).&lt;operName2&gt;((&lt;arg1 type&gt;)arg1, (&lt;arg2 type&gt;)arg2, ...);
    *
    *          ...
    *
    *          else
    *             super.invoke(actionName, args, signature);
    *      }
    *      catch (Throwable t)
    *      {
    *          super.invoke(actionName, args, signature);
    *      }
    *    }
    * }
    * </pre>
    *
    * @param   cp                   constant pool of the class being generated
    * @param   info                 metadata of the MBean
    * @param   className            name of the class being generated
    * @param   resourceClassName    name of the resource class being invoked
    */
   protected static MethodGen createInvoke(ConstantPoolGen cp, MBeanInfo info, String className, String resourceClassName)
   {
      InstructionList invokeInstructions = new InstructionList();
      MethodEntry[] operations           = getOperations(info);

      // load operation name strings and methods to constant pool
      for (int i = 0; i < operations.length; ++i) {
         operations[i].nameIndexInCP   = cp.addString(operations[i].getName());
         operations[i].methodIndexInCP = cp.addMethodref(
                  resourceClassName,
                  operations[i].getName(),
                  operations[i].methodDescriptor
         );
      }


      int invokeIndex = cp.addMethodref(
            SUPER_CLASS.getName(),
            "invoke",
            "(" + getDescriptorForType(String.class.getName())
                + getDescriptorForType(Object[].class.getName())
                + getDescriptorForType(String[].class.getName())
                +
            ")" + getDescriptorForType(Object.class.getName())
      );

      int getResourceObjectIndex = cp.addMethodref(
            SUPER_CLASS.getName(),
            "getResourceObject",
            "()Ljava/lang/Object;"
      );

      int strEqualsIndex = cp.addMethodref(
            String.class.getName(),
            "equals",
            "(Ljava/lang/Object;)Z"
      );

      InstructionHandle beginTryBlock             = null;
      InstructionHandle endTryBlock               = null;

      IFNULL ifOperationEqualsNull                = new IFNULL(null);
      IFEQ operationElseIfBranch                  = null;

      if (operations.length > 0)
      {
         //
         //    if (actionName != null)
         //
         invokeInstructions.append(new ALOAD(1));                                   // Stack:  => ..., arg1 [String]

         beginTryBlock =
            invokeInstructions.append(ifOperationEqualsNull);                       // Stack:  => ...

         for (int i = 0; i < operations.length; ++i)
         {
            //
            //  if (actionName.equals(operations[i].getName());
            //
            InstructionHandle jumpToNextElse =
                  invokeInstructions.append(new ALOAD(1));                          // Stack:  => ..., arg1 [String]
            invokeInstructions.append(new LDC(operations[i].nameIndexInCP));        // Stack:  => ..., opName [String]
            invokeInstructions.append(new INVOKEVIRTUAL(strEqualsIndex));           // Stack:  => ..., 0 | 1 [boolean]

            // set the jump target for previous else if branch
            if (operationElseIfBranch != null)
               operationElseIfBranch.setTarget(jumpToNextElse);

            operationElseIfBranch = new IFEQ(null);
            invokeInstructions.append(operationElseIfBranch);                       // Stack:  => ...

            invokeInstructions.append(new ALOAD(0));                                // Stack:  => ..., this
            invokeInstructions.append(new INVOKEVIRTUAL(getResourceObjectIndex));   // Stack:  => ..., resource [Object]

            int x = cp.addClass(resourceClassName);
            invokeInstructions.append(new CHECKCAST(x));                            // Stack:  => ..., resource [<resource object type>]

            // if invocation has args, we need to push them into stack
            if (operations[i].getSignature().length > 0)
            {

               for (int arrayIndex = 0; arrayIndex < operations[i].getSignature().length; ++arrayIndex)
               {
                  invokeInstructions.append(new ALOAD(2));                          // Stack:  => ..., resource [<type>], arg2 [Object[]]
                  invokeInstructions.append(new PUSH(cp, arrayIndex));              // Stack:  => ..., resource [<type>], arg2 [Object[]], array index [int]
                  invokeInstructions.append(new AALOAD());                          // Stack:  => ..., resource [<type>], array[index] [Object]

                  // Args come in as objects. If signature has a primitive type
                  // we need to convert the arg before we can invoke the operation
                  String type = operations[i].getSignature() [arrayIndex].getName();

                  if (isPrimitive(type))
                     invokeInstructions.append(convertObjectToPrimitive(cp, type)); // Stack:  => ..., resource[<type>], value [<primitive>]

                  else
                  {
                     x = cp.addClass(type);
                     invokeInstructions.append(new CHECKCAST(x));                   // Stack:  => ..., resource[<type>], value [<reference>]
                  }
               }
            }

            //
            //    resource.<operation>(<arg 1, ... arg n>)
            //
            x = operations[i].methodIndexInCP;
            invokeInstructions.append(new INVOKEVIRTUAL(x));                        // Stack:  => ..., returnvalue

            // Wrap primitive return values into their corresponding wrapper objects
            String type = operations[i].getReturnType();

            if (isPrimitive(type))
            {
               invokeInstructions.append(convertPrimitiveToObject(cp, type));       // Stack:  => ..., objectref [wrapper]
               invokeInstructions.append(new ARETURN());                            // Stack:  => <empty>
            }
            else if (type.equals(Void.TYPE.getName()))
            {
               invokeInstructions.append(new ACONST_NULL());                        // Stack:  => ..., null
               invokeInstructions.append(new ARETURN());                            // Stack:  => <empty>
            }
            else
            {
               invokeInstructions.append(new ARETURN());                            // Stack:  => <empty>
            }
         }
      }

      //
      //  super.invoke(actionName, args, signature)  if no match was found
      //
      InstructionHandle jumpToSuperInvoke =
            invokeInstructions.append(new ALOAD(0));                                // Stack:  => ..., this
      invokeInstructions.append(new ALOAD(1));                                      // Stack:  => ..., this, arg1 [String]
      invokeInstructions.append(new ALOAD(2));                                      // Stack:  => ..., this, arg1 [String], arg2 [Object[]]
      invokeInstructions.append(new ALOAD(3));                                      // Stack:  => ..., this, arg1 [String], arg2 [Object[]], arg3 [String[]]
      invokeInstructions.append(new INVOKESPECIAL(invokeIndex));                    // Stack:  => ..., reference [Object]
      invokeInstructions.append(new ARETURN());                                     // Stack:  => <empty>

      // set the jump targets
      ifOperationEqualsNull.setTarget(jumpToSuperInvoke);

      if (operations.length > 0)
      {
         // set the last else branch to call super.invoke
         if (operationElseIfBranch != null)
            operationElseIfBranch.setTarget(jumpToSuperInvoke);

         // set the try catch block limits
         beginTryBlock = beginTryBlock.getNext();
         endTryBlock = jumpToSuperInvoke.getPrev();
      }


      // exception handler (it's a cheap shot -- if there is any exception, re-invoke
      // on super class and let it handle all exceptions)
      InstructionHandle exceptionHandlerCode =
            invokeInstructions.append(new ALOAD(0));
      invokeInstructions.append(new ALOAD(1));
      invokeInstructions.append(new ALOAD(2));
      invokeInstructions.append(new ALOAD(3));
      invokeInstructions.append(new INVOKESPECIAL(invokeIndex));
      invokeInstructions.append(new ARETURN());

      MethodGen invokeMethod = new MethodGen(
            Constants.ACC_PUBLIC,
            Type.OBJECT,
            new Type[] {
                  Type.STRING,
                  new ArrayType(Object.class.getName(), 1),
                  new ArrayType(String.class.getName(), 1)
            },
            new String[] {
                  "operationName",
                  "args",
                  "signature"
            },
            "invoke",
            className, invokeInstructions, cp
      );
      invokeMethod.setMaxLocals(7);
      invokeMethod.setMaxStack(calculateMaxStackSize(info));

      invokeMethod.addException(ReflectionException.class.getName());
      invokeMethod.addException(MBeanException.class.getName());

      if (operations.length > 0) {
         invokeMethod.addExceptionHandler(beginTryBlock, endTryBlock, exceptionHandlerCode, new ObjectType("java.lang.Throwable"));
      }

      return invokeMethod;
   }

   private static int calculateMaxStackSize(MBeanInfo info)
   {
      MBeanOperationInfo[] operations = info.getOperations();
      int maxSize = 7;

      for (int i = 0; i < operations.length; ++i)
      {
         if (operations[i].getSignature().length > maxSize + 2)
            maxSize = operations[i].getSignature().length + 2;
      }

      return maxSize;
   }

   /**
    * Converts a reference of a primitve wrapper object into a primite value type.
    * This method assumes that the wrapper object reference is already loaded at the
    * top of the operand stack. The stack is modified so that the object reference
    * to a primitive wrapper is replaced by the corresponding value in the stack.
    *
    * @param   cp    constant pool
    * @param   type  class name of the primitive wrapper object to convert
    * @return  an instruction list that replaces an object reference of a primitive
    *          wrapper object to its corresponding value in the operand stack
    */
   protected static InstructionList convertObjectToPrimitive(ConstantPoolGen cp, String type)
   {
      InstructionList il = new InstructionList();

      int intValueIndex     = cp.addMethodref(Integer.class.getName(), "intValue", "()I");
      int byteValueIndex    = cp.addMethodref(Byte.class.getName(), "byteValue", "()B");
      int charValueIndex    = cp.addMethodref(Character.class.getName(), "charValue", "()C");
      int doubleValueIndex  = cp.addMethodref(Double.class.getName(), "doubleValue", "()D");
      int floatValueIndex   = cp.addMethodref(Float.class.getName(), "floatValue", "()F");
      int longValueIndex    = cp.addMethodref(Long.class.getName(), "longValue", "()J");
      int shortValueIndex   = cp.addMethodref(Short.class.getName(), "shortValue", "()S");
      int booleanValueIndex = cp.addMethodref(Boolean.class.getName(), "booleanValue", "()Z");

      //
      // Assumes the wrapper object reference is on top of the stack
      //

      if (type.equals(Integer.TYPE.getName()))
      {
         int x = cp.addClass("java.lang.Integer");
         il.append(new CHECKCAST(x));                                            // Stack:  => ..., type [Integer]
         il.append(new INVOKEVIRTUAL(intValueIndex));                            // Stack:  => ..., value [int]
      }

      else if (type.equals(Byte.TYPE.getName()))
      {
         int x = cp.addClass("java.lang.Byte");
         il.append(new CHECKCAST(x));                                            // Stack:  => ..., type [Boolean]
         il.append(new INVOKEVIRTUAL(byteValueIndex));                           // Stack:  => ..., 0 | 1 [boolean]
      }

      else if (type.equals(Character.TYPE.getName()))
      {
         int x = cp.addClass("java.lang.Character");
         il.append(new CHECKCAST(x));                                            // Stack:  => ..., type [Character]
         il.append(new INVOKEVIRTUAL(charValueIndex));                           // Stack:  => ..., value [char]
      }

      else if (type.equals(Double.TYPE.getName()))
      {
         int x = cp.addClass("java.lang.Double");
         il.append(new CHECKCAST(x));                                            // Stack:  => ..., type [Double]
         il.append(new INVOKEVIRTUAL(doubleValueIndex));                         // Stack:  => ..., value [double]
      }

      else if (type.equals(Float.TYPE.getName()))
      {
         int x = cp.addClass("java.lang.Float");
         il.append(new CHECKCAST(x));                                            // Stack:  => ..., type [Float]
         il.append(new INVOKEVIRTUAL(floatValueIndex));                          // Stack:  => ..., value [float]
      }

      else if (type.equals(Long.TYPE.getName()))
      {
         int x = cp.addClass("java.lang.Long");
         il.append(new CHECKCAST(x));                                            // Stack:  => ..., type [Long]
         il.append(new INVOKEVIRTUAL(longValueIndex));                           // Stack:  => ..., value [long]
      }

      else if (type.equals(Short.TYPE.getName()))
      {
         int x = cp.addClass("java.lang.Short");
         il.append(new CHECKCAST(x));                                            // Stack:  => ..., type [Short]
         il.append(new INVOKEVIRTUAL(shortValueIndex));                          // Stack:  => ..., value [short]
      }

      else if (type.equals(Boolean.TYPE.getName()))
      {
         int x = cp.addClass("java.lang.Boolean");
         il.append(new CHECKCAST(x));                                            // Stack:  => ..., type [Boolean]
         il.append(new INVOKEVIRTUAL(booleanValueIndex));                        // Stack:  => ..., value [boolean]
      }

      return il;
   }


   /**
    * Converts a primitive into its corresponding object wrapper reference.
    * This method assumes the primitve is already pushed to the top of the operand
    * stack. The stack is modified so that the primitive value is replaced
    * by a reference to its corresponding wrapper object that has been
    * initialized to contain the same value.
    *
    * @param   cp    constant pool
    * @param   type  type string of the primitive, for example {@link java.lang.Integer#TYPE Integer.TYPE.getName()}
    * @return  an instruction list that replaces the primitive type at the top of
    *          the operand stack with its corresponding, initialized, wrapper object
    */
   protected static InstructionList convertPrimitiveToObject(ConstantPoolGen cp, String type)
   {
      InstructionList il = new InstructionList();

      if (type.equals(Boolean.TYPE.getName()))
      {
         int x = cp.addClass("java.lang.Boolean");
         int constrIndex = cp.addMethodref("java.lang.Boolean", "<init>", "(B)V");

         il.append(new ISTORE(4));
         il.append(new NEW(x));
         il.append(new ASTORE(5));
         il.append(new ALOAD(5));
         il.append(new ILOAD(4));
         il.append(new INVOKESPECIAL(constrIndex));
         il.append(new ALOAD(5));
      }

      else if (type.equals(Short.TYPE.getName()))
      {
         int x = cp.addClass("java.lang.Short");
         int constrIndex = cp.addMethodref("java.lang.Short", "<init>", "(S)V");

         il.append(new ISTORE(4));
         il.append(new NEW(x));
         il.append(new ASTORE(5));
         il.append(new ALOAD(5));
         il.append(new ILOAD(4));
         il.append(new INVOKESPECIAL(constrIndex));
         il.append(new ALOAD(5));
      }

      else if (type.equals(Long.TYPE.getName()))
      {
         int x = cp.addClass("java.lang.Long");
         int constrIndex = cp.addMethodref("java.lang.Long", "<init>", "(J)V");

         il.append(new LSTORE(4));
         il.append(new NEW(x));
         il.append(new ASTORE(6));
         il.append(new ALOAD(6));
         il.append(new LLOAD(4));
         il.append(new INVOKESPECIAL(constrIndex));
         il.append(new ALOAD(6));
      }

      else if (type.equals(Integer.TYPE.getName()))
      {
         int x = cp.addClass("java.lang.Integer");
         int constrIndex = cp.addMethodref("java.lang.Integer", "<init>", "(I)V");

         il.append(new ISTORE(4));
         il.append(new NEW(x));
         il.append(new ASTORE(5));
         il.append(new ALOAD(5));
         il.append(new ILOAD(4));
         il.append(new INVOKESPECIAL(constrIndex));
         il.append(new ALOAD(5));
      }

      else if (type.equals(Float.TYPE.getName()))
      {
         int x = cp.addClass("java.lang.Float");
         int constrIndex = cp.addMethodref("java.lang.Float", "<init>", "(F)V");

         il.append(new FSTORE(4));
         il.append(new NEW(x));
         il.append(new ASTORE(5));
         il.append(new ALOAD(5));
         il.append(new FLOAD(4));
         il.append(new INVOKESPECIAL(constrIndex));
         il.append(new ALOAD(5));
      }

      else if (type.equals(Double.TYPE.getName()))
      {
         int x = cp.addClass("java.lang.Double");
         int constrIndex = cp.addMethodref("java.lang.Double", "<init>", "(D)V");

         il.append(new DSTORE(4));
         il.append(new NEW(x));
         il.append(new ASTORE(6));
         il.append(new ALOAD(6));
         il.append(new DLOAD(4));
         il.append(new INVOKESPECIAL(constrIndex));
         il.append(new ALOAD(6));
      }

      else if (type.equals(Character.TYPE.getName()))
      {
         int x = cp.addClass("java.lang.Character");
         int constrIndex = cp.addMethodref("java.lang.Character", "<init>", "(C)V");

         il.append(new ISTORE(4));
         il.append(new NEW(x));
         il.append(new ASTORE(5));
         il.append(new ALOAD(5));
         il.append(new ILOAD(4));
         il.append(new INVOKESPECIAL(constrIndex));
         il.append(new ALOAD(5));
      }

      else if (type.equals(Byte.TYPE.getName()))
      {
         int x = cp.addClass("java.lang.Byte");
         int constrIndex = cp.addMethodref("java.lang.Byte", "<init>", "(B)V");

         il.append(new ISTORE(4));
         il.append(new NEW(x));
         il.append(new ASTORE(5));
         il.append(new ALOAD(5));
         il.append(new ILOAD(4));
         il.append(new INVOKESPECIAL(constrIndex));
         il.append(new ALOAD(5));
      }

      return il;
   }


   /**
    * Returns a subset of MBean's operations. Overloaded operations are not supported yet,
    * so they're left out of the list and their invocations are delegated to the reflection
    * based super class. <p>
    *
    * Overloaded operations that differ in their arg list length may be able to gain in
    * performance if implemented directly with byte code. Overloaded operations with
    * equal arg list length may not show much difference compared to ternary search tree
    * based resolver.
    */
   protected static MethodEntry[] getOperations(MBeanInfo info)
   {
      HashMap operationMap            = new HashMap();
      ArrayList overloadList          = new ArrayList();
      MBeanOperationInfo[] operations = info.getOperations();

      for (int i = 0; i < operations.length; ++i)
      {
         String methodName = operations[i].getName();

         if (operationMap.containsKey(methodName))
            overloadList.add(methodName);
         else
            operationMap.put(methodName, new MethodEntry(operations[i]));
      }

      // method overloading not supported yet
      Iterator it = overloadList.iterator();
      while (it.hasNext())
         operationMap.remove(it.next());

      return (MethodEntry[])operationMap.values().toArray(new MethodEntry[0]);
   }


   // Inner classes -------------------------------------------------
   private static class MethodEntry extends MBeanOperationInfo {

      String methodDescriptor = null;
      int nameIndexInCP       = -1;
      int methodIndexInCP     = -1;

      public MethodEntry(MBeanOperationInfo info)
      {
         super(info.getName(), info.getDescription(), info.getSignature(), info.getReturnType(), info.getImpact());

         this.methodDescriptor = getMethodDescriptor(info.getSignature(), info.getReturnType());
      }
   }


}




