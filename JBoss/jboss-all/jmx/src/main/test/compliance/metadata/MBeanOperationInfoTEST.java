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

import javax.management.MBeanServerFactory;
import javax.management.MBeanServer;
import javax.management.MBeanOperationInfo;
import javax.management.MBeanParameterInfo;
import javax.management.JMRuntimeException;

/**
 * Tests MBeanOperationInfo.
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.1 $ 
 */
public class MBeanOperationInfoTEST extends TestCase
{
   public MBeanOperationInfoTEST(String s)
   {
      super(s);
   }

   /**
    * Tests <tt>MBeanOperationInfo(String descr, Method m)</tt> constructor.
    */
   public void testConstructorWithMethod()
   {
      try 
      {
         Class c = this.getClass();
         Method m = c.getMethod("testConstructorWithMethod", new Class[0]);
         
         MBeanOperationInfo info = new MBeanOperationInfo("This is a description.", m);
         
         assertTrue(info.getDescription().equals("This is a description."));
         assertTrue(info.getName().equals(m.getName()));
         assertTrue(info.getReturnType().equals("void"));
         assertTrue(info.getSignature().length == 0);
         assertTrue(info.getImpact() == MBeanOperationInfo.UNKNOWN);
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
    * Tests <tt>MBeanOperationInfo(String name, String descr, MBeanParameterInfo[] sign, String returnType, int impact)</tt> constructor.
    */
   public void testConstructor()
   {
      try
      {
         MBeanOperationInfo info = new MBeanOperationInfo(
               "MyOperation",
               "This is a description.",
               new MBeanParameterInfo[] {
                        new MBeanParameterInfo("FooParam", "java.lang.Object", "description"),
                        new MBeanParameterInfo("BarParam", "java.lang.String", "description")
               },
               "java.util.StringBuffer",
               MBeanOperationInfo.INFO
         );
         
         assertTrue(info.getDescription().equals("This is a description."));
         assertTrue(info.getName().equals("MyOperation"));
         assertTrue(info.getReturnType().equals("java.util.StringBuffer"));
         assertTrue(info.getSignature().length == 2);
         assertTrue(info.getImpact() == MBeanOperationInfo.INFO);
         assertTrue(info.getSignature() [0].getName().equals("FooParam"));
         assertTrue(info.getSignature() [1].getName().equals("BarParam"));
         assertTrue(info.getSignature() [0].getDescription().equals("description"));
         assertTrue(info.getSignature() [1].getDescription().equals("description"));
         assertTrue(info.getSignature() [0].getType().equals("java.lang.Object"));
         assertTrue(info.getSignature() [1].getType().equals("java.lang.String"));
         
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
    * Tests the clone operation.
    */
   public void testClone()
   {
      try
      {
         MBeanOperationInfo info = new MBeanOperationInfo(
               "MyOperation",
               "This is a description.",
               new MBeanParameterInfo[] {
                        new MBeanParameterInfo("FooParam", "java.lang.Object", "description"),
                        new MBeanParameterInfo("BarParam", "java.lang.String", "description")
               },
               "java.util.StringBuffer",
               MBeanOperationInfo.ACTION_INFO
         );
         
         MBeanOperationInfo clone = (MBeanOperationInfo)info.clone();
         
         assertTrue(clone.getDescription().equals("This is a description."));
         assertTrue(clone.getName().equals("MyOperation"));
         assertTrue(clone.getReturnType().equals("java.util.StringBuffer"));
         assertTrue(clone.getSignature().length == 2);
         assertTrue(clone.getImpact() == MBeanOperationInfo.ACTION_INFO);
         assertTrue(clone.getSignature() [0].getName().equals("FooParam"));
         assertTrue(clone.getSignature() [1].getName().equals("BarParam"));
         assertTrue(clone.getSignature() [0].getDescription().equals("description"));
         assertTrue(clone.getSignature() [1].getDescription().equals("description"));
         assertTrue(clone.getSignature() [0].getType().equals("java.lang.Object"));
         assertTrue(clone.getSignature() [1].getType().equals("java.lang.String"));
         
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
    * Tests <tt>MBeanOperationInfo</tt> creation and <tt>getName()</tt> accessor with empty name.
    */
   public void testGetNameEmpty()
   {
      try
      {
         MBeanOperationInfo info1 = new MBeanOperationInfo(
               "",
               "This is a description.",
               new MBeanParameterInfo[] {
                        new MBeanParameterInfo("FooParam", "java.lang.Object", "description"),
                        new MBeanParameterInfo("BarParam", "java.lang.String", "description")
               },
               "java.util.StringBuffer",
               MBeanOperationInfo.ACTION_INFO
         );
         
         assertTrue(info1.getName().equals(""));
         
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
    * Tests <tt>MBeanOperationInfo</tt> creation and <tt>getName()</tt> accessor with <tt>null</tt> name.
    */
   public void testGetNameNull()
   {
      try
      {
         MBeanOperationInfo info1 = new MBeanOperationInfo(
               null,
               "This is a description.",
               new MBeanParameterInfo[] {
                        new MBeanParameterInfo("FooParam", "java.lang.Object", "description"),
                        new MBeanParameterInfo("BarParam", "java.lang.String", "description")
               },
               "java.util.StringBuffer",
               MBeanOperationInfo.ACTION_INFO
         );
         
         assertTrue(info1.getName() == null);
         
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
    * Tests <tt>MBeanOperationInfo</tt> creation and <tt>getDescription()</tt> accessor with <tt>null</tt> description.
    */
   public void testGetDescriptionNull()
   {
      try
      {
         MBeanOperationInfo info1 = new MBeanOperationInfo(
               "SomeName",
               null,
               new MBeanParameterInfo[] {
                        new MBeanParameterInfo("FooParam", "java.lang.Object", "description"),
                        new MBeanParameterInfo("BarParam", "java.lang.String", "description")
               },
               "java.util.StringBuffer",
               MBeanOperationInfo.ACTION_INFO
         );
         
         assertTrue(info1.getDescription() == null);
         
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
    * Tests <tt>MBeanOperationInfo</tt> creation and <tt>getImpact()</tt> accessor with invalid value.
    */
   public void testGetImpactInvalid()
   {
      try
      {
         MBeanOperationInfo info1 = new MBeanOperationInfo(
               "SomeName",
               "some description",
               new MBeanParameterInfo[] {
                        new MBeanParameterInfo("FooParam", "java.lang.Object", "description"),
                        new MBeanParameterInfo("BarParam", "java.lang.String", "description")
               },
               "java.util.StringBuffer",
               -22342
         );
         
         // according to javadoc, getImpact() is only allowed to return a value that matches
         // either ACTION, ACTION_INFO, INFO or UNKNOWN constant value.
         if (info1.getImpact() != MBeanOperationInfo.ACTION)
            if (info1.getImpact() != MBeanOperationInfo.INFO)
               if (info1.getImpact() != MBeanOperationInfo.ACTION_INFO)
                  if (info1.getImpact() != MBeanOperationInfo.UNKNOWN)
                     
                     // JPL: This fails in RI. The spec doesn't define how invalid impact types should be
                     //      handled. This could be checked at construction time (early) or at getImpact()
                     //      invocation time (late). Since behaviour is not specified, I've opted to check
                     //      late and throw an JMRuntimeException in case there is an invalid impact value.
                     fail("FAILS IN RI: MBeanOperation.getImpact() is only allowed to return values that match either ACTION, ACTION_INFO, INFO or UNKNOWN constant values.");
      
         // should not reach here unless -22342 has somehow become a valid impact value (in which case this test should be modified)
         fail("ERROR IN TEST: invalid impact value test does not work correctly.");               
      }
      catch (AssertionFailedError e)
      {
         throw e;
      }
      catch (JMRuntimeException e)
      {
         // should reach here due to invalid impact value
      }
      catch (Throwable t)
      {
         t.printStackTrace();
         fail("Unexpected error: " + t.toString());
      }
   }
   
   /**
    * Tests <tt>MBeanOperationInfo</tt> creation and <tt>getSignature()</tt> with <tt>null</tt> signature.
    */
   public void testGetSignatureNull()
   {
      try
      {
         MBeanOperationInfo info1 = new MBeanOperationInfo(
               "SomeName",
               "some description",
               null,
               "java.util.StringBuffer",
               MBeanOperationInfo.ACTION
         );
         
         assertTrue(info1.getSignature().length == 0);
         
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
    * Tests <tt>MBeanOperationInfo</tt> creation and <tt>getSignature()</tt> with empty signature array.
    */
   public void testGetSignatureEmpty()
   {
      try
      {
         MBeanOperationInfo info1 = new MBeanOperationInfo(
               "SomeName",
               "some description",
               new MBeanParameterInfo[0],
               "java.util.StringBuffer",
               MBeanOperationInfo.ACTION
         );
         
         assertTrue(info1.getSignature().length == 0);
         
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
    * Tests <tt>MBeanOperationInfo</tt> creation and <tt>getReturnType()</tt> with empty return type string.
    */
   public void testGetReturnTypeEmpty()
   {
      try
      {
         MBeanOperationInfo info1 = new MBeanOperationInfo(
               "SomeName",
               "some description",
               new MBeanParameterInfo[0],
               "",
               MBeanOperationInfo.ACTION
         );
         
         // JPL: IMHO empty or null strings should not be allowed as return type strings since they can
         //      never match any class name. However, RI allows both cases and I'm leaving JBossMX as is,
         //      unless and until the issue comes up somewhere else.
         assertTrue(info1.getReturnType().equals(""));
         
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
    * Tests <tt>MBeanOperationInfo</tt> creation and <tt>getReturnType()</tt> with <tt>null</tt> return type.
    */
   public void testGetReturnTypeNull()
   {
      try
      {
         MBeanOperationInfo info1 = new MBeanOperationInfo(
               "SomeName",
               "some description",
               new MBeanParameterInfo[0],
               null,
               MBeanOperationInfo.INFO
         );
         
         // JPL: IMHO empty or null strings should not be allowed as return type strings since they can
         //      never match any class name. However, RI allows both cases and I'm leaving JBossMX as is,
         //      unless and until the issue comes up somewhere else.
         assertTrue(info1.getReturnType() == null);
         
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
