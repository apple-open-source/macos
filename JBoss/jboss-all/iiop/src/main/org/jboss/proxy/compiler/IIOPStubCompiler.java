/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.proxy.compiler; // IIOPStubCompiler is in this package 
                                  // because it calls some ProxyAssembler 
                                  // methods that currently are package 
                                  // accessible

import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.rmi.RemoteException;

import org.jboss.iiop.rmi.AttributeAnalysis;
import org.jboss.iiop.rmi.ExceptionAnalysis;
import org.jboss.iiop.rmi.InterfaceAnalysis;
import org.jboss.iiop.rmi.OperationAnalysis;
import org.jboss.iiop.rmi.RMIIIOPViolationException;
import org.jboss.iiop.rmi.marshal.CDRStream;
import org.jboss.iiop.rmi.marshal.strategy.StubStrategy;
import org.jboss.proxy.ejb.DynamicIIOPStub;

/**
 * Utility class responsible for the dynamic generation of bytecodes of
 * IIOP stub classes.
 *
 * @author Unknown
 * @author <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @version $Revision: 1.3.2.1 $
 */
public class IIOPStubCompiler
{
   // Constants  --------------------------------------------------------------

   /**
    * Parameter type array for <code>StubStrategy.forMethod()</code> 
    * invocations. 
    */
   private static final Class[] stubStrategyParams = {
      String[].class, String[].class, String[].class, String.class, 
      ClassLoader.class
   };
   
   /**
    * Parameter type array for <code>DynamicIIOPStub.invoke()</code> 
    * invocations.
    */
   private static final Class[] invokeParams = {
      String.class, StubStrategy.class, Object[].class
   };

   /**
    * Parameter type array for
    * <code>org.omg.CORBA.ORB.object_to_string()</code> invocations.
    */ 
   private static final Class[] corbaObjectParam = { 
      org.omg.CORBA.Object.class 
   };
   
   /**
    * Parameter type array for a method that takes a single string parameter.
    */ 
   private static final Class[] stringParam = {  
      String.class 
   };

   /**
    * Parameter type array for a method that takes no parameters.
    */ 
   private static final Class[] noParams = { };

   // Private methods --------------------------------------------------------

   /**
    * Returns the name of the stub strategy field associated with the method 
    * whose index is <code>methodIndex</code>.
    */ 
   private static String strategy(int methodIndex) {
      return "$s" + methodIndex;
   }
   
   /**
    * Returns the name of static initializer method associated with the method
    * whose index is <code>methodIndex</code>.
    */ 
   private static String init(int methodIndex) {
      return "$i" + methodIndex;      
   }

   /**
    * Generates the code of a given method within a stub class.
    *
    * @param asm           the <code>ProxyAssembler</code> used to assemble
    *                      the method code
    * @param superclass    the superclass of the stub class within which the 
    *                      method will be generated
    * @param m             a <code>Method</code> instance describing the
    *                      method declaration by an RMI/IDL interface
    * @param idlName       a string with the method name mapped to IDL
    * @param strategyField a string with the name of the strategy field that
    *                      will be associated with the generated method
    * @param initMethod    a string with the name of the static initialization
    *                      method that will be associated with the generated
    *                      method.
    */
   private static void generateMethodCode(ProxyAssembler asm,
                                          Class superclass,
                                          Method m, 
                                          String idlName, 
                                          String strategyField, 
                                          String initMethod)
   {
      String methodName = m.getName();
      Class returnType = m.getReturnType();
      Class[] paramTypes = m.getParameterTypes();
      Class[] exceptions = m.getExceptionTypes();

      // Generate a static field with the StubStrategy for the method
      asm.addMember(Modifier.PRIVATE + Modifier.STATIC, 
                    StubStrategy.class, null, strategyField);

      // Generate the method code
      asm.addMember(Modifier.PUBLIC + Modifier.FINAL,
                    returnType, methodName, paramTypes, exceptions);
      {
         // The method code issues a call
         // super.invoke*(idlName, strategyField, args)
         asm.pushLocal(0); // super (this)
         asm.pushConstant(idlName);
         asm.pushField(asm, strategyField);
         // Push args 
         if (paramTypes.length == 0) {
            asm.pushField(Util.class, "NOARGS");
         } else {
            asm.pushConstant(paramTypes.length);
            asm.pushNewArray(Object.class);
            for (int j = 0; j < paramTypes.length; j++) {
               Class t = paramTypes[j];
               asm.dup();
               asm.pushConstant(j);
               asm.pushLocal(1 + j);
               if (t.isPrimitive()) {
                  asm.invoke(Util.class, "wrap", new Class[]{ t });
               }
               asm.setElement(Object.class);
            }
         }
         // Generate the call to a invoke* method ot the superclass
         String invoke = "invoke";
         if (returnType.isPrimitive() && returnType != Void.TYPE) {
            String typeName = returnType.getName();
            invoke += (Character.toUpperCase(typeName.charAt(0))
                       + typeName.substring(1));
         }
         asm.invoke(superclass, invoke, invokeParams);
         if (!returnType.isPrimitive() && returnType != Object.class) {
            asm.checkCast(returnType);
         }
         asm.ret();
      }

      // Generate a static method that initializes the method's strategy field
      asm.addMember(Modifier.PRIVATE + Modifier.STATIC, 
                    Void.TYPE, initMethod, noParams, null);
      {
         int i;
         int len;

         // Push first argument for StubStrategy constructor:
         // array with abbreviated names of the param marshallers
         len = paramTypes.length;
         asm.pushConstant(len);
         asm.pushNewArray(String.class);
         for (i = 0; i < len; i++) {
            asm.dup();
            asm.pushConstant(i);
            asm.pushConstant(CDRStream.abbrevFor(paramTypes[i]));
            asm.setElement(String.class);
         }

         // Push second argument for StubStrategy constructor:
         // array with exception repository ids
         len = exceptions.length; 
         int n = 0;
         for (i = 0; i < len; i++) {
            if (!RemoteException.class.isAssignableFrom(exceptions[i])) {
               n++;
            }
         }
         asm.pushConstant(n);
         asm.pushNewArray(String.class);
         try {
            int j = 0;
            for (i = 0; i < len; i++) {
               if (!RemoteException.class.isAssignableFrom(exceptions[i])) {
                  asm.dup();
                  asm.pushConstant(j);
                  asm.pushConstant(
                        ExceptionAnalysis.getExceptionAnalysis(exceptions[i])
                                         .getExceptionRepositoryId());
                  asm.setElement(String.class);
                  j++;
               }
            }
         }
         catch (RMIIIOPViolationException e) {
            throw new RuntimeException("Cannot obtain "
                                       + "exception repository id for " 
                                       + exceptions[i].getName() + ":\n" + e);
         }

         // Push third argument for StubStrategy constructor:
         // array with exception class names
         asm.pushConstant(n);
         asm.pushNewArray(String.class);
         int j = 0;
         for (i = 0; i < len; i++) {
            if (!RemoteException.class.isAssignableFrom(exceptions[i])) {
               asm.dup();
               asm.pushConstant(j);
               asm.pushConstant(exceptions[i].getName());
               asm.setElement(String.class);
               j++;
            }
         }

         // Push fourth argument for StubStrategy constructor:
         // abbreviated name of the return value marshaller
         asm.pushConstant(CDRStream.abbrevFor(returnType));

         // Push fifth argument for StubStrategy constructor:
         // null (no ClassLoader specified)
         asm.pushField(Util.class, "NULLCL");

         // Constructs the StubStrategy
         asm.invoke(StubStrategy.class, "forMethod", stubStrategyParams);

         // Set the strategy field of this stub class
         asm.setField(asm, strategyField);

         asm.ret();
      }
   }                         

   /**
    * Generates the bytecodes of a stub class for a given interface.
    * 
    * @param intfaceAnalysis  an <code>InterfaceAnalysis</code> instance
    *                         describing the RMI/IIOP interface to be
    *                         implemented by the stub class
    * @param superclass       the superclass of the stub class
    * @param stubClassName    the name of the stub class 
    * @return                 a byte array with the generated bytecodes.
    */
   private static byte[] generateCode(InterfaceAnalysis interfaceAnalysis,
                                      Class superclass, String stubClassName)
   {
      ProxyAssembler asm =
         new ProxyAssembler(stubClassName,
                            Modifier.PUBLIC | Modifier.FINAL,
                            superclass, 
                            new Class[] { interfaceAnalysis.getCls() });

      int methodIndex = 0;
      
      AttributeAnalysis[] attrs = interfaceAnalysis.getAttributes();
      for (int i = 0; i < attrs.length; i++) {
         OperationAnalysis op = attrs[i].getAccessorAnalysis();
         generateMethodCode(asm, superclass, op.getMethod(), op.getIDLName(),
                            strategy(methodIndex), init(methodIndex));
         methodIndex++;
         op = attrs[i].getMutatorAnalysis();
         if (op != null) {
            generateMethodCode(asm, superclass, 
                               op.getMethod(), op.getIDLName(),
                               strategy(methodIndex), init(methodIndex));
            methodIndex++;
         }
      }
      
      OperationAnalysis[] ops = interfaceAnalysis.getOperations();
      for (int i = 0; i < ops.length; i++) {
         generateMethodCode(asm, superclass, 
                            ops[i].getMethod(), ops[i].getIDLName(),
                            strategy(methodIndex), init(methodIndex));
         methodIndex++;
      }

      // Generate the constructor
      asm.addMember(Modifier.PUBLIC, Void.TYPE, "<init>", noParams, null);
      {
         asm.pushLocal(0);
         asm.invoke(superclass, "<init>", noParams);
         asm.ret();
      }

      // Generate the toString() method
      asm.addMember(Modifier.PUBLIC, String.class, "toString", noParams, null);
      {
         {
            asm.pushConstant("JBossDynStub[" 
                             + interfaceAnalysis.getCls().getName() + ", ");
            {
               {
                  asm.pushLocal(0);
                  asm.invoke(superclass, "_orb", noParams);
               }
               asm.pushLocal(0);
               asm.invoke(org.omg.CORBA.ORB.class, 
                          "object_to_string", corbaObjectParam); 
            }
            asm.invoke(String.class, "concat", stringParam);
         }
         asm.pushConstant("]");
         asm.invoke(String.class, "concat", stringParam);
         asm.ret();
      }

      // Generate the method _ids(), declared as abstract in ObjectImpl
      String[] ids = interfaceAnalysis.getAllTypeIds();
      asm.addMember(Modifier.PRIVATE + Modifier.STATIC, String[].class, 
                    null, "$ids");
      asm.addMember(Modifier.PUBLIC + Modifier.FINAL, 
                    String[].class, "_ids", noParams, null);
      {
         asm.pushField(asm, "$ids");
         asm.ret();
      }

      // Generate the static initializer
      asm.addMember(Modifier.STATIC, Void.TYPE, "<clinit>", noParams, null);
      {
         //asm.pushField(System.class, "err");
         //asm.pushConstant("ENTERING CLASS INITIALIZER !!!!!!!!!!!!!!!!!!!!");
         //asm.invoke(java.io.PrintStream.class, "println", stringParam);

         asm.pushConstant(ids.length);
         asm.pushNewArray(String.class);
         for (int i = 0; i < ids.length; i++) {
            asm.dup();
            asm.pushConstant(i);
            asm.pushConstant(ids[i]);
            asm.setElement(String.class);
         }
         asm.setField(asm, "$ids");
         
         int n = methodIndex; // last methodIndex + 1
         for (methodIndex = 0; methodIndex < n; methodIndex++) {
            asm.invoke(asm, init(methodIndex), noParams);
         }

         //asm.pushField(System.class, "err");
         //asm.pushConstant("LEAVING CLASS INITIALIZER !!!!!!!!!!!!!!!!!!!!!");
         //asm.invoke(java.io.PrintStream.class, "println", stringParam);

         asm.ret();
      }

      return asm.getCode();
   }

   /**
    * Generates the bytecodes of a stub class for a given interface.
    * 
    * @param intfaceAnalysis  an <code>InterfaceAnalysis</code> instance
    *                         describing the RMI/IIOP interface to be
    *                         implemented by the stub class
    * @param superclass       the superclass of the stub class
    * @param stubClassName    the name of the stub class 
    * @return                 a byte array with the generated bytecodes.
    */
   private static byte[] makeCode(InterfaceAnalysis interfaceAnalysis,
                                  Class superclass, String stubClassName)
   {
      
      byte code[] = generateCode(interfaceAnalysis, superclass, stubClassName);
      //try {
      //   String fname = stubClassName;
      //   fname = fname.substring(1 + fname.lastIndexOf('.')) + ".class";
      //   fname = "/tmp/" + fname;
      //   java.io.OutputStream cf = new java.io.FileOutputStream(fname);
      //   cf.write(code);
      //   cf.close();
      //   System.err.println("wrote " + fname);
      //} 
      //catch(java.io.IOException ee) { 
      //}
      return code;
   }
   
   // Public method ----------------------------------------------------------

   /**
    * Generates the bytecodes of a stub class for a given interface.
    * 
    * @param intf          RMI/IIOP interface to be implemented by the 
    *                      stub class
    * @param stubClassName the name of the stub class 
    * @return              a byte array with the generated bytecodes;
    */
   public static byte[] compile(Class intf, String stubClassName)
   {
      InterfaceAnalysis interfaceAnalysis = null;
      
      try {
         interfaceAnalysis = InterfaceAnalysis.getInterfaceAnalysis(intf);
      }
      catch (RMIIIOPViolationException e) {
         throw new RuntimeException("RMI/IIOP Violation:\n" + e);
      }
      return makeCode(interfaceAnalysis, DynamicIIOPStub.class, stubClassName);
   }

}
