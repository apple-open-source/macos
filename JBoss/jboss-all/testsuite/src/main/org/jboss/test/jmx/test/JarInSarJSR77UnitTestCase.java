
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jmx.test;


import javax.management.ObjectName;
import junit.framework.*;
import org.jboss.test.JBossTestCase;



/**
 * JarInSarJSR77UnitTestCase.java
 *
 *
 * Created: Sat Mar 2 23:54:55 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class JarInSarJSR77UnitTestCase extends JBossTestCase 
{
   public JarInSarJSR77UnitTestCase(String name)
   {
      super(name);
   }

   /**
    * The <code>testFakeParentCreatedAndRemoved</code> method tests if a package
    * consisting of a sar (which has no jsr-77 representation as of this writing) 
    * containing a jar (which does) gets deployed successfully with jsr-77 mbeans.
    * in particular, a fake Application parent for the jar should be created.
    * if and when the jsr77 support is extended to sars, this test should be reexamined.
    * 
    *
    * @exception Exception if an error occurs
    */
   public void testFakeParentCreatedAndRemoved() throws Exception
   {
      String testUrl = "jarinsar.sar";
      getLog().debug("testUrl is : " + testUrl);
      ObjectName fakeApp = new ObjectName("jboss.management.local:J2EEServer=Local,name=jarinsar.sar,j2eeType=J2EEApplication");
      ObjectName ejbModule = new ObjectName("jboss.management.local:name=jarinsar.jar,J2EEServer=Local,J2EEApplication=jarinsar.sar,j2eeType=EJBModule");
      ObjectName bean = new ObjectName("jboss.management.local:J2EEServer=Local,name=TestDataSource,J2EEApplication=jarinsar.sar,EJBModule=jarinsar.jar,j2eeType=StatelessSessionBean");

      //deploy the test package.
      deploy(testUrl);
      try 
      {
         assertTrue("fakeApp jsr-77 mbean is missing", getServer().isRegistered(fakeApp));
         assertTrue("ejbModule jsr-77 mbean is missing", getServer().isRegistered(ejbModule));
         assertTrue("bean jsr-77 mbean is missing", getServer().isRegistered(bean));
      }
      finally
      {
         undeploy(testUrl);
         assertTrue("fakeApp jsr-77 mbean is still present", !getServer().isRegistered(fakeApp));
         assertTrue("ejbModule jsr-77 mbean is still present", !getServer().isRegistered(ejbModule));
         assertTrue("bean jsr-77 mbean is still present", !getServer().isRegistered(bean));

      } // end of try-catch
      
   }


   
}// JarInSarJSR77UnitTestCase
