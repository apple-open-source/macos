/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.metadata;


import junit.framework.TestCase;
import junit.framework.AssertionFailedError;

import javax.management.MBeanFeatureInfo;

/**
 * Tests MBeanFeatureInfo.
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.1 $ 
 */
public class MBeanFeatureInfoTEST extends TestCase
{
   public MBeanFeatureInfoTEST(String s)
   {
      super(s);
   }

   /**
    * Tests <tt>MBeanOperationInfo(String descr, Method m)</tt> constructor.
    */
   public void testConstructor()
   {
      try 
      {
         MBeanFeatureInfo info = new MBeanFeatureInfo("Name", "This is a description.");
         
         assertTrue(info.getName().equals("Name"));
         assertTrue(info.getDescription().equals("This is a description."));
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
