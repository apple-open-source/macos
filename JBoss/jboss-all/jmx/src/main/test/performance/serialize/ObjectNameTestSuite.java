/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.performance.serialize;

import junit.framework.Test;
import junit.framework.TestSuite;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;

import javax.management.ObjectName;

/**
 * Tests the size and speed of ObjectName serialization
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class ObjectNameTestSuite
   extends TestSuite
{
   // Attributes ----------------------------------------------------------------

   // Constructor ---------------------------------------------------------------

   public static void main(String[] args)
   {
      junit.textui.TestRunner.run(suite());
   }

   /**
    * Construct the tests
    */
   public static Test suite()
   {
      TestSuite suite = new TestSuite("All Object Name tests");

      try
      {
         ObjectName name1 = new ObjectName("a:a=a");
         ObjectName name10 = new ObjectName("a:a=a,b=b,c=c,d=d,e=e,f=f,g=g,h=h,i=i,j=j");
         StringBuffer buffer = new StringBuffer("a:0=0");
         for (int i=1; i < 100; i++)
            buffer.append("," + i + "=" + i);
         ObjectName name100 = new ObjectName(buffer.toString());
         // Speed Tests
         suite.addTest(new SerializeTEST("testIt", name1, "ObjectName 1 property"));
         suite.addTest(new SerializeTEST("testIt", name10, "ObjectName 10 properties"));
         suite.addTest(new SerializeTEST("testIt", name100, "ObjectName 100 properties"));
      }
      catch (Exception e)
      {
         throw new Error(e.toString());
      }

      return suite;
   }
}
