/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.naming.test;

import javax.naming.InitialContext;
import javax.rmi.PortableRemoteObject;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.jboss.test.JBossTestCase;
import org.jboss.test.naming.interfaces.TestEjbLinkHome;
import org.jboss.test.naming.interfaces.TestEjbLink;

/**
 * Tests of EjbLink.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 */
public class EjbLinkUnitTestCase extends JBossTestCase
{
   /**
    * Constructor for the test
    *
    * @param name  Testcase name
    */
   public EjbLinkUnitTestCase(String name)
   {
      super(name);
   }

   /**
    * Test an ejblink
    *
    * @exception Exception  Description of Exception
    */
   public void testEjbLinkNamed() throws Exception
   {
      Object obj = getInitialContext().lookup("naming/SessionB");
      obj = PortableRemoteObject.narrow(obj, TestEjbLinkHome.class);
      TestEjbLinkHome home = (TestEjbLinkHome)obj;
      getLog().debug("Found naming/SessionB");

      TestEjbLink bean = home.create();
      getLog().debug("Created the bean");
      assertEquals("Works", bean.testEjbLinkCaller("java:comp/env/ejb/SessionA"));
      getLog().debug("Test succeeded");
      bean.remove();
   }

   /**
    * Test an ejblink with a relative path
    *
    * @exception Exception  Description of Exception
    */
   public void testEjbLinkRelative() throws Exception
   {
      Object obj = getInitialContext().lookup("naming/SessionB");
      obj = PortableRemoteObject.narrow(obj, TestEjbLinkHome.class);
      TestEjbLinkHome home = (TestEjbLinkHome)obj;
      getLog().debug("Found naming/SessionB");

      TestEjbLink bean = home.create();
      getLog().debug("Created the bean");
      assertEquals("Works", bean.testEjbLinkCaller("java:comp/env/ejb/RelativeSessionA"));
      getLog().debug("Test succeeded");
      bean.remove();
   }

   /**
    * Test an ejblink using a local ejb-ref
    *
    * @exception Exception  Description of Exception
    */
   public void testEjbLinkLocalNamed() throws Exception
   {
      Object obj = getInitialContext().lookup("naming/SessionB");
      obj = PortableRemoteObject.narrow(obj, TestEjbLinkHome.class);
      TestEjbLinkHome home = (TestEjbLinkHome)obj;
      getLog().debug("Found naming/SessionB");

      TestEjbLink bean = home.create();
      getLog().debug("Created the bean");
      assertEquals("Works", bean.testEjbLinkCallerLocal("java:comp/env/ejb/LocalSessionA"));
      getLog().debug("Test succeeded");
      bean.remove();
   }

   /**
    * Test an ejblink using a local ejb-ref with a relative path
    *
    * @exception Exception  Description of Exception
    */
   public void testEjbLinkLocalRelative() throws Exception
   {
      Object obj = getInitialContext().lookup("naming/SessionB");
      obj = PortableRemoteObject.narrow(obj, TestEjbLinkHome.class);
      TestEjbLinkHome home = (TestEjbLinkHome)obj;
      getLog().debug("Found naming/SessionB");

      TestEjbLink bean = home.create();
      getLog().debug("Created the bean");
      assertEquals("Works", bean.testEjbLinkCallerLocal("java:comp/env/ejb/LocalRelativeSessionA"));
      getLog().debug("Test succeeded");
      bean.remove();
   }


    public void testEjbNoLinkLocal () throws Exception
    {      
        Object obj = getInitialContext().lookup("naming/SessionB");
        obj = PortableRemoteObject.narrow(obj, TestEjbLinkHome.class);
        TestEjbLinkHome home = (TestEjbLinkHome)obj;
        getLog().debug("Found naming/SessionB");

      TestEjbLink bean = home.create();
      getLog().debug("Created the bean");      
      assertEquals("Works", bean.testEjbLinkCallerLocal("java:comp/env/ejb/NoLinkLocalSessionA"));     
      getLog().debug("Test succeeded");
      bean.remove();
    }


    public void testEjbNoLink () throws Exception
    {      
        Object obj = getInitialContext().lookup("naming/SessionB");
        obj = PortableRemoteObject.narrow(obj, TestEjbLinkHome.class);
        TestEjbLinkHome home = (TestEjbLinkHome)obj;
        getLog().debug("Found naming/SessionB");

        TestEjbLink bean = home.create();
        getLog().debug("Created the bean");
        assertEquals("Works", bean.testEjbLinkCaller("java:comp/env/ejb/NoLinkSessionA"));
        getLog().debug("Test succeeded");
        bean.remove();
    }

    public void testEjbNames() throws Exception
    {      
        Object obj = getInitialContext().lookup("naming/SessionB");
        obj = PortableRemoteObject.narrow(obj, TestEjbLinkHome.class);
        TestEjbLinkHome home = (TestEjbLinkHome) obj;
        getLog().debug("Found naming/SessionB home: "+home);

       obj = getInitialContext().lookup("naming/SessionB1");
       obj = PortableRemoteObject.narrow(obj, TestEjbLinkHome.class);
       home = (TestEjbLinkHome) obj;
       getLog().debug("Found naming/SessionB1 home: "+home);
    }

   public static Test suite() throws Exception
   {
      return getDeploySetup(EjbLinkUnitTestCase.class, "naming.ear");
   }
}
