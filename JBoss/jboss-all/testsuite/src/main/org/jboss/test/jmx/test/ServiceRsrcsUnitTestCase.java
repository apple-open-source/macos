/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jmx.test;

import javax.management.ObjectName;
import junit.framework.*;
import org.jboss.test.JBossTestCase;

/** Tests of deployed services locating resources within their sar
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.3 $
 */
public class ServiceRsrcsUnitTestCase extends JBossTestCase
{
   // Constants -----------------------------------------------------
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------
   // Constructors --------------------------------------------------

   public ServiceRsrcsUnitTestCase(String name)
   {
      super(name);
   }

   // Public --------------------------------------------------------

   /** Test deployment of 
    *
    * @exception Exception  Description of Exception
    */
   public void testDeploySAR() throws Exception
   {
      ObjectName rsrc1 = new ObjectName("test:name=ResourceTsts1");
      ObjectName rsrc2 = new ObjectName("test:name=ResourceTsts2");
      String sar1 = "rsrc1.sar";
      String sar2 = "rsrc2.sar";
      String sar3 = "rsrc3.jar";
      String sar4 = "rsrc4.sar";

      // ResourceTsts case#1, a sar with the classes and resources
      try 
      {
         deploy(sar1);
         assertTrue("test:name=ResourceTsts1", getServer().isRegistered(rsrc1));
      }
      finally
      {
         undeploy(sar1);
      } // end of try-catch
      getLog().info("test 1 passed");

      // ResourceTsts case#2, a sar with the resources and the service in a nested jar
      try 
      {
         deploy(sar2);
         assertTrue("test:name=ResourceTsts2", getServer().isRegistered(rsrc2));
      }
      finally
      {
         undeploy(sar2);
      } // end of try-catch
      getLog().info("test 2 passed");

      /** ResourceTsts case#3, a jar with two sars with only
      the service descriptor metadata and a jar containing the
      service code.
      */
      try 
      {
         deploy(sar3);
         assertTrue("test:name=ResourceTsts1", getServer().isRegistered(rsrc1));
         assertTrue("test:name=ResourceTsts2", getServer().isRegistered(rsrc2));
      }
      finally
      {
         undeploy(sar3);
      } // end of try-catch
      getLog().info("test 3 passed");

      /** ResourceTsts case#4, a sar with a service descriptor with
      mulitiple mbeans, a resource file with multiple configs
      using a namespace, and the service code in a nested jar
      */
      try 
      {
         deploy(sar4);
         assertTrue("test:name=ResourceTsts1", getServer().isRegistered(rsrc1));
         assertTrue("test:name=ResourceTsts2", getServer().isRegistered(rsrc2));
      }
      finally
      {
         undeploy(sar4);
      } // end of try-catch
      getLog().info("test 4 passed");
   }

}
