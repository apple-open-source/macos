/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.modelmbean;

import junit.framework.TestCase;
import junit.framework.AssertionFailedError;

import javax.management.Descriptor;
import javax.management.modelmbean.DescriptorSupport;
import javax.management.modelmbean.ModelMBeanInfoSupport;
import javax.management.modelmbean.RequiredModelMBean;
import javax.management.RuntimeOperationsException;

/**
 * Tests the standard required <tt>DescriptorSupport</tt> implementation.
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.1 $   
 */
public class DescriptorTEST
   extends TestCase
{

   public DescriptorTEST(String s)
   {
      super(s);
   }

   /**
    * Tests the <tt>DescriptorSupport</tt> default constructor.
    */
   public void testDescriptorSupportDefaultConstructor()
   {
      try
      {
         Descriptor d1 = new DescriptorSupport();
      }
      catch (AssertionFailedError e)
      {
         throw e;
      }
      catch (Throwable t)
      {
         fail("Unexpected error: " + t.toString());
      }
   }
   
   /**
    * Tests the <tt>DescriptorSupport</tt> initial size constructor.
    */
   public void testDescriptorSupportInitialSizeConstructor()
   {
      try
      {
         Descriptor d2 = new DescriptorSupport(100);
        
         try
         {
            Descriptor d3 = new DescriptorSupport(0);
            
            // shouldn't reach here
            fail("RuntimeOperationsException should have been thrown when DescriptorSupport is created with zero initial size.");
         }
         catch (RuntimeOperationsException e)
         {
            // this is expected
         }
         
         try
         {
            Descriptor d4 = new DescriptorSupport(-100);
            
            // shouldn't reach here
            fail("RuntimeOperationsException should have been thrown when DescriptorSupport is created with negative initial size.");
         }
         catch (RuntimeOperationsException e)
         {
            // this is expected
         }
      }
      catch (AssertionFailedError e)
      {
         throw e;
      }
      catch (Throwable t)
      {
         fail("Unexpected error: " + t.toString());
      }
   }

   /**
    * Tests the <tt>DescriptorSupport</tt> copy constructor.
    */
   public void testDescriptorSupportCopyConstructor()
   {
      try
      {
         DescriptorSupport d1 = new DescriptorSupport();
         d1.setField("foo", "bar");
         d1.setField("wombat", new Integer(666));
         
         DescriptorSupport d2 = new DescriptorSupport(d1);
         assertTrue(d2.getFieldValue("foo").equals("bar"));
         assertTrue(d2.getFieldValue("wombat").equals(new Integer(666)));
         
         DescriptorSupport d4 = null;
         DescriptorSupport d3 = new DescriptorSupport(d4);
            
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
    * Tests clone.
    */
   public void testDescriptorSupportClone()
   {
      try
      {
         DescriptorSupport d = new DescriptorSupport();
         d.setField("foo", "bar");
         
         Descriptor clone = (Descriptor)d.clone();
         assertTrue(clone.getFieldValue("foo").equals("bar"));
      }
      catch (AssertionFailedError e)
      {
         throw e;
      }
      catch (Throwable t)
      {
         fail("Unexpected error: " + t.toString());
      }
   }
   
}
