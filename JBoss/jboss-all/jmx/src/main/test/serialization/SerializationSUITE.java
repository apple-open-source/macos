/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package test.serialization;

import junit.framework.Test;
import junit.framework.TestSuite;

import java.io.File;
import java.net.URL;
import java.net.URLClassLoader;

/**
 * Serialization tests
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */

public class SerializationSUITE extends TestSuite
{

   public static ClassLoader jmxri;
   public static ClassLoader jbossmx;
   public static int form = 11; // 1.1

   public static void main(String[] args)
   {
      junit.textui.TestRunner.run(suite());
   }

   public static Test suite()
   {
      TestSuite suite = new TestSuite("All Serialization Tests");

      try
      {

         File riLocation = new File(System.getProperty("jboss.test.location.jmxri"));
         jmxri = new URLClassLoader(new URL[] {riLocation.toURL()},
                                    SerializationSUITE.class.getClassLoader());
         File jbossmxLocation = new File(System.getProperty("jboss.test.location.jbossmx"));
         jbossmx = new URLClassLoader(new URL[] {jbossmxLocation.toURL()},
                                    SerializationSUITE.class.getClassLoader());

         String prop = (String) System.getProperty("jmx.serial.form");
         if (prop != null && prop.equals("1.0"))
            form = 10; // 1.0
         System.err.println("Serialization Tests: jmx.serial.form=" + prop);
      
         suite.addTest(new TestSuite(SerializeTestCase.class));
      }
      catch (Exception e)
      {
         e.printStackTrace();
         throw new RuntimeException(e.toString());
      }
      
      return suite;
   }
}
