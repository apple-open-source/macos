/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jrmp.test;

import java.io.File;
import java.net.URL;
import java.rmi.RemoteException;
import java.security.CodeSource;
import javax.ejb.CreateException;
import javax.naming.InitialContext;
import javax.naming.NamingException;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.jboss.test.JBossTestCase;

import org.jboss.test.jrmp.interfaces.IString;
import org.jboss.test.jrmp.interfaces.StatelessSession;
import org.jboss.test.jrmp.interfaces.StatelessSessionHome;

/**
 * Test of RMI dynamic class loading.
 *
 * @author    Scott.Stark@jboss.org
 * @author    Author: david jencks d_jencks@users.sourceforge.net
 * @version   $Revision: 1.5 $
 */
public class DynLoadingUnitTestCase
       extends JBossTestCase
{
   /**
    * Constructor for the DynLoadingUnitTestCase object
    *
    * @param name  Description of Parameter
    */
   public DynLoadingUnitTestCase(String name)
   {
      super(name);
   }

   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testAccess() throws Exception
   {
      InitialContext jndiContext = new InitialContext();
      getLog().debug("Lookup StatefulSession");
      Object obj = jndiContext.lookup("StatefulSession");
      StatelessSessionHome home = (StatelessSessionHome)obj;
      getLog().debug("Found StatefulSession Home");
      StatelessSession bean = home.create();
      getLog().debug("Created StatefulSession");
      IString echo = bean.copy("jrmp-dl");
      getLog().debug("bean.copy(jrmp-dl) = " + echo);
      Class clazz = echo.getClass();
      CodeSource cs = clazz.getProtectionDomain().getCodeSource();
      URL location = cs.getLocation();
      getLog().debug("IString.class = " + clazz);
      getLog().debug("IString.class location = " + location);
      assertTrue("CodeSource URL.protocol != file", location.getProtocol().equals("file") == false);
      bean.remove();
   }

   //not modified to use getJ2eeSetup since there is only one test.

   /**
    * Remove any local IString implementation so that we test RMI class loading.
    *
    * @exception Exception  Description of Exception
    */
   protected void setUp() throws Exception
   {
      super.setUp();
      URL istringImpl = getClass().getResource("/org/jboss/test/jrmp/ejb/AString.class");
      if (istringImpl != null)
      {
         getLog().debug("Found IString impl at: " + istringImpl);
         File implFile = new File(istringImpl.getFile());
         getLog().debug("Removed: " + implFile.delete());
      }
      deploy("jrmp-dl.jar");
   }

   /**
    * The teardown method for JUnit
    *
    * @exception Exception  Description of Exception
    */
   protected void tearDown() throws Exception
   {
      undeploy("jrmp-dl.jar");
      super.tearDown();
   }

}
