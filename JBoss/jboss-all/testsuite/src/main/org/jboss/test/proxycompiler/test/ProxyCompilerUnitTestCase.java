/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.proxycompiler.test;

import java.lang.reflect.Method;

import junit.framework.*;

import org.apache.log4j.Category;

import org.jboss.proxy.compiler.*;

/**
 * Test the ability of the proxy compiler to create proxies for
 * simple/complex interfaces and classes.
 *
 * @version <tt>$Revision: 1.2 $</tt>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class ProxyCompilerUnitTestCase
   extends TestCase 
{
   private static final Category log = Category.getInstance(ProxyCompilerUnitTestCase.class);

   protected InvocationHandler handler;

   public ProxyCompilerUnitTestCase(String name)
   {
      super(name);
   }

   protected void setUp() throws Exception
   {
      super.setUp();

      handler = new LoggingInvocationHandler(log);
   }

   /**
    * Create a value for the given type.  If non-void primitive, then
    * return a wrapper, else return null.
    */
   protected static Object createValue(Class type)
   {
      Object rv = null;

      if (type.isPrimitive()) {
         if (type == Boolean.TYPE)
            rv = new Boolean(false);
         
         else if (type == Byte.TYPE)
            rv = new Byte((byte)0);
         
         else if (type == Character.TYPE)
            rv = new Character((char)0);
         
         else if (type == Short.TYPE)
            rv = new Short((short)0);
         
         else if (type == Integer.TYPE)
            rv = new Integer(0);
         
         else if (type == Long.TYPE)
            rv = new Long(0);
         
         else if (type == Float.TYPE)
            rv = new Float(0);
         
         else if (type == Double.TYPE)
            rv = new Double(0);
         
         else if (type == Void.TYPE)
            rv = null;
         
         else 
            throw new Error("unreachable");
      }

      return rv;
   }

   /**
    * An InvocationHandler which simplly logs all calls
    */
   public static class LoggingInvocationHandler
      implements InvocationHandler
   {
      private Category log;

      public LoggingInvocationHandler(final Category log) {
         this.log = log;
      }

      public Object invoke(Object dummy, Method method, Object[] args) throws Throwable {
         log.debug("invoked: " + dummy + "," + method + "," + args);

         log.debug("arguments: ");
         for (int i=0; i<args.length; i++) {
            String msg = "   arg" + i + ": " + args[i];
            if (args[i] != null) msg += ", type=" + args[i].getClass();
            log.debug(msg);
         }

         Object value = createValue(method.getReturnType());
         log.debug("return value: " + value);
         return value;
      }
   }

   protected Object createProxy(Class type) throws Exception
   {
      Object proxy = Proxy.newProxyInstance(type.getClass().getClassLoader(),
                                            new Class[] { type },
                                            handler);

      log.debug("new proxy: " + proxy);

      return proxy;
   }

   protected void invokeDeclaredMethods(Object obj, Class type) throws Exception
   {
      Method[] methods = type.getDeclaredMethods();

      for (int i=0; i<methods.length; i++) {
         log.debug("Invoking method: "  + methods[i]);

         Class[] pTypes = methods[i].getParameterTypes();
         Object[] args = new Object[pTypes.length];

         // create some dummy arg values
         for (int j=0; j<args.length; j++) {
            args[j] = createValue(pTypes[j]);
         }

         Object rv = methods[i].invoke(obj, args);

         log.debug("Method returned: " + rv);
      }
   }

   public static interface EmptyInterface
   {
      // empty
   }

   public void testEmptyInterface() throws Exception
   {
      createProxy(EmptyInterface.class);
   }

   public static interface SimpleInterface
   {
      void simple();
   }

   public void testSimpleInterface() throws Exception
   {
      Object obj = createProxy(SimpleInterface.class);
      invokeDeclaredMethods(obj, SimpleInterface.class);
   }

   public static interface ReturnValues
   {
      // returns, no args

      void noargs();

      Object Object_noargs();

      byte byte__noargs();

      boolean boolean_noargs();

      char char_noargs();

      int int_noargs();

      short short_noargs();

      long long_noargs();

      float float_noargs();

      double double_noargs();
   }

   public void testReturnValues() throws Exception
   {
      Object obj = createProxy(ReturnValues.class);
      invokeDeclaredMethods(obj, ReturnValues.class);
   }

   public static interface CommonMethodParameters
   {
      // no returns, different args

      void boolean1(boolean a);

      void boolean2(boolean a, boolean b);

      void boolean3(boolean a, boolean b, boolean c);

      void boolean4(boolean a, boolean b, boolean c, boolean d);

      void byte1(byte a);

      void byte2(byte a, byte b);

      void byte3(byte a, byte b, byte c);

      void byte4(byte a, byte b, byte c, byte d);

      void char1(char a);

      void char2(char a, char b);

      void char3(char a, char b, char c);

      void char4(char a, char b, char c, char d);

      void short1(short a);

      void short2(short a, short b);

      void short3(short a, short b, short c);

      void short4(short a, short b, short c, short d);

      void int1(int a);

      void int2(int a, int b);

      void int3(int a, int b, int c);

      void int4(int a, int b, int c, int d);

      void long1(long a);

      void long2(long a, long b);

      void long3(long a, long b, long c);

      void long4(long a, long b, long c, long d);

      void long5(long a, long b, long c, long d, long e);

      void long6(long a, long b, long c, long d, long e, long f);

      void float1(float a);

      void float2(float a, float b);

      void float3(float a, float b, float c);

      void float4(float a, float b, float c, float d);

      void double1(double a);

      void double2(double a, double b);

      void double3(double a, double b, double c);

      void double4(double a, double b, double c, double d);

      void double5(double a, double b, double c, double d, double e);

      void double6(double a, double b, double c, double d, double e, double f);

      void Object1(Object a);

      void Object2(Object a, Object b);

      void Object3(Object a, Object b, Object c);

      void Object4(Object a, Object b, Object c, Object d);
   }

   public void testCommonMethodParameters() throws Exception
   {
      Object obj = createProxy(CommonMethodParameters.class);
      invokeDeclaredMethods(obj, CommonMethodParameters.class);
   }

   public static abstract class SimpleAbstractClass
   {
      public abstract void test1();

      public abstract Object test2();

      public abstract Object test3(Object obj);

      public abstract Object test4(Object obj) throws Exception;
   }

   public void testSimpleAbstractClass() throws Exception
   {
      Object obj = createProxy(SimpleAbstractClass.class);
      invokeDeclaredMethods(obj, SimpleAbstractClass.class);
   }

   public static interface ComplexInterface
      extends EmptyInterface, SimpleInterface, CommonMethodParameters
   {
      interface NestedInterface
         extends CommonMethodParameters
      {
         // blah
      }

      abstract class NestedAbstractClass
         extends SimpleAbstractClass
      {
         // blah
      }

      class ConcreteClass
      {
         // blah
      }

      long complex1(boolean a, byte b, char c, short d, int e, long f, float g, double h, Object i)
         throws Exception, Error, Throwable;

      Object[] complex2(boolean[] a, byte[] b, char[] c, short[] d, int[] e, long[] f, float[] g, double[] h, Object[] i)
         throws Exception, Error, Throwable;

      Object[][] complex3(boolean[][] a, byte[][] b, char[][] c, short[][] d, int[][] e, long[][] f, float[][] g, double[][] h, Object[][] i)
         throws Exception, Error, Throwable;

      Object[][][] complex4(boolean[] a, byte[][] b, char[][][] c, short[][][][] d, int[][][][][] e, long[][][][][][] f, float[][][][][][][] g, double[][][][][][][][] h, Object[][][][][][][][][] i)
         throws Exception, Error, Throwable;
   }

   public void testComplexInterface() throws Exception
   {
      Object obj = createProxy(ComplexInterface.class);
      invokeDeclaredMethods(obj, EmptyInterface.class);
      invokeDeclaredMethods(obj, SimpleInterface.class);
      invokeDeclaredMethods(obj, CommonMethodParameters.class);
      invokeDeclaredMethods(obj, ComplexInterface.class);
   }
}
