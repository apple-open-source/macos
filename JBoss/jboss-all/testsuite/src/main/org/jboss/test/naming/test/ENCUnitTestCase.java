/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.naming.test;

import java.io.IOException;
import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.EJBObject;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.rmi.PortableRemoteObject;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.jboss.test.JBossTestCase;
import org.jboss.test.naming.interfaces.TestENCHome;
import org.jboss.test.naming.interfaces.TestENCHome2;

/**
 * Tests of the secure access to EJBs.
 *
 * @author   Scott.Stark@jboss.org
 * @author   <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version $Revision: 1.4.4.1 $
 */
public class ENCUnitTestCase extends JBossTestCase
{
   /**
    * Constructor for the ENCUnitTestCase object
    *
    * @param name  Testcase name
    */
   public ENCUnitTestCase(String name)
   {
      super(name);
   }

   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testENC() throws Exception
   {
      Object obj = getInitialContext().lookup("ENCBean");
      obj = PortableRemoteObject.narrow(obj, TestENCHome.class);
      TestENCHome home = (TestENCHome)obj;
      getLog().debug("Found TestENCHome");

      EJBObject bean = home.create();
      getLog().debug("Created ENCBean");
      bean.remove();
   }

   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testENC2() throws Exception
   {
      Object obj = getInitialContext().lookup("ENCBean0");
      obj = PortableRemoteObject.narrow(obj, TestENCHome2.class);
      TestENCHome2 home = (TestENCHome2)obj;
      getLog().debug("Found TestENCHome2");

      EJBObject bean = home.create();
      getLog().debug("Created ENCBean0");
      bean.remove();
   }


   public static Test suite() throws Exception
   {
      return getDeploySetup(ENCUnitTestCase.class, "naming.jar");
   }



}
