/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.metadata;


import junit.framework.TestCase;
import junit.framework.AssertionFailedError;

import java.lang.reflect.Method;

import javax.management.MBeanAttributeInfo;
import javax.management.IntrospectionException;

import test.compliance.metadata.support.Trivial;

/**
 * Tests MBeanAttributeInfo.
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.1 $ 
 */
public class MBeanAttributeInfoTEST extends TestCase
{
   public MBeanAttributeInfoTEST(String s)
   {
      super(s);
   }

   /**
    * Tests <tt>MBeanAttributeInfo(String name, String descr, Method getter, Method setter)</tt> constructor.
    */
   public void testConstructorWithAccessorMethods()
   {
      try 
      {
         Class c = Trivial.class;
         Method getter = c.getMethod("getSomething", new Class[0]);
         Method setter = c.getMethod("setSomething", new Class[] { String.class });
         
         MBeanAttributeInfo info = new MBeanAttributeInfo("Something", "a description", getter, setter);
         
         assertTrue(info.getDescription().equals("a description"));
         assertTrue(info.getName().equals("Something"));
         assertTrue(info.getType().equals("java.lang.String"));
         assertTrue(info.isReadable() == true);
         assertTrue(info.isWritable() == true);
         assertTrue(info.isIs() == false);         
      }
      catch (AssertionFailedError e)
      {
         throw e;
      }
      catch (Throwable t)
      {
         t.printStackTrace();
         fail("Unexpected error: " + t.toString());
      }
   }
 
   /**
    * Tests <tt>MBeanAttributeInfo(String name, String descr, Method getter, Method setter)</tt> with misplaced accessor methods.
    */
   public void testConstructorWithMisplacedAccessorMethods()
   {
      try
      {
         Class c = Trivial.class;
         Method getter = c.getMethod("getSomething", new Class[0]);
         Method setter = c.getMethod("setSomething", new Class[] { String.class });
         
         MBeanAttributeInfo info = new MBeanAttributeInfo("Something", "a description", setter, getter);
         
         // shouldn't reach here
         fail("Introspection exception should have been thrown.");
      }
      catch (IntrospectionException e)
      {
         // this is expected
      }
      catch (AssertionFailedError e)
      {
         throw e;
      }
      catch (Throwable t)
      {
         t.printStackTrace();
         fail("Unexpected error: " + t.toString());
      }
   }
   
   /**
    * Tests <tt>MBeanAttributeInfo(String name, String descr, Method getter, Method setter)</tt> with invalid getter method.
    */
   public void testConstructorWithInvalidGetterMethod()
   {
      try
      {
         Class c = Trivial.class;
         Method getter = c.getMethod("getSomethingInvalid", new Class[] { Object.class });
         Method setter = c.getMethod("setSomethingInvalid", new Class[] { String.class });
         
         MBeanAttributeInfo info = new MBeanAttributeInfo("Something", "a description", getter, setter);
         
         // shouldn't reach here
         fail("Introspection exception should have been thrown.");
      }
      catch (IntrospectionException e)
      {
         // this is expected
      }
      catch (AssertionFailedError e)
      {
         throw e;
      }
      catch (Throwable t)
      {
         t.printStackTrace();
         fail("Unexpected error: " + t.toString());
      }
   }
   
   /**
    * Tests <tt>MBeanAttributeInfo(String name, String descr, Method getter, Method setter)</tt> with invalid getter method (void return type).
    */
   public void testConstructorWithInvalidGetterMethod2()
   {
      try
      {
         Class c = Trivial.class;
         Method getter = c.getMethod("getSomethingInvalid2", new Class[] { } );
         Method setter = c.getMethod("setSomethingInvalid2", new Class[] { String.class });
         
         MBeanAttributeInfo info = new MBeanAttributeInfo("Something", "a description", getter, setter);
         
         // shouldn't reach here
         fail("Introspection exception should have been thrown.");
      }
      catch (IntrospectionException e)
      {
         // this is expected
      }
      catch (AssertionFailedError e)
      {
         throw e;
      }
      catch (Throwable t)
      {
         t.printStackTrace();
         fail("Unexpected error: " + t.toString());
      }
   }
            
}
